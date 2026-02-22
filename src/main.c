#include <stdio.h>
#include <stdlib.h>
#include "emulator.h"
#include "ui.h"

#ifdef _EE
#include <kernel.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <iopcontrol.h>
#include <sifrpc.h>
#include <libpad.h>
#include <fileXio_rpc.h>

#define IRX_DEFINE(mod) \
    extern unsigned char mod##_irx[]; \
    extern unsigned int size_##mod##_irx

IRX_DEFINE(iomanX);
IRX_DEFINE(fileXio);
IRX_DEFINE(sio2man);
IRX_DEFINE(padman);
IRX_DEFINE(usbd);
IRX_DEFINE(usbhdfsd);

void load_modules(void) {
    int ret;
    
    SifExecModuleBuffer(iomanX_irx, size_iomanX_irx, 0, NULL, &ret);
    SifExecModuleBuffer(fileXio_irx, size_fileXio_irx, 0, NULL, &ret);
    SifExecModuleBuffer(sio2man_irx, size_sio2man_irx, 0, NULL, &ret);
    SifExecModuleBuffer(padman_irx, size_padman_irx, 0, NULL, &ret);
    SifExecModuleBuffer(usbd_irx, size_usbd_irx, 0, NULL, &ret);
    SifExecModuleBuffer(usbhdfsd_irx, size_usbhdfsd_irx, 0, NULL, &ret);
    
    fileXioInit();
}

int main(int argc, char** argv) {
    SifInitRpc(0);
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
    
    load_modules();
    
    // Aspetta che USB sia pronto
    int i;
    for (i = 0; i < 50; i++) {
        if (fileXioMount("mass:", "mass0:", 0) >= 0) break;
        nopdelay(1000000);
    }
    
    if (!ui_init()) {
        return 1;
    }
    
    emu_init();
    
    // File browser
    char* rom_path = ui_file_browser("mass:/ROMS");
    if (!rom_path) {
        rom_path = "mass:/ROMS/game.bin"; // Default
    }
    
    if (!cart_load(rom_path)) {
        printf("Failed to load ROM\n");
        ui_shutdown();
        return 1;
    }
    
    emu_reset();
    
    while (emu_state.running) {
        ui_handle_input();
        emu_run_frame();
        ui_render_frame();
    }
    
    emu_shutdown();
    ui_shutdown();
    
    return 0;
}

#else
// Versione PC
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <rom_file>\n", argv[0]);
        return 1;
    }
    
    if (!ui_init()) {
        return 1;
    }
    
    emu_init();
    
    if (!cart_load(argv[1])) {
        printf("Failed to load ROM: %s\n", argv[1]);
        return 1;
    }
    
    emu_reset();
    
    while (emu_state.running) {
        ui_handle_input();
        emu_run_frame();
        ui_render_frame();
    }
    
    emu_shutdown();
    ui_shutdown();
    
    return 0;
}
#endif
