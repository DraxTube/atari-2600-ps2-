EE_BIN = atari2600.elf

EE_OBJS = \
	src/main.o \
	src/emulator.o \
	src/cpu6507.o \
	src/tia.o \
	src/riot.o \
	src/cartridge.o \
	src/ui.o

PS2SDK ?= /usr/local/ps2dev/ps2sdk

# IRX modules
EE_IRX_OBJS = \
	usbd_irx.o \
	usbhdfsd_irx.o

EE_OBJS += $(EE_IRX_OBJS)

EE_INCS = -Isrc -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include

EE_LIBS = -ldebug -lpad -lpatches -lfileXio -lc -lkernel

EE_CFLAGS += -D_EE -O2 -Wall -Wno-unused-variable -Wno-unused-function

all: $(EE_BIN)

clean:
	rm -f $(EE_OBJS) src/*.o $(EE_BIN)

# Embed IRX files
%_irx.o: $(PS2SDK)/iop/irx/%.irx
	bin2o $< $@ $*_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
