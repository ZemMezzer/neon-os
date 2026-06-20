#pragma once

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char* text);
void console_backspace(void);

void console_suspend(void);
void console_resume(void);

void console_cursor_enable(int enabled);
void console_cursor_show(void);
void console_cursor_hide(void);
void console_cursor_update(void);
int console_get_cursor_x(void);
int console_get_cursor_y(void);
int console_get_columns(void);
void console_set_cursor_pos(int x, int y);
void console_clear_line_from_cursor(void);
