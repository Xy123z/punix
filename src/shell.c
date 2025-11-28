/**
 * src/shell.c - Shell implementation
 */
#include "../include/shell.h"
#include "../include/console.h" // REPLACED vga.h with console.h
#include "../include/memory.h"
#include "../include/interrupt.h"
#include "../include/string.h"
#include "../include/text.h"
#include "../include/fs.h" // NEW: Include File System

int ROOT_ACCESS_GRANTED = 0;
char ROOT_PASSWORD[MAX_PASSWORD_LEN] = {0};
char USERNAME[MAX_USERNAME_LEN] = {0};
// History storage (dynamic)
static char** history = 0;
static int history_count = 0;
static int history_capacity = 0;
static char last_result[MAX_RESULT_LEN] = "";
static int has_result = 0;

// Constants
static const char* current_user = USERNAME;
static const char* kernel_name = "punix-v1.03";

// Function prototypes for history (defined later)
void history_show();
void history_save();
void history_delete(int index);

// Helper: Read line with visual feedback
void read_line_with_display(char* buffer, int max_len) {
    int i = 0;

    while (i < max_len - 1) {
        char c = keyboard_read();

        if (c == '\n') {
            buffer[i] = '\0';
            console_putchar('\n', COLOR_WHITE_ON_BLACK); // USE CONSOLE
            break;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                console_putchar('\b', COLOR_WHITE_ON_BLACK); // USE CONSOLE
                console_putchar(' ', COLOR_WHITE_ON_BLACK); // Clear char (USE CONSOLE)
                console_putchar('\b', COLOR_WHITE_ON_BLACK); // Move cursor back (USE CONSOLE)
            }
        } else if ((c >= ' ' && c <= '~')) {
            buffer[i++] = c;
            console_putchar(c, COLOR_WHITE_ON_BLACK); // USE CONSOLE
        }
    }

    buffer[i] = '\0';
}

/**
 * @brief Helper function to recursively build and print the full path.
 */
static void print_full_path_recursive(fs_node_t* node) {
    if (!node) return;

    // Handle the root specially
    if (node == fs_root) {
        console_print_colored("/", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // If parent is not root, print separator before name
    if (node->parent != fs_root) {
        print_full_path_recursive(node->parent);
        console_print_colored("/", COLOR_YELLOW_ON_BLACK);
    }
    // If parent is root, the root's '/' is already printed by the recursive call end
    else if (node->parent == fs_root) {
         print_full_path_recursive(node->parent);
    }

    // Print the current node's name
    console_print_colored(node->name, COLOR_YELLOW_ON_BLACK);
}

/**
 * @brief Constructs and displays the full path of the current directory.
 */
static void show_prompt() {
    console_print_colored(current_user, COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print_colored("@", COLOR_WHITE_ON_BLACK); // USE CONSOLE
    console_print_colored(kernel_name, COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print_colored(":", COLOR_WHITE_ON_BLACK); // USE CONSOLE

    // --- VFS PATH DISPLAY ---
    if (fs_current_dir == fs_root) {
        console_print_colored("/", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
    }
    else if (fs_current_dir->parent == fs_root && strcmp(fs_current_dir->name, "a") == 0) {
        // Common UNIX home dir alias
        console_print_colored("~", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
    }
    else {
        // Full recursive path printing (NEW)
        print_full_path_recursive(fs_current_dir);
    }
    // --- END VFS PATH DISPLAY ---

     if(ROOT_ACCESS_GRANTED){
        console_print_colored("# ", COLOR_LIGHT_RED);
     }
     else{
         console_print_colored("$ ", COLOR_WHITE_ON_BLACK);
    }
}

void shell_init() {
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print_colored("|           PUNIX: AN EXPERIMENTAL KERNEL        |\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print("\n"); // USE CONSOLE
}

// Command implementations

/**
 * @brief Prints the full working directory path (now handles full traversal).
 */
void cmd_pwd() {
    // Print path components recursively
    print_full_path_recursive(fs_current_dir);
    console_print_colored("\n", COLOR_YELLOW_ON_BLACK);
}

/**
 * @brief Lists the contents of the current directory (or history if in /h).
 */
void cmd_ls() {
    // Check if we are in the special history directory (/h)
    if (strcmp(fs_current_dir->name, "h") == 0 && fs_current_dir->parent == fs_root) {
        history_show();
        return;
    }

    // List contents of a regular directory node
    if (fs_current_dir->num_children == 0) {
        console_print_colored("Directory is empty.\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        return;
    }

    console_print_colored("Contents:\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE

    for (int i = 0; i < fs_current_dir->num_children; i++) {
        fs_node_t* child = fs_current_dir->children[i];

        if (child->type == FS_DIRECTORY) {
            console_print_colored(child->name, COLOR_YELLOW_ON_BLACK); // USE CONSOLE
            console_print_colored("/", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        } else { // FS_FILE
            console_print_colored(child->name, COLOR_WHITE_ON_BLACK); // USE CONSOLE
            console_print(" ("); // USE CONSOLE
            char size_str[12];
            int_to_str((int)child->size, size_str);
            console_print(size_str); // USE CONSOLE
            console_print(" bytes)"); // USE CONSOLE
        }
        console_print("\n"); // USE CONSOLE
    }
}

/**
 * @brief Changes the current working directory using VFS.
 */
void cmd_cd(char* dir) {
    int result = fs_change_dir(dir);

    if (result == 0) {
        console_print_colored("Changed directory to ", COLOR_GREEN_ON_BLACK); // USE CONSOLE

        // Path printing is handled by show_prompt, just print newline
        console_print_colored("\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    }
    else if(result == -1){
        console_print_colored("root access denied\n",COLOR_LIGHT_RED);
        return;
    }
    else {
        console_print_colored("cd: no such directory or not a directory\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
    }
}

/**
 * @brief Creates a new directory.
 */
// shell.c (Proposed Change for cmd_rmdir)
void cmd_rmdir(char* path) { // Renamed argument to path for clarity
    if (strlen(path) == 0) {
        console_print_colored("Usage: rmdir <directory_path>\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // *** FIX: Use fs_find_node for full path resolution ***
    fs_node_t* node_to_delete = fs_find_node(path, fs_current_dir);

    if (!node_to_delete) {
        console_print_colored("rmdir: Directory '", COLOR_YELLOW_ON_BLACK);
        console_print(path); // Use path here for correct error message
        console_print_colored("' not found.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    if (node_to_delete->type != FS_DIRECTORY) {
        console_print_colored("rmdir: '", COLOR_YELLOW_ON_BLACK);
        console_print(path);
        console_print_colored("' is not a directory.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    if (node_to_delete == fs_current_dir) {
        console_print_colored("rmdir: Cannot remove current directory.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // fs_delete_node includes the check for directory emptiness
    if (fs_delete_node(node_to_delete) == 0) {
        console_print_colored("Directory '", COLOR_GREEN_ON_BLACK);
        console_print_colored(path, COLOR_YELLOW_ON_BLACK);
        console_print_colored("' removed successfully.\n", COLOR_GREEN_ON_BLACK);
    } else {
        // fs_delete_node prints the "not empty" error
        console_print_colored("rmdir: Failed to remove directory.\n", COLOR_YELLOW_ON_BLACK);
    }
}

/**
 * @brief Removes an empty directory.
 */
// shell.c (Proposed Change for cmd_mkdir)
void cmd_mkdir(char* path) { // Renamed argument to path for clarity
    if (strlen(path) == 0) {
        console_print_colored("Usage: mkdir <directory_path>\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // A temporary copy of the path is needed because strtok/manual parsing modifies the string.
    char temp_path[40];
    strncpy(temp_path, path, 39);
    temp_path[39] = '\0';

    char* last_component = temp_path;
    char* separator = 0;
    fs_node_t* parent_dir = fs_current_dir;

    // Resolve absolute path start
    if (temp_path[0] == '/') {
        parent_dir = fs_root;
        last_component++; // Start name parsing after initial /
    }

    // Find the last '/' to separate parent path and new directory name
    // NOTE: This simple logic is brittle for paths like ".." or consecutive slashes.
    // For a robust solution, you'd implement a path splitting function in fs.c.

    // Simple path parsing (finds the last component)
    for (int i = 0; temp_path[i] != '\0'; i++) {
        if (temp_path[i] == '/') {
            separator = &temp_path[i];
        }
    }

    char final_name[FS_MAX_NAME];

    if (separator) {
        // If a separator exists, resolve the parent path
        *separator = '\0'; // Temporarily terminate the string at the parent path
        last_component = separator + 1;

        // Resolve the parent directory path
        fs_node_t* resolved_parent = fs_find_node(temp_path, fs_current_dir);

        if (!resolved_parent || resolved_parent->type != FS_DIRECTORY) {
            console_print_colored("mkdir: Parent directory not found or not a directory.\n", COLOR_YELLOW_ON_BLACK);
            return;
        }
        parent_dir = resolved_parent;

        // Restore separator for path display/debugging (optional)
        *separator = '/';
    }

    // The name to create is the final component
    if (strlen(last_component) >= FS_MAX_NAME) {
        console_print_colored("Error: Directory name too long.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }
    strcpy(final_name, last_component);

    if (strlen(final_name) == 0) {
        console_print_colored("Error: Invalid directory name.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Attempt to create node, using the resolved parent directory
    fs_node_t* new_dir = fs_create_node(final_name, FS_DIRECTORY, parent_dir);

    if (new_dir) {
        console_print_colored("Directory '", COLOR_GREEN_ON_BLACK);
        console_print_colored(path, COLOR_YELLOW_ON_BLACK);
        console_print_colored("' created successfully.\n", COLOR_GREEN_ON_BLACK);
    } else {
        // fs_create_node prints specific errors (name exists, invalid name, memory error)
        console_print_colored("mkdir: Failed to create directory.\n", COLOR_YELLOW_ON_BLACK);
    }
}

void cmd_add() {
    // Original logic required directory == 1 (/a)
    if (fs_current_dir->parent != fs_root || strcmp(fs_current_dir->name, "a") != 0) {
        console_print_colored("Mount /a for executing this command\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        return;
    }

    char input[40];

    console_print_colored("Enter first number: ", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
    read_line_with_display(input, 40);

    int num1 = str_to_int(input);

    console_print_colored("Enter second number: ", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
    read_line_with_display(input, 40);
    int num2 = str_to_int(input);

    int sum = num1 + num2;

    last_result[0] = '\0';
    char temp[12];

    int_to_str(num1, temp);
    strcpy(last_result, temp);
    strcpy(last_result + strlen(last_result), " + ");

    int_to_str(num2, temp);
    strcpy(last_result + strlen(last_result), temp);
    strcpy(last_result + strlen(last_result), " = ");

    int_to_str(sum, temp);
    strcpy(last_result + strlen(last_result), temp);

    has_result = 1;

    console_print(last_result); // USE CONSOLE
    console_print("\n"); // USE CONSOLE
}

void cmd_su(){
    if(ROOT_ACCESS_GRANTED){
        console_print_colored("already in root mode\n", COLOR_GREEN_ON_BLACK);
        return;
    }
    char pass[MAX_PASSWORD_LEN];
    console_print_colored("enter root password: ", COLOR_GREEN_ON_BLACK);
    read_line_with_display(pass,MAX_PASSWORD_LEN);
    if(strcmp(pass,ROOT_PASSWORD) == 0){
        console_print_colored("root access granted\n", COLOR_GREEN_ON_BLACK);
        ROOT_ACCESS_GRANTED = 1;
        fs_change_dir("/");
    }
    else{
        console_print_colored("root access denied\n", COLOR_YELLOW_ON_BLACK);
    }
}

void cmd_mem() {
    console_print_colored("=== Memory Statistics ===\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE

    uint32_t total, used, free;
    pmm_get_stats(&total, &used, &free);

    uint32_t total_kb = (total * PAGE_SIZE) / 1024;
    uint32_t used_kb = (used * PAGE_SIZE) / 1024;
    uint32_t free_kb = (free * PAGE_SIZE) / 1024;

    console_print("Total physical: "); // USE CONSOLE
    char num[16];
    int_to_str(total_kb, num);
    console_print(num); // USE CONSOLE
    console_print(" KB\n"); // USE CONSOLE

    console_print("Used physical:  "); // USE CONSOLE
    int_to_str(used_kb, num);
    console_print(num); // USE CONSOLE
    console_print(" KB\n"); // USE CONSOLE

    console_print("Free physical:  "); // USE CONSOLE
    int_to_str(free_kb, num);
    console_print(num); // USE CONSOLE
    console_print(" KB\n"); // USE CONSOLE

    uint32_t percent_used = total > 0 ? (used * 100) / total : 0;
    console_print("Usage: "); // USE CONSOLE
    int_to_str(percent_used, num);
    console_print(num); // USE CONSOLE
    console_print("%\n"); // USE CONSOLE
}

void cmd_help() {
    console_clear_screen(); // USE CONSOLE
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print_colored("|      PUNIX: LIST OF AVAILABLE COMMANDS         |\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print("\n"); // USE CONSOLE

    // The help menu is now updated for new VFS features
    console_print_colored("Filesystem & Global Commands:\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
    console_print("  ls            - List contents of current directory\n"); // USE CONSOLE
    console_print("  cd [dir]      - Change directory (e.g., /a, /h, .., /new/dir)\n"); // USE CONSOLE
    console_print("  pwd           - Print full working directory path\n"); // USE CONSOLE
    console_print("  mkdir [name]  - Create a new directory\n"); // NEW
    console_print("  rmdir [name]  - Remove an empty directory\n"); // NEW
    console_print("  text [file]   - Use simple text editor (creates/edits file)\n"); // USE CONSOLE
    console_print("  mem           - Show memory statistics\n"); // USE CONSOLE
    console_print("  clear         - Clear screen\n"); // USE CONSOLE
    console_print("  exit          - Quit (shutdown)\n"); // USE CONSOLE
    console_print("  help          - Show this help\n"); // USE CONSOLE

    console_print("\n"); // USE CONSOLE

    console_print_colored("Kernel-Specific Commands (Requires /a):\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
    console_print("  add           - Add two numbers\n"); // USE CONSOLE
    console_print("  s             - Save last calculation result to history (/h)\n"); // USE CONSOLE

    console_print("\n"); // USE CONSOLE

    console_print_colored("History Commands (Requires /h):\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
    console_print("  del [n]       - Delete history entry number n\n"); // USE CONSOLE


    console_print("\nPress any key to continue..."); // USE CONSOLE
    keyboard_read();
    console_clear_screen(); // USE CONSOLE
    shell_init();
}

void cmd_clear() {
    console_clear_screen(); // USE CONSOLE
    shell_init();
}

void cmd_exit() {
    if(ROOT_ACCESS_GRANTED){
        ROOT_ACCESS_GRANTED = 0;
        console_print_colored("returned to normal mode\n", COLOR_GREEN_ON_BLACK);
        fs_change_dir("a");
        return;
    }
    console_clear_screen(); // USE CONSOLE
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print_colored("|             SHUTTING DOWN...                   |\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    console_print("\n"); // USE CONSOLE
    console_print("Thank you for using Calculator Kernel!\n"); // USE CONSOLE
    console_print("Goodbye!\n\n"); // USE CONSOLE
    console_print("It is now safe to close this window.\n"); // USE CONSOLE

    // QEMU shutdown
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x604));
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x605));

    __asm__ volatile("cli; hlt");
    while(1) __asm__ volatile("hlt");
}

// History functions (History storage remains external to VFS, but accessed via /h)
void history_save() {
    // Original logic required directory == 1 (/a)
    if (fs_current_dir->parent != fs_root || strcmp(fs_current_dir->name, "a") != 0) {
        console_print_colored("Command 's' requires the current directory to be /a.\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        return;
    }

    if (!has_result) {
        console_print_colored("Nothing to save!\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        return;
    }

    if (history_count >= history_capacity) {
        int new_capacity = history_capacity == 0 ? 10 : history_capacity * 2;
        if (new_capacity > MAX_HISTORY) new_capacity = MAX_HISTORY;

        char** new_history = (char**)kmalloc(new_capacity * sizeof(char*));
        if (!new_history) {
            console_print_colored("Failed to allocate memory for history growth.\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
            return;
        }

        for (int i = 0; i < history_count; i++) {
            new_history[i] = history[i];
        }

        if (history) {
            kfree(history);
        }

        history = new_history;
        history_capacity = new_capacity;
    }

    if (history_count < MAX_HISTORY) {
        size_t len = strlen(last_result);
        history[history_count] = (char*)kmalloc(len + 1);
        if (history[history_count]) {
            strcpy(history[history_count], last_result);
            history_count++;
            console_print_colored("Saved to history! (File system abstraction used)\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
        } else {
            console_print_colored("Failed to allocate memory for history entry.\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        }
    } else {
        console_print_colored("History full! (max 50 entries)\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
    }
}

void history_delete(int index) {
    if (strcmp(fs_current_dir->name, "h") != 0 || fs_current_dir->parent != fs_root) {
        console_print_colored("Command 'del' is only available in the /h directory.\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        return;
    }

    if (index < 1 || index > history_count) {
        console_print_colored("Invalid index for the history!\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        return;
    }

    int array_index = index - 1;

    kfree(history[array_index]);

    for (int i = array_index; i < history_count - 1; i++) {
        history[i] = history[i + 1];
    }

    history_count--;
    console_print_colored("History entry deleted! (Memory freed)\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
}

void history_show() {
    if (history_count == 0) {
        console_print_colored("No history yet!\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        return;
    }

    console_print_colored("=== Calculation History (/h) ===\n", COLOR_GREEN_ON_BLACK); // USE CONSOLE
    for (int i = 0; i < history_count; i++) {
        char num[12];
        int_to_str(i + 1, num);
        console_print(num); // USE CONSOLE
        console_print(". "); // USE CONSOLE
        console_print(history[i]); // USE CONSOLE
        console_print("\n"); // USE CONSOLE
    }
}

// Main shell loop
void shell_run() {
    while (1) {
        char input[40];

        show_prompt();
        read_line_with_display(input, 40);

        if (strlen(input) == 0) continue;

        // Parse command and arguments
        char cmd[40];
        char args[40];
        int i = 0;

        while (input[i] && input[i] != ' ') {
            cmd[i] = input[i];
            i++;
        }
        cmd[i] = '\0';

        int j = 0;
        if (input[i] == ' ') {
            i++;
            // Skip multiple spaces before arguments
            while (input[i] == ' ') i++;

            while (input[i]) {
                args[j++] = input[i++];
            }
        }
        args[j] = '\0';

        // Global commands
        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "EXIT") == 0) {
            cmd_exit();
        }
        else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "CLEAR") == 0) {
            cmd_clear();
        }
        else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "HELP") == 0) {
            cmd_help();
        }
        else if (strcmp(cmd, "pwd") == 0 || strcmp(cmd, "PWD") == 0) {
            cmd_pwd();
        }
        else if (strcmp(cmd, "mem") == 0 || strcmp(cmd, "MEM") == 0) {
            cmd_mem();
        }
        else if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "CD") == 0) {
            cmd_cd(args);
        }
        else if (strcmp(cmd, "mkdir") == 0) {
            cmd_mkdir(args);
        }
        else if (strcmp(cmd, "rmdir") == 0) {
            cmd_rmdir(args);
        }
        else if (strcmp(cmd, "add") == 0 || strcmp(cmd, "ADD") == 0) {
            cmd_add();
        }
        else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "LS") == 0) {
            cmd_ls();
        }
        else if (strcmp(cmd, "text") == 0 || strcmp(cmd, "TEXT") == 0) {
            text_editor(args);
        }
         else if (strcmp(cmd, "su") == 0 || strcmp(cmd, "SU") == 0) {
            cmd_su();
        }

        // Removed `show` command, `ls` now handles file listing
        else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "S") == 0) {
            history_save();
        }
        else if (strcmp(cmd, "del") == 0 || strcmp(cmd, "DEL") == 0) {
            int index = str_to_int(args);
            history_delete(index);
        }
        else {
            console_print(cmd); // USE CONSOLE
            console_print_colored(": command not found\n", COLOR_YELLOW_ON_BLACK); // USE CONSOLE
        }
    }
}
