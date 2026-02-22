#include "bus.h"
#include <stdio.h>
#include <string.h>

Atari2600 atari;

void bus_init(void) {
    memset(&atari, 0, sizeof(Atari2600));
    tia_init(&atari.tia);
    pia_init(&atari.pia);
}

// ── Decode indirizzo 13-bit ───────────────────────────────────────────────────
uint8_t bus_read(uint16_t addr) {
    addr &= 0x1FFF;

    // ROM: bit12 alto
    if (addr & 0x1000) return atari.rom[addr & 0x0FFF];

    // PIA RAM: A7=1, A9=0
    if ((addr & 0x0280) == 0x0080) return pia_read(&atari.pia, 0x80 | (addr & 0x7F));

    // PIA I/O: A7=0, A9=1
    if ((addr & 0x0280) == 0x0200) return pia_read(&atari.pia, addr & 0x07);

    // TIA: A12=0, A7=0, A9=0
    if ((addr & 0x1080) == 0x0000) return tia_read(&atari.tia, addr & 0x0F);

    return 0;
}

void bus_write(uint16_t addr, uint8_t val) {
    addr &= 0x1FFF;
    if (addr & 0x1000) return; // ROM: non scrivibile

    // PIA RAM
    if ((addr & 0x0280) == 0x0080) { pia_write(&atari.pia, 0x80 | (addr & 0x7F), val); return; }

    // PIA I/O
    if ((addr & 0x0280) == 0x0200) { pia_write(&atari.pia, addr & 0x1F, val); return; }

    // TIA
    if ((addr & 0x1080) == 0x0000) { tia_write(&atari.tia, addr & 0x3F, val); return; }
}

// ── Carica ROM ────────────────────────────────────────────────────────────────
int atari_load_rom(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz != 4096 && sz != 2048) { fclose(f); return 0; }
    memset(atari.rom, 0xFF, 4096);
    if (sz == 2048) {
        fread(atari.rom,      1, 2048, f);
        fread(atari.rom+2048, 1, 2048, f); // specchia
    } else {
        fread(atari.rom, 1, 4096, f);
    }
    fclose(f);
    atari.rom_loaded = 1;
    return 1;
}

// ── Reset ─────────────────────────────────────────────────────────────────────
void atari_reset(void) {
    tia_init(&atari.tia);
    pia_init(&atari.pia);
    cpu_reset(&atari.cpu);
}

// ── Esegui un frame ───────────────────────────────────────────────────────────
void atari_run_frame(void) {
    atari.tia.frame_ready = 0;
    while (!atari.tia.frame_ready) {
        // Se CPU è in WSYNC, avanza solo la TIA
        if (atari.tia.wsync_halt) {
            // Avanza fino a fine scanline (1 clock CPU)
            tia_clock(&atari.tia, 1);
            pia_clock(&atari.pia, 1);
        } else {
            int cyc = cpu_step(&atari.cpu);
            int fr  = tia_clock(&atari.tia, cyc);
            pia_clock(&atari.pia, cyc);
            if (fr) atari.tia.frame_ready = 1;
        }
    }
}
