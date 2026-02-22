#include "ui.h"
#include <string.h>
#include <stdio.h>

#ifdef _EE

#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <malloc.h>
#include <libpad.h>
#include <dirent.h>

static GSGLOBAL* gsGlobal = NULL;
static GSTEXTURE fbTexture;
static char padBuf[256] __attribute__((aligned(64)));

/* Debounce */
static uint16_t pad_old = 0xFFFF;

int ui_init(void)
{
    /* GS init */
    gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->PSM = GS_PSM_CT32;
    gsGlobal->PSMZ = GS_PSMZ_16S;
    gsGlobal->DoubleBuffering = GS_SETTING_ON;
    gsGlobal->ZBuffering = GS_SETTING_OFF;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    /* Texture for framebuffer */
    memset(&fbTexture, 0, sizeof(fbTexture));
    fbTexture.Width = 160;
    fbTexture.Height = 192;
    fbTexture.PSM = GS_PSM_CT32;
    fbTexture.Filter = GS_FILTER_LINEAR;
    fbTexture.Mem = (u32*)memalign(128, 160 * 192 * 4);

    if (!fbTexture.Mem) return 0;

    fbTexture.Vram = gsKit_vram_alloc(gsGlobal,
        gsKit_texture_size(160, 192, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);

    /* Pad init */
    padInit(0);
    padPortOpen(0, 0, padBuf);

    return 1;
}

void ui_shutdown(void)
{
    padPortClose(0, 0);
    padEnd();
    if (fbTexture.Mem) free(fbTexture.Mem);
}

void ui_render_frame(EmulatorState* emu)
{
    /* Copy framebuffer to texture */
    memcpy(fbTexture.Mem, emu->framebuffer, 160 * 192 * 4);
    gsKit_TexManager_invalidate(gsGlobal, &fbTexture);

    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00));

    /* Scale to screen keeping aspect ratio */
    float sx = 640.0f / 160.0f;
    float sy = 448.0f / 192.0f;
    float s = (sx < sy) ? sx : sy;
    float w = 160.0f * s;
    float h = 192.0f * s;
    float ox = (640.0f - w) / 2.0f;
    float oy = (448.0f - h) / 2.0f;

    gsKit_prim_sprite_texture(gsGlobal, &fbTexture,
        ox, oy, 0.0f, 0.0f,
        ox + w, oy + h, 160.0f, 192.0f,
        1, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00));

    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);
}

void ui_handle_input(EmulatorState* emu)
{
    struct padButtonStatus buttons;
    int state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) return;

    if (padRead(0, 0, &buttons) == 0) return;

    uint16_t btns = buttons.btns;

    /* Joystick */
    emu->joy0_up    = !(btns & PAD_UP);
    emu->joy0_down  = !(btns & PAD_DOWN);
    emu->joy0_left  = !(btns & PAD_LEFT);
    emu->joy0_right = !(btns & PAD_RIGHT);
    emu->joy0_fire  = !(btns & PAD_CROSS);

    /* Console switches */
    emu->switch_reset  = !(btns & PAD_START);
    emu->switch_select = !(btns & PAD_SELECT);

    /* Triangle = exit */
    if (!(btns & PAD_TRIANGLE)) {
        emu->running = 0;
    }

    pad_old = btns;
}

char* ui_file_browser(const char* path)
{
    static char selected[512];
    char files[64][256];
    int count = 0;
    int selection = 0;

    DIR* dir = opendir(path);
    if (!dir) return NULL;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && count < 64) {
        int len = strlen(entry->d_name);
        if (len > 4) {
            char* ext = entry->d_name + len - 4;
            if (strcmp(ext, ".bin") == 0 || strcmp(ext, ".a26") == 0 ||
                strcmp(ext, ".BIN") == 0 || strcmp(ext, ".A26") == 0) {
                strncpy(files[count], entry->d_name, 255);
                files[count][255] = 0;
                count++;
            }
        }
    }
    closedir(dir);

    if (count == 0) return NULL;

    int done = 0;
    uint16_t prev_btns = 0xFFFF;

    while (!done) {
        /* Clear screen */
        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x10, 0x10, 0x30, 0x80, 0x00));

        /* Very simple list rendering - just colored rectangles for now */
        /* A proper implementation would use gsKit font rendering */
        int start = (selection > 15) ? selection - 15 : 0;
        for (int i = start; i < count && i < start + 20; i++) {
            float y = 20.0f + (float)(i - start) * 20.0f;
            uint64_t color;
            if (i == selection) {
                color = GS_SETREG_RGBAQ(0xFF, 0xFF, 0x00, 0x80, 0x00);
            } else {
                color = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00);
            }
            gsKit_prim_sprite(gsGlobal, 20.0f, y, 620.0f, y + 16.0f, 1, color);
        }

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);

        /* Read pad */
        struct padButtonStatus buttons;
        int state = padGetState(0, 0);
        if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) {
            if (padRead(0, 0, &buttons) != 0) {
                uint16_t btns = buttons.btns;
                uint16_t pressed = prev_btns & ~btns;

                if (pressed & PAD_UP) {
                    selection = (selection - 1 + count) % count;
                }
                if (pressed & PAD_DOWN) {
                    selection = (selection + 1) % count;
                }
                if (pressed & PAD_CROSS) {
                    snprintf(selected, sizeof(selected), "%s/%s", path, files[selection]);
                    done = 1;
                }
                if (pressed & PAD_CIRCLE) {
                    return NULL;
                }
                prev_btns = btns;
            }
        }
    }

    return selected;
}

#else
/* ================ PC Stub (for testing) ================ */

int ui_init(void) { return 1; }
void ui_shutdown(void) {}
void ui_render_frame(EmulatorState* emu) { (void)emu; }
void ui_handle_input(EmulatorState* emu) { (void)emu; }
char* ui_file_browser(const char* path) { (void)path; return NULL; }

#endif
