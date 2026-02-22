EE_BIN = atari2600.elf

EE_OBJS = \
	src/main.o \
	src/emulator.o \
	src/cpu6507.o \
	src/tia.o \
	src/riot.o \
	src/cartridge.o \
	src/ui.o \
	usbd_irx.o \
	usbhdfsd_irx.o

PS2SDK ?= /usr/local/ps2dev/ps2sdk

EE_INCS = -Isrc -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I.

EE_LIBS = -ldebug -lpad -lpatches -lfileXio -lc -lkernel

EE_CFLAGS += -D_EE -O2 -Wall -Wno-unused-variable -Wno-unused-function

all: $(EE_BIN)

clean:
	rm -f $(EE_OBJS) src/*.o $(EE_BIN) *_irx.c *_irx.o

# Generate C source from IRX, then compile
usbd_irx.c: $(PS2SDK)/iop/irx/usbd.irx
	bin2c $< $@ usbd_irx

usbhdfsd_irx.c: $(PS2SDK)/iop/irx/usbhdfsd.irx
	bin2c $< $@ usbhdfsd_irx

usbd_irx.o: usbd_irx.c
	$(EE_CC) $(EE_CFLAGS) -c $< -o $@

usbhdfsd_irx.o: usbhdfsd_irx.c
	$(EE_CC) $(EE_CFLAGS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
