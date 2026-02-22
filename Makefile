EE_BIN = atari2600.elf
EE_OBJS = src/main.o src/emulator.o src/cpu6507.o src/tia.o src/riot.o src/cartridge.o src/ui.o

# Percorsi PS2SDK
PS2SDK ?= /usr/local/ps2dev/ps2sdk
GSKIT ?= /usr/local/ps2dev/gsKit

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(GSKIT)/include -Isrc
EE_LDFLAGS := -L$(PS2SDK)/ee/lib -L$(GSKIT)/lib
EE_LIBS = -lgskit -ldmakit -lpad -lfileXio -lpatches -lpng -lz -lm

# IRX modules
EE_IRX_FILES = iomanX.irx fileXio.irx sio2man.irx padman.irx usbd.irx usbhdfsd.irx

EE_CFLAGS = -D_EE -O2 -G0 -Wall -Wno-unused-variable

# Regole
all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)
	$(EE_CC) $(EE_CFLAGS) $(EE_LDFLAGS) -o $@ $^ $(EE_LIBS)

%.o: %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

clean:
	rm -f $(EE_OBJS) $(EE_BIN)

rebuild: clean all

.PHONY: all clean rebuild

# Include PS2SDK makefiles se disponibili
-include $(PS2SDK)/samples/Makefile.pref
-include $(PS2SDK)/samples/Makefile.eeglobal
