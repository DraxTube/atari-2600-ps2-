#ifndef UI_H
#define UI_H

int ui_init(void);
void ui_shutdown(void);
char* ui_file_browser(const char* path);
void ui_render_frame(void);
void ui_handle_input(void);

#endif
