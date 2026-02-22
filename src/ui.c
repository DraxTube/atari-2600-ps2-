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
static uint16_t pad_old = 0xFFFF;
static int frame_count = 0;
static int pad_initialized = 0;

/* File browser state */
#define MAX_ENTRIES 256
#define MAX_PATH_LEN 256

typedef struct {
    char name[128];
    int is_dir;
} FileEntry;

static FileEntry file_list[MAX_ENTRIES];
static int file_count = 0;
static char current_path[MAX_PATH_LEN];

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

static int is_rom_file(const char* name)
{
    int len = strlen(name);
    if (len < 4) return 0;
    
    const char* ext = name + len - 4;
    
    if (strcasecmp(ext, ".bin") == 0) return 1;
    if (strcasecmp(ext, ".a26") == 0) return 1;
    
    return 0;
}

static int scan_directory(const char* path)
{
    DIR* dir;
    struct dirent* entry;
    char fullpath[MAX_PATH_LEN];
    
    file_count = 0;
    
    scr_printf("  Scanning: %s\n", path);
    
    /* Add parent directory entry if not at root */
    if (strcmp(path, "mass:/") != 0 && strcmp(path, "mass:") != 0) {
        strcpy(file_list[file_count].name, "..");
        file_list[file_count].is_dir = 1;
        file_count++;
    }
    
    scr_printf("  Opening directory...\n");
    
    dir = opendir(path);
    if (dir == NULL) {
        scr_printf("  ERROR: Cannot open directory!\n");
        scr_printf("  Make sure USB is FAT32 formatted.\n");
        return 0;
    }
    
    scr_printf("  Reading entries...\n");
    
    while ((entry = readdir(dir)) != NULL && file_count < MAX_ENTRIES) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0) continue;
        if (strcmp(entry->d_name, "..") == 0) continue;
        
        scr_printf("    Found: %s\n", entry->d_name);
        
        /* Check if it's a directory by trying to open it */
        snprintf(fullpath, MAX_PATH_LEN, "%s%s%s", 
            path, 
            (path[strlen(path)-1] == '/') ? "" : "/",
            entry->d_name);
        
        int is_dir = 0;
        DIR* test = opendir(fullpath);
        if (test) {
            is_dir = 1;
            closedir(test);
        }
        
        /* Show directories and ROM files */
        if (is_dir || is_rom_file(entry->d_name)) {
            strncpy(file_list[file_count].name, entry->d_name, 127);
            file_list[file_count].name[127] = '\0';
            file_list[file_count].is_dir = is_dir;
            file_count++;
        }
    }
    
    closedir(dir);
    
    scr_printf("  Found %d items.\n", file_count);
    
    return file_count;
}

static void draw_file_list(int selected, int scroll)
{
    int i;
    int lines_to_show = 18;
    
    scr_clear();
    scr_printf("=== Atari 2600 ROM Browser ===\n\n");
    scr_printf("Path: %s\n\n", current_path);
    
    if (file_count == 0) {
        scr_printf("No ROM files found!\n\n");
        scr_printf("Put .bin or .a26 files on USB\n");
    } else {
        for (i = scroll; i < file_count && i < scroll + lines_to_show; i++) {
            if (i == selected) {
                scr_printf("> ");
            } else {
                scr_printf("  ");
            }
            
            if (file_list[i].is_dir) {
                scr_printf("[DIR] %s\n", file_list[i].name);
            } else {
                scr_printf("      %s\n", file_list[i].name);
            }
        }
    }
    
    scr_printf("\n------------------------------\n");
    scr_printf("UP/DOWN=Navigate X=Select\n");
    scr_printf("O=Back TRIANGLE=Cancel\n");
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

    pad_old = btns;
}

char* ui_file_browser(const char* start_path)
{
    static char selected_file[MAX_PATH_LEN];
    int selected = 0;
    int scroll = 0;
    uint16_t btns, old_btns = 0xFFFF;
    int debounce = 0;
    
    scr_printf("File browser starting...\n");
    scr_printf("Start path: %s\n\n", start_path);
    
    strncpy(current_path, start_path, MAX_PATH_LEN - 1);
    current_path[MAX_PATH_LEN - 1] = '\0';
    
    /* Initial scan */
    scr_printf("Scanning directory...\n");
    int count = scan_directory(current_path);
    
    scr_printf("\nPress X to continue...\n");
    
    /* Wait for X button */
    while (1) {
        btns = read_pad();
        if (!(btns & PAD_CROSS)) break;
        if (!(btns & PAD_TRIANGLE)) return NULL;
        simple_delay(1);
    }
    /* Wait for release */
    while (1) {
        btns = read_pad();
        if (btns & PAD_CROSS) break;
        simple_delay(1);
    }
    
    if (count == 0) {
        scr_printf("\nNo files found!\n");
        scr_printf("Put .bin or .a26 ROM files on USB.\n");
        SleepThread();
        return NULL;
    }
    
    draw_file_list(selected, scroll);
    
    while (1) {
        btns = read_pad();
        
        /* Debounce */
        if (debounce > 0) {
            debounce--;
            simple_delay(1);
            continue;
        }
        
        /* Detect button press */
        uint16_t pressed = old_btns & ~btns;
        old_btns = btns;
        
        if (pressed == 0) {
            simple_delay(1);
            continue;
        }
        
        int redraw = 0;
        
        /* UP */
        if (pressed & PAD_UP) {
            if (selected > 0) {
                selected--;
                if (selected < scroll) scroll = selected;
                redraw = 1;
            }
            debounce = 5;
        }
        
        /* DOWN */
        if (pressed & PAD_DOWN) {
            if (selected < file_count - 1) {
                selected++;
                if (selected >= scroll + 18) scroll = selected - 17;
                redraw = 1;
            }
            debounce = 5;
        }
        
        /* X - Select */
        if (pressed & PAD_CROSS) {
            if (file_count > 0) {
                if (file_list[selected].is_dir) {
                    if (strcmp(file_list[selected].name, "..") == 0) {
                        char* last_slash = strrchr(current_path, '/');
                        if (last_slash && last_slash != current_path) {
                            *last_slash = '\0';
                            if (strlen(current_path) < 6) {
                                strcpy(current_path, "mass:/");
                            }
                        }
                    } else {
                        int len = strlen(current_path);
                        if (current_path[len-1] != '/') {
                            strcat(current_path, "/");
                        }
                        strcat(current_path, file_list[selected].name);
                    }
                    selected = 0;
                    scroll = 0;
                    scan_directory(current_path);
                    redraw = 1;
                } else {
                    snprintf(selected_file, MAX_PATH_LEN, "%s%s%s",
                        current_path,
                        (current_path[strlen(current_path)-1] == '/') ? "" : "/",
                        file_list[selected].name);
                    return selected_file;
                }
            }
            debounce = 10;
        }
        
        /* O - Back */
        if (pressed & PAD_CIRCLE) {
            char* last_slash = strrchr(current_path, '/');
            if (last_slash && last_slash != current_path) {
                *last_slash = '\0';
                if (strlen(current_path) < 6) {
                    strcpy(current_path, "mass:/");
                }
                selected = 0;
                scroll = 0;
                scan_directory(current_path);
                redraw = 1;
            }
            debounce = 10;
        }
        
        /* Triangle - Cancel */
        if (pressed & PAD_TRIANGLE) {
            return NULL;
        }
        
        if (redraw) {
            draw_file_list(selected, scroll);
        }
        
        simple_delay(1);
    }
    
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
