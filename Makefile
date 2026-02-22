# Makefile per haunted2600-ps2
# Richiede ps2dev toolchain installata (ps2sdk)

EE_BIN   = haunted2600.elf
EE_OBJS  = src/main.o src/cpu6507.o src/tia.o src/pia.o src/bus.o

# Librerie ps2sdk
EE_LIBS  = -lpad -lgraph -ldraw -ldraw3d -lpacket -ldma \
           -lfileXio -lloadfile -lsifrpc -lkernel \
           -lc -lm

EE_INCS  = -I$(PS2SDK)/ee/include \
           -I$(PS2SDK)/common/include \
           -I$(PS2SDK)/ports/include \
           -Isrc

EE_CFLAGS = -O2 -Wall -Wextra -G0

# Definisce NTSC
EE_CFLAGS += -DNTSC

# Link con ps2 startup
EE_LDFLAGS = -L$(PS2SDK)/ee/lib -L$(PS2SDK)/ports/lib

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

clean:
	rm -f $(EE_OBJS) $(EE_BIN)
