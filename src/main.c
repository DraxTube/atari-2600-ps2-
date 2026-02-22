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
#include <libmc.h>

/* External IRX modules */
extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char usbhdfsd_irx[];
extern unsigned int size_usbhdfsd_irx;

static void simple_delay(int loops)
{
    volatile int i, j;
    for (i = 0; i < loops; i++) {
        for (j = 0; j < 50000; j++) {
            /* Empty loop */
        }
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

    scr_printf("\nWaiting for USB");
    int i;
    for (i = 0; i < 30; i++) {
        simple_delay(100);
        scr_printf(".");
    }
    scr_printf(" done!\n\n");
}

int main(int argc, char** argv)
{
    EmulatorState emu;
    char* rom_path = NULL;

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
    scr_printf("Initializing emulator... ");
    emu_init(&emu);
    scr_printf("OK\n");

    /* Init UI */
    scr_printf("Initializing controls... ");
    if (!ui_init()) {
        scr_printf("FAILED\n");
        SleepThread();
        return 1;
    }
    scr_printf("OK\n\n");

    /* File browser */
    scr_printf("Opening file browser...\n\n");
    simple_delay(50);
    
    rom_path = ui_file_browser("mass:/");
    
    if (rom_path == NULL) {
        scr_printf("\nNo ROM selected.\n");
        SleepThread();
        return 1;
    }

    scr_printf("\nSelected: %s\n", rom_path);
    scr_printf("Loading ROM... ");
    
    if (!cart_load(&emu, rom_path)) {
        scr_printf("FAILED\n");
        scr_printf("Cannot load ROM file.\n");
        SleepThread();
        return 1;
    }
    
    scr_printf("OK\n");
    scr_printf("Size: %lu bytes\n\n", (unsigned long)emu.cart.rom_size);

    scr_printf("Starting emulation...\n");
    scr_printf("Press TRIANGLE to exit\n\n");
    
    simple_delay(100);

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
