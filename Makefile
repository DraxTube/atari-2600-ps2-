EE_BIN = atari2600.elf

EE_OBJS = \
	src/main.o \
	src/emulator.o \
	src/cpu6507.o \
	src/tia.o \
	src/riot.o \
	src/cartridge.o \
	src/ui.o \
	sio2man_irx.o \
	padman_irx.o \
	usbd_irx.o \
	usbhdfsd_irx.o

PS2SDK ?= /usr/local/ps2dev/ps2sdk
GSKIT  ?= /usr/local/ps2dev/gsKit

EE_INCS := -I$(GSKIT)/include -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -Isrc -I.
EE_LDFLAGS := -L$(GSKIT)/lib -L$(PS2SDK)/ee/lib
EE_LIBS := -lgskit -ldmakit -ldebug -lpad -lpatches -lfileXio -lc -lkernel

EE_CFLAGS += -D_EE -O2 -Wall -Wno-unused-variable -Wno-unused-function

all: $(EE_BIN)

clean:
	rm -f $(EE_OBJS) $(EE_BIN) *_irx.c *_irx.o

# IRX modules
sio2man_irx.c:
	bin2c $(PS2SDK)/iop/irx/sio2man.irx sio2man_irx.c sio2man_irx

padman_irx.c:
	bin2c $(PS2SDK)/iop/irx/padman.irx padman_irx.c padman_irx

usbd_irx.c:
	bin2c $(PS2SDK)/iop/irx/usbd.irx usbd_irx.c usbd_irx

usbhdfsd_irx.c:
	bin2c $(PS2SDK)/iop/irx/usbhdfsd.irx usbhdfsd_irx.c usbhdfsd_irx

%_irx.o: %_irx.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
