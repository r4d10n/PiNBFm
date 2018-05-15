/*
 * PiFmRds - FM/RDS transmitter for the Raspberry Pi
 * Copyright (C) 1028 Evariste Courjaud, F5OEO	
 * Copyright (C) 2014, 2015 Christophe Jacquet, F8FTK
 * Copyright (C) 2012, 2015 Richard Hirst
 * Copyright (C) 2012 Oliver Mattos and Oskar Weigl
 *
 * See https://github.com/ChristopheJacquet/PiFmRds
 *
 * PI-FM-RDS: RaspberryPi FM transmitter, with RDS. 
 *
 * This file contains the VHF FM modulator. All credit goes to the original
 * authors, Oliver Mattos and Oskar Weigl for the original idea, and to
 * Richard Hirst for using the Pi's DMA engine, which reduced CPU usage
 * dramatically.
 *
 * I (Christophe Jacquet) have adapted their idea to transmitting samples
 * at 228 kHz, allowing to build the 57 kHz subcarrier for RDS BPSK data.
 *
 * To make it work on the Raspberry Pi 2, I used a fix by Richard Hirst
 * (again) to request memory using Broadcom's mailbox interface. This fix
 * was published for ServoBlaster here:
 * https://www.raspberrypi.org/forums/viewtopic.php?p=699651#p699651
 *
 * Never use this to transmit VHF-FM data through an antenna, as it is
 * illegal in most countries. This code is for testing purposes only.
 * Always connect a shielded transmission line from the RaspberryPi directly
 * to a radio receiver, so as *not* to emit radio waves.
 *
 * ---------------------------------------------------------------------------
 * These are the comments from Richard Hirst's version:
 *
 * RaspberryPi based FM transmitter.  For the original idea, see:
 *
 * http://www.icrobotics.co.uk/wiki/index.php/Turning_the_Raspberry_Pi_Into_an_FM_Transmitter
 *
 * All credit to Oliver Mattos and Oskar Weigl for creating the original code.
 * 
 * I have taken their idea and reworked it to use the Pi DMA engine, so
 * reducing the CPU overhead for playing a .wav file from 100% to about 1.6%.
 *
 * I have implemented this in user space, using an idea I picked up from Joan
 * on the Raspberry Pi forums - credit to Joan for the DMA from user space
 * idea.
 *
 * The idea of feeding the PWM FIFO in order to pace DMA control blocks comes
 * from ServoBlaster, and I take credit for that :-)
 *
 * This code uses DMA channel 5 and the PWM hardware, with no regard for
 * whether something else might be trying to use it at the same time (such as
 * the 3.5mm jack audio driver).
 *
 * I know nothing much about sound, subsampling, or FM broadcasting, so it is
 * quite likely the sound quality produced by this code can be improved by
 * someone who knows what they are doing.  There may be issues realting to
 * caching, as the user space process just writes to its virtual address space,
 * and expects the DMA controller to see the data; it seems to work for me
 * though.
 *
 * NOTE: THIS CODE MAY WELL CRASH YOUR PI, TRASH YOUR FILE SYSTEMS, AND
 * POTENTIALLY EVEN DAMAGE YOUR HARDWARE.  THIS IS BECAUSE IT STARTS UP THE DMA
 * CONTROLLER USING MEMORY OWNED BY A USER PROCESS.  IF THAT USER PROCESS EXITS
 * WITHOUT STOPPING THE DMA CONTROLLER, ALL HELL COULD BREAK LOOSE AS THE
 * MEMORY GETS REALLOCATED TO OTHER PROCESSES WHILE THE DMA CONTROLLER IS STILL
 * USING IT.  I HAVE ATTEMPTED TO MINIMISE ANY RISK BY CATCHING SIGNALS AND
 * RESETTING THE DMA CONTROLLER BEFORE EXITING, BUT YOU HAVE BEEN WARNED.  I
 * ACCEPT NO LIABILITY OR RESPONSIBILITY FOR ANYTHING THAT HAPPENS AS A RESULT
 * OF YOU RUNNING THIS CODE.  IF IT BREAKS, YOU GET TO KEEP ALL THE PIECES.
 *
 * NOTE ALSO:  THIS MAY BE ILLEGAL IN YOUR COUNTRY.  HERE ARE SOME COMMENTS
 * FROM MORE KNOWLEDGEABLE PEOPLE ON THE FORUM:
 *
 * "Just be aware that in some countries FM broadcast and especially long
 * distance FM broadcast could get yourself into trouble with the law, stray FM
 * broadcasts over Airband aviation is also strictly forbidden."
 *
 * "A low pass filter is really really required for this as it has strong
 * harmonics at the 3rd, 5th 7th and 9th which sit in licensed and rather
 * essential bands, ie GSM, HAM, emergency services and others. Polluting these
 * frequencies is immoral and dangerous, whereas "breaking in" on FM bands is
 * just plain illegal."
 *
 * "Don't get caught, this GPIO use has the potential to exceed the legal
 * limits by about 2000% with a proper aerial."
 *
 *
 * As for the original code, this code is released under the GPL.
 *
 * Richard Hirst <richardghirst@gmail.com>  December 2012
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sndfile.h>

extern "C"
{
#include "fm_mpx.h"
}

#include "librpitx/src/librpitx.h"

ngfmdmasync *fmmod;
// The deviation specifies how wide the signal is. 
// Use 75kHz for WBFM (broadcast radio) 
// and about 2.5kHz for NBFM (walkie-talkie style radio)
//#define DEVIATION        75000
//FOR NBFM
//#define DEVIATION 	   2500
//for DMR
//#define DEVIATION        6250 

static void
terminate(int num)
{
    delete fmmod;
    fm_mpx_close();
    
    exit(num);
}

static void
fatal(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    terminate(0);
}

#define SUBSIZE 512
#define DATA_SIZE 5000


int tx(uint32_t carrier_freq, char *audio_file, float deviation)  {
    // Catch all signals possible - it is vital we kill the DMA engine
    // on process exit!
    for (int i = 0; i < 64; i++) {
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = terminate;
        sigaction(i, &sa, NULL);
    }

    // Data structures for baseband data
    float data[DATA_SIZE];
    float devfreq[DATA_SIZE];
    //float max_devfreq =0, min_devfreq=0;
    int data_len = 0;

    // Initialize the baseband generator
    if(fm_mpx_open(audio_file, DATA_SIZE) < 0) return 1;
    
    printf("Starting to transmit on %3.1f MHz.\n", carrier_freq/1e6);

	
    float deviation_scale_factor;
    //if( divider ) // PLL modulation
    {   // note samples are [-10:10]
        deviation_scale_factor=  0.1 * (deviation) ; // todo PPM
    }
    
    for (;;) 	{
			
	if( fm_mpx_get_samples(data) < 0 ) {
                    terminate(0);
        }

        data_len = DATA_SIZE;
	
	for(int i=0;i< data_len;i++) {
			
        	devfreq[i] = data[i]*deviation_scale_factor;
		
		/*if (devfreq[i] > max_devfreq)
			max_devfreq = devfreq[i];
		else if (devfreq[i] < min_devfreq)				
			min_devfreq = devfreq[i];		*/
        }	

		//printf("min: %f | max: %f \n",min_devfreq, max_devfreq);

	fmmod->SetFrequencySamples(devfreq,data_len);

	}    

    return 0;
}


int main(int argc, char **argv) {
    char *audio_file = NULL;
    uint32_t carrier_freq = 144500000;  
    float deviation = 6250;	// default for DMR ? 

    // Parse command-line arguments
    for(int i=1; i<argc; i++) {
        char *arg = argv[i];
        char *param = NULL;
        
        if(arg[0] == '-' && i+1 < argc) param = argv[i+1];
        
        if((strcmp("-wav", arg)==0 || strcmp("-audio", arg)==0) && param != NULL) {
            i++;
            audio_file = param;
        } else if(strcmp("-freq", arg)==0 && param != NULL) {
            i++;
            carrier_freq = 1e6 * atof(param);
            //if(carrier_freq < 76e6 || carrier_freq > 108e6)
              //  fatal("Incorrect frequency specification. Must be in megahertz, of the form 107.9, between 76 and 108.\n");
        } else if(strcmp("-dev", arg)==0 && param != NULL) {
	    i++;
 	    deviation = atof(param);
        }
	else {
            fatal("Unrecognised argument: %s.\n"
            "Syntax: pi_fm [-freq <freq in MHz> (def:144.5)] [-audio <file>] [-dev <deviation in Hz> (def:6250)] \n", arg);
        }
    }
	int FifoSize=DATA_SIZE*2;
    
    fmmod=new ngfmdmasync(carrier_freq,228000,14,FifoSize);
    int errcode = tx(carrier_freq,  audio_file, deviation);
    
    terminate(errcode);
}
