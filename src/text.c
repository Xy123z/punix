#include "../include/text.h"
// Define Control characters (standard ASCII control codes)
#define CTRL_S 0x13 // ASCII 19 (Ctrl+S from keyboard driver)
#define CTRL_X 0x18 // ASCII 24 (Ctrl+X from keyboard driver)

// Assuming this constant is defined in ../include/text.h
#define MAX_FILE_SIZE 8192

// The structure definition is usually in text.h, but kept here as context from user's provided code

// Global list head for stored files
static file_entry_t* file_list_head = 0;

/**
 * @brief Finds a file entry by name in the linked list.
 */
static file_entry_t* find_file(const char* name) {
    if (!name || strlen(name) == 0) return 0;

    file_entry_t* current = file_list_head;
    while (current) {
        // Assuming strcmp is available in std.h
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return 0;
}

/**
 * @brief Simple read_line function for prompting file names.
 * @note This function handles its own VGA output.
 */
static void read_line(char* buffer, int max_len) {
    int i = 0;

    while (i < max_len - 1) {
        char c = keyboard_read();

        if (c == '\n') {
            buffer[i] = '\0';
            vga_putchar('\n', COLOR_WHITE_ON_BLACK);
            break;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                vga_putchar('\b', COLOR_WHITE_ON_BLACK);
            }
        } else if ((c >= '0' && c <= '9') || c == '-' ||
                   (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   c == ' ' || c == '.' || c == '_') {
            buffer[i++] = c;
            vga_putchar(c, COLOR_WHITE_ON_BLACK);
        }
    }

    buffer[i] = '\0';
}

/**
 * @brief Simple text editor command.
 * Clears the screen, allows text input, and saves content to the heap.
 * Now uses Ctrl+S for Save and Ctrl+X for Exit, and supports opening existing files.
 * * @param edit_filename The name of the file to load for editing (or NULL/empty for new file).
 */
void text_editor(const char* edit_filename) {
    char editor_buffer[MAX_FILE_SIZE];
    size_t current_len = 0;
    file_entry_t* target_entry = 0; // Pointer to the file being edited/overwritten

    // 0. Initial Loading Logic
    if (edit_filename && strlen(edit_filename) > 0) {
        target_entry = find_file(edit_filename);
        if (target_entry) {
            if (target_entry->size >= MAX_FILE_SIZE) {
                vga_print_colored("Error: File is too large to fit in editor buffer.\n", COLOR_GREEN_ON_BLACK);
                return;
            }
            // Load content and size
            strcpy(editor_buffer, target_entry->content);
            current_len = target_entry->size;
        }
    }

    // 1. Clear screen and provide instructions
    vga_clear_screen();
    vga_print_colored("Simple Text Editor. Press Ctrl+S to save, Ctrl+X to exit.\n\n", COLOR_YELLOW_ON_BLACK);

    // Display status and initial content
    if (target_entry) {
        vga_print_colored("Editing: ", COLOR_YELLOW_ON_BLACK);
        vga_print_colored(target_entry->name, COLOR_YELLOW_ON_BLACK);
        vga_print_colored("\n\n", COLOR_WHITE_ON_BLACK);

        // Print the existing content
        for (size_t i = 0; i < current_len; ++i) {
            vga_putchar(editor_buffer[i], COLOR_WHITE_ON_BLACK);
        }
    } else {
        vga_print_colored("New File.\n\n", COLOR_YELLOW_ON_BLACK);
    }

    // 2. Main editing loop
    int done = 0; // 1 = save, 2 = exit
    while (!done) {
        char c = keyboard_read(); // Read character from keyboard buffer

        // --- UPDATED CONTROL LOGIC ---
        if (c == CTRL_S) {
            done = 1; // Prepare to save
            break;
        } else if (c == CTRL_X) {
            done = 2; // Prepare to exit without saving
            break;
        }
        // --- END UPDATED CONTROL LOGIC ---

        else if (c == '\b') {
            // Backspace handling
            if (current_len > 0) {
                current_len--;
                vga_putchar('\b', COLOR_WHITE_ON_BLACK);
            }
        } else if (c == '\n') {
            // Newline handling
            if (current_len < MAX_FILE_SIZE - 1) {
                editor_buffer[current_len++] = '\n';
                vga_putchar('\n', COLOR_WHITE_ON_BLACK);
            }
        }
        // Now, all printable ASCII characters, including 's' and 'x', are treated as normal text
        else if ((c >= ' ' && c <= '~') && current_len < MAX_FILE_SIZE - 1) {
            editor_buffer[current_len++] = c;
            vga_putchar(c, COLOR_WHITE_ON_BLACK);
        }
    }

    // 3. Post-editing logic
    vga_clear_screen(); // Clear editing screen to return to prompt environment

    if (done == 2) {
        vga_print_colored("Exit without saving.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // 4. Saving logic (done == 1)

    // a. Prompt for file name, pre-filling if editing an existing file
    char filename[40];
    if (target_entry) {
        strcpy(filename, target_entry->name); // Pre-fill with existing name
    } else {
        filename[0] = '\0';
    }

    vga_print_colored("Enter filename (max 39 chars): ", COLOR_GREEN_ON_BLACK);

    // Display the current (pre-filled or empty) name
    vga_print_colored(filename, COLOR_WHITE_ON_BLACK);

    // Call read_line; user input will overwrite/clear the displayed default name
    read_line(filename, 40);

    // b. Validation
    if (current_len == 0 || strlen(filename) == 0) {
        vga_print_colored("Save cancelled: No content or filename provided.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Determine if we are overwriting an existing file or creating a new one
    file_entry_t* final_target = find_file(filename);

    // c. Copy data and link the new file / Overwrite logic
    char* content_ptr = 0;

    // --- Overwrite/Update Existing File Logic ---
    if (final_target) {
        // We are saving to a file that already exists with the final name.

        // 1. Handle memory reallocation if the size has changed
        int needs_realloc = (final_target->size + 1 != current_len + 1);

        if (needs_realloc) {
            kfree(final_target->content);
            content_ptr = (char*)kmalloc(current_len + 1);
            if (!content_ptr) {
                vga_print_colored("Memory reallocation failed. File not updated.\n", COLOR_YELLOW_ON_BLACK);
                return;
            }
        } else {
            // No reallocation needed, use the existing pointer
            content_ptr = final_target->content;
        }

        // 2. Update content and metadata
        final_target->content = content_ptr;
        final_target->size = current_len;

        // Copy the edited content (must null-terminate)
        editor_buffer[current_len] = '\0';
        strcpy(final_target->content, editor_buffer);

    } else {
        // --- New File Creation Logic ---

        // Allocate memory for file content
        content_ptr = (char*)kmalloc(current_len + 1);

        if (!content_ptr) {
            vga_print_colored("Memory allocation failed. File not saved.\n", COLOR_YELLOW_ON_BLACK);
            return;
        }

        // Allocate memory for the file entry structure
        file_entry_t* new_file = (file_entry_t*)kmalloc(sizeof(file_entry_t));
        if (!new_file) {
            kfree(content_ptr);
            vga_print_colored("Failed to allocate file structure. File not saved.\n", COLOR_YELLOW_ON_BLACK);
            return;
        }

        // Copy data and link the new file
        editor_buffer[current_len] = '\0';
        strcpy(content_ptr, editor_buffer);

        strcpy(new_file->name, filename);
        new_file->content = content_ptr;
        new_file->size = current_len;
        new_file->next = file_list_head; // Prepend to the list
        file_list_head = new_file;
    }

    // d. Confirmation message
    vga_print_colored("File '", COLOR_GREEN_ON_BLACK);
    vga_print_colored(filename, COLOR_GREEN_ON_BLACK);
    vga_print_colored("' saved. Size: ", COLOR_GREEN_ON_BLACK);
    char size_str[12];
    int_to_str((int)current_len, size_str);
    vga_print_colored(size_str, COLOR_GREEN_ON_BLACK);
    vga_print_colored(" bytes. (Check 'mem' to see the page allocation increase)\n", COLOR_GREEN_ON_BLACK);
}

/**
 * @brief Lists all files currently stored in memory.
 */
void show_files() {
    if (!file_list_head) {
        vga_print_colored("No files stored yet.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    vga_print_colored("=== Stored Files (In-Memory) ===\n", COLOR_GREEN_ON_BLACK);
    file_entry_t* current = file_list_head;
    while (current) {
        vga_print_colored("Name: ", COLOR_WHITE_ON_BLACK);
        vga_print_colored(current->name, COLOR_YELLOW_ON_BLACK);

        // Print size
        vga_print_colored(" (Size: ", COLOR_WHITE_ON_BLACK);
        char size_str[12];
        int_to_str((int)current->size, size_str);
        vga_print_colored(size_str, COLOR_WHITE_ON_BLACK);
        vga_print_colored(" bytes) ", COLOR_WHITE_ON_BLACK);

        // Print content address
        vga_print_colored("[Addr: 0x", COLOR_WHITE_ON_BLACK);
        char addr_str[12];
        int_to_str((int)current->content, addr_str);
        vga_print_colored(addr_str, COLOR_WHITE_ON_BLACK);
        vga_print_colored("]\n", COLOR_WHITE_ON_BLACK);

        current = current->next;
    }
}
