#include "ui.h"
#include <string.h>
#include <stdio.h>

#ifdef _EE

#include <kernel.h>
#include <debug.h>
#include <libpad.h>

static char padBuf[256] __attribute__((aligned(64)));
static uint16_t pad_old = 0xFFFF;
static int frame_count = 0;

int ui_init(void)
{
    /* Simple pad init */
    padInit(0);
    if (padPortOpen(0, 0, padBuf) == 0) {
        scr_printf("Warning: padPortOpen failed\n");
    }
    
    return 1;
}

void ui_shutdown(void)
{
    padPortClose(0, 0);
    padEnd();
}

void ui_render_frame(EmulatorState* emu)
{
    /* For now just show frame count on debug screen every 60 frames */
    frame_count++;
    if ((frame_count % 60) == 0) {
        scr_printf("\rFrame: %d  ", frame_count);
    }
    
    (void)emu;
}

void ui_handle_input(EmulatorState* emu)
{
    struct padButtonStatus buttons;
    int state = padGetState(0, 0);
    
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) {
        return;
    }

    if (padRead(0, 0, &buttons) == 0) {
        return;
    }

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
    (void)path;
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
