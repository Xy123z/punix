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
#include "../include/ata.h"
#include "../include/syscall.h"
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

static void show_prompt() {
    sys_print(current_user);
    sys_print("@");
    sys_print(kernel_name);
    sys_print(":");

    char cwd_buf[256];
    sys_getcwd(cwd_buf, 256);
    sys_print(cwd_buf);

    if (ROOT_ACCESS_GRANTED) {
        sys_print_colored("# ", COLOR_WHITE_ON_BLACK);
    } else {
        sys_print_colored("$ ", COLOR_WHITE_ON_BLACK);
    }
}

void shell_init() {
    sys_print_colored("+================================================+\n", COLOR_WHITE_ON_BLACK);
    sys_print_colored("|          PUNIX: AN EXPERIMENTAL KERNEL         |\n", COLOR_WHITE_ON_BLACK);
    sys_print_colored("+================================================+\n", COLOR_WHITE_ON_BLACK);
    sys_print("\n");
}

// --- Command Implementations ---

void cmd_pwd() {
    char cwd_buf[256];
    sys_getcwd(cwd_buf, 256);
    sys_print(cwd_buf);
    sys_print("\n");
}

void cmd_ls(char* path) {
    char dir_path[256];
    if (strlen(path) == 0) {
        strcpy(dir_path, ".");
    } else {
        strcpy(dir_path, path);
    }

    struct dirent entries[16];
    int count = sys_getdents(dir_path, entries, 16);

    if (count < 0) {
        sys_print("ls: cannot access directory\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        if (entries[i].d_type == FS_TYPE_DIRECTORY) {
            sys_print(entries[i].d_name);
            sys_print("/\n");
        } else {
            sys_print(entries[i].d_name);
            sys_print("\n");
        }
    }

    if (count == 0) {
        sys_print("Directory is empty.\n");
    }
}

void cmd_cd(char* path) {
    if (strlen(path) == 0) return;

    if (sys_chdir(path) == 0) {
        sys_print("Changed directory.\n");
    } else {
        sys_print("cd: Directory not found or invalid.\n");
    }
}

void cmd_mkdir(char* path) {
    if (strlen(path) == 0) {
        sys_print("Usage: mkdir <path>\n");
        return;
    }

    if (sys_mkdir(path) == 0) {
        sys_print("Directory created.\n");
    } else {
        sys_print("mkdir: Failed to create directory.\n");
    }
}

void cmd_rmdir(char* path) {
    if (strlen(path) == 0) {
        sys_print("Usage: rmdir <path>\n");
        return;
    }
    if (sys_rmdir(path) == 0) {
        sys_print("Directory removed.\n");
    } else {
        sys_print("rmdir: Failed to remove directory (check if empty).\n");
    }
}

// --- NEW: Enhanced ADD command with save functionality ---
void cmd_add(char* args) {
    char cwd_buf[256];
    sys_getcwd(cwd_buf, 256);

    // Check if we're in /a directory
    if (strcmp(cwd_buf, "/a") != 0) {
        sys_print_colored("Mount /a for executing this command\n", COLOR_WHITE_ON_BLACK);
        return;
    }

    char input[40];
    sys_print_colored("Enter first number: ", COLOR_WHITE_ON_BLACK);
    read_line_with_display(input, 40);
    int num1 = str_to_int(input);

    sys_print_colored("Enter second number: ", COLOR_WHITE_ON_BLACK);
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
    sys_print(last_result);
    sys_print("\n");

    // Check if save mode is enabled (args = "s")
    if (args && strlen(args) > 0 && args[0] == 's') {
        sys_print_colored("Saving result to disk...\n", COLOR_WHITE_ON_BLACK);

        char full_content[256];
        strcpy(full_content, last_result);
        strcat(full_content, "\n");
            
        int fd = sys_open("/a/results.txt", O_RDWR | O_CREAT);
        if (fd >= 0) {
            if (sys_write(fd, full_content, strlen(full_content)) >= 0) {
                sys_print("Result saved to /a/results.txt\n");
            } else {
                sys_print("Error: Failed to write to file.\n");
            }
            sys_close(fd);
        } else {
            sys_print("Error: Failed to open results.txt\n");
        }
    }
}

// --- NEW: Enhanced MEM command with disk space and cache stats ---
void cmd_mem() {
    sys_print_colored("=== Memory Statistics ===\n", COLOR_WHITE_ON_BLACK);

    // Physical memory stats
    uint32_t total, used, free;
    pmm_get_stats(&total, &used, &free);

    uint32_t total_kb = (total * PAGE_SIZE) / 1024;
    uint32_t used_kb = (used * PAGE_SIZE) / 1024;
    uint32_t free_kb = (free * PAGE_SIZE) / 1024;

    char num[16];
    sys_print("Total RAM: "); int_to_str(total_kb, num); sys_print(num); sys_print(" KB\n");
    sys_print("Used RAM:  "); int_to_str(used_kb, num); sys_print(num); sys_print(" KB\n");
    sys_print("Free RAM:  "); int_to_str(free_kb, num); sys_print(num); sys_print(" KB\n");

    sys_print("\n");
    sys_print_colored("=== Disk Statistics ===\n", COLOR_WHITE_ON_BLACK);

    // Get real-time disk statistics from filesystem
    uint32_t total_disk_kb, used_disk_kb, free_disk_kb;
    sys_get_disk_stats(&total_disk_kb, &used_disk_kb, &free_disk_kb);

    sys_print("Total Disk: "); int_to_str(total_disk_kb, num); sys_print(num); sys_print(" KB\n");
    sys_print("Used Disk:  "); int_to_str(used_disk_kb, num); sys_print(num); sys_print(" KB\n");
    sys_print("Free Disk:  "); int_to_str(free_disk_kb, num); sys_print(num); sys_print(" KB\n");

    sys_print("\n");
    sys_print_colored("=== Filesystem Cache ===\n", COLOR_WHITE_ON_BLACK);

    // Get cache statistics
    uint32_t cache_size, cached_nodes, dirty_nodes;
    sys_get_cache_stats(&cache_size, &cached_nodes, &dirty_nodes);

    sys_print("Cache Size:    "); int_to_str(cache_size, num); sys_print(num); sys_print(" slots\n");
    sys_print("Cached Nodes:  "); int_to_str(cached_nodes, num); sys_print(num); sys_print("\n");
    sys_print("Dirty Nodes:   "); int_to_str(dirty_nodes, num); sys_print(num); sys_print(" (pending write)\n");

    uint32_t cache_usage = (cached_nodes * 100) / cache_size;
    sys_print("Cache Usage:   "); int_to_str(cache_usage, num); sys_print(num); sys_print("%\n");
}

void cmd_su() {
    if (sys_getuid() == 0) {
        ROOT_ACCESS_GRANTED = 1;
        sys_print_colored("Already in root mode\n", COLOR_WHITE_ON_BLACK);
        return;
    }

    char pass[MAX_PASSWORD_LEN];
    sys_print_colored("Password: ", COLOR_WHITE_ON_BLACK);
    read_line_with_display(pass, MAX_PASSWORD_LEN);

    if (sys_authenticate(pass) == 0) {
        ROOT_ACCESS_GRANTED = 1;
        sys_print_colored("Root access granted.\n", COLOR_GREEN_ON_BLACK);
        sys_chdir("/");
    } else {
        sys_print_colored("Authentication failed.\n", COLOR_LIGHT_RED);
    }
}

// --- NEW: SUDO command for temporary privilege escalation ---
void cmd_sudo(char* args) {
    if (strlen(args) == 0) {
        sys_print_colored("Usage: sudo <command>\n", COLOR_WHITE_ON_BLACK);
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
    if (strcmp(cmd, "chuser") != 0 &&
        strcmp(cmd, "chpasswd") != 0) {
        sys_print_colored("sudo: only 'chuser', 'chpasswd' are supported.\n", COLOR_WHITE_ON_BLACK);
        return;
    }

    // Ask for password
    char pass[MAX_PASSWORD_LEN];
    sys_print_colored("[sudo] password for ", COLOR_WHITE_ON_BLACK);
    sys_print(current_user);
    sys_print(": ");
    read_line_with_display(pass, MAX_PASSWORD_LEN);

    if (sys_authenticate(pass) == 0) {
        // Execute command with temporary privilege
        if (strcmp(cmd, "shutdown") == 0) {
            cmd_shutdown();
        } else if (strcmp(cmd, "chuser") == 0) {
            cmd_chuser();
        } else if (strcmp(cmd, "chpasswd") == 0) {
            cmd_chpasswd();
        }
        
        // Drop privileges back immediately
        sys_setuid(1000);
    } else {
        sys_print_colored("sudo: authentication failed\n", COLOR_LIGHT_RED);
    }
}

// --- Power Management Commands ---
void cmd_shutdown() {
    sys_clear_screen();
    sys_print_colored("Goodbye!\n", COLOR_WHITE_ON_BLACK);
    sys_shutdown();
}

void cmd_restart() {
    sys_clear_screen();
    sys_print_colored("Restarting...\n", COLOR_WHITE_ON_BLACK);
    sys_restart();
}

// --- NEW: Change Username Command ---
void cmd_chuser() {
    if (sys_getuid() != 0) {
        sys_print_colored("chuser: permission denied (try 'sudo chuser')\n", COLOR_LIGHT_RED);
        return;
    }

    char new_username[MAX_USERNAME_LEN];
    sys_print_colored("Enter new username (max 39 chars): ", COLOR_GREEN_ON_BLACK);
    read_line_with_display(new_username, MAX_USERNAME_LEN);

    if (strlen(new_username) == 0) {
        sys_print_colored("Error: Username cannot be empty.\n", COLOR_LIGHT_RED);
        return;
    }

    if (sys_chuser(new_username) == 0) {
        sys_print_colored("Username changed successfully!\n", COLOR_GREEN_ON_BLACK);
    } else {
        sys_print_colored("Error: Failed to change username.\n", COLOR_LIGHT_RED);
    }
}

// --- NEW: Change Password Command ---
void cmd_chpasswd() {
    if (sys_getuid() != 0) {
        sys_print_colored("chpasswd: permission denied (try 'sudo chpasswd')\n", COLOR_LIGHT_RED);
        return;
    }

    char new_pass[MAX_PASSWORD_LEN];
    char new_pass_conf[MAX_PASSWORD_LEN];

    sys_print_colored("Enter new password (max 39 chars): ", COLOR_GREEN_ON_BLACK);
    read_line_with_display(new_pass, MAX_PASSWORD_LEN);
    sys_print_colored("Confirm new password: ", COLOR_GREEN_ON_BLACK);
    read_line_with_display(new_pass_conf, MAX_PASSWORD_LEN);

    if (strcmp(new_pass, new_pass_conf) != 0) {
        sys_print_colored("Error: Passwords don't match.\n", COLOR_LIGHT_RED);
        return;
    }

    if (strlen(new_pass) == 0) {
        sys_print_colored("Error: Password cannot be empty.\n", COLOR_LIGHT_RED);
        return;
    }

    if (sys_chpass(new_pass) == 0) {
        sys_print_colored("Password changed successfully!\n", COLOR_GREEN_ON_BLACK);
    } else {
        sys_print_colored("Error: Failed to change password.\n", COLOR_LIGHT_RED);
    }
}

// --- NEW: System Info Command ---
void cmd_sysinfo() {
    sys_print("=== PUNIX System Information ===\n\n");
    
    int fd = sys_open("/boot/version", O_RDONLY);
    if (fd >= 0) {
        char content[256];
        int read_bytes = sys_read(fd, content, 255);
        if (read_bytes > 0) {
            content[read_bytes] = '\0';
            sys_print(content);
            sys_print("\n");
        }
        sys_close(fd);
    }

    // Disk layout info
    sys_print_colored("Disk Layout:\n", COLOR_WHITE_ON_BLACK);
    sys_print("  Sector 0:       Bootloader (512 bytes)\n");
    sys_print("  Sectors 1-60:   Kernel binary (~30 KB)\n");
    sys_print("  Sector 61:      Filesystem superblock\n");
    sys_print("  Sectors 62+:    Filesystem data\n");
    sys_print("\n");

    // Memory info
    uint32_t total, used, free;
    pmm_get_stats(&total, &used, &free);
    sys_print_colored("Memory:\n", COLOR_WHITE_ON_BLACK);
    char num[16];
    sys_print("  Total: ");
    int_to_str((total * PAGE_SIZE) / 1024, num);
    sys_print(num);
    sys_print(" KB\n");

    // Disk info
    uint32_t total_kb, used_kb, free_kb;
    sys_get_disk_stats(&total_kb, &used_kb, &free_kb);
    sys_print_colored("Storage:\n", COLOR_WHITE_ON_BLACK);
    sys_print("  Total: ");
    int_to_str(total_kb, num);
    sys_print(num);
    sys_print(" KB\n");

    sys_print("\n");
    sys_print_colored("Current User: ", COLOR_WHITE_ON_BLACK);
    sys_print(USERNAME);
    sys_print("\n");
}

// --- NEW: Show Message of the Day ---
void cmd_motd() {
    int fd = sys_open("/etc/motd", O_RDONLY);
    if (fd >= 0) {
        char content[256];
        int read = sys_read(fd, content, 255);
        if (read >= 0) {
            content[read] = '\0';
            sys_print_colored(content, COLOR_GREEN_ON_BLACK);
            sys_print("\n");
        }
        sys_close(fd);
    } else {
        sys_print("No message of the day.\n");
    }
}

void cmd_help() {
    sys_clear_screen();
    sys_print_colored("+================================================+\n", COLOR_WHITE_ON_BLACK);
    sys_print_colored("|       PUNIX: LIST OF AVAILABLE COMMANDS        |\n", COLOR_WHITE_ON_BLACK);
    sys_print_colored("+================================================+\n", COLOR_WHITE_ON_BLACK);
    sys_print("\n");

    sys_print_colored("Filesystem Commands:\n", COLOR_WHITE_ON_BLACK);
    sys_print("  ls            - List directory contents\n");
    sys_print("  cd [dir]      - Change directory\n");
    sys_print("  pwd           - Show current path\n");
    sys_print("  mkdir [name]  - Create directory\n");
    sys_print("  rmdir [name]  - Remove empty directory\n");
    sys_print("  text [file]   - Open text editor\n");
    sys_print("  sync          - Flush cache to disk\n");
    sys_print("\n");

    sys_print_colored("System Commands:\n", COLOR_WHITE_ON_BLACK);
    sys_print("  mem           - Show memory, disk, and cache stats\n");
    sys_print("  sysinfo       - Show system information\n");
    sys_print("  motd          - Show message of the day\n");
    sys_print("  shutdown      - Power off the system\n");
    sys_print("  restart       - Reboot the system\n");
    sys_print("  clear         - Clear screen\n");
    sys_print("  help          - Show this help\n");
    sys_print("\n");

    sys_print_colored("Privilege Commands:\n", COLOR_WHITE_ON_BLACK);
    sys_print("  root          - Switch to root mode\n");
    sys_print("  exit          - Exit root mode\n");
    sys_print("  sudo [cmd]    - Execute command with root privilege\n");
    sys_print("  chuser        - Change username (requires root)\n");
    sys_print("  chpasswd      - Change password (requires root)\n");
    sys_print("\n");

    sys_print_colored("Application Commands (requires /a):\n", COLOR_WHITE_ON_BLACK);
    sys_print("  add           - Simple calculator\n");
    sys_print("  add s         - Calculator with disk save\n");
    sys_print("\n");
}

void cmd_clear() {
    sys_clear_screen();
    shell_init();
}

// --- UPDATED: Exit command now only exits root mode ---
void cmd_exit() {
    if (sys_getuid() == 0) {
        sys_setuid(1000);
        ROOT_ACCESS_GRANTED = 0;
        sys_print_colored("Exited root mode\n", COLOR_WHITE_ON_BLACK);
        sys_chdir("/a");
    } else {
        sys_print_colored("Not in root mode. Use 'shutdown' to power off.\n", COLOR_WHITE_ON_BLACK);
    }
}

// History Functions (Stubs for now)
void history_save() {
    sys_print_colored("History save not available in this version.\n", COLOR_WHITE_ON_BLACK);
}
void history_delete(int index) { sys_print("Not implemented.\n"); }
void history_show() { sys_print("Not implemented.\n"); }

// Main Loop
void shell_run() {
    // Drop initial privileges
    sys_setuid(1000);
    ROOT_ACCESS_GRANTED = 0;

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
        if (strcmp(cmd, "ls") == 0) cmd_ls(args);
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
        else if (strcmp(cmd, "restart") == 0) cmd_restart();
        else if (strcmp(cmd, "sync") == 0) sys_sync();
        else if (strcmp(cmd, "chuser") == 0) cmd_chuser();
        else if (strcmp(cmd, "chpasswd") == 0) cmd_chpasswd();
        else if (strcmp(cmd, "sysinfo") == 0) cmd_sysinfo();
        else if (strcmp(cmd, "motd") == 0) cmd_motd();
        else {
            sys_print(cmd);
            sys_print_colored(": command not found\n", COLOR_WHITE_ON_BLACK);
        }
    }
}
