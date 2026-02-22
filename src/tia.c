#include "tia.h"
#include <string.h>

/* Full NTSC palette (128 colors, luminance pairs) */
static const uint32_t ntsc_palette[256] = {
    /* 0x00 */ 0x000000, 0x000000, 0x4a4a4a, 0x4a4a4a,
    /* 0x04 */ 0x6f6f6f, 0x6f6f6f, 0x8e8e8e, 0x8e8e8e,
    /* 0x08 */ 0xaaaaaa, 0xaaaaaa, 0xc0c0c0, 0xc0c0c0,
    /* 0x0C */ 0xd6d6d6, 0xd6d6d6, 0xececec, 0xececec,
    /* 0x10 */ 0x484800, 0x484800, 0x69690f, 0x69690f,
    /* 0x14 */ 0x86861d, 0x86861d, 0xa2a22a, 0xa2a22a,
    /* 0x18 */ 0xbbbb35, 0xbbbb35, 0xd2d240, 0xd2d240,
    /* 0x1C */ 0xe8e84a, 0xe8e84a, 0xfcfc54, 0xfcfc54,
    /* 0x20 */ 0x7c2c00, 0x7c2c00, 0x904811, 0x904811,
    /* 0x24 */ 0xa26221, 0xa26221, 0xb47a30, 0xb47a30,
    /* 0x28 */ 0xc3903d, 0xc3903d, 0xd2a44a, 0xd2a44a,
    /* 0x2C */ 0xdfb755, 0xdfb755, 0xecc860, 0xecc860,
    /* 0x30 */ 0x901c00, 0x901c00, 0xa33915, 0xa33915,
    /* 0x34 */ 0xb55328, 0xb55328, 0xc56b3a, 0xc56b3a,
    /* 0x38 */ 0xd5804a, 0xd5804a, 0xe39359, 0xe39359,
    /* 0x3C */ 0xf0a567, 0xf0a567, 0xfcb574, 0xfcb574,
    /* 0x40 */ 0x940000, 0x940000, 0xa71a1a, 0xa71a1a,
    /* 0x44 */ 0xb83232, 0xb83232, 0xc84848, 0xc84848,
    /* 0x48 */ 0xd65c5c, 0xd65c5c, 0xe46e6e, 0xe46e6e,
    /* 0x4C */ 0xf08080, 0xf08080, 0xfc9090, 0xfc9090,
    /* 0x50 */ 0x840064, 0x840064, 0x97197a, 0x97197a,
    /* 0x54 */ 0xa8308f, 0xa8308f, 0xb846a2, 0xb846a2,
    /* 0x58 */ 0xc659b3, 0xc659b3, 0xd46cc3, 0xd46cc3,
    /* 0x5C */ 0xe07cd2, 0xe07cd2, 0xec8ce0, 0xec8ce0,
    /* 0x60 */ 0x500084, 0x500084, 0x68199a, 0x68199a,
    /* 0x64 */ 0x7d30ad, 0x7d30ad, 0x9246c0, 0x9246c0,
    /* 0x68 */ 0xa459d0, 0xa459d0, 0xb56ce0, 0xb56ce0,
    /* 0x6C */ 0xc57cee, 0xc57cee, 0xd48cfc, 0xd48cfc,
    /* 0x70 */ 0x140090, 0x140090, 0x331aa3, 0x331aa3,
    /* 0x74 */ 0x4e32b5, 0x4e32b5, 0x6848c6, 0x6848c6,
    /* 0x78 */ 0x7f5cd5, 0x7f5cd5, 0x956ee3, 0x956ee3,
    /* 0x7C */ 0xa980f0, 0xa980f0, 0xbc90fc, 0xbc90fc,
    /* 0x80 */ 0x000094, 0x000094, 0x181aa7, 0x181aa7,
    /* 0x84 */ 0x2d32b8, 0x2d32b8, 0x4248c8, 0x4248c8,
    /* 0x88 */ 0x545cd6, 0x545cd6, 0x656ee4, 0x656ee4,
    /* 0x8C */ 0x7580f0, 0x7580f0, 0x8490fc, 0x8490fc,
    /* 0x90 */ 0x001c88, 0x001c88, 0x183b9d, 0x183b9d,
    /* 0x94 */ 0x2d57b0, 0x2d57b0, 0x4272c2, 0x4272c2,
    /* 0x98 */ 0x548ad2, 0x548ad2, 0x65a0e1, 0x65a0e1,
    /* 0x9C */ 0x75b5ef, 0x75b5ef, 0x84c8fc, 0x84c8fc,
    /* 0xA0 */ 0x003064, 0x003064, 0x185080, 0x185080,
    /* 0xA4 */ 0x2d6d98, 0x2d6d98, 0x4288b0, 0x4288b0,
    /* 0xA8 */ 0x54a0c5, 0x54a0c5, 0x65b7d9, 0x65b7d9,
    /* 0xAC */ 0x75cceb, 0x75cceb, 0x84e0fc, 0x84e0fc,
    /* 0xB0 */ 0x004400, 0x004400, 0x1a661a, 0x1a661a,
    /* 0xB4 */ 0x328432, 0x328432, 0x48a048, 0x48a048,
    /* 0xB8 */ 0x5cb85c, 0x5cb85c, 0x6ece6e, 0x6ece6e,
    /* 0xBC */ 0x80e280, 0x80e280, 0x90f490, 0x90f490,
    /* 0xC0 */ 0x143c00, 0x143c00, 0x355f18, 0x355f18,
    /* 0xC4 */ 0x527e2d, 0x527e2d, 0x6e9c42, 0x6e9c42,
    /* 0xC8 */ 0x87b754, 0x87b754, 0x9ed065, 0x9ed065,
    /* 0xCC */ 0xb4e775, 0xb4e775, 0xc8fc84, 0xc8fc84,
    /* 0xD0 */ 0x303800, 0x303800, 0x505916, 0x505916,
    /* 0xD4 */ 0x6d762b, 0x6d762b, 0x88923e, 0x88923e,
    /* 0xD8 */ 0xa0ab4f, 0xa0ab4f, 0xb7c25f, 0xb7c25f,
    /* 0xDC */ 0xccd86e, 0xccd86e, 0xe0ec7c, 0xe0ec7c,
    /* 0xE0 */ 0x482c00, 0x482c00, 0x694d14, 0x694d14,
    /* 0xE4 */ 0x866a26, 0x866a26, 0xa28638, 0xa28638,
    /* 0xE8 */ 0xbb9f47, 0xbb9f47, 0xd2b656, 0xd2b656,
    /* 0xEC */ 0xe8cc63, 0xe8cc63, 0xfce070, 0xfce070,
    /* 0xF0 */ 0x681400, 0x681400, 0x833915, 0x833915,
    /* 0xF4 */ 0x9b5b28, 0x9b5b28, 0xb07b3a, 0xb07b3a,
    /* 0xF8 */ 0xc4984a, 0xc4984a, 0xd6b359, 0xd6b359,
    /* 0xFC */ 0xe8cc67, 0xe8cc67, 0xf8e474, 0xf8e474,
};

void tia_init(EmulatorState* emu)
{
    memset(&emu->tia, 0, sizeof(TIA));
    emu->tia.inpt4 = 0x80;
    emu->tia.inpt5 = 0x80;
}

void tia_reset(EmulatorState* emu)
{
    tia_init(emu);
}

uint8_t tia_read(EmulatorState* emu, uint16_t addr)
{
    TIA* tia = &emu->tia;
    addr &= 0x0F;

    switch (addr) {
        case 0x00: return tia->cxm0p;
        case 0x01: return tia->cxm1p;
        case 0x02: return tia->cxp0fb;
        case 0x03: return tia->cxp1fb;
        case 0x04: return tia->cxm0fb;
        case 0x05: return tia->cxm1fb;
        case 0x06: return tia->cxblpf;
        case 0x07: return tia->cxppmm;
        case 0x0C: return tia->inpt4;
        case 0x0D: return tia->inpt5;
        default:   return 0;
    }
}

void tia_write(EmulatorState* emu, uint16_t addr, uint8_t value)
{
    TIA* tia = &emu->tia;
    addr &= 0x3F;

    switch (addr) {
        case 0x00: /* VSYNC */
            tia->vsync = value;
            if (value & 0x02) {
                tia->scanline = 0;
                tia->dot = 0;
            }
            break;
        case 0x01: /* VBLANK */
            tia->vblank = value;
            /* Input latch */
            if (value & 0x40) {
                tia->inpt4 = emu->joy0_fire ? 0x00 : 0x80;
                tia->inpt5 = emu->joy1_fire ? 0x00 : 0x80;
            }
            break;
        case 0x02: /* WSYNC */
            emu->cpu.halted = 1;
            break;
        case 0x04: tia->nusiz0 = value; break;
        case 0x05: tia->nusiz1 = value; break;
        case 0x06: tia->colup0 = value; break;
        case 0x07: tia->colup1 = value; break;
        case 0x08: tia->colupf = value; break;
        case 0x09: tia->colubk = value; break;
        case 0x0A: tia->ctrlpf = value; break;
        case 0x0B: tia->refp0 = value; break;
        case 0x0C: tia->refp1 = value; break;
        case 0x0D: tia->pf0 = value; break;
        case 0x0E: tia->pf1 = value; break;
        case 0x0F: tia->pf2 = value; break;
        case 0x10: tia->posp0 = tia->dot - 68; break; /* RESP0 */
        case 0x11: tia->posp1 = tia->dot - 68; break; /* RESP1 */
        case 0x12: tia->posm0 = tia->dot - 68; break; /* RESM0 */
        case 0x13: tia->posm1 = tia->dot - 68; break; /* RESM1 */
        case 0x14: tia->posbl = tia->dot - 68; break;  /* RESBL */
        case 0x15: tia->audc0 = value; break;
        case 0x16: tia->audc1 = value; break;
        case 0x17: tia->audf0 = value; break;
        case 0x18: tia->audf1 = value; break;
        case 0x19: tia->audv0 = value; break;
        case 0x1A: tia->audv1 = value; break;
        case 0x1B: /* GRP0 */
            tia->grp0_old = tia->grp0;
            tia->grp0 = value;
            break;
        case 0x1C: /* GRP1 */
            tia->grp1_old = tia->grp1;
            tia->grp1 = value;
            break;
        case 0x1D: tia->enam0 = value; break;
        case 0x1E: tia->enam1 = value; break;
        case 0x1F: /* ENABL */
            tia->enabl_old = tia->enabl;
            tia->enabl = value;
            break;
        case 0x20: tia->hmp0 = value; break;
        case 0x21: tia->hmp1 = value; break;
        case 0x22: tia->hmm0 = value; break;
        case 0x23: tia->hmm1 = value; break;
        case 0x24: tia->hmbl = value; break;
        case 0x25: tia->vdelp0 = value; break;
        case 0x26: tia->vdelp1 = value; break;
        case 0x27: tia->vdelbl = value; break;
        case 0x28: tia->resmp0 = value; break;
        case 0x29: tia->resmp1 = value; break;
        case 0x2A: /* HMOVE */
            tia->posp0 += (int8_t)(tia->hmp0 & 0xF0) >> 4;
            tia->posp1 += (int8_t)(tia->hmp1 & 0xF0) >> 4;
            tia->posm0 += (int8_t)(tia->hmm0 & 0xF0) >> 4;
            tia->posm1 += (int8_t)(tia->hmm1 & 0xF0) >> 4;
            tia->posbl += (int8_t)(tia->hmbl & 0xF0) >> 4;
            /* Wrap positions */
            while (tia->posp0 < 0) tia->posp0 += 160;
            while (tia->posp1 < 0) tia->posp1 += 160;
            while (tia->posm0 < 0) tia->posm0 += 160;
            while (tia->posm1 < 0) tia->posm1 += 160;
            while (tia->posbl < 0) tia->posbl += 160;
            tia->posp0 %= 160;
            tia->posp1 %= 160;
            tia->posm0 %= 160;
            tia->posm1 %= 160;
            tia->posbl %= 160;
            break;
        case 0x2B: /* HMCLR */
            tia->hmp0 = tia->hmp1 = 0;
            tia->hmm0 = tia->hmm1 = 0;
            tia->hmbl = 0;
            break;
        case 0x2C: /* CXCLR */
            tia->cxm0p = tia->cxm1p = 0;
            tia->cxp0fb = tia->cxp1fb = 0;
            tia->cxm0fb = tia->cxm1fb = 0;
            tia->cxblpf = tia->cxppmm = 0;
            break;
    }
}

/* Check if a player pixel is active at position x */
static int player_pixel(uint8_t grp, uint8_t ref, int pos, uint8_t nusiz, int x)
{
    int copies = 1;
    int spacing = 0;
    int width = 8;

    switch (nusiz & 0x07) {
        case 0: copies = 1; break;
        case 1: copies = 2; spacing = 16; break;
        case 2: copies = 2; spacing = 32; break;
        case 3: copies = 3; spacing = 16; break;
        case 4: copies = 2; spacing = 64; break;
        case 5: width = 16; copies = 1; break;
        case 6: copies = 3; spacing = 32; break;
        case 7: width = 32; copies = 1; break;
    }

    for (int c = 0; c < copies; c++) {
        int start = pos + c * spacing;
        if (start < 0) start += 160;
        start %= 160;

        int rel = x - start;
        if (rel < 0) rel += 160;

        if (rel >= 0 && rel < width) {
            int bit;
            if (width == 8) {
                bit = rel;
            } else {
                bit = rel / (width / 8);
            }
            if (ref & 0x08) bit = 7 - bit;

            if (grp & (0x80 >> bit))
                return 1;
        }
    }
    return 0;
}

/* Check if playfield pixel is set at position x */
static int pf_pixel(TIA* tia, int x)
{
    int bit;
    int half = (x < 80) ? x : (tia->ctrlpf & 0x01) ? (159 - x) : (x - 80);

    if (half < 16) {
        /* PF0 bits 4-7 */
        bit = (tia->pf0 >> (4 + half / 4)) & 1;
    } else if (half < 48) {
        /* PF1 bits 7-0 */
        bit = (tia->pf1 >> (7 - (half - 16) / 4)) & 1;
    } else {
        /* PF2 bits 0-7 */
        bit = (tia->pf2 >> ((half - 48) / 4)) & 1;
    }
    return bit;
}

static int ball_pixel(TIA* tia, int x)
{
    uint8_t en = (tia->vdelbl) ? tia->enabl_old : tia->enabl;
    if (!(en & 0x02)) return 0;

    int bw = 1 << ((tia->ctrlpf >> 4) & 0x03);
    int rel = x - tia->posbl;
    if (rel < 0) rel += 160;

    return (rel >= 0 && rel < bw);
}

static int missile_pixel(int x, int pos, uint8_t en, uint8_t nusiz)
{
    if (!(en & 0x02)) return 0;
    int mw = 1 << ((nusiz >> 4) & 0x03);
    int rel = x - pos;
    if (rel < 0) rel += 160;
    return (rel >= 0 && rel < mw);
}

void tia_tick(EmulatorState* emu, int cpu_cycles)
{
    TIA* tia = &emu->tia;
    int tia_cycles = cpu_cycles * 3; /* 3 TIA clocks per CPU cycle */

    for (int i = 0; i < tia_cycles; i++) {
        int x = tia->dot - 68; /* visible area starts at dot 68 */
        int y = tia->scanline - 40; /* visible starts at scanline ~40 */

        /* Render pixel */
        if (x >= 0 && x < 160 && y >= 0 && y < 192) {
            if (tia->vblank & 0x02) {
                emu->framebuffer[y * 160 + x] = 0x000000;
            } else {
                uint32_t color = ntsc_palette[tia->colubk & 0xFE];

                int pf = pf_pixel(tia, x);
                int bl = ball_pixel(tia, x);

                uint8_t g0 = (tia->vdelp0) ? tia->grp0_old : tia->grp0;
                uint8_t g1 = (tia->vdelp1) ? tia->grp1_old : tia->grp1;
                int p0 = player_pixel(g0, tia->refp0, tia->posp0, tia->nusiz0, x);
                int p1 = player_pixel(g1, tia->refp1, tia->posp1, tia->nusiz1, x);

                int m0 = missile_pixel(x, tia->posm0, tia->enam0, tia->nusiz0);
                int m1 = missile_pixel(x, tia->posm1, tia->enam1, tia->nusiz1);

                /* Priority: ctrlpf bit 2 */
                if (tia->ctrlpf & 0x04) {
                    /* PF/BL on top */
                    if (pf || bl) {
                        color = ntsc_palette[(tia->ctrlpf & 0x02) ?
                            ((x < 80) ? tia->colup0 : tia->colup1) & 0xFE :
                            tia->colupf & 0xFE];
                    } else if (p0 || m0) {
                        color = ntsc_palette[tia->colup0 & 0xFE];
                    } else if (p1 || m1) {
                        color = ntsc_palette[tia->colup1 & 0xFE];
                    }
                } else {
                    /* Players on top */
                    if (p0 || m0) {
                        color = ntsc_palette[tia->colup0 & 0xFE];
                    } else if (p1 || m1) {
                        color = ntsc_palette[tia->colup1 & 0xFE];
                    } else if (pf || bl) {
                        color = ntsc_palette[(tia->ctrlpf & 0x02) ?
                            ((x < 80) ? tia->colup0 : tia->colup1) & 0xFE :
                            tia->colupf & 0xFE];
                    }
                }

                /* Collisions */
                if (m0 && p0) tia->cxm0p |= 0x40;
                if (m0 && p1) tia->cxm0p |= 0x80;
                if (m1 && p1) tia->cxm1p |= 0x40;
                if (m1 && p0) tia->cxm1p |= 0x80;
                if (p0 && pf) tia->cxp0fb |= 0x80;
                if (p0 && bl) tia->cxp0fb |= 0x40;
                if (p1 && pf) tia->cxp1fb |= 0x80;
                if (p1 && bl) tia->cxp1fb |= 0x40;
                if (m0 && pf) tia->cxm0fb |= 0x80;
                if (m0 && bl) tia->cxm0fb |= 0x40;
                if (m1 && pf) tia->cxm1fb |= 0x80;
                if (m1 && bl) tia->cxm1fb |= 0x40;
                if (bl && pf) tia->cxblpf |= 0x80;
                if (p0 && p1) tia->cxppmm |= 0x80;
                if (m0 && m1) tia->cxppmm |= 0x40;

                emu->framebuffer[y * 160 + x] = color;
            }
        }

        /* Advance dot/scanline */
        tia->dot++;
        if (tia->dot >= 228) {
            tia->dot = 0;
            tia->scanline++;
            emu->cpu.halted = 0; /* Release WSYNC */

            if (tia->scanline >= 262) {
                tia->scanline = 0;
                emu->frame_ready = 1;
                tia->frame_done = 1;
            }
        }
    }
}
