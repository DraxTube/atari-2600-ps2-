#include "ui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _EE

#include <kernel.h>
#include <debug.h>
#include <libpad.h>
#include <dirent.h>

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

    emu->joy0_up    = !(btns & PAD_UP);
    emu->joy0_down  = !(btns & PAD_DOWN);
    emu->joy0_left  = !(btns & PAD_LEFT);
    emu->joy0_right = !(btns & PAD_RIGHT);
    emu->joy0_fire  = !(btns & PAD_CROSS);

    emu->switch_reset  = !(btns & PAD_START);
    emu->switch_select = !(btns & PAD_SELECT);

    if (!(btns & PAD_TRIANGLE)) {
        emu->running = 0;
    }
}

char* ui_file_browser(const char* start_path)
{
    static char selected_file[MAX_PATH_LEN];
    
    scr_printf("Searching for ROMs...\n\n");
    
    /* Try common paths directly */
    const char* try_paths[] = {
        "mass:/ROMS",
        "mass:/roms",
        "mass:/Roms",
        "mass:/",
        "mass0:/ROMS",
        "mass0:/",
        NULL
    };
    
    int p;
    for (p = 0; try_paths[p] != NULL; p++) {
        scr_printf("Checking %s ... ", try_paths[p]);
        
        DIR* dir = opendir(try_paths[p]);
        if (dir == NULL) {
            scr_printf("no\n");
            continue;
        }
        
        scr_printf("ok\n");
        scr_printf("  Reading contents...\n");
        
        struct dirent* entry;
        int found_count = 0;
        
        while ((entry = readdir(dir)) != NULL) {
            /* Skip hidden files */
            if (entry->d_name[0] == '.') continue;
            
            int len = strlen(entry->d_name);
            if (len < 4) continue;
            
            /* Check extension */
            char* ext = entry->d_name + len - 4;
            if (strcasecmp(ext, ".bin") == 0 || strcasecmp(ext, ".a26") == 0) {
                found_count++;
                scr_printf("  FOUND: %s\n", entry->d_name);
                
                /* Build full path */
                snprintf(selected_file, MAX_PATH_LEN, "%s/%s", 
                    try_paths[p], entry->d_name);
                
                closedir(dir);
                
                scr_printf("\n");
                scr_printf("Press X to load this ROM\n");
                scr_printf("Press TRIANGLE to cancel\n");
                
                /* Wait for button */
                while (1) {
                    uint16_t btns = read_pad();
                    
                    if (!(btns & PAD_CROSS)) {
                        /* Wait for release */
                        while (!(read_pad() & PAD_CROSS)) {
                            simple_delay(1);
                        }
                        return selected_file;
                    }
                    
                    if (!(btns & PAD_TRIANGLE)) {
                        /* Wait for release */
                        while (!(read_pad() & PAD_TRIANGLE)) {
                            simple_delay(1);
                        }
                        scr_printf("Cancelled.\n");
                        return NULL;
                    }
                    
                    simple_delay(1);
                }
            }
        }
        
        closedir(dir);
        
        if (found_count == 0) {
            scr_printf("  No ROMs here\n");
        }
    }
    
    scr_printf("\n");
    scr_printf("================================\n");
    scr_printf("ERROR: No ROM files found!\n");
    scr_printf("================================\n");
    scr_printf("\n");
    scr_printf("Instructions:\n");
    scr_printf("1. USB must be FAT32\n");
    scr_printf("2. Create folder ROMS\n");
    scr_printf("3. Put .bin or .a26 files inside\n");
    scr_printf("\n");
    
    (void)start_path;
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
