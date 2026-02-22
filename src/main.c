#include "emulator.h"
#include "cartridge.h"
#include "cpu6507.h"
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

extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char usbhdfsd_irx[];
extern unsigned int size_usbhdfsd_irx;
extern unsigned char sio2man_irx[];
extern unsigned int size_sio2man_irx;
extern unsigned char padman_irx[];
extern unsigned int size_padman_irx;

static void simple_delay(int loops)
{
    volatile int i, j;
    for (i = 0; i < loops; i++) {
        for (j = 0; j < 50000; j++) {
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

    ret = SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, NULL, NULL);
    scr_printf("  SIO2MAN: %s\n", ret >= 0 ? "OK" : "FAILED");

    ret = SifExecModuleBuffer(padman_irx, size_padman_irx, 0, NULL, NULL);
    scr_printf("  PADMAN: %s\n", ret >= 0 ? "OK" : "FAILED");

    ret = SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, NULL);
    scr_printf("  USBD: %s\n", ret >= 0 ? "OK" : "FAILED");

    ret = SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, NULL);
    scr_printf("  USBHDFSD: %s\n", ret >= 0 ? "OK" : "FAILED");

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

    init_scr();
    scr_clear();
    scr_printf("==============================\n");
    scr_printf(" Atari 2600 Emulator for PS2\n");
    scr_printf("==============================\n\n");

    reset_IOP();
    load_modules();

    scr_printf("Initializing controller...\n");
    if (!ui_init()) {
        scr_printf("  FAILED\n");
        SleepThread();
        return 1;
    }
    scr_printf("  OK\n\n");

    scr_printf("Testing controller...\n");
    scr_printf("Press X to continue\n\n");
    
    int timeout = 500;
    while (timeout > 0) {
        struct padButtonStatus buttons;
        int state = padGetState(0, 0);
        
        if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) {
            if (padRead(0, 0, &buttons) != 0) {
                if ((buttons.btns & PAD_CROSS) == 0) {
                    scr_printf("X pressed! Controller OK.\n\n");
                    simple_delay(30);
                    break;
                }
            }
        }
        
        simple_delay(1);
        timeout--;
    }
    
    if (timeout <= 0) {
        scr_printf("Controller timeout - continuing anyway...\n\n");
    }

    scr_printf("Initializing emulator... ");
    emu_init(&emu);
    scr_printf("OK\n\n");

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
        SleepThread();
        return 1;
    }
    
    scr_printf("OK\n");
    scr_printf("Size: %lu bytes\n", (unsigned long)emu.cart.rom_size);
    scr_printf("Type: ");
    switch(emu.cart.type) {
        case CART_2K: scr_printf("2K\n"); break;
        case CART_4K: scr_printf("4K\n"); break;
        case CART_F8: scr_printf("F8 (8K)\n"); break;
        case CART_F6: scr_printf("F6 (16K)\n"); break;
        case CART_F4: scr_printf("F4 (32K)\n"); break;
        default: scr_printf("Unknown\n"); break;
    }
    
    scr_printf("\nResetting CPU...\n");
    emu_reset(&emu);
    
    scr_printf("Reset vector: 0x%04X\n", emu.cpu.PC);
    scr_printf("First bytes: %02X %02X %02X %02X\n",
        mem_read(&emu, emu.cpu.PC),
        mem_read(&emu, emu.cpu.PC + 1),
        mem_read(&emu, emu.cpu.PC + 2),
        mem_read(&emu, emu.cpu.PC + 3));
    
    scr_printf("\nStarting emulation...\n");
    scr_printf("SELECT = show debug\n");
    scr_printf("START = reset\n");
    scr_printf("TRIANGLE = exit\n\n");
    
    simple_delay(100);

    int debug_counter = 0;

    while (emu.running) {
        ui_handle_input(&emu);

        if (emu.switch_reset) {
            scr_printf("RESET!\n");
            emu_reset(&emu);
            emu.switch_reset = 0;
        }

        if (emu.switch_select && (debug_counter % 60 == 0)) {
            scr_printf("PC:%04X A:%02X X:%02X Y:%02X P:%02X SP:%02X Scan:%d\n",
                emu.cpu.PC, emu.cpu.A, emu.cpu.X, emu.cpu.Y, 
                emu.cpu.P, emu.cpu.SP, emu.tia.scanline);
        }
        debug_counter++;

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
