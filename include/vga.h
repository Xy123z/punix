// include/vga.h - VGA text mode driver

#ifndef VGA_H
#define VGA_H

#include "types.h"

// VGA constants
#define VGA_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Colors
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

// VGA functions
void vga_init();
void vga_clear_screen();
void vga_putchar(char c, char color);
void vga_print(const char* str);
void vga_print_colored(const char* str, char color);
void vga_update_cursor();

// Internal cursor position
extern int vga_cursor_x;
extern int vga_cursor_y;

#endif // VGA_H
