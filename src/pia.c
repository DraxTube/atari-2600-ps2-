#include "pia.h"
#include <string.h>

void pia_init(PIA *pia) {
    memset(pia, 0, sizeof(PIA));
    pia->swcha  = 0xFF; // tutto rilasciato
    pia->swchb  = 0x0B; // color, P1 facile, P2 facile, niente premuto
    pia->timer_val  = 0xFF;
    pia->timer_step = 1024; // TIM1024T default
}

void pia_set_joystick(PIA *pia, uint8_t joy1, uint8_t joy2) {
    // joy: bit4=U, bit3=D, bit2=L, bit1=R per ogni player
    // SWCHA: bit7..4=P1 UDLR, bit3..0=P2 UDLR (0=premuto)
    uint8_t p1 = 0xF0;
    uint8_t p2 = 0x0F;
    if (joy1 & 0x10) p1 &= ~0x10; // P1 Up
    if (joy1 & 0x08) p1 &= ~0x20; // P1 Down
    if (joy1 & 0x04) p1 &= ~0x40; // P1 Left
    if (joy1 & 0x02) p1 &= ~0x80; // P1 Right
    if (joy2 & 0x10) p2 &= ~0x01; // P2 Up
    if (joy2 & 0x08) p2 &= ~0x02; // P2 Down
    if (joy2 & 0x04) p2 &= ~0x04; // P2 Left
    if (joy2 & 0x02) p2 &= ~0x08; // P2 Right
    pia->swcha = p1 | p2;
}

void pia_set_switches(PIA *pia, uint8_t reset_pressed, uint8_t select_pressed) {
    pia->swchb = 0x0B;
    if (!reset_pressed)  pia->swchb |= 0x00; // bit0=0 = reset premuto
    if (reset_pressed)   pia->swchb |= 0x01;
    if (!select_pressed) pia->swchb &= ~0x02;
    else                 pia->swchb |=  0x02;
}

uint8_t pia_read(PIA *pia, uint8_t reg) {
    // RAM: 0x80-0xFF
    if (reg >= 0x80) return pia->ram[reg & 0x7F];

    switch (reg & 0x07) {
    case 0x00: return pia->swcha;
    case 0x01: return 0xFF;      // SWACNT (DDR)
    case 0x02: return pia->swchb;
    case 0x03: return 0xFF;      // SWBCNT
    case 0x04:
    case 0x06: { // INTIM
        uint8_t v = (uint8_t)(pia->timer_val & 0xFF);
        if (pia->timer_val == 0 || pia->timer_int) v |= 0x80;
        return v;
    }
    case 0x05:
    case 0x07: return pia->timer_int ? 0x80 : 0x00; // INSTAT
    }
    return 0;
}

void pia_write(PIA *pia, uint8_t reg, uint8_t val) {
    if (reg >= 0x80) { pia->ram[reg & 0x7F] = val; return; }

    switch (reg & 0x1F) {
    case 0x00: pia->swcha = val; break;
    case 0x02: pia->swchb = val; break;
    case 0x14: pia->timer_val=val; pia->timer_step=  1; pia->timer_int=0; pia->timer_count=0; break; // TIM1T
    case 0x15: pia->timer_val=val; pia->timer_step=  8; pia->timer_int=0; pia->timer_count=0; break; // TIM8T
    case 0x16: pia->timer_val=val; pia->timer_step= 64; pia->timer_int=0; pia->timer_count=0; break; // TIM64T
    case 0x17: pia->timer_val=val; pia->timer_step=1024;pia->timer_int=0; pia->timer_count=0; break; // TIM1024T
    }
}

void pia_clock(PIA *pia, int cpu_cycles) {
    pia->timer_count += cpu_cycles;
    while (pia->timer_count >= pia->timer_step) {
        pia->timer_count -= pia->timer_step;
        if (pia->timer_val == 0) {
            pia->timer_int = 1;
            pia->timer_val = 0xFF; // wrap
        } else {
            pia->timer_val--;
        }
    }
}
