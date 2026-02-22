#ifndef TIA_H
#define TIA_H

#include "types.h"

void    tia_init(EmulatorState* emu);
void    tia_reset(EmulatorState* emu);
uint8_t tia_read(EmulatorState* emu, uint16_t addr);
void    tia_write(EmulatorState* emu, uint16_t addr, uint8_t value);
void    tia_tick(EmulatorState* emu, int cpu_cycles);

#endif
