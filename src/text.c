/**
 * src/text.c - Simple Text Editor
 * Refactored for ID-based FS and ATA Persistence
 */

#include "../include/text.h"
#include "../include/fs.h"
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
        char c = keyboard_read();
        if (c == '\n') {
            buffer[i] = '\0';
            console_putchar('\n', COLOR_WHITE_ON_BLACK);
            break;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                console_putchar('\b', COLOR_WHITE_ON_BLACK);
                console_putchar(' ', COLOR_WHITE_ON_BLACK);
                console_putchar('\b', COLOR_WHITE_ON_BLACK);
            }
        } else if ((c >= ' ' && c <= '~')) {
            buffer[i++] = c;
            console_putchar(c, COLOR_WHITE_ON_BLACK);
        }
    }
    buffer[i] = '\0';
}

// --- Main Editor Function ---

void text_editor(const char* edit_filename) {
    // We use a stack buffer. Ensure it doesn't exceed stack limits.
    // For this simple FS, size is limited to ~300 bytes anyway.
    char editor_buffer[512];
    size_t current_len = 0;

    // Globals from shell/fs context
    extern uint32_t fs_current_dir_id;

    fs_node_t* target_node = 0;
    char initial_filename[FS_MAX_NAME] = {0};

    // 0. Initial Loading Logic
    if (edit_filename && strlen(edit_filename) > 0) {
        // Use the new FS find function
        target_node = fs_find_node((char*)edit_filename, fs_current_dir_id);

        if (target_node) {
            if (target_node->type != FS_TYPE_FILE) {
                console_print_colored("Error: Cannot edit a directory.\n", COLOR_LIGHT_RED);
                return;
            }

            // Load content using the new FS API
            fs_get_inode_name(target_node->id, initial_filename);
            
            // Read from data blocks
            int read = fs_read(target_node, 0, MAX_EDITOR_SIZE, (uint8_t*)editor_buffer);
            editor_buffer[read] = '\0';
            current_len = read;
        } else {
            // New file setup
            strncpy(initial_filename, edit_filename, FS_MAX_NAME - 1);
        }
    }

    // 1. UI Setup
    console_clear_screen();
    console_print_colored("Simple Text Editor (Fixed Block Mode)\n", COLOR_GREEN_ON_BLACK);
    console_print_colored("CTRL+S: Save | CTRL+X: Exit\n", COLOR_GREEN_ON_BLACK);
    console_print_colored("----------------------------------\n", COLOR_WHITE_ON_BLACK);

    // Print Header
    console_print_colored("File: ", COLOR_YELLOW_ON_BLACK);
    if (strlen(initial_filename) > 0) {
        console_print_colored(initial_filename, COLOR_WHITE_ON_BLACK);
    } else {
        console_print_colored("[New File]", COLOR_WHITE_ON_BLACK);
    }
    console_print("\n\n");

    // Print Content
    for (size_t i = 0; i < current_len; ++i) {
        console_putchar(editor_buffer[i], COLOR_WHITE_ON_BLACK);
    }

    // 2. Editing Loop
    int done = 0;
    while (!done) {
        char c = keyboard_read();

        if (c == CTRL_S) {
            done = 1; // Save
        } else if (c == CTRL_X) {
            done = 2; // Exit
        } else if (c == '\b') {
            if (current_len > 0) {
                current_len--;
                console_putchar('\b', COLOR_WHITE_ON_BLACK);
                console_putchar(' ', COLOR_WHITE_ON_BLACK);
                console_putchar('\b', COLOR_WHITE_ON_BLACK);
            }
        } else if (c == '\n') {
            if (current_len < MAX_EDITOR_SIZE) {
                editor_buffer[current_len++] = '\n';
                console_putchar('\n', COLOR_WHITE_ON_BLACK);
            }
        } else if ((c >= ' ' && c <= '~')) {
            if (current_len < MAX_EDITOR_SIZE) {
                editor_buffer[current_len++] = c;
                console_putchar(c, COLOR_WHITE_ON_BLACK);
            }
        }
    }

    console_clear_screen();

    if (done == 2) {
        console_print_colored("Exited without saving.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // 3. Save Logic
    char filename[FS_MAX_NAME];

    // Determine filename
    if (strlen(initial_filename) > 0) {
        strcpy(filename, initial_filename);
    } else {
        console_print_colored("Enter filename: ", COLOR_GREEN_ON_BLACK);
        read_line(filename, FS_MAX_NAME);
    }

    if (strlen(filename) == 0) {
        console_print_colored("Save cancelled.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Check if file exists to decide Update vs Create
    fs_node_t* final_node = fs_find_node(filename, fs_current_dir_id);

    if (final_node) {
        // --- Update Existing ---
        if (final_node->type == FS_TYPE_DIRECTORY) {
             console_print_colored("Error: Name conflict with directory.\n", COLOR_LIGHT_RED);
             return;
        }

        // Write buffer to data blocks
        if (fs_write(final_node, 0, current_len, (uint8_t*)editor_buffer)) {
            console_print_colored("File updated successfully.\n", COLOR_GREEN_ON_BLACK);
        } else {
            console_print_colored("Error writing to disk.\n", COLOR_LIGHT_RED);
        }

    } else {
        // --- Create New ---
        // 1. Create the entry
        if (fs_create_node(fs_current_dir_id, filename, FS_TYPE_FILE)) {
            // 2. Retrieve it (fs_create_node only creates empty)
            // We need to find the ID of the file we just created
            uint32_t new_id = fs_find_node_local_id(fs_current_dir_id, filename);
            final_node = fs_get_node(new_id);

            if (final_node) {
                // 3. Fill Content using new API
                if (fs_write(final_node, 0, current_len, (uint8_t*)editor_buffer)) {
                    console_print_colored("File created and saved.\n", COLOR_GREEN_ON_BLACK);
                } else {
                    console_print_colored("Error writing content to disk.\n", COLOR_LIGHT_RED);
                }
            } else {
                console_print_colored("Error retrieving new file handle.\n", COLOR_LIGHT_RED);
            }
        } else {
            console_print_colored("Failed to create file.\n", COLOR_LIGHT_RED);
        }
    }
}
