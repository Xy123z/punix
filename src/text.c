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

// Helper macro to access the content area of the node (using the padding)
// We treat the padding bytes as the raw file content storage.
#define NODE_CONTENT(node) ((char*)((node)->padding))
// Calculate max safe size based on the struct definition
#define MAX_EDITOR_SIZE (sizeof(((fs_node_t*)0)->padding) - 1)

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

            // Load content from the node's internal storage
            strcpy(initial_filename, target_node->name);
            char* content = NODE_CONTENT(target_node);

            // Safety copy
            strncpy(editor_buffer, content, MAX_EDITOR_SIZE);
            editor_buffer[MAX_EDITOR_SIZE] = '\0';
            current_len = strlen(editor_buffer);
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

        // Write buffer to node padding
        char* node_storage = NODE_CONTENT(final_node);
        memset(node_storage, 0, sizeof(final_node->padding)); // Clear old
        strncpy(node_storage, editor_buffer, current_len);

        final_node->size = current_len;

        // Persist to Disk
        if (fs_update_node(final_node)) {
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
                // 3. Fill Content
                char* node_storage = NODE_CONTENT(final_node);
                strncpy(node_storage, editor_buffer, current_len);
                final_node->size = current_len;

                // 4. Persist
                fs_update_node(final_node);
                console_print_colored("File created and saved.\n", COLOR_GREEN_ON_BLACK);
            } else {
                console_print_colored("Error retrieving new file handle.\n", COLOR_LIGHT_RED);
            }
        } else {
            console_print_colored("Failed to create file.\n", COLOR_LIGHT_RED);
        }
    }
}
