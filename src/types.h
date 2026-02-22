#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/* ============================================
 * TIA - Television Interface Adaptor
 * ============================================ */
typedef struct {
    /* Sync */
    uint8_t vsync;
    uint8_t vblank;
    uint8_t wsync;

    /* Player colors */
    uint8_t colup0, colup1;
    uint8_t colupf, colubk;

    /* Playfield */
    uint8_t pf0, pf1, pf2;
    uint8_t ctrlpf;
    uint8_t refp0, refp1;

    /* Player graphics */
    uint8_t grp0, grp1;
    uint8_t grp0_old, grp1_old;

    /* Missile & Ball */
    uint8_t enam0, enam1;
    uint8_t enabl, enabl_old;
    uint8_t nusiz0, nusiz1;

    /* Horizontal motion */
    uint8_t hmp0, hmp1, hmm0, hmm1, hmbl;

    /* Position counters */
    int16_t posp0, posp1;
    int16_t posm0, posm1;
    int16_t posbl;

    /* Collision registers */
    uint8_t cxm0p, cxm1p, cxp0fb, cxp1fb;
    uint8_t cxm0fb, cxm1fb, cxblpf, cxppmm;

    /* VDELP / VDELBL */
    uint8_t vdelp0, vdelp1, vdelbl;

    /* RSYNC */
    uint8_t resmp0, resmp1;

    /* Internal */
    int scanline;
    int dot;
    int frame_done;

    /* Audio (stub) */
    uint8_t audc0, audc1;
    uint8_t audf0, audf1;
    uint8_t audv0, audv1;

    /* Input latches */
    uint8_t inpt4, inpt5;
} TIA;

/* ============================================
 * RIOT - RAM, I/O, Timer
 * ============================================ */
typedef struct {
    uint8_t ddra;
    uint8_t ddrb;
    uint8_t porta;
    uint8_t portb;

    uint32_t timer_value;
    uint32_t timer_interval;
    uint8_t  timer_underflow;
} RIOT;

/* ============================================
 * Cartridge
 * ============================================ */
typedef enum {
    CART_2K = 0,
    CART_4K,
    CART_F8,    /* 8K  - Atari standard */
    CART_F6,    /* 16K - Atari standard */
    CART_F4,    /* 32K - Atari standard */
    CART_FE,    /* 8K  - Activision */
    CART_E0,    /* 8K  - Parker Bros */
    CART_3F,    /* up to 512K - Tigervision */
    CART_E7,    /* 16K - M-Network */
    CART_FA,    /* 12K - CBS RAM Plus */
    CART_CV,    /* Commavid */
    CART_UA     /* UA Ltd */
} CartType;

typedef struct {
    uint8_t* rom;
    uint32_t rom_size;
    CartType type;
    int current_bank;
    int num_banks;
    uint8_t extra_ram[256]; /* For bankswitching with RAM */
} Cartridge;

/* ============================================
 * CPU 6507
 * ============================================ */
typedef struct {
    uint8_t  A;
    uint8_t  X;
    uint8_t  Y;
    uint8_t  SP;
    uint16_t PC;
    uint8_t  P;
    uint64_t cycles;
    int      halted; /* WSYNC halt */
} CPU6507;

/* Status flags */
#define FLAG_C  0x01
#define FLAG_Z  0x02
#define FLAG_I  0x04
#define FLAG_D  0x08
#define FLAG_B  0x10
#define FLAG_U  0x20
#define FLAG_V  0x40
#define FLAG_N  0x80

/* ============================================
 * Emulator State
 * ============================================ */
#define SCREEN_W 160
#define SCREEN_H 192

typedef struct {
    CPU6507   cpu;
    TIA       tia;
    RIOT      riot;
    Cartridge cart;

    uint8_t  ram[128];
    uint32_t framebuffer[SCREEN_W * SCREEN_H];

    /* Input: joystick directions + fire */
    uint8_t joy0_up, joy0_down, joy0_left, joy0_right, joy0_fire;
    uint8_t joy1_up, joy1_down, joy1_left, joy1_right, joy1_fire;

    /* Console switches */
    uint8_t switch_reset;
    uint8_t switch_select;
    uint8_t switch_color; /* 1=color, 0=bw */
    uint8_t switch_p0_diff; /* 0=B(amateur), 1=A(pro) */
    uint8_t switch_p1_diff;

    int frame_ready;
    int running;
} EmulatorState;

#endif /* TYPES_H */
