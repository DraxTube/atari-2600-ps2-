#pragma once
#include <stdint.h>

// TIA genera il segnale video NTSC per Atari 2600
// 160 pixel per scanline, 228 clock TIA per linea
// 262 scanline totali (NTSC), di cui 3 VSYNC + 37 VBLANK + 192 visibili + 30 overscan

#define TIA_SCREEN_W   160
#define TIA_SCREEN_H   192

// Registri write TIA (scritti dalla CPU)
#define TIA_VSYNC  0x00
#define TIA_VBLANK 0x01
#define TIA_WSYNC  0x02
#define TIA_RSYNC  0x03
#define TIA_NUSIZ0 0x04
#define TIA_NUSIZ1 0x05
#define TIA_COLUP0 0x06
#define TIA_COLUP1 0x07
#define TIA_COLUPF 0x08
#define TIA_COLUBK 0x09
#define TIA_CTRLPF 0x0A
#define TIA_REFP0  0x0B
#define TIA_REFP1  0x0C
#define TIA_PF0    0x0D
#define TIA_PF1    0x0E
#define TIA_PF2    0x0F
#define TIA_RESP0  0x10
#define TIA_RESP1  0x11
#define TIA_RESM0  0x12
#define TIA_RESM1  0x13
#define TIA_RESBL  0x14
#define TIA_AUDC0  0x15
#define TIA_AUDC1  0x16
#define TIA_AUDF0  0x17
#define TIA_AUDF1  0x18
#define TIA_AUDV0  0x19
#define TIA_AUDV1  0x1A
#define TIA_GRP0   0x1B
#define TIA_GRP1   0x1C
#define TIA_ENAM0  0x1D
#define TIA_ENAM1  0x1E
#define TIA_ENABL  0x1F
#define TIA_HMP0   0x20
#define TIA_HMP1   0x21
#define TIA_HMM0   0x22
#define TIA_HMM1   0x23
#define TIA_HMBL   0x24
#define TIA_VDELP0 0x25
#define TIA_VDELP1 0x26
#define TIA_VDELBL 0x27
#define TIA_RESMP0 0x28
#define TIA_RESMP1 0x29
#define TIA_HMOVE  0x2A
#define TIA_HMCLR  0x2B
#define TIA_CXCLR  0x2C

// Registri read TIA
#define TIA_CXM0P  0x00
#define TIA_CXM1P  0x01
#define TIA_CXP0FB 0x02
#define TIA_CXP1FB 0x03
#define TIA_CXM0FB 0x04
#define TIA_CXM1FB 0x05
#define TIA_CXBLPF 0x06
#define TIA_CXPPMM 0x07
#define TIA_INPT0  0x08
#define TIA_INPT1  0x09
#define TIA_INPT2  0x0A
#define TIA_INPT3  0x0B
#define TIA_INPT4  0x0C
#define TIA_INPT5  0x0D

typedef struct {
    // Framebuffer: indice colore NTSC per ogni pixel
    uint8_t  fb[TIA_SCREEN_H][TIA_SCREEN_W];

    // Stato interno
    uint8_t  reg[0x40];      // shadow dei registri write
    uint8_t  collision;      // registro collisioni
    int      hpos;           // posizione orizzontale (0-227)
    int      scanline;       // scanline corrente (0-261)
    int      frame_ready;    // 1 quando un frame e' completo
    int      wsync_halt;     // 1 = CPU in attesa di WSYNC

    // Posizioni sprite (in clock TIA)
    int      p0_x, p1_x;
    int      m0_x, m1_x;
    int      bl_x;

    // GRP delayed
    uint8_t  grp0_old, grp1_old;

    // Input
    uint8_t  inpt4, inpt5;   // fire buttons (bit7=0 premuto)
} TIA;

void    tia_init(TIA *tia);
void    tia_write(TIA *tia, uint8_t reg, uint8_t val);
uint8_t tia_read(TIA *tia, uint8_t reg);
// Avanza di 'cycles' clock CPU (= cycles*3 clock TIA)
// Ritorna 1 se un nuovo frame e' pronto
int     tia_clock(TIA *tia, int cpu_cycles);
void    tia_set_input(TIA *tia, uint8_t inpt4, uint8_t inpt5);
