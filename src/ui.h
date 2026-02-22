#ifndef UI_H
#define UI_H

#include "types.h"

int   ui_init(void);
void  ui_shutdown(void);
void  ui_render_frame(EmulatorState* emu);
void  ui_handle_input(EmulatorState* emu);
char* ui_file_browser(const char* path);

#endif
