#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "types.h"

int  cart_load(EmulatorState* emu, const char* filename);
void cart_unload(EmulatorState* emu);
uint8_t cart_read(EmulatorState* emu, uint16_t addr);
void cart_write(EmulatorState* emu, uint16_t addr, uint8_t value);

#endif
