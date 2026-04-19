# GameSpy Presence & Messaging SDK Makefile
# Copyright 2004 GameSpy Industries

PROJECT=gpsdk

CC=gcc
BASE_CFLAGS=-D_LINUX

#use these cflags to optimize it
CFLAGS=$(BASE_CFLAGS) -march=i486 -O6 -ffast-math -funroll-loops \
	-fomit-frame-pointer -fexpensive-optimizations -falign-loops=2 \
	-falign-jumps=2 -falign-functions=2 -lpthread
#use these when debugging 
#CFLAGS=$(BASE_CFLAGS) -g

PROG_OBJS = \
	../../../md5c.o\
	../../../darray.o\
	../../../hashtable.o\
	../../../common/gsAvailable.o\
	../../../common/gsStringUtil.o\
	../../../common/gsPlatform.o\
	../../../common/gsPlatformSocket.o\
	../../../common/gsPlatformThread.o\
	../../../common/gsPlatformUtil.o\
	../../../common/gsMemory.o\
	../../../common/linux/LinuxCommon.o\
	../../gp.o\
	../../gpi.o\
	../../gpiBuddy.o\
	../../gpiBuffer.o\
	../../gpiCallback.o\
	../../gpiConnect.o\
	../../gpiInfo.o\
	../../gpiOperation.o\
	../../gpiPeer.o\
	../../gpiProfile.o\
	../../gpiSearch.o\
	../../gpiUnique.o\
	../../gpiUtility.o\
	../../gpiTransfer.o\
	../gptestc.o

#############################################################################
# SETUP AND BUILD
#############################################################################

$(PROJECT): $(PROG_OBJS)        
	$(CC) $(CFLAGS) -o $@ $(PROG_OBJS)

#############################################################################
# MISC
#############################################################################

clean:	
	rm -f $(PROG_OBJS)

depend:
	gcc -MM $(PROG_OBJS:.o=.c)

