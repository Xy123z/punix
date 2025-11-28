#ifndef CONSOLE_H
#define CONSOLE_H

#include "types.h"

// VGA constants (kept for reference, but now applied to the internal buffer)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Colors (copied from original vga.h)
#define COLOR_BLACK 0x00
#define COLOR_BLUE 0x01
#define COLOR_GREEN 0x02
#define COLOR_CYAN 0x03
#define COLOR_RED 0x04
#define COLOR_MAGENTA 0x05
#define COLOR_BROWN 0x06
#define COLOR_LIGHT_GREY 0x07
#define COLOR_DARK_GREY 0x08
#define COLOR_LIGHT_BLUE 0x09
#define COLOR_LIGHT_GREEN 0x0A
#define COLOR_LIGHT_CYAN 0x0B
#define COLOR_LIGHT_RED 0x0C
#define COLOR_LIGHT_MAGENTA 0x0D
#define COLOR_YELLOW 0x0E
#define COLOR_WHITE 0x0F

#define COLOR_WHITE_ON_BLACK 0x0F
#define COLOR_GREEN_ON_BLACK 0x0A
#define COLOR_YELLOW_ON_BLACK 0x0E

// Console functions (new buffered implementation)
void console_init();
void console_clear_screen();
void console_putchar(char c, char color);
void console_print(const char* str);
void console_print_colored(const char* str, char color);

// Mouse Scroll Interface
void console_scroll_by(int lines);
int console_get_scroll_offset();

// Internal cursor position (now internal to the console state)
extern int console_cursor_x;
extern int console_cursor_y;

#endif // CONSOLE_H
