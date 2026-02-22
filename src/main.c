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

static void wait_vsync(void)
{
    /* Simple delay to ~60fps */
    int i;
    for (i = 0; i < 10000; i++) asm volatile("nop");
}

static void load_irx_modules(void)
{
    int ret;

    /* Try loading from rom/builtin first, then from files */
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);

    ret = SifLoadModule("mass:/SYSTEM/USBD.IRX", 0, NULL);
    if (ret < 0) {
        SifLoadModule("rom0:USBD", 0, NULL);
    }

    ret = SifLoadModule("mass:/SYSTEM/USBHDFSD.IRX", 0, NULL);
    if (ret < 0) {
        /* Try alternate paths */
        SifLoadModule("mc0:/BOOT/USBD.IRX", 0, NULL);
        SifLoadModule("mc0:/BOOT/USBHDFSD.IRX", 0, NULL);
    }
}

int main(int argc, char** argv)
{
    EmulatorState emu;
    char* rom_path;

    (void)argc;
    (void)argv;

    /* PS2 init */
    SifInitRpc(0);
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();

    load_irx_modules();

    /* Wait for USB */
    int i;
    for (i = 0; i < 100; i++) {
        FILE* test = fopen("mass:/", "r");
        if (test) {
            fclose(test);
            break;
        }
        /* Small delay */
        int j;
        for (j = 0; j < 500000; j++) asm volatile("nop");
    }

    if (!ui_init()) {
        return 1;
    }

    emu_init(&emu);

    /* Try file browser */
    rom_path = ui_file_browser("mass:/ROMS");
    if (!rom_path) {
        /* Try default */
        rom_path = "mass:/ROMS/game.bin";
    }

    if (!cart_load(&emu, rom_path)) {
        ui_shutdown();
        return 1;
    }

    emu_reset(&emu);

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
