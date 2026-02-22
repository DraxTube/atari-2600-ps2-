EE_PREFIX = mips64r5900el-ps2-elf
EE_CC     = $(EE_PREFIX)-gcc
EE_SIZE   = $(EE_PREFIX)-size

PS2DEV   ?= /usr/local/ps2dev
PS2SDK   ?= $(PS2DEV)/ps2sdk

EE_BIN   = haunted2600.elf
EE_OBJS  = src/main.o src/cpu6507.o src/tia.o src/pia.o src/bus.o

EE_INCS  = -I$(PS2SDK)/ee/include \
           -I$(PS2SDK)/common/include \
           -I$(PS2SDK)/ports/include \
           -Isrc

EE_CFLAGS = -O2 -Wall -G0 \
            -mno-abicalls -mno-gpopt \
            -march=r5900 -mabi=eabi \
            -fno-common \
            $(EE_INCS)

EE_LDFLAGS = -L$(PS2SDK)/ee/lib \
             -L$(PS2SDK)/ports/lib \
             -T$(PS2SDK)/ee/startup/linkfile \
             -mno-abicalls -mno-gpopt \
             -march=r5900 -mabi=eabi

EE_LIBS  = -lpad -lgraph -ldraw -ldraw3d -lpacket -ldma \
           -lfileXio -lloadfile -lsifrpc -lkernel \
           -lc -lm

EE_STARTUP = $(PS2SDK)/ee/startup/crt0.o

all: $(EE_BIN)

$(EE_BIN): $(EE_OBJS)
	$(EE_CC) $(EE_LDFLAGS) -o $@ $(EE_STARTUP) $(EE_OBJS) $(EE_LIBS)
	$(EE_SIZE) $@

src/%.o: src/%.c
	$(EE_CC) $(EE_CFLAGS) -c -o $@ $<

clean:
	rm -f $(EE_OBJS) $(EE_BIN)
