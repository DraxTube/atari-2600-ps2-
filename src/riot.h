#ifndef RIOT_H
#define RIOT_H

#include "types.h"

void    riot_init(EmulatorState* emu);
void    riot_reset(EmulatorState* emu);
uint8_t riot_read(EmulatorState* emu, uint16_t addr);
void    riot_write(EmulatorState* emu, uint16_t addr, uint8_t value);
void    riot_tick(EmulatorState* emu, int cycles);

#endif
