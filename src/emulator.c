#include "emulator.h"
#include <string.h>
#include <stdlib.h>

EmulatorState emu_state;

void emu_init(void) {
    memset(&emu_state, 0, sizeof(EmulatorState));
    
    emu_state.framebuffer = malloc(160 * 192 * sizeof(uint32_t));
    
    cpu_init(&emu_state.cpu);
    tia_init(&emu_state.tia);
    riot_init(&emu_state.riot);
    
    emu_state.running = 1;
}

void emu_reset(void) {
    cpu_reset(&emu_state.cpu);
    tia_reset(&emu_state.tia);
    memset(emu_state.ram, 0, sizeof(emu_state.ram));
}

void emu_run_frame(void) {
    emu_state.frame_ready = 0;
    
    // ~30000 cicli per frame NTSC
    while (!emu_state.frame_ready && emu_state.running) {
        int cycles = cpu_step(&emu_state.cpu);
        tia_step(&emu_state.tia, cycles);
        riot_step(&emu_state.riot, cycles);
    }
}

void emu_shutdown(void) {
    cart_unload();
    if (emu_state.framebuffer) {
        free(emu_state.framebuffer);
        emu_state.framebuffer = NULL;
    }
}
