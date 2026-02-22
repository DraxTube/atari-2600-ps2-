#ifndef CPU6507_H
#define CPU6507_H

#include "types.h"

void    cpu_init(EmulatorState* emu);
void    cpu_reset(EmulatorState* emu);
int     cpu_step(EmulatorState* emu);
uint8_t mem_read(EmulatorState* emu, uint16_t addr);
void    mem_write(EmulatorState* emu, uint16_t addr, uint8_t value);

#endif
