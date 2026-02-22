#include "emulator.h"
#include "cpu6507.h"
#include "tia.h"
#include "riot.h"
#include "cartridge.h"
#include <string.h>

void emu_init(EmulatorState* emu)
{
    memset(emu, 0, sizeof(EmulatorState));
    emu->switch_color = 1;
    emu->running = 1;

    cpu_init(emu);
    tia_init(emu);
    riot_init(emu);
}

void emu_reset(EmulatorState* emu)
{
    memset(emu->ram, 0, sizeof(emu->ram));
    memset(emu->framebuffer, 0, sizeof(emu->framebuffer));

    cpu_reset(emu);
    tia_reset(emu);
    riot_reset(emu);
}

void emu_run_frame(EmulatorState* emu)
{
    emu->frame_ready = 0;
    emu->tia.frame_done = 0;

    /* Fire button maps to TIA input */
    emu->tia.inpt4 = emu->joy0_fire ? 0x00 : 0x80;
    emu->tia.inpt5 = emu->joy1_fire ? 0x00 : 0x80;

    while (!emu->frame_ready && emu->running) {
        int cycles = cpu_step(emu);
        tia_tick(emu, cycles);
        riot_tick(emu, cycles);
    }
}

void emu_shutdown(EmulatorState* emu)
{
    cart_unload(emu);
}
