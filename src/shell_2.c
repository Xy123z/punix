/**
 * src/shell.c - Enhanced Shell implementation
 * Features: sudo, shutdown command, add with save, mem with disk stats, credential management
 */
#include "../include/shell.h"
#include "../include/console.h"
#include "../include/memory.h"
#include "../include/interrupt.h"
#include "../include/string.h"
#include "../include/text.h"
#include "../include/fs.h"
#include "../include/auth.h"
#include"../include/syscall.h"
// --- External FS Globals (From fs.c) ---
extern uint32_t fs_root_id;
extern uint32_t fs_current_dir_id;

// --- Shell Globals ---
int ROOT_ACCESS_GRANTED = 0;
char ROOT_PASSWORD[MAX_PASSWORD_LEN] = {0};
char USERNAME[MAX_USERNAME_LEN] = {0};

// History storage
static char** history = 0;
static int history_count = 0;
static int history_capacity = 0;
static char last_result[MAX_RESULT_LEN] = "";
static int has_result = 0;

static const char* current_user = USERNAME;
static const char* kernel_name = "punix-v1.03";

// Prototypes
void history_show();
void history_save();
void history_delete(int index);
void cmd_shutdown(); // New shutdown command

// Helper: Read line with visual feedback
void read_line_with_display(char* buffer, int max_len) {
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

/**
 * @brief Recursively prints the path of a directory node using its ID.
 */
static void print_full_path_recursive(uint32_t node_id) {
    fs_node_t* node = fs_get_node(node_id);
    if (!node) return;

    if (node_id == fs_root_id) {
        console_print_colored("/", COLOR_YELLOW_ON_BLACK);
        return;
    }

    if (node->parent_id != 0 && node->parent_id != node_id) {
        print_full_path_recursive(node->parent_id);
    }

    if (node->parent_id != fs_root_id) {
         console_print_colored("/", COLOR_YELLOW_ON_BLACK);
    }

    console_print_colored(node->name, COLOR_YELLOW_ON_BLACK);
}

static void show_prompt() {
    console_print_colored(current_user, COLOR_GREEN_ON_BLACK);
    console_print_colored("@", COLOR_WHITE_ON_BLACK);
    console_print_colored(kernel_name, COLOR_GREEN_ON_BLACK);
    console_print_colored(":", COLOR_WHITE_ON_BLACK);

    if (fs_current_dir_id == fs_root_id) {
        console_print_colored("/", COLOR_YELLOW_ON_BLACK);
    } else {
        print_full_path_recursive(fs_current_dir_id);
    }

    if (ROOT_ACCESS_GRANTED) {
        console_print_colored("# ", COLOR_LIGHT_RED);
    } else {
        console_print_colored("$ ", COLOR_WHITE_ON_BLACK);
    }
}

void shell_init() {
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    console_print_colored("|          PUNIX: AN EXPERIMENTAL KERNEL         |\n", COLOR_GREEN_ON_BLACK);
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    console_print("\n");
}

// --- Command Implementations ---

void cmd_pwd() {
    if (fs_current_dir_id == fs_root_id) {
        console_print_colored("/", COLOR_YELLOW_ON_BLACK);
    } else {
        print_full_path_recursive(fs_current_dir_id);
    }
    console_print("\n");
}

void cmd_ls() {
    fs_node_t* current_dir = fs_get_node(fs_current_dir_id);

    // Check history folder special case
    if (current_dir && strcmp(current_dir->name, "h") == 0 && current_dir->parent_id == fs_root_id) {
        history_show();
        return;
    }

    if (!current_dir) {
        console_print_colored("Error: Invalid current directory.\n", COLOR_LIGHT_RED);
        return;
    }

    if (current_dir->child_count == 0) {
        console_print_colored("Directory is empty.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    console_print_colored("Contents:\n", COLOR_YELLOW_ON_BLACK);

    for (uint32_t i = 0; i < current_dir->child_count; i++) {
        uint32_t child_id = current_dir->child_ids[i];
        fs_node_t* child = fs_get_node(child_id);

        if (child) {
            if (child->type == FS_TYPE_DIRECTORY) {
                console_print_colored(child->name, COLOR_YELLOW_ON_BLACK);
                console_print_colored("/", COLOR_YELLOW_ON_BLACK);
            } else {
                console_print_colored(child->name, COLOR_WHITE_ON_BLACK);
                console_print(" (");
                char size_str[12];
                int_to_str((int)child->size, size_str);
                console_print(size_str);
                console_print(" bytes)");
            }
            console_print("\n");
        }
    }
}

void cmd_cd(char* path) {
    if (strlen(path) == 0) return;

    fs_node_t* target = fs_find_node(path, fs_current_dir_id);

    if (target && target->type == FS_TYPE_DIRECTORY) {
        if (target->id == fs_root_id && !ROOT_ACCESS_GRANTED && fs_current_dir_id != fs_root_id) {
             console_print_colored("root access denied\n", COLOR_LIGHT_RED);
             return;
        }

        fs_current_dir_id = target->id;
        console_print_colored("Changed directory.\n", COLOR_GREEN_ON_BLACK);
    } else {
        console_print_colored("cd: Directory not found or invalid.\n", COLOR_YELLOW_ON_BLACK);
    }
}

void cmd_mkdir(char* path) {
    if (strlen(path) == 0) {
        console_print_colored("Usage: mkdir <name>\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    char temp_path[40];
    strncpy(temp_path, path, 39);
    temp_path[39] = '\0';

    char* last_component = temp_path;
    char* separator = 0;
    uint32_t parent_id = fs_current_dir_id;

    for (int i = 0; temp_path[i] != '\0'; i++) {
        if (temp_path[i] == '/') {
            separator = &temp_path[i];
        }
    }

    char final_name[64];

    if (separator) {
        *separator = '\0';
        last_component = separator + 1;

        if (temp_path[0] == '\0' && path[0] == '/') {
            parent_id = fs_root_id;
        } else {
            fs_node_t* resolved_parent = fs_find_node(temp_path, fs_current_dir_id);
            if (!resolved_parent || resolved_parent->type != FS_TYPE_DIRECTORY) {
                console_print_colored("mkdir: Parent directory not found.\n", COLOR_YELLOW_ON_BLACK);
                return;
            }
            parent_id = resolved_parent->id;
        }
    }

    if (strlen(last_component) == 0) {
        console_print_colored("Error: Invalid name.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }
    strcpy(final_name, last_component);

    if (fs_find_node_local_id(parent_id, final_name) != 0) {
        console_print_colored("mkdir: Directory already exists.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    if (fs_create_node(parent_id, final_name, FS_TYPE_DIRECTORY)) {
        console_print_colored("Directory created.\n", COLOR_GREEN_ON_BLACK);
    } else {
        console_print_colored("mkdir: Failed to create directory.\n", COLOR_LIGHT_RED);
    }
}

void cmd_rmdir(char* path) {
    if (strlen(path) == 0) {
        console_print_colored("Usage: rmdir <name>\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    fs_node_t* target = fs_find_node(path, fs_current_dir_id);

    if (!target) {
        console_print_colored("rmdir: Directory not found.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    if (target->type != FS_TYPE_DIRECTORY) {
        console_print_colored("rmdir: Not a directory.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    if (target->id == fs_current_dir_id || target->id == fs_root_id) {
        console_print_colored("rmdir: Cannot remove current or root directory.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    if (fs_delete_node(target->id) == 0) {
        console_print_colored("Directory removed.\n", COLOR_GREEN_ON_BLACK);
    } else {
        console_print_colored("rmdir: Failed (is it empty?).\n", COLOR_LIGHT_RED);
    }
}

// --- NEW: Enhanced ADD command with save functionality ---
void cmd_add(char* args) {
    fs_node_t* cur = fs_get_node(fs_current_dir_id);

    // Check if we're in /a directory
    if (!cur || cur->parent_id != fs_root_id || strcmp(cur->name, "a") != 0) {
        console_print_colored("Mount /a for executing this command\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    char input[40];
    console_print_colored("Enter first number: ", COLOR_YELLOW_ON_BLACK);
    read_line_with_display(input, 40);
    int num1 = str_to_int(input);

    console_print_colored("Enter second number: ", COLOR_YELLOW_ON_BLACK);
    read_line_with_display(input, 40);
    int num2 = str_to_int(input);

    int sum = num1 + num2;

    // Build result string
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

    // Display result
    console_print(last_result);
    console_print("\n");

    // Check if save mode is enabled (args = "s")
    if (args && strlen(args) > 0 && args[0] == 's') {
        console_print_colored("Saving result to disk...\n", COLOR_YELLOW_ON_BLACK);

        // Find or create "results.txt" file in /a
        uint32_t a_id = fs_find_node_local_id(fs_root_id, "a");
        if (a_id == 0) {
            console_print_colored("Error: /a directory not found.\n", COLOR_LIGHT_RED);
            return;
        }

        // Check if results.txt exists
        uint32_t file_id = fs_find_node_local_id(a_id, "results.txt");
        fs_node_t* file_node = 0;

        if (file_id == 0) {
            // Create new file
            if (fs_create_node(a_id, "results.txt", FS_TYPE_FILE)) {
                file_id = fs_find_node_local_id(a_id, "results.txt");
                file_node = fs_get_node(file_id);
            } else {
                console_print_colored("Error: Could not create results file.\n", COLOR_LIGHT_RED);
                return;
            }
        } else {
            file_node = fs_get_node(file_id);
        }

        if (file_node) {
            // For simplicity, we'll store the result in the node's padding area
            // (In a real FS, you'd write to data sectors)
            int result_len = strlen(last_result);
            if (result_len < 200) { // Safety check
                // Copy result to padding area (acts as simple content storage)
                for (int i = 0; i < result_len; i++) {
                    file_node->padding[i] = last_result[i];
                }
                file_node->padding[result_len] = '\n';
                file_node->padding[result_len + 1] = '\0';
                file_node->size = result_len + 1;

                // Persist to disk
                fs_update_node(file_node);
                console_print_colored("Result saved to /a/results.txt\n", COLOR_GREEN_ON_BLACK);
            } else {
                console_print_colored("Error: Result too large to save.\n", COLOR_LIGHT_RED);
            }
        }
    }
}

// --- NEW: Enhanced MEM command with disk space and cache stats ---
void cmd_mem() {
    console_print_colored("=== Memory Statistics ===\n", COLOR_GREEN_ON_BLACK);

    // Physical memory stats
    uint32_t total, used, free;
    pmm_get_stats(&total, &used, &free);

    uint32_t total_kb = (total * PAGE_SIZE) / 1024;
    uint32_t used_kb = (used * PAGE_SIZE) / 1024;
    uint32_t free_kb = (free * PAGE_SIZE) / 1024;

    char num[16];
    console_print("Total RAM: "); int_to_str(total_kb, num); console_print(num); console_print(" KB\n");
    console_print("Used RAM:  "); int_to_str(used_kb, num); console_print(num); console_print(" KB\n");
    console_print("Free RAM:  "); int_to_str(free_kb, num); console_print(num); console_print(" KB\n");

    console_print("\n");
    console_print_colored("=== Disk Statistics ===\n", COLOR_GREEN_ON_BLACK);

    // Get real-time disk statistics from filesystem
    uint32_t total_disk_kb, used_disk_kb, free_disk_kb;
    fs_get_disk_stats(&total_disk_kb, &used_disk_kb, &free_disk_kb);

    console_print("Total Disk: "); int_to_str(total_disk_kb, num); console_print(num); console_print(" KB\n");
    console_print("Used Disk:  "); int_to_str(used_disk_kb, num); console_print(num); console_print(" KB\n");
    console_print("Free Disk:  "); int_to_str(free_disk_kb, num); console_print(num); console_print(" KB\n");

    console_print("\n");
    console_print_colored("=== Filesystem Cache ===\n", COLOR_GREEN_ON_BLACK);

    // Get cache statistics
    uint32_t cache_size, cached_nodes, dirty_nodes;
    fs_get_cache_stats(&cache_size, &cached_nodes, &dirty_nodes);

    console_print("Cache Size:    "); int_to_str(cache_size, num); console_print(num); console_print(" slots\n");
    console_print("Cached Nodes:  "); int_to_str(cached_nodes, num); console_print(num); console_print("\n");
    console_print("Dirty Nodes:   "); int_to_str(dirty_nodes, num); console_print(num); console_print(" (pending write)\n");

    uint32_t cache_usage = (cached_nodes * 100) / cache_size;
    console_print("Cache Usage:   "); int_to_str(cache_usage, num); console_print(num); console_print("%\n");
}

void cmd_su() {
    if (ROOT_ACCESS_GRANTED) {
        console_print_colored("already in root mode\n", COLOR_GREEN_ON_BLACK);
        return;
    }
    char pass[MAX_PASSWORD_LEN];
    console_print_colored("enter root password: ", COLOR_GREEN_ON_BLACK);
    read_line_with_display(pass, MAX_PASSWORD_LEN);
    if (strcmp(pass, ROOT_PASSWORD) == 0) {
        console_print_colored("root access granted\n", COLOR_GREEN_ON_BLACK);
        ROOT_ACCESS_GRANTED = 1;
        fs_current_dir_id = fs_root_id; // Go to root
    } else {
        console_print_colored("root access denied\n", COLOR_YELLOW_ON_BLACK);
    }
}

// --- NEW: SUDO command for temporary privilege escalation ---
void cmd_sudo(char* args) {
    if (strlen(args) == 0) {
        console_print_colored("Usage: sudo <command>\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Parse the command after sudo
    char cmd[40];
    char cmd_args[40];
    int i = 0;

    // Extract command
    while (args[i] && args[i] != ' ') {
        cmd[i] = args[i];
        i++;
    }
    cmd[i] = '\0';

    // Extract arguments
    int j = 0;
    if (args[i] == ' ') {
        i++;
        while (args[i] == ' ') i++;
        while (args[i]) {
            cmd_args[j++] = args[i++];
        }
    }
    cmd_args[j] = '\0';

    // Check if it's a privileged command
    if (strcmp(cmd, "shutdown") != 0 &&
        strcmp(cmd, "chuser") != 0 &&
        strcmp(cmd, "chpasswd") != 0) {
        console_print_colored("sudo: only 'shutdown', 'chuser', and 'chpasswd' commands are supported\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Ask for password
    char pass[MAX_PASSWORD_LEN];
    console_print_colored("[sudo] password for ", COLOR_YELLOW_ON_BLACK);
    console_print(current_user);
    console_print(": ");
    read_line_with_display(pass, MAX_PASSWORD_LEN);

    if (strcmp(pass, ROOT_PASSWORD) == 0) {
        // Execute command with temporary privilege
        if (strcmp(cmd, "shutdown") == 0) {
            cmd_shutdown();
        } else if (strcmp(cmd, "chuser") == 0) {
            auth_change_username(read_line_with_display);
        } else if (strcmp(cmd, "chpasswd") == 0) {
            auth_change_password(read_line_with_display);
        }
    } else {
        console_print_colored("sudo: authentication failed\n", COLOR_LIGHT_RED);
    }
}

// --- NEW: Shutdown command (requires root) ---
void cmd_shutdown() {
    if (!ROOT_ACCESS_GRANTED) {
        console_print_colored("shutdown: permission denied (try 'sudo shutdown')\n", COLOR_LIGHT_RED);
        return;
    }

    console_clear_screen();
    console_print_colored("SHUTTING DOWN SYSTEM...\n", COLOR_LIGHT_RED);
    console_print_colored("Goodbye!\n", COLOR_GREEN_ON_BLACK);

    // QEMU shutdown via ACPI
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x604));

    // Alternative shutdown methods if ACPI fails
    __asm__ volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));

    // If all else fails, halt
    while(1) __asm__ volatile("hlt");
}

// --- NEW: Change Username Command ---
void cmd_chuser() {
    if (!ROOT_ACCESS_GRANTED) {
        console_print_colored("chuser: permission denied (try 'sudo chuser')\n", COLOR_LIGHT_RED);
        return;
    }

    auth_change_username(read_line_with_display);
}

// --- NEW: Change Password Command ---
void cmd_chpasswd() {
    if (!ROOT_ACCESS_GRANTED) {
        console_print_colored("chpasswd: permission denied (try 'sudo chpasswd')\n", COLOR_LIGHT_RED);
        return;
    }

    auth_change_password(read_line_with_display);
}

// --- NEW: System Info Command ---
void cmd_sysinfo() {
    console_print_colored("=== PUNIX System Information ===\n", COLOR_GREEN_ON_BLACK);
    console_print("\n");

    // Try to read version from /boot/version
    fs_node_t* boot_dir = fs_find_node("boot", fs_root_id);
    if (boot_dir) {
        uint32_t version_id = fs_find_node_local_id(boot_dir->id, "version");
        if (version_id) {
            fs_node_t* version_file = fs_get_node(version_id);
            if (version_file && version_file->size > 0) {
                char* content = (char*)version_file->padding;
                console_print(content);
                console_print("\n");
            }
        }
    }

    // Disk layout info
    console_print_colored("Disk Layout:\n", COLOR_YELLOW_ON_BLACK);
    console_print("  Sector 0:       Bootloader (512 bytes)\n");
    console_print("  Sectors 1-60:   Kernel binary (~30 KB)\n");
    console_print("  Sector 61:      Filesystem superblock\n");
    console_print("  Sectors 62+:    Filesystem data\n");
    console_print("\n");

    // Memory info
    uint32_t total, used, free;
    pmm_get_stats(&total, &used, &free);
    console_print_colored("Memory:\n", COLOR_YELLOW_ON_BLACK);
    char num[16];
    console_print("  Total: ");
    int_to_str((total * PAGE_SIZE) / 1024, num);
    console_print(num);
    console_print(" KB\n");

    // Disk info
    uint32_t total_kb, used_kb, free_kb;
    fs_get_disk_stats(&total_kb, &used_kb, &free_kb);
    console_print_colored("Storage:\n", COLOR_YELLOW_ON_BLACK);
    console_print("  Total: ");
    int_to_str(total_kb, num);
    console_print(num);
    console_print(" KB\n");

    console_print("\n");
    console_print_colored("Current User: ", COLOR_YELLOW_ON_BLACK);
    console_print(USERNAME);
    console_print("\n");
}

// --- NEW: Show Message of the Day ---
void cmd_motd() {
    fs_node_t* etc_dir = fs_find_node("etc", fs_root_id);
    if (!etc_dir) {
        console_print_colored("Error: /etc directory not found.\n", COLOR_LIGHT_RED);
        return;
    }

    uint32_t motd_id = fs_find_node_local_id(etc_dir->id, "motd");
    if (motd_id == 0) {
        console_print_colored("No message of the day.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    fs_node_t* motd_file = fs_get_node(motd_id);
    if (motd_file && motd_file->size > 0) {
        char* content = (char*)motd_file->padding;
        console_print_colored(content, COLOR_GREEN_ON_BLACK);
    }
}

void cmd_help() {
    console_clear_screen();
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    console_print_colored("|       PUNIX: LIST OF AVAILABLE COMMANDS        |\n", COLOR_GREEN_ON_BLACK);
    console_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    console_print("\n");

    console_print_colored("Filesystem Commands:\n", COLOR_YELLOW_ON_BLACK);
    console_print("  ls            - List directory contents\n");
    console_print("  cd [dir]      - Change directory\n");
    console_print("  pwd           - Show current path\n");
    console_print("  mkdir [name]  - Create directory\n");
    console_print("  rmdir [name]  - Remove empty directory\n");
    console_print("  text [file]   - Open text editor\n");
    console_print("  sync          - Flush cache to disk\n");
    console_print("\n");

    console_print_colored("System Commands:\n", COLOR_YELLOW_ON_BLACK);
    console_print("  mem           - Show memory, disk, and cache stats\n");
    console_print("  sysinfo       - Show system information\n");
    console_print("  motd          - Show message of the day\n");
    console_print("  clear         - Clear screen\n");
    console_print("  help          - Show this help\n");
    console_print("\n");

    console_print_colored("Privilege Commands:\n", COLOR_YELLOW_ON_BLACK);
    console_print("  root          - Switch to root mode\n");
    console_print("  exit          - Exit root mode\n");
    console_print("  sudo [cmd]    - Execute command with root privilege\n");
    console_print("  shutdown      - Shutdown system (requires root)\n");
    console_print("  chuser        - Change username (requires root)\n");
    console_print("  chpasswd      - Change password (requires root)\n");
    console_print("\n");

    console_print_colored("Application Commands (requires /a):\n", COLOR_YELLOW_ON_BLACK);
    console_print("  add           - Simple calculator\n");
    console_print("  add s         - Calculator with disk save\n");
    console_print("\n");
}

void cmd_clear() {
    console_clear_screen();
    shell_init();
}

// --- UPDATED: Exit command now only exits root mode ---
void cmd_exit() {
    if (ROOT_ACCESS_GRANTED) {
        ROOT_ACCESS_GRANTED = 0;
        console_print_colored("Exited root mode\n", COLOR_GREEN_ON_BLACK);

        // Return to /a directory
        uint32_t a_id = fs_find_node_local_id(fs_root_id, "a");
        if (a_id != 0) {
            fs_current_dir_id = a_id;
        } else {
            fs_current_dir_id = fs_root_id;
        }
    } else {
        console_print_colored("Not in root mode. Use 'shutdown' to power off.\n", COLOR_YELLOW_ON_BLACK);
    }
}

// History Functions (Stubs for now)
void history_save() {
    console_print_colored("History save not available in this version.\n", COLOR_YELLOW_ON_BLACK);
}
void history_delete(int index) { console_print("Not implemented.\n"); }
void history_show() { console_print("Not implemented.\n"); }

// Main Loop
void shell_run() {
    while (1) {
        char input[40];
        show_prompt();
        read_line_with_display(input, 40);
        if (strlen(input) == 0) continue;

        char cmd[40];
        char args[40];
        int i = 0;
        while (input[i] && input[i] != ' ') { cmd[i] = input[i]; i++; }
        cmd[i] = '\0';
        int j = 0;
        if (input[i] == ' ') {
            i++;
            while (input[i] == ' ') i++;
            while (input[i]) { args[j++] = input[i++]; }
        }
        args[j] = '\0';

        // Command routing
        if (strcmp(cmd, "ls") == 0) cmd_ls();
        else if (strcmp(cmd, "pwd") == 0) cmd_pwd();
        else if (strcmp(cmd, "cd") == 0) cmd_cd(args);
        else if (strcmp(cmd, "mkdir") == 0) cmd_mkdir(args);
        else if (strcmp(cmd, "rmdir") == 0) cmd_rmdir(args);
        else if (strcmp(cmd, "help") == 0) cmd_help();
        else if (strcmp(cmd, "clear") == 0) cmd_clear();
        else if (strcmp(cmd, "mem") == 0) cmd_mem();
        else if (strcmp(cmd, "root") == 0) cmd_su();
        else if (strcmp(cmd, "exit") == 0) cmd_exit();
        else if (strcmp(cmd, "add") == 0) cmd_add(args);
        else if (strcmp(cmd, "text") == 0) text_editor(args);
        else if (strcmp(cmd, "sudo") == 0) cmd_sudo(args);
        else if (strcmp(cmd, "shutdown") == 0) cmd_shutdown();
        else if (strcmp(cmd, "sync") == 0) fs_sync();
        else if (strcmp(cmd, "chuser") == 0) cmd_chuser();
        else if (strcmp(cmd, "chpasswd") == 0) cmd_chpasswd();
        else if (strcmp(cmd, "sysinfo") == 0) cmd_sysinfo();
        else if (strcmp(cmd, "motd") == 0) cmd_motd();
        else {
            console_print(cmd);
            console_print_colored(": command not found\n", COLOR_YELLOW_ON_BLACK);
        }
    }
}
