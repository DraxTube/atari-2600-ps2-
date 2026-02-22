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

static void delay_sec(int seconds)
{
    int i;
    for (i = 0; i < seconds * 50; i++) {
        /* VSync wait - more reliable */
        asm volatile(
            "li $v0, 2\n"
            "syscall\n"
            ::: "v0"
        );
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

    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    scr_printf("  SIO2MAN: %s\n", ret >= 0 ? "OK" : "FAILED");

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    scr_printf("  PADMAN: %s\n", ret >= 0 ? "OK" : "FAILED");

    scr_printf("  USBD: ");
    ret = SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, NULL);
    scr_printf("%s\n", ret >= 0 ? "OK" : "FAILED");

    scr_printf("  USBHDFSD: ");
    ret = SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, NULL);
    scr_printf("%s\n", ret >= 0 ? "OK" : "FAILED");

    scr_printf("\nWaiting 3 seconds for USB...\n");
    delay_sec(3);
    scr_printf("Done waiting.\n\n");
}

int main(int argc, char** argv)
{
    EmulatorState emu;
    const char* rom_path = NULL;
    int rom_loaded = 0;
    int i;

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
    scr_printf("Step 1: Initializing emulator...\n");
    emu_init(&emu);
    scr_printf("Step 1: OK\n\n");

    /* Init UI (pad only for now) */
    scr_printf("Step 2: Initializing controls...\n");
    if (!ui_init()) {
        scr_printf("Step 2: FAILED\n");
        SleepThread();
        return 1;
    }
    scr_printf("Step 2: OK\n\n");

    /* Try to load ROM from USB */
    const char* rom_paths[] = {
        "mass:/ROMS/game.bin",
        "mass:/ROMS/GAME.BIN",
        "mass:/game.bin",
        "mass:/GAME.BIN",
        "mass0:/ROMS/game.bin",
        "mass0:/game.bin",
        NULL
    };

    scr_printf("Step 3: Searching for ROM...\n");
    
    for (i = 0; rom_paths[i] != NULL; i++) {
        scr_printf("  Trying: %s\n", rom_paths[i]);
        
        if (cart_load(&emu, rom_paths[i])) {
            rom_path = rom_paths[i];
            rom_loaded = 1;
            scr_printf("  FOUND!\n");
            break;
        }
    }

    if (!rom_loaded) {
        scr_printf("\n");
        scr_printf("ERROR: No ROM found!\n\n");
        scr_printf("Put game.bin in USB:/ROMS/\n");
        scr_printf("USB must be FAT32.\n\n");
        SleepThread();
        return 1;
    }

    scr_printf("\nStep 4: ROM loaded!\n");
    scr_printf("  Size: %lu bytes\n\n", (unsigned long)emu.cart.rom_size);

    scr_printf("Step 5: Starting emulation...\n");
    scr_printf("  TRIANGLE = Exit\n\n");

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
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    printf("This is PS2 only.\n");
    return 1;
}
#endif
