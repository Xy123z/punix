#include "../include/text.h"
// Define Control characters (standard ASCII control codes)
#define CTRL_S 0x13 // ASCII 19 (Ctrl+S from keyboard driver)
#define CTRL_X 0x18 // ASCII 24 (Ctrl+X from keyboard driver)

// The old file_list_head global variable is removed.
// File management is now handled by the VFS (fs_node_t).

/**
 * @brief Simple read_line function for prompting file names.
 */
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
                console_putchar(' ', COLOR_WHITE_ON_BLACK); // Clear character
                console_putchar('\b', COLOR_WHITE_ON_BLACK); // Move cursor back
            }
        } else if ((c >= ' ' && c <= '~')) { // Allow all printable chars
            buffer[i++] = c;
            console_putchar(c, COLOR_WHITE_ON_BLACK);
        }
    }

    buffer[i] = '\0';
}

/**
 * @brief Simple text editor command.
 * @param edit_filename The name of the file to load for editing (or NULL/empty for new file).
 */
void text_editor(const char* edit_filename) {
    char editor_buffer[MAX_FILE_SIZE];
    size_t current_len = 0;
    fs_node_t* target_node = 0; // Pointer to the VFS node being edited/overwritten

    // 0. Initial Loading Logic
    char initial_filename[FS_MAX_NAME] = {0};

    if (edit_filename && strlen(edit_filename) > 0) {
        // Find the node in the current directory
        target_node = fs_find_node_local(edit_filename, fs_current_dir);

        if (target_node) {
            if (target_node->type != FS_FILE) {
                console_print_colored("Error: Cannot edit a directory.\n", COLOR_YELLOW_ON_BLACK);
                return;
            }
            if (target_node->size >= MAX_FILE_SIZE) {
                console_print_colored("Error: File is too large to fit in editor buffer.\n", COLOR_YELLOW_ON_BLACK);
                return;
            }

            // Load content and size
            strcpy(initial_filename, target_node->name);
            strcpy(editor_buffer, target_node->content_data);
            current_len = target_node->size;
        } else {
            // New file with pre-defined name
            strcpy(initial_filename, edit_filename);
        }
    }

    // 1. Clear screen and provide instructions
    console_clear_screen();
    console_print_colored("Simple Text Editor. Press Ctrl+S to save, Ctrl+X to exit.\n\n", COLOR_YELLOW_ON_BLACK);

    // Display status and initial content
    console_print_colored("Editing: ", COLOR_YELLOW_ON_BLACK);
    if (target_node) {
        console_print_colored(target_node->name, COLOR_YELLOW_ON_BLACK);
    } else if (strlen(initial_filename) > 0) {
        console_print_colored(initial_filename, COLOR_YELLOW_ON_BLACK);
    } else {
        console_print_colored("NEW FILE", COLOR_YELLOW_ON_BLACK);
    }
    console_print_colored("\n\n", COLOR_WHITE_ON_BLACK);


    // Print the existing content
    for (size_t i = 0; i < current_len; ++i) {
        console_putchar(editor_buffer[i], COLOR_WHITE_ON_BLACK);
    }


    // 2. Main editing loop
    int done = 0; // 1 = save, 2 = exit
    while (!done) {
        char c = keyboard_read();

        if (c == CTRL_S) {
            done = 1; // Save
            break;
        } else if (c == CTRL_X) {
            done = 2; // Exit
            break;
        } else if (c == '\b') {
            // Backspace handling
            if (current_len > 0) {
                current_len--;
                console_putchar('\b', COLOR_WHITE_ON_BLACK);
                console_putchar(' ', COLOR_WHITE_ON_BLACK); // Clear character
                console_putchar('\b', COLOR_WHITE_ON_BLACK); // Move cursor back
            }
        } else if (c == '\n') {
            // Newline handling
            if (current_len < MAX_FILE_SIZE - 1) {
                editor_buffer[current_len++] = '\n';
                console_putchar('\n', COLOR_WHITE_ON_BLACK);
            }
        }
        else if ((c >= ' ' && c <= '~') && current_len < MAX_FILE_SIZE - 1) {
            // Printable characters
            editor_buffer[current_len++] = c;
            console_putchar(c, COLOR_WHITE_ON_BLACK);
        }
    }

    // 3. Post-editing logic
    console_clear_screen(); // Clear editing screen to return to prompt environment

    if (done == 2) {
        console_print_colored("Exit without saving.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // 4. Saving logic (done == 1)

    // a. Prompt for file name
    char filename[FS_MAX_NAME];

    // Use the initial filename if available, otherwise prompt
    if (strlen(initial_filename) > 0) {
        strcpy(filename, initial_filename);
    } else {
        filename[0] = '\0';
    }

    console_print_colored("Enter filename (max 39 chars): ", COLOR_GREEN_ON_BLACK);

    // Display the current (pre-filled or empty) name
    console_print_colored(filename, COLOR_WHITE_ON_BLACK);

    // Call read_line; user input will overwrite/clear the displayed default name
    read_line(filename, FS_MAX_NAME);

    // b. Validation
    if (current_len == 0 || strlen(filename) == 0) {
        console_print_colored("Save cancelled: No content or filename provided.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Determine the final target node based on the user-entered filename
    fs_node_t* final_target = fs_find_node_local(filename, fs_current_dir);

    // c. Overwrite/Creation Logic
    char* content_ptr = 0;

    if (final_target) {
        // --- Overwrite/Update Existing File ---

        // 1. Free old memory if content exists and needs reallocation
        if (final_target->content_data) {
             kfree(final_target->content_data);
        }

        // 2. Allocate new memory for content + null terminator
        content_ptr = (char*)kmalloc(current_len + 1);
        if (!content_ptr) {
            console_print_colored("Memory reallocation failed. File not updated.\n", COLOR_YELLOW_ON_BLACK);
            return;
        }

        // 3. Update VFS node metadata
        final_target->content_data = content_ptr;
        final_target->size = current_len;
        final_target->allocated_size = current_len + 1;

    } else {
        // --- New File Creation Logic ---

        // 1. Allocate memory for file content
        content_ptr = (char*)kmalloc(current_len + 1);

        if (!content_ptr) {
            console_print_colored("Memory allocation failed. File not saved.\n", COLOR_YELLOW_ON_BLACK);
            return;
        }

        // 2. Create the new node in the VFS
        fs_node_t* new_file = fs_create_node(filename, FS_FILE, fs_current_dir);

        if (!new_file) {
            kfree(content_ptr);
            console_print_colored("Failed to create file system node. File not saved.\n", COLOR_YELLOW_ON_BLACK);
            return;
        }

        // 3. Update VFS node metadata
        new_file->content_data = content_ptr;
        new_file->size = current_len;
        new_file->allocated_size = current_len + 1;
        final_target = new_file; // Set target for confirmation message
    }

    // d. Copy the edited content (must null-terminate)
    editor_buffer[current_len] = '\0';
    strcpy(content_ptr, editor_buffer);

    // e. Confirmation message
    console_print_colored("File '", COLOR_GREEN_ON_BLACK);
    console_print_colored(final_target->name, COLOR_YELLOW_ON_BLACK);
    console_print_colored("' saved. Size: ", COLOR_GREEN_ON_BLACK);
    char size_str[12];
    int_to_str((int)current_len, size_str);
    console_print_colored(size_str, COLOR_YELLOW_ON_BLACK);
    console_print_colored(" bytes.\n", COLOR_GREEN_ON_BLACK);
}

// The old show_files() is removed as listing is now handled by shell's cmd_ls
