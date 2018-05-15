Pi-NBFM
=======

## Narrow-band FM transmitter using the Raspberry Pi

This is a minimal stripped version of the Pi-FM-RDS built on LibRPiTX by Evariste F5OEO. 
For original version, see [<F5OEO/PiFmRds>](https://github.com/F5OEO/PiFmRds).


## How to use it?

Pi-FM-RDS, depends on the `sndfile` library. To install this library on Debian-like distributions, for instance Raspbian, run `sudo apt-get install libsndfile1-dev`.

Pi-FM-RDS also depends on the Linux `rpi-mailbox` driver, so you need a recent Linux kernel. The Raspbian releases from August 2015 have this.

**Important.** The binaries compiled for the Raspberry Pi 1 are not compatible with the Raspberry Pi 2/3, and conversely. Always re-compile when switching models, so do not skip the `make clean` step in the instructions below!

Clone the source repository and run `make` in the `src` directory:

```bash
git clone https://github.com/r4d10n/PiNBFm.git
git clone https://github.com/F5OEO/librpitx.git
cd librpitx/src
make
cd ../../
cd PiNBFm/src
make clean
make
```

Then you can just run:

```
sudo ./pi_nbfm -freq <freq in MHz> -audio <file> -dev <deviation in Hz>
```

```
sudo ./pi_nbfm -audio <file>
```

This default setting will generate an FM transmission on 144.5 MHz, with deviation 6250 Hz. The radiofrequency signal is emitted on GPIO 4 (pin 7 on header P1).




### References

* [EN 50067, Specification of the radio data system (RDS) for VHF/FM sound broadcasting in the frequency range 87.5 to 108.0 MHz](http://www.interactive-radio-system.com/docs/EN50067_RDS_Standard.pdf)


--------

© [Christophe Jacquet](http://www.jacquet80.eu/) (F8FTK), 2014-2015. Released under the GNU GPL v3.
