#include "tia.h"
#include <string.h>

// ── Palette NTSC Atari 2600 (128 colori) → BGR5551 per GS della PS2 ──────────
// Ogni voce: 0xRRGGBB (convertiremo dopo per GS)
static const uint32_t ntsc_palette[128] = {
    0x000000,0x404040,0x6C6C6C,0x909090,0xB0B0B0,0xC8C8C8,0xDCDCDC,0xECECEC,
    0x444400,0x646410,0x848424,0xA0A034,0xB8B840,0xD0D050,0xE8E85C,0xFCFC68,
    0x702800,0x844414,0x985C28,0xAC783C,0xBC8C4C,0xCCA05C,0xDCB468,0xECC878,
    0x841800,0x983418,0xAC5030,0xC06848,0xD0805C,0xE09470,0xECA880,0xFCBC94,
    0x880000,0x9C2020,0xB03C3C,0xC05858,0xD07070,0xE08888,0xECA0A0,0xFCB4B4,
    0x78005C,0x8C2074,0xA03C88,0xB0589C,0xC070B0,0xD084C0,0xDC9CD0,0xECB0E0,
    0x480078,0x602090,0x7840A8,0x8C5CBC,0xA070D0,0xB484E0,0xC49CF0,0xD4B0FC,
    0x140084,0x302098,0x4C3CAC,0x6858C0,0x7C70D0,0x9488E0,0xA8A0EC,0xBCB4FC,
    0x000088,0x1C209C,0x3840B0,0x505CC0,0x6874D0,0x7C8CE0,0x90A4EC,0xA4B8FC,
    0x00187C,0x1C3890,0x3854A8,0x5070BC,0x6888CC,0x7C9CDC,0x90B4EC,0xA4C8FC,
    0x002C5C,0x1C4C78,0x386890,0x5484AC,0x6C9CBC,0x84B4CC,0x9CCCE0,0xB0E0F4,
    0x003C2C,0x1C5C48,0x387C64,0x549880,0x6CB494,0x84C8AC,0x9CDCC0,0xB0ECD4,
    0x003C00,0x205C20,0x407C40,0x5C9C5C,0x74B474,0x8CD08C,0xA4E4A4,0xB8FCB8,
    0x143800,0x345C1C,0x507C38,0x6C9850,0x84B468,0x9CCC7C,0xB4E490,0xC8FCA4,
    0x2C3000,0x4C501C,0x687034,0x84904C,0x9CAC60,0xB4C878,0xCCE08C,0xE0F4A0,
    0x442800,0x644818,0x846830,0xA08448,0xB89C5C,0xD0B470,0xE8CC84,0xFCE09C,
};

// Converti indice colore TIA (0..127) → RGBA packed
static inline uint32_t tia_color(uint8_t col) {
    return ntsc_palette[(col >> 1) & 0x7F];
}

void tia_init(TIA *tia) {
    memset(tia, 0, sizeof(TIA));
    tia->inpt4 = 0xFF;
    tia->inpt5 = 0xFF;
}

void tia_set_input(TIA *tia, uint8_t inpt4, uint8_t inpt5) {
    tia->inpt4 = inpt4;
    tia->inpt5 = inpt5;
}

// ── Lettura registri TIA ───────────────────────────────────────────────────────
uint8_t tia_read(TIA *tia, uint8_t reg) {
    switch (reg & 0x0F) {
    case 0x0C: return tia->inpt4;
    case 0x0D: return tia->inpt5;
    default:   return 0;
    }
}

// ── Scrittura registri TIA ────────────────────────────────────────────────────
void tia_write(TIA *tia, uint8_t reg, uint8_t val) {
    tia->reg[reg & 0x3F] = val;
    switch (reg & 0x3F) {
    case TIA_WSYNC: tia->wsync_halt = 1; break;
    case TIA_RESP0: tia->p0_x = tia->hpos; break;
    case TIA_RESP1: tia->p1_x = tia->hpos; break;
    case TIA_RESM0: tia->m0_x = tia->hpos; break;
    case TIA_RESM1: tia->m1_x = tia->hpos; break;
    case TIA_RESBL: tia->bl_x = tia->hpos; break;
    case TIA_GRP0:
        tia->grp0_old = tia->reg[TIA_GRP0];
        tia->reg[TIA_GRP0] = val;
        break;
    case TIA_GRP1:
        tia->grp1_old = tia->reg[TIA_GRP1];
        tia->reg[TIA_GRP1] = val;
        break;
    case TIA_HMOVE:
        // Applica motion registers
        tia->p0_x = (tia->p0_x + (int8_t)((tia->reg[TIA_HMP0] >> 4) << 4 >> 4)) & 0xFF;
        tia->p1_x = (tia->p1_x + (int8_t)((tia->reg[TIA_HMP1] >> 4) << 4 >> 4)) & 0xFF;
        tia->m0_x = (tia->m0_x + (int8_t)((tia->reg[TIA_HMM0] >> 4) << 4 >> 4)) & 0xFF;
        tia->m1_x = (tia->m1_x + (int8_t)((tia->reg[TIA_HMM1] >> 4) << 4 >> 4)) & 0xFF;
        tia->bl_x = (tia->bl_x + (int8_t)((tia->reg[TIA_HMBL] >> 4) << 4 >> 4)) & 0xFF;
        break;
    case TIA_HMCLR:
        tia->reg[TIA_HMP0]=tia->reg[TIA_HMP1]=0;
        tia->reg[TIA_HMM0]=tia->reg[TIA_HMM1]=0;
        tia->reg[TIA_HMBL]=0;
        break;
    case TIA_CXCLR: tia->collision = 0; break;
    }
}

// ── Generazione pixel per una singola colonna ─────────────────────────────────
static uint8_t tia_pixel(TIA *tia, int x) {
    // x: 0..159 (pixel visibili)
    uint8_t color = tia->reg[TIA_COLUBK]; // sfondo

    // ─ Playfield ─
    uint8_t pf_bit = 0;
    if (x < 80) {
        // Left half
        if (x < 4)       pf_bit = (tia->reg[TIA_PF0] >> (4 + x)) & 1;
        else if (x < 12) pf_bit = (tia->reg[TIA_PF1] >> (7 - (x-4))) & 1;
        else if (x < 20) pf_bit = (tia->reg[TIA_PF2] >> ((x-12))) & 1;
    } else {
        // Right half (mirrored or repeated)
        int rx = x - 80;
        if (tia->reg[TIA_CTRLPF] & 0x01) {
            // Mirrored
            if (rx < 20)      pf_bit = (tia->reg[TIA_PF2] >> (19-rx)) & 1;
            else if (rx < 28) pf_bit = (tia->reg[TIA_PF1] >> (rx-20)) & 1;
            else if (rx < 32) pf_bit = (tia->reg[TIA_PF0] >> (4+(31-rx))) & 1;
        } else {
            // Repeat
            if (rx < 4)       pf_bit = (tia->reg[TIA_PF0] >> (4 + rx)) & 1;
            else if (rx < 12) pf_bit = (tia->reg[TIA_PF1] >> (7 - (rx-4))) & 1;
            else if (rx < 20) pf_bit = (tia->reg[TIA_PF2] >> ((rx-12))) & 1;
        }
    }
    if (pf_bit) color = (tia->reg[TIA_CTRLPF] & 0x06) ? tia->reg[TIA_COLUP0] : tia->reg[TIA_COLUPF];

    // ─ Ball ─
    if (tia->reg[TIA_ENABL] & 0x02) {
        int bsz = 1 << ((tia->reg[TIA_CTRLPF] >> 4) & 3);
        int bx  = tia->bl_x & 0xFF;
        if (x >= bx && x < bx + bsz) color = tia->reg[TIA_COLUPF];
    }

    // ─ Player 1 ─
    {
        uint8_t grp = (tia->reg[TIA_VDELP1] & 1) ? tia->grp1_old : tia->reg[TIA_GRP1];
        if (grp) {
            int px = tia->p1_x & 0xFF;
            int dx = x - px;
            if (dx >= 0 && dx < 8) {
                uint8_t refl = tia->reg[TIA_REFP1] & 0x08;
                uint8_t bit  = refl ? (grp >> dx) & 1 : (grp >> (7-dx)) & 1;
                if (bit) color = tia->reg[TIA_COLUP1];
            }
        }
    }

    // ─ Missile 1 ─
    if (tia->reg[TIA_ENAM1] & 0x02) {
        int msz = 1 << ((tia->reg[TIA_NUSIZ1] >> 4) & 3);
        int mx  = tia->m1_x & 0xFF;
        if (x >= mx && x < mx + msz) color = tia->reg[TIA_COLUP1];
    }

    // ─ Player 0 ─ (priorità alta)
    {
        uint8_t grp = (tia->reg[TIA_VDELP0] & 1) ? tia->grp0_old : tia->reg[TIA_GRP0];
        if (grp) {
            int px = tia->p0_x & 0xFF;
            int dx = x - px;
            if (dx >= 0 && dx < 8) {
                uint8_t refl = tia->reg[TIA_REFP0] & 0x08;
                uint8_t bit  = refl ? (grp >> dx) & 1 : (grp >> (7-dx)) & 1;
                if (bit) color = tia->reg[TIA_COLUP0];
            }
        }
    }

    // ─ Missile 0 ─
    if (tia->reg[TIA_ENAM0] & 0x02) {
        int msz = 1 << ((tia->reg[TIA_NUSIZ0] >> 4) & 3);
        int mx  = tia->m0_x & 0xFF;
        if (x >= mx && x < mx + msz) color = tia->reg[TIA_COLUP0];
    }

    return color;
}

// ── Avanza TIA di N clock CPU ─────────────────────────────────────────────────
int tia_clock(TIA *tia, int cpu_cycles) {
    int tia_ticks = cpu_cycles * 3;
    int frame_done = 0;

    while (tia_ticks-- > 0) {
        int visible_y = tia->scanline - 40; // 3 VSYNC + 37 VBLANK = 40
        int visible_x = tia->hpos - 68;     // 68 clock di blanking orizzontale

        if (visible_y >= 0 && visible_y < TIA_SCREEN_H &&
            visible_x >= 0 && visible_x < TIA_SCREEN_W) {
            tia->fb[visible_y][visible_x] = tia_pixel(tia, visible_x);
        }

        tia->hpos++;
        if (tia->hpos >= 228) {
            tia->hpos = 0;
            tia->wsync_halt = 0; // fine scanline: risveglia CPU
            tia->scanline++;
            if (tia->scanline >= 262) {
                tia->scanline = 0;
                frame_done    = 1;
            }
        }
    }
    return frame_done;
}
