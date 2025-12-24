/**
 * src/text.c - Simple Text Editor
 * Refactored for ID-based FS and ATA Persistence
 */

#include "../include/text.h"
#include "../include/fs.h"
#include "../include/syscall.h"
#include "../include/console.h"
#include "../include/string.h"
#include "../include/interrupt.h"
#include "../include/memory.h"

// Define Control characters
#define CTRL_S 0x13
#define CTRL_X 0x18

// Max safe size for the editor buffer
#define MAX_EDITOR_SIZE 511

// --- Helper Functions ---

static void read_line(char* buffer, int max_len) {
    int i = 0;
    while (i < max_len - 1) {
        char c = sys_getchar();
        if (c == '\n') {
            buffer[i] = '\0';
            sys_putchar('\n');
            break;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                sys_putchar('\b');
                sys_putchar(' ');
                sys_putchar('\b');
            }
        } else if ((c >= ' ' && c <= '~')) {
            buffer[i++] = c;
            sys_putchar(c);
        }
    }
    buffer[i] = '\0';
}

// --- Main Editor Function ---

void text_editor(const char* edit_filename) {
    char editor_buffer[512];
    size_t current_len = 0;
    int fd = -1;
    char initial_filename[FS_MAX_NAME] = {0};

    // 0. Initial Loading Logic
    if (edit_filename && strlen(edit_filename) > 0) {
        strncpy(initial_filename, edit_filename, FS_MAX_NAME - 1);
        fd = sys_open(edit_filename, O_RDWR);

        if (fd >= 0) {
            // Read from file using syscall
            int read_count = sys_read(fd, editor_buffer, MAX_EDITOR_SIZE);
            if (read_count >= 0) {
                editor_buffer[read_count] = '\0';
                current_len = read_count;
            }
            sys_close(fd);
        }
    }

    // 1. UI Setup
    sys_clear_screen();
    sys_print("Simple Text Editor (Fixed Block Mode)\n");
    sys_print("CTRL+S: Save | CTRL+X: Exit\n");
    sys_print("----------------------------------\n");
    sys_print("File: ");
    if (strlen(initial_filename) > 0) {
        sys_print(initial_filename);
    } else {
        sys_print("[New File]");
    }
    sys_print("\n\n");

    // Print Content
    for (size_t i = 0; i < current_len; ++i) {
        sys_putchar(editor_buffer[i]);
    }

    // 2. Editing Loop
    int done = 0;
    while (!done) {
        char c = sys_getchar();
        if (c == CTRL_S) done = 1;
        else if (c == CTRL_X) done = 2;
        else if (c == '\b') {
            if (current_len > 0) {
                current_len--;
                sys_putchar('\b');
                sys_putchar(' ');
                sys_putchar('\b');
            }
        } else if (c == '\n') {
            if (current_len < MAX_EDITOR_SIZE) {
                editor_buffer[current_len++] = '\n';
                sys_putchar('\n');
            }
        } else if ((c >= ' ' && c <= '~')) {
            if (current_len < MAX_EDITOR_SIZE) {
                editor_buffer[current_len++] = c;
                sys_putchar(c);
            }
        }
    }

    sys_clear_screen();
    if (done == 2) {
        sys_print("Exited without saving.\n");
        return;
    }

    // 3. Save Logic
    char filename[FS_MAX_NAME];
    if (strlen(initial_filename) > 0) {
        strcpy(filename, initial_filename);
    } else {
        sys_print("Enter filename: ");
        // Note: Simple read loop here for filename
        int i = 0;
        while(1) {
            char c = sys_getchar();
            if (c == '\n') { filename[i] = '\0'; sys_putchar('\n'); break; }
            else if (c == '\b' && i > 0) { i--; sys_putchar('\b'); sys_putchar(' '); sys_putchar('\b'); }
            else if (c >= ' ' && c <= '~' && i < FS_MAX_NAME - 1) { filename[i++] = c; sys_putchar(c); }
        }
    }

    if (strlen(filename) == 0) {
        sys_print("Save cancelled.\n");
        return;
    }

    // Try to open/create file
    fd = sys_open(filename, O_RDWR | O_CREAT);
    if (fd >= 0) {
        int written = sys_write(fd, editor_buffer, current_len);
        if (written >= 0) {
            sys_print("File saved successfully.\n");
        } else {
            sys_print("Error: Could not write to file.\n");
        }
        sys_close(fd);
    } else {
        sys_print("Error: Could not open/create file.\n");
    }
}
