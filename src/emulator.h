#ifndef EMULATOR_H
#define EMULATOR_H

#include "types.h"

void emu_init(EmulatorState* emu);
void emu_reset(EmulatorState* emu);
void emu_run_frame(EmulatorState* emu);
void emu_shutdown(EmulatorState* emu);

#endif
