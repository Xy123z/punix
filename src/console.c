// src/console.c - Buffered Console Driver with Mouse Scrolling Support
#include "../include/console.h"
#include "../include/string.h" // For memset, memcpy, etc.
#include "../include/types.h"

// Hardware address
#define VGA_MEMORY 0xB8000

// Depth of the internal history buffer (200 lines * 80 chars)
#define CONSOLE_LINES 200
#define CONSOLE_SIZE (VGA_WIDTH * CONSOLE_LINES)

// The internal RAM buffer that holds all console output history (char + attribute)
static uint16_t console_buffer[CONSOLE_SIZE];

// --- State Variables ---
int console_cursor_x = 0;
int console_cursor_y = 0;
// The line index in console_buffer that is currently displayed at VGA_ADDRESS row 0
static int console_scroll_offset = 0;
// The maximum line index containing content (used to set the scroll limit)
static int console_content_end_y = 0;

// Inline assembly for I/O port access
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Function to update the hardware cursor position on the screen
static void console_update_hw_cursor() {
    uint16_t position = 0;

    // Only set cursor position if it is currently visible
    if (console_cursor_y >= console_scroll_offset &&
        console_cursor_y < console_scroll_offset + VGA_HEIGHT) {

        // Calculate relative position within the 25-line visible window
        position = (console_cursor_y - console_scroll_offset) * VGA_WIDTH + console_cursor_x;

        // Update VGA registers
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t)(position & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t)((position >> 8) & 0xFF));
    }
}

// Redraws the visible portion of the screen by copying from the internal buffer to VGA memory.
static void console_update_vga() {
    uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
    int start_index = console_scroll_offset * VGA_WIDTH;

    // Loop through the visible VGA rows (0 to 24)
    for (int y = 0; y < VGA_HEIGHT; y++) {
        int source_index = start_index + (y * VGA_WIDTH);

        // Check bounds against the internal buffer size
        if (source_index < CONSOLE_SIZE) {
            // Copy one full line from RAM buffer to VGA memory
            for (int x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = console_buffer[source_index + x];
            }
        } else {
            // If we scrolled past the end of the conceptual buffer, fill the rest with black space
            uint16_t blank_char = 0x20 | (COLOR_WHITE_ON_BLACK << 8);
            for (int x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = blank_char; // ' '
            }
        }
    }

    console_update_hw_cursor();
}

// Handles content scrolling when the internal buffer fills up (monolithic style scroll)
static void console_content_scroll() {
    // If the cursor is at or past the maximum line index (200), shift all content up by one line.
    // This is needed to make space for new content at the bottom.
    if (console_cursor_y >= CONSOLE_LINES) {
        // Shift buffer contents up by one line (VGA_WIDTH elements per line, 2 bytes per element)
        // NOTE: Requires a working memcpy, assumed available through string.h
        // If memcpy is not available, replace this with a standard for loop copy.
        // For simplicity, we assume `string.h` includes `memcpy`.

        // Move CONSOLE_LINES - 1 lines up
        memcpy(console_buffer, console_buffer + VGA_WIDTH, (CONSOLE_LINES - 1) * VGA_WIDTH * sizeof(uint16_t));

        // Zero out the last line (the new blank line at the bottom)
        uint16_t blank_char = 0x20 | (COLOR_WHITE_ON_BLACK << 8);
        for (int i = 0; i < VGA_WIDTH; i++) {
            console_buffer[(CONSOLE_LINES - 1) * VGA_WIDTH + i] = blank_char;
        }

        console_cursor_y = CONSOLE_LINES - 1; // Cursor stays at the last line
        console_content_end_y = CONSOLE_LINES - 1; // Content end stays at the last line
    }
}


// PUBLIC API: Initializes the console system
void console_init() {
    // Clear the internal buffer completely with blank spaces and default color
    uint16_t blank_char = 0x20 | (COLOR_WHITE_ON_BLACK << 8);
    for (int i = 0; i < CONSOLE_SIZE; i++) {
        console_buffer[i] = blank_char;
    }
    console_cursor_x = 0;
    console_cursor_y = 0;
    console_scroll_offset = 0;
    console_content_end_y = 0;

    console_update_vga(); // Draw the initialized screen
}

// PUBLIC API: Prints a single character
void console_putchar(char c, char color) {
    if (c == '\n') {
        console_cursor_x = 0;
        console_cursor_y++;
    } else if (c == '\r') {
        console_cursor_x = 0;
    } else if (c == '\b') {
        if (console_cursor_x > 0) {
            console_cursor_x--;
        } else if (console_cursor_y > 0) {
            console_cursor_y--;
            console_cursor_x = VGA_WIDTH - 1;
        }
        int index = console_cursor_y * VGA_WIDTH + console_cursor_x;
        // Place a blank character at the new cursor position
        console_buffer[index] = 0x20 | (color << 8);
    } else {
        // Place character in the buffer
        int index = console_cursor_y * VGA_WIDTH + console_cursor_x;
        if (index < CONSOLE_SIZE) {
            console_buffer[index] = c | (color << 8);
        }
        console_cursor_x++;
    }

    // Handle line wrap
    if (console_cursor_x >= VGA_WIDTH) {
        console_cursor_x = 0;
        console_cursor_y++;
    }

    // Handle full buffer
    if (console_cursor_y >= CONSOLE_LINES) {
        console_content_scroll(); // Shift content up
    }

    // Update content end marker
    if (console_cursor_y > console_content_end_y) {
        console_content_end_y = console_cursor_y;
    }

    // When new content is written, automatically scroll to the bottom of the content
    int required_offset = console_cursor_y - (VGA_HEIGHT - 1);
    if (required_offset < 0) required_offset = 0;
    console_scroll_offset = required_offset;

    console_update_vga(); // Redraw the visible screen
}

// PUBLIC API: Prints a string
void console_print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        console_putchar(str[i], COLOR_WHITE_ON_BLACK);
    }
}

// PUBLIC API: Prints a colored string
void console_print_colored(const char* str, char color) {
    for (int i = 0; str[i] != '\0'; i++) {
        console_putchar(str[i], color);
    }
}

// PUBLIC API: Handles scrolling the view up or down (called by mouse driver)
void console_scroll_by(int lines) {
    int new_offset = console_scroll_offset - lines; // Negative lines scrolls down

    // Calculate maximum scroll (the point where the last line of content hits the bottom of the screen)
    // The maximum line index is console_content_end_y.
    // If the entire screen is full of content, max_scroll should be console_content_end_y - (VGA_HEIGHT - 1)
    int max_scroll = console_content_end_y - (VGA_HEIGHT - 1);
    if (max_scroll < 0) max_scroll = 0;

    // Boundary check: Top (offset >= 0)
    if (new_offset < 0) {
        new_offset = 0;
    }

    // Boundary check: Bottom (offset <= max_scroll)
    if (new_offset > max_scroll) {
        new_offset = max_scroll;
    }

    // If the screen is showing the absolute latest input, move the cursor back to visible area
    if (new_offset < console_scroll_offset) {
        // Scrolling up (away from latest input) - we hide the cursor by placing it off-screen
        console_update_hw_cursor();
    }

    if (new_offset != console_scroll_offset) {
        console_scroll_offset = new_offset;
        console_update_vga(); // Redraw screen to show the new view window
    }
}

// PUBLIC API: Get the current line offset
int console_get_scroll_offset() {
    return console_scroll_offset;
}

// PUBLIC API: Clears the entire buffer
void console_clear_screen() {
    console_init(); // Reuse initialization to clear the buffer and reset state
    // Note: shell.c expects clear to also call shell_init, which calls console_print_colored
    // console_update_vga is called inside console_init, so we are good.
}

// --------------------------------------------------------------------------
// PUBLIC SCROLL WRAPPERS FOR KEYBOARD/MOUSE HANDLERS (Missing functions)
// --------------------------------------------------------------------------

/**
 * @brief Scrolls the view up (toward older content/history) by 3 lines.
 * This increases the scroll offset toward the maximum content end.
 */
void console_scroll_up() {
    // Scroll up 3 lines (view moves up, offset increases).
    // The implementation of console_scroll_by uses subtraction: new_offset = current_offset - lines.
    // To increase offset (scroll up), 'lines' must be negative.
    console_scroll_by(1);
}

/**
 * @brief Scrolls the view down (toward newer content/current prompt) by 3 lines.
 * This decreases the scroll offset toward 0.
 */
void console_scroll_down() {
    // Scroll down 3 lines (view moves down, offset decreases).
    // To decrease offset (scroll down), 'lines' must be positive.
    console_scroll_by(-1);
}
