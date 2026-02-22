# ============================================
# Atari 2600 Emulator for PS2
# ============================================

EE_BIN = atari2600.elf

EE_OBJS = \
	src/main.o \
	src/emulator.o \
	src/cpu6507.o \
	src/tia.o \
	src/riot.o \
	src/cartridge.o \
	src/ui.o

# Percorsi
PS2SDK ?= /usr/local/ps2dev/ps2sdk
GSKIT  ?= /usr/local/ps2dev/gsKit

# Include
EE_INCS = \
	-Isrc \
	-I$(GSKIT)/include

# Librerie
EE_LDFLAGS = -L$(GSKIT)/lib
EE_LIBS = -lgskit -ldmakit -lpad -lfileXio -lpatches -lm -lc

# Flag di compilazione
EE_CFLAGS += -D_EE -O2 -Wall -Wno-unused-variable -Wno-unused-function

# Target
all: $(EE_BIN)

clean:
	rm -f $(EE_OBJS) $(EE_BIN)

# Include PS2SDK build system (DEVE essere in fondo)
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
