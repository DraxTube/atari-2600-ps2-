#include "riot.h"
#include <string.h>

void riot_init(EmulatorState* emu)
{
    memset(&emu->riot, 0, sizeof(RIOT));
    emu->riot.portb = 0x3F; /* Default: color TV, both A difficulty */
    emu->riot.timer_value = 1024;
    emu->riot.timer_interval = 1024;
}

void riot_reset(EmulatorState* emu)
{
    riot_init(emu);
}

static void update_porta(EmulatorState* emu)
{
    uint8_t val = 0xFF;

    /* P0 joystick (bits 4-7) */
    if (emu->joy0_up)    val &= ~0x10;
    if (emu->joy0_down)  val &= ~0x20;
    if (emu->joy0_left)  val &= ~0x40;
    if (emu->joy0_right) val &= ~0x80;

    /* P1 joystick (bits 0-3) */
    if (emu->joy1_up)    val &= ~0x01;
    if (emu->joy1_down)  val &= ~0x02;
    if (emu->joy1_left)  val &= ~0x04;
    if (emu->joy1_right) val &= ~0x08;

    emu->riot.porta = val;
}

static void update_portb(EmulatorState* emu)
{
    uint8_t val = 0xFF;

    if (emu->switch_reset)   val &= ~0x01;
    if (emu->switch_select)  val &= ~0x02;
    if (!emu->switch_color)  val &= ~0x08;
    if (emu->switch_p0_diff) val &= ~0x40;
    if (emu->switch_p1_diff) val &= ~0x80;

    emu->riot.portb = val;
}

uint8_t riot_read(EmulatorState* emu, uint16_t addr)
{
    RIOT* riot = &emu->riot;
    addr &= 0x7FF;

    /* RAM: mirrored at 0x80-0xFF */
    if ((addr & 0x200) == 0 && (addr & 0x80)) {
        return emu->ram[addr & 0x7F];
    }

    switch (addr & 0x07) {
        case 0x00: /* SWCHA */
            update_porta(emu);
            return riot->porta;
        case 0x01: /* SWACNT */
            return riot->ddra;
        case 0x02: /* SWCHB */
            update_portb(emu);
            return riot->portb;
        case 0x03: /* SWBCNT */
            return riot->ddrb;
        case 0x04: /* INTIM */
            riot->timer_underflow = 0;
            return (uint8_t)(riot->timer_value >> 10);
        case 0x05: /* INSTAT (timer status) */
        {
            uint8_t ret = 0;
            if (riot->timer_underflow) ret |= 0xC0;
            return ret;
        }
        default:
            return 0;
    }
}

void riot_write(EmulatorState* emu, uint16_t addr, uint8_t value)
{
    RIOT* riot = &emu->riot;
    addr &= 0x7FF;

    /* RAM */
    if ((addr & 0x200) == 0 && (addr & 0x80)) {
        emu->ram[addr & 0x7F] = value;
        return;
    }

    /* Timer registers */
    if (addr & 0x10) {
        switch (addr & 0x03) {
            case 0x00: /* TIM1T  - 1 cycle */
                riot->timer_value = value * 1;
                riot->timer_interval = 1;
                riot->timer_underflow = 0;
                break;
            case 0x01: /* TIM8T  - 8 cycles */
                riot->timer_value = value * 8;
                riot->timer_interval = 8;
                riot->timer_underflow = 0;
                break;
            case 0x02: /* TIM64T - 64 cycles */
                riot->timer_value = value * 64;
                riot->timer_interval = 64;
                riot->timer_underflow = 0;
                break;
            case 0x03: /* T1024T - 1024 cycles */
                riot->timer_value = value * 1024;
                riot->timer_interval = 1024;
                riot->timer_underflow = 0;
                break;
        }
        return;
    }

    switch (addr & 0x07) {
        case 0x00: riot->porta = value; break;
        case 0x01: riot->ddra = value; break;
        case 0x02: riot->portb = value; break;
        case 0x03: riot->ddrb = value; break;
    }
}

void riot_tick(EmulatorState* emu, int cycles)
{
    RIOT* riot = &emu->riot;

    if (riot->timer_value > 0) {
        if ((uint32_t)cycles >= riot->timer_value) {
            riot->timer_value = 0;
            riot->timer_underflow = 1;
        } else {
            riot->timer_value -= cycles;
        }
    }
}
