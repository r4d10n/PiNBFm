CC = gcc
STD_CFLAGS = -Wall -std=gnu99 -c -g -O3 -I../..


# Enable ARM-specific options only on ARM, and compilation of the app only on ARM
UNAME := $(shell uname -m)

# Determine the hardware platform. Below, pi1 stands for the RaspberryPi 1 (the original one),
# and pi2 stands for both the RaspberryPi 2 and 3.
ifeq ($(UNAME), armv6l)
	CFLAGS = $(STD_CFLAGS) -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -ffast-math -DRASPI=1
	TARGET = pi1
else ifeq ($(UNAME), armv7l)
	CFLAGS = $(STD_CFLAGS) -march=armv7-a -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -ffast-math -DRASPI=2
	TARGET = pi2
else
	CFLAGS = $(STD_CFLAGS)
	TARGET = other
endif

ifneq ($(TARGET), other)

app:  pi_nbfm.o fm_mpx.o mailbox.o
	g++ -Wall -g -O3 -o pi_nbfm pi_nbfm.o fm_mpx.o -I../.. ../../librpitx/src/librpitx.a -lm -lsndfile -lrt -lpthread 

endif

mailbox.o: mailbox.c mailbox.h
	$(CC) $(CFLAGS) mailbox.c

pi_nbfm.o: pi_nbfm.cpp fm_mpx.h 
	g++ -Wall -c -g -O3 pi_nbfm.cpp -I../..

fm_mpx.o: fm_mpx.c fm_mpx.h
	$(CC) $(CFLAGS) fm_mpx.c

clean:
	rm -f *.o
	rm pi_nbfm
