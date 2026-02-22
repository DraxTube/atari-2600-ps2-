#include "ui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _EE

#include <kernel.h>
#include <debug.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <malloc.h>

static char padBuf[256] __attribute__((aligned(64)));
static int frame_count = 0;
static int pad_initialized = 0;

/* Graphics */
static GSGLOBAL* gsGlobal = NULL;
static GSTEXTURE texture;
static int gfx_initialized = 0;

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
    scr_printf("    Opening... ");
    FILE* f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        scr_printf("YES (%ld bytes)\n", size);
        return 1;
    }
    scr_printf("NO\n");
    return 0;
}

int ui_init(void)
{
    /* Init pad */
    padInit(0);
    if (padPortOpen(0, 0, padBuf) == 0) {
        scr_printf("Warning: padPortOpen failed\n");
        return 0;
    }
    
    int wait = 100;
    while (wait-- > 0) {
        int state = padGetState(0, 0);
        if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) break;
        simple_delay(1);
    }
    
    pad_initialized = 1;
    
    /* Init graphics AFTER selecting ROM */
    gfx_initialized = 0;
    
    return 1;
}

static int init_graphics(void)
{
    if (gfx_initialized) return 1;
    
    scr_printf("\nInitializing graphics...\n");
    
    /* DMA init */
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    
    /* GS init */
    gsGlobal = gsKit_init_global();
    if (!gsGlobal) {
        scr_printf("ERROR: gsKit_init_global failed\n");
        return 0;
    }
    
    gsGlobal->PSM = GS_PSM_CT32;
    gsGlobal->PSMZ = GS_PSMZ_16S;
    gsGlobal->ZBuffering = GS_SETTING_OFF;
    gsGlobal->DoubleBuffering = GS_SETTING_ON;
    gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
    
    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);
    
    /* Texture for Atari 2600 framebuffer (160x192) */
    texture.Width = 160;
    texture.Height = 192;
    texture.PSM = GS_PSM_CT32;
    texture.Mem = memalign(128, 160 * 192 * 4);
    
    if (!texture.Mem) {
        scr_printf("ERROR: Cannot allocate texture memory\n");
        return 0;
    }
    
    texture.Vram = gsKit_vram_alloc(gsGlobal, 
        gsKit_texture_size(160, 192, GS_PSM_CT32), 
        GSKIT_ALLOC_USERBUFFER);
    
    texture.Filter = GS_FILTER_NEAREST;
    
    scr_printf("Graphics OK!\n");
    scr_printf("Starting game...\n\n");
    
    gfx_initialized = 1;
    
    return 1;
}

void ui_shutdown(void)
{
    if (pad_initialized) {
        padPortClose(0, 0);
        padEnd();
        pad_initialized = 0;
    }
    
    if (gfx_initialized && texture.Mem) {
        free(texture.Mem);
        texture.Mem = NULL;
    }
}

void ui_render_frame(EmulatorState* emu)
{
    if (!gfx_initialized) {
        /* Initialize graphics on first render */
        if (!init_graphics()) {
            scr_printf("Graphics init failed!\n");
            emu->running = 0;
            return;
        }
    }
    
    /* Copy framebuffer to texture */
    memcpy(texture.Mem, emu->framebuffer, 160 * 192 * 4);
    
    /* Upload to VRAM */
    gsKit_TexManager_invalidate(gsGlobal, &texture);
    
    /* Clear screen */
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00));
    
    /* Calculate centered position with integer scaling */
    float scale_x = (float)gsGlobal->Width / 160.0f;
    float scale_y = (float)gsGlobal->Height / 192.0f;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    
    /* Use integer scaling for crisp pixels */
    scale = (int)scale;
    if (scale < 1) scale = 1;
    
    float w = 160.0f * scale;
    float h = 192.0f * scale;
    float x = ((float)gsGlobal->Width - w) / 2.0f;
    float y = ((float)gsGlobal->Height - h) / 2.0f;
    
    /* Draw texture */
    gsKit_prim_sprite_texture(gsGlobal, &texture,
        x, y,           /* top-left */
        0.0f, 0.0f,     /* UV start */
        x + w, y + h,   /* bottom-right */
        160.0f, 192.0f, /* UV end */
        2,              /* z */
        GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00));
    
    /* Flip */
    gsKit_sync_flip(gsGlobal);
    gsKit_queue_exec(gsGlobal);
    
    frame_count++;
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
    
    /* First test if USB is accessible */
    scr_printf("Testing USB access...\n");
    FILE* test = fopen("mass:/", "rb");
    if (test) {
        fclose(test);
        scr_printf("  mass:/ OK\n");
    } else {
        scr_printf("  mass:/ FAILED\n");
    }
    
    test = fopen("mass0:/", "rb");
    if (test) {
        fclose(test);
        scr_printf("  mass0:/ OK\n");
    } else {
        scr_printf("  mass0:/ FAILED\n");
    }
    
    scr_printf("\n");
    
    const char* try_files[] = {
        "mass:/game.bin",
        "mass:/game.a26",
        "mass:/GAME.BIN",
        "mass:/GAME.A26",
        "mass:/ROMS/game.bin",
        "mass:/ROMS/game.a26",
        "mass:/ROMS/GAME.BIN",
        "mass:/ROMS/GAME.A26",
        "mass0:/game.bin",
        "mass0:/game.a26",
        "mass0:/ROMS/game.bin",
        "mass0:/ROMS/game.a26",
        NULL
    };
    
    scr_printf("Trying file paths:\n");
    
    for (i = 0; try_files[i] != NULL; i++) {
        scr_printf("  %s\n", try_files[i]);
        
        if (file_exists(try_files[i])) {
            strcpy(selected_file, try_files[i]);
            
            scr_printf("\n\nROM FOUND: %s\n\n", selected_file);
            scr_printf("Loading in 3 seconds...\n");
            
            simple_delay(150);
            
            return selected_file;
        }
    }
    
    scr_printf("\n");
    scr_printf("ROM NOT FOUND!\n");
    scr_printf("Put game.bin on USB root\n");
    
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
