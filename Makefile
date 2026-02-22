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

EE_INCS := -Isrc -I$(GSKIT)/include -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I.

EE_LDFLAGS := -L$(GSKIT)/lib -L$(PS2SDK)/ee/lib

EE_LIBS = -lgskit -ldmakit -ldebug -lpad -lpatches -lfileXio -lc -lkernel

EE_CFLAGS := -D_EE -O2 -Wall -Wno-unused-variable -Wno-unused-function $(EE_INCS)

all: $(EE_BIN)

clean:
	rm -f $(EE_OBJS) src/*.o $(EE_BIN) *_irx.c *_irx.o

# Source files
src/%.o: src/%.c
	$(EE_CC) $(EE_CFLAGS) -c $< -o $@

# IRX modules
sio2man_irx.c:
	bin2c $(PS2SDK)/iop/irx/sio2man.irx sio2man_irx.c sio2man_irx

sio2man_irx.o: sio2man_irx.c
	$(EE_CC) $(EE_CFLAGS) -c sio2man_irx.c -o sio2man_irx.o

padman_irx.c:
	bin2c $(PS2SDK)/iop/irx/padman.irx padman_irx.c padman_irx

padman_irx.o: padman_irx.c
	$(EE_CC) $(EE_CFLAGS) -c padman_irx.c -o padman_irx.o

usbd_irx.c:
	bin2c $(PS2SDK)/iop/irx/usbd.irx usbd_irx.c usbd_irx

usbd_irx.o: usbd_irx.c
	$(EE_CC) $(EE_CFLAGS) -c usbd_irx.c -o usbd_irx.o

usbhdfsd_irx.c:
	bin2c $(PS2SDK)/iop/irx/usbhdfsd.irx usbhdfsd_irx.c usbhdfsd_irx

usbhdfsd_irx.o: usbhdfsd_irx.c
	$(EE_CC) $(EE_CFLAGS) -c usbhdfsd_irx.c -o usbhdfsd_irx.o

# Link
$(EE_BIN): $(EE_OBJS)
	$(EE_CC) -T$(PS2SDK)/ee/startup/linkfile $(EE_LDFLAGS) -o $@ $(EE_OBJS) $(EE_LIBS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal_cpp
