EE_BIN = atari2600.elf
EE_OBJS = src/main.o src/emulator.o src/cpu6507.o src/tia.o src/riot.o src/cartridge.o src/ui.o

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(GSKIT)/include
EE_LDFLAGS := -L$(PS2SDK)/ee/lib -L$(GSKIT)/lib
EE_LIBS = -lgskit -ldmakit -lpad -lfileXio -lpatches -lpng -lz -lm

EE_IRX_FILES = iomanX.irx fileXio.irx sio2man.irx padman.irx usbd.irx usbhdfsd.irx

EE_CFLAGS = -D_EE -O2 -G0 -Wall

all: $(EE_BIN)

clean:
	rm -f $(EE_OBJS) $(EE_BIN)

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
