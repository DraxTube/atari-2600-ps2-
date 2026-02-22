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

/* External IRX modules - will be embedded in ELF */
extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char usbhdfsd_irx[];
extern unsigned int size_usbhdfsd_irx;

static void delay(int count)
{
    int i, j;
    for (i = 0; i < count; i++) {
        for (j = 0; j < 100000; j++) nopdelay();
    }
}

static void reset_IOP(void)
{
    SifInitRpc(0);
    scr_printf("Resetting IOP...\n");
    while (!SifIopReset("", 0)) {};
    while (!SifIopSync()) {};
    SifInitRpc(0);
    SifLoadFileInit();
    SifInitIopHeap();
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    scr_printf("IOP reset complete.\n\n");
}

static void load_modules(void)
{
    int ret;

    scr_printf("Loading IOP modules...\n");

    /* SIO2MAN and PADMAN from ROM */
    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    scr_printf("  SIO2MAN: %s\n", ret >= 0 ? "OK" : "FAILED");

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    scr_printf("  PADMAN: %s\n", ret >= 0 ? "OK" : "FAILED");

    /* Load USB modules embedded in ELF */
    scr_printf("  USBD: ");
    ret = SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, NULL);
    scr_printf("%s\n", ret >= 0 ? "OK" : "FAILED");

    scr_printf("  USBHDFSD: ");
    ret = SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, NULL);
    scr_printf("%s\n", ret >= 0 ? "OK" : "FAILED");

    /* Wait for USB to initialize */
    scr_printf("\nWaiting for USB to initialize");
    delay(10);
    scr_printf(".");
    delay(10);
    scr_printf(".");
    delay(10);
    scr_printf(".");
    delay(10);
    scr_printf(" done!\n\n");
}

int main(int argc, char** argv)
{
    EmulatorState emu;
    const char* rom_path = NULL;
    int rom_loaded = 0;

    (void)argc;
    (void)argv;

    /* Debug screen init FIRST */
    init_scr();
    scr_clear();
    scr_printf("==============================\n");
    scr_printf(" Atari 2600 Emulator for PS2\n");
    scr_printf("==============================\n\n");

    /* PS2 init */
    reset_IOP();
    load_modules();

    /* Initialize emulator */
    scr_printf("Initializing emulator...\n");
    emu_init(&emu);
    scr_printf("  OK\n\n");

    /* Init UI (pad only for now) */
    scr_printf("Initializing controls...\n");
    if (!ui_init()) {
        scr_printf("  FAILED\n\n");
        scr_printf("Cannot continue.\n");
        SleepThread();
        return 1;
    }
    scr_printf("  OK\n\n");

    /* Try to load ROM from USB */
    const char* rom_paths[] = {
        "mass:/ROMS/game.bin",
        "mass:/ROMS/GAME.BIN",
        "mass:/roms/game.bin",
        "mass:/game.bin",
        "mass:/GAME.BIN",
        "mass0:/ROMS/game.bin",
        "mass0:/game.bin",
        NULL
    };

    scr_printf("Searching for ROM...\n");
    int i;
    for (i = 0; rom_paths[i] != NULL; i++) {
        scr_printf("  %s ... ", rom_paths[i]);
        
        if (cart_load(&emu, rom_paths[i])) {
            rom_path = rom_paths[i];
            rom_loaded = 1;
            scr_printf("OK!\n\n");
            break;
        } else {
            scr_printf("no\n");
        }
    }

    if (!rom_loaded) {
        scr_printf("\n");
        scr_printf("================================\n");
        scr_printf("ERROR: ROM not found!\n");
        scr_printf("================================\n");
        scr_printf("\n");
        scr_printf("Instructions:\n");
        scr_printf("1. Format USB as FAT32\n");
        scr_printf("2. Create folder: ROMS\n");
        scr_printf("3. Copy Atari 2600 ROM as:\n");
        scr_printf("   ROMS/game.bin\n");
        scr_printf("4. Insert USB in PS2\n");
        scr_printf("5. Restart emulator\n");
        scr_printf("\n");
        scr_printf("Press any button to exit...\n");
        SleepThread();
        return 1;
    }

    scr_printf("ROM Info:\n");
    scr_printf("  Path: %s\n", rom_path);
    scr_printf("  Size: %lu bytes\n", (unsigned long)emu.cart.rom_size);
    scr_printf("  Type: ");
    switch(emu.cart.type) {
        case CART_2K: scr_printf("2K\n"); break;
        case CART_4K: scr_printf("4K\n"); break;
        case CART_F8: scr_printf("F8 (8K)\n"); break;
        case CART_F6: scr_printf("F6 (16K)\n"); break;
        case CART_F4: scr_printf("F4 (32K)\n"); break;
        default: scr_printf("Unknown\n"); break;
    }
    scr_printf("\n");

    scr_printf("Starting emulation...\n");
    scr_printf("\n");
    scr_printf("Controls:\n");
    scr_printf("  D-Pad    = Joystick\n");
    scr_printf("  X        = Fire\n");
    scr_printf("  START    = Reset\n");
    scr_printf("  TRIANGLE = Exit\n");
    scr_printf("\n");

    emu_reset(&emu);

    /* Main loop */
    while (emu.running) {
        ui_handle_input(&emu);

        if (emu.switch_reset) {
            scr_printf("Reset pressed!\n");
            emu_reset(&emu);
            emu.switch_reset = 0;
        }

        emu_run_frame(&emu);
        ui_render_frame(&emu);
    }

    scr_printf("\n\nEmulation stopped.\n");
    scr_printf("Shutting down...\n");

    emu_shutdown(&emu);
    ui_shutdown();

    scr_printf("Goodbye!\n");
    return 0;
}

#else
/* PC stub */
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    printf("This is PS2 only.\n");
    return 1;
}
#endif
