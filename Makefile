#
#	Makefile for QNX native ipc course module
#

# build option variables
DEBUG = -g

# specification of compiler and linker  
CC = qcc
LD = qcc

# specifications for target platform and compiler version
TARGET = -Vgcc_ntox86_64
#TARGET = -Vgcc_ntoarmv7le
#TARGET = -Vgcc_ntoaarch64le


# compile and link options
CFLAGS += $(TARGET) $(DEBUG) -Wall
LDFLAGS+= $(TARGET) $(DEBUG)

# binaries to be built
BINS = ipc_receivefile ipc_sendfile

# uncomment for the pulse client and server exercise:
#BINS += pulse_server

# uncomment for the name_attach/name_open exerise:
#BINS += name_lookup_server name_lookup_client

# uncomment for the multi-part message exercise:
#BINS += iov_client iov_server

# make target to build all
all: $(BINS)

# make target to clean up object files, binaries and stripped (.tmp) files
#clean:
#	rm -f *.o $(BINS)
#	cd solutions && make clean_solutions

# dependencies

ipc_receivefile.o: ipc_receivefile.c ipc_common.c
ipc_sendfile.o: ipc_sendfile.c ipc_common.c

