#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdint.h>
#include "cpu6507.h"
#include "tia.h"
#include "riot.h"
#include "cartridge.h"

typedef struct {
    CPU6507 cpu;
    TIA tia;
    RIOT riot;
    Cartridge cart;
    
    uint8_t ram[128];
    uint8_t input[2];
    
    uint32_t* framebuffer;
    int frame_ready;
    int running;
} EmulatorState;

void emu_init(void);
void emu_reset(void);
void emu_run_frame(void);
void emu_shutdown(void);

extern EmulatorState emu_state;

#endif
