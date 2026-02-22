#include "ui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _EE

#include <kernel.h>
#include <debug.h>
#include <libpad.h>

static char padBuf[256] __attribute__((aligned(64)));
static int frame_count = 0;
static int pad_initialized = 0;

#define MAX_PATH_LEN 256

static void simple_delay(int loops)
{
    volatile int i, j;
    for (i = 0; i < loops; i++) {
        for (j = 0; j < 50000; j++) {
            /* Empty loop */
        }
    }
}

static uint16_t read_pad(void)
{
    struct padButtonStatus buttons;
    int state;
    
    if (!pad_initialized) return 0xFFFF;
    
    state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        return 0xFFFF;
    }

    if (padRead(0, 0, &buttons) == 0) {
        return 0xFFFF;
    }

    return buttons.btns;
}

static int file_exists(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

int ui_init(void)
{
    padInit(0);
    if (padPortOpen(0, 0, padBuf) == 0) {
        scr_printf("Warning: padPortOpen failed\n");
        return 0;
    }
    
    /* Wait for pad to stabilize */
    int wait = 100;
    while (wait-- > 0) {
        int state = padGetState(0, 0);
        if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) break;
        simple_delay(1);
    }
    
    pad_initialized = 1;
    return 1;
}

void ui_shutdown(void)
{
    if (pad_initialized) {
        padPortClose(0, 0);
        padEnd();
        pad_initialized = 0;
    }
}

void ui_render_frame(EmulatorState* emu)
{
    frame_count++;
    if ((frame_count % 60) == 0) {
        scr_printf("\rFrame: %d  ", frame_count);
    }
    (void)emu;
}

void ui_handle_input(EmulatorState* emu)
{
    uint16_t btns = read_pad();
    
    if (btns == 0xFFFF) return;

    emu->joy0_up    = (btns & PAD_UP) == 0;
    emu->joy0_down  = (btns & PAD_DOWN) == 0;
    emu->joy0_left  = (btns & PAD_LEFT) == 0;
    emu->joy0_right = (btns & PAD_RIGHT) == 0;
    emu->joy0_fire  = (btns & PAD_CROSS) == 0;

    emu->switch_reset  = (btns & PAD_START) == 0;
    emu->switch_select = (btns & PAD_SELECT) == 0;

    if ((btns & PAD_TRIANGLE) == 0) {
        emu->running = 0;
    }
}

char* ui_file_browser(const char* start_path)
{
    static char selected_file[MAX_PATH_LEN];
    int i;
    
    (void)start_path;
    
    scr_printf("Searching for ROM file...\n\n");
    
    /* Try many common file paths directly */
    const char* try_files[] = {
        "mass:/ROMS/game.bin",
        "mass:/ROMS/game.a26",
        "mass:/ROMS/GAME.BIN",
        "mass:/ROMS/GAME.A26",
        "mass:/ROMS/Game.bin",
        "mass:/ROMS/Game.a26",
        "mass:/roms/game.bin",
        "mass:/roms/game.a26",
        "mass:/game.bin",
        "mass:/game.a26",
        "mass:/GAME.BIN",
        "mass:/GAME.A26",
        "mass0:/ROMS/game.bin",
        "mass0:/ROMS/game.a26",
        "mass0:/game.bin",
        "mass0:/game.a26",
        /* Common ROM names */
        "mass:/ROMS/pitfall.bin",
        "mass:/ROMS/pacman.bin",
        "mass:/ROMS/asteroids.bin",
        "mass:/ROMS/spaceinvaders.bin",
        "mass:/ROMS/breakout.bin",
        "mass:/ROMS/combat.bin",
        "mass:/ROMS/adventure.bin",
        "mass:/ROMS/pitfall.a26",
        "mass:/ROMS/pacman.a26",
        "mass:/ROMS/asteroids.a26",
        "mass:/ROMS/spaceinvaders.a26",
        "mass:/ROMS/breakout.a26",
        "mass:/ROMS/combat.a26",
        "mass:/ROMS/adventure.a26",
        NULL
    };
    
    for (i = 0; try_files[i] != NULL; i++) {
        scr_printf("  %s ... ", try_files[i]);
        
        if (file_exists(try_files[i])) {
            scr_printf("FOUND!\n\n");
            
            strcpy(selected_file, try_files[i]);
            
            scr_printf("ROM: %s\n\n", selected_file);
            scr_printf("Press X to load\n");
            scr_printf("Press TRIANGLE to cancel\n");
            
            /* Wait a bit for any previous button to release */
            simple_delay(50);
            
            /* Wait for button press */
            while (1) {
                uint16_t btns = read_pad();
                
                /* X pressed (bit is 0 when pressed) */
                if ((btns & PAD_CROSS) == 0) {
                    scr_printf("\nLoading...\n");
                    simple_delay(20);
                    return selected_file;
                }
                
                /* Triangle pressed */
                if ((btns & PAD_TRIANGLE) == 0) {
                    scr_printf("\nCancelled.\n");
                    simple_delay(20);
                    return NULL;
                }
                
                simple_delay(1);
            }
        } else {
            scr_printf("no\n");
        }
    }
    
    scr_printf("\n");
    scr_printf("================================\n");
    scr_printf("ROM NOT FOUND!\n");
    scr_printf("================================\n");
    scr_printf("\n");
    scr_printf("Rename your ROM to 'game.bin'\n");
    scr_printf("and put it in USB:/ROMS/\n");
    scr_printf("\n");
    scr_printf("USB must be FAT32 format.\n");
    scr_printf("\n");
    
    return NULL;
}

#else
/* PC Stub */
int ui_init(void) { return 1; }
void ui_shutdown(void) {}
void ui_render_frame(EmulatorState* emu) { (void)emu; }
void ui_handle_input(EmulatorState* emu) { (void)emu; }
char* ui_file_browser(const char* path) { (void)path; return NULL; }
#endif
