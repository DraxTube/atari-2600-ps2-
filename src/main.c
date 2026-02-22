#include "emulator.h"
#include "cartridge.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

#ifdef _EE
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <libpad.h>
#include <iopcontrol.h>
#include <iopheap.h>
#include <debug.h>

/* Embedded IRX modules */
extern unsigned char sio2man_irx[];
extern unsigned int size_sio2man_irx;
extern unsigned char padman_irx[];
extern unsigned int size_padman_irx;

static void reset_IOP(void)
{
    SifInitRpc(0);
    while (!SifIopReset("", 0)) {};
    while (!SifIopSync()) {};
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

static void load_modules(void)
{
    int ret;

    /* Load from ROM first */
    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) {
        scr_printf("Warning: Failed to load SIO2MAN from ROM\n");
    }

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0) {
        scr_printf("Warning: Failed to load PADMAN from ROM\n");
    }
}

int main(int argc, char** argv)
{
    EmulatorState emu;
    const char* rom_path = NULL;

    (void)argc;
    (void)argv;

    /* PS2 init */
    reset_IOP();
    load_modules();

    /* Debug screen init */
    init_scr();
    scr_clear();
    scr_printf("Atari 2600 Emulator for PS2\n");
    scr_printf("===========================\n\n");

    /* Initialize emulator */
    emu_init(&emu);
    scr_printf("Emulator initialized.\n");

    /* Try to init graphics */
    scr_printf("Initializing graphics...\n");
    if (!ui_init()) {
        scr_printf("ERROR: Failed to initialize graphics!\n");
        scr_printf("Press any button to exit.\n");
        SleepThread();
        return 1;
    }
    scr_printf("Graphics OK.\n");

    /* For now, try a hardcoded ROM path for testing */
    rom_path = "mass:/ROMS/game.bin";
    
    /* Also try host: path for PCSX2 */
    if (argc > 1) {
        rom_path = argv[1];
    }

    scr_printf("Loading ROM: %s\n", rom_path);

    if (!cart_load(&emu, rom_path)) {
        scr_printf("ERROR: Failed to load ROM!\n");
        scr_printf("Make sure ROM exists at: %s\n", rom_path);
        scr_printf("\nWaiting 5 seconds...\n");
        
        /* Wait a bit so user can read the message */
        int i;
        for (i = 0; i < 5 * 50; i++) {
            /* ~50 vsyncs per second */
            asm volatile("sync.l; ei");
            asm volatile(
                "li $v0, 0x1F\n"
                "syscall\n"
                ::: "v0"
            );
        }
        
        ui_shutdown();
        return 1;
    }

    scr_printf("ROM loaded successfully!\n");
    scr_printf("Starting emulation...\n");

    emu_reset(&emu);

    /* Main loop */
    while (emu.running) {
        ui_handle_input(&emu);

        if (emu.switch_reset) {
            emu_reset(&emu);
            emu.switch_reset = 0;
        }

        emu_run_frame(&emu);
        ui_render_frame(&emu);
    }

    emu_shutdown(&emu);
    ui_shutdown();

    return 0;
}

#else
/* PC version for testing */
#include <stdio.h>

int main(int argc, char** argv)
{
    EmulatorState emu;

    if (argc < 2) {
        printf("Usage: %s <rom.bin>\n", argv[0]);
        return 1;
    }

    emu_init(&emu);

    if (!cart_load(&emu, argv[1])) {
        printf("Failed to load ROM: %s\n", argv[1]);
        return 1;
    }

    emu_reset(&emu);

    /* Run a few frames for testing */
    int frame;
    for (frame = 0; frame < 300 && emu.running; frame++) {
        emu_run_frame(&emu);
    }

    printf("Ran %d frames OK\n", frame);

    emu_shutdown(&emu);
    return 0;
}
#endif
