#pragma once
#include <stdint.h>

// PIA 6532 (RIOT = RAM/IO/Timer)
// RAM: 128 byte @ 0x80-0xFF
// I/O: joystick + console switches
// Timer: countdown a 4 velocità

typedef struct {
    uint8_t  ram[128];

    // Joystick & switches
    uint8_t  swcha;  // direzioni joystick (bit alto = P1, basso = P2)
    uint8_t  swchb;  // switch console (reset, select, B&W/color, difficoltà)

    // Timer
    uint32_t timer_val;    // valore corrente
    uint32_t timer_step;   // divisore (1/8/64/1024)
    uint32_t timer_count;  // contatore parziale
    uint8_t  timer_int;    // interrupt flag (bit7)
} PIA;

void    pia_init(PIA *pia);
uint8_t pia_read(PIA *pia, uint8_t reg);
void    pia_write(PIA *pia, uint8_t reg, uint8_t val);
void    pia_clock(PIA *pia, int cpu_cycles);

// Imposta stato joystick (chiamato dal codice PS2 per tradurre pad)
// joy: bitmask UDLR fire per P1 (bit0=fuoco, 1=R, 2=L, 3=D, 4=U)
void    pia_set_joystick(PIA *pia, uint8_t joy1, uint8_t joy2);
void    pia_set_switches(PIA *pia, uint8_t reset_pressed, uint8_t select_pressed);
