/**
 * src/shell.c - Shell implementation using System Calls
 * All filesystem operations now go through syscall interface
 */
#include "../include/shell.h"
#include "../include/syscall.h"
#include "../include/console.h"
#include "../include/memory.h"
#include "../include/interrupt.h"
#include "../include/string.h"
#include "../include/text.h"
#include "../include/auth.h"

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
static const char* kernel_name = "punix-v1.04";

// Prototypes
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
 * @brief Show shell prompt with current directory
 */
static void show_prompt() {
    console_print_colored(current_user, COLOR_GREEN_ON_BLACK);
    console_print_colored("@", COLOR_WHITE_ON_BLACK);
    console_print_colored(kernel_name, COLOR_GREEN_ON_BLACK);
    console_print_colored(":", COLOR_WHITE_ON_BLACK);

    // Get current directory from kernel via syscall
    char cwd[128];
    if (sys_getcwd(cwd, 128) == 0) {
        console_print_colored(cwd, COLOR_YELLOW_ON_BLACK);
    } else {
        console_print_colored("???", COLOR_YELLOW_ON_BLACK);
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

// --- Command Implementations using System Calls ---

void cmd_pwd() {
    char cwd[128];
    if (sys_getcwd(cwd, 128) == 0) {
        console_print(cwd);
        console_print("\n");
    } else {
        console_print_colored("Error: Cannot get current directory\n", COLOR_LIGHT_RED);
    }
}

void cmd_ls() {
    struct dirent entries[16];

    // Get directory entries via syscall
    int count = sys_getdents(".", entries, 16);

    if (count < 0) {
        console_print_colored("Error: Cannot read directory\n", COLOR_LIGHT_RED);
        return;
    }

    if (count == 0) {
        console_print_colored("Directory is empty.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    console_print_colored("Contents:\n", COLOR_YELLOW_ON_BLACK);

    for (int i = 0; i < count; i++) {
        if (entries[i].d_type == FS_TYPE_DIRECTORY) {
            console_print_colored(entries[i].d_name, COLOR_YELLOW_ON_BLACK);
            console_print_colored("/", COLOR_YELLOW_ON_BLACK);
        } else {
            console_print_colored(entries[i].d_name, COLOR_WHITE_ON_BLACK);
            console_print(" (file)");
        }
        console_print("\n");
    }
}

void cmd_cd(char* path) {
    if (strlen(path) == 0) return;

    // Special handling for root directory access
    if (strcmp(path, "/") == 0 && !ROOT_ACCESS_GRANTED) {
        console_print_colored("root access denied\n", COLOR_LIGHT_RED);
        return;
    }

    // Change directory via syscall
    int result = sys_chdir(path);

    if (result == 0) {
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

    // Create directory via syscall
    int result = sys_mkdir(path);

    if (result == 0) {
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

    // Remove directory via syscall
    int result = sys_rmdir(path);

    if (result == 0) {
        console_print_colored("Directory removed.\n", COLOR_GREEN_ON_BLACK);
    } else {
        console_print_colored("rmdir: Failed (is it empty?).\n", COLOR_LIGHT_RED);
    }
}

// --- NEW: cat command (display file contents) ---
void cmd_cat(char* filename) {
    if (strlen(filename) == 0) {
        console_print_colored("Usage: cat <filename>\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Open file via syscall
    int fd = sys_open(filename, O_RDONLY);

    if (fd < 0) {
        console_print_colored("cat: Cannot open file '", COLOR_LIGHT_RED);
        console_print(filename);
        console_print("'\n");
        return;
    }

    // Read file contents
    char buffer[401];
    int bytes_read = sys_read(fd, buffer, 400);

    if (bytes_read < 0) {
        console_print_colored("cat: Error reading file\n", COLOR_LIGHT_RED);
    } else if (bytes_read == 0) {
        console_print_colored("(empty file)\n", COLOR_YELLOW_ON_BLACK);
    } else {
        buffer[bytes_read] = '\0';
        console_print(buffer);
        if (buffer[bytes_read - 1] != '\n') {
            console_print("\n");
        }
    }

    sys_close(fd);
}

// --- NEW: touch command (create empty file) ---
void cmd_touch(char* filename) {
    if (strlen(filename) == 0) {
        console_print_colored("Usage: touch <filename>\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Create file via syscall
    int result = sys_create_file(filename);

    if (result == 0) {
        console_print_colored("File created: ", COLOR_GREEN_ON_BLACK);
        console_print(filename);
        console_print("\n");
    } else {
        console_print_colored("touch: Failed to create file\n", COLOR_LIGHT_RED);
    }
}

// --- NEW: echo command (write text to file) ---
void cmd_echo(char* args) {
    if (strlen(args) == 0) {
        console_print("\n");
        return;
    }

    // Parse: echo text [filename]
    char text[128];
    char filename[64];

    int i = 0, j = 0;

    // Get text (everything until last space)
    int last_space = -1;
    for (int k = 0; args[k] != '\0'; k++) {
        if (args[k] == ' ') last_space = k;
    }

    if (last_space == -1) {
        // No filename - just print to console
        console_print(args);
        console_print("\n");
        return;
    }

    // Extract text
    for (i = 0; i < last_space; i++) {
        text[i] = args[i];
    }
    text[i] = '\0';

    // Extract filename
    i = last_space + 1;
    j = 0;
    while (args[i] != '\0') {
        filename[j++] = args[i++];
    }
    filename[j] = '\0';

    if (strlen(filename) == 0) {
        console_print(text);
        console_print("\n");
        return;
    }

    // Try to open file
    int fd = sys_open(filename, O_WRONLY);

    if (fd < 0) {
        // File doesn't exist, create it
        sys_create_file(filename);
        fd = sys_open(filename, O_WRONLY);
    }

    if (fd < 0) {
        console_print_colored("echo: Cannot write to file\n", COLOR_LIGHT_RED);
        return;
    }

    // Write text
    sys_write(fd, text, strlen(text));
    sys_write(fd, "\n", 1);

    sys_close(fd);

    console_print_colored("Text written to ", COLOR_GREEN_ON_BLACK);
    console_print(filename);
    console_print("\n");
}

// --- NEW: cp command (copy file) ---
void cmd_cp(char* args) {
    char source[64];
    char dest[64];

    int i = 0, j = 0;

    // Parse source
    while (args[i] && args[i] != ' ') {
        source[j++] = args[i++];
    }
    source[j] = '\0';

    if (args[i] == ' ') i++;

    // Parse destination
    j = 0;
    while (args[i]) {
        dest[j++] = args[i++];
    }
    dest[j] = '\0';

    if (strlen(source) == 0 || strlen(dest) == 0) {
        console_print_colored("Usage: cp <source> <dest>\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Open source
    int fd_src = sys_open(source, O_RDONLY);
    if (fd_src < 0) {
        console_print_colored("cp: Cannot open source file\n", COLOR_LIGHT_RED);
        return;
    }

    // Read source
    char buffer[401];
    int bytes_read = sys_read(fd_src, buffer, 400);
    sys_close(fd_src);

    if (bytes_read < 0) {
        console_print_colored("cp: Error reading source file\n", COLOR_LIGHT_RED);
        return;
    }

    // Create destination
    sys_create_file(dest);

    // Open destination
    int fd_dest = sys_open(dest, O_WRONLY);
    if (fd_dest < 0) {
        console_print_colored("cp: Cannot create destination file\n", COLOR_LIGHT_RED);
        return;
    }

    // Write to destination
    int bytes_written = sys_write(fd_dest, buffer, bytes_read);
    sys_close(fd_dest);

    console_print_colored("Copied ", COLOR_GREEN_ON_BLACK);
    char num[12];
    int_to_str(bytes_written, num);
    console_print(num);
    console_print(" bytes\n");
}

// --- Standard Shell Commands ---

void cmd_add(char* args) {
    // Check if in /a directory
    char cwd[128];
    sys_getcwd(cwd, 128);

    if (strcmp(cwd, "/a") != 0) {
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

    console_print(last_result);
    console_print("\n");

    // Check if save mode
    if (args && strlen(args) > 0 && args[0] == 's') {
        console_print_colored("Saving result to disk...\n", COLOR_YELLOW_ON_BLACK);

        // Create/open results.txt via syscall
        int fd = sys_open("results.txt", O_WRONLY);
        if (fd < 0) {
            sys_create_file("results.txt");
            fd = sys_open("results.txt", O_WRONLY);
        }

        if (fd >= 0) {
            sys_write(fd, last_result, strlen(last_result));
            sys_write(fd, "\n", 1);
            sys_close(fd);
            console_print_colored("Result saved to results.txt\n", COLOR_GREEN_ON_BLACK);
        } else {
            console_print_colored("Error: Could not save result\n", COLOR_LIGHT_RED);
        }
    }
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
        sys_chdir("/");  // Go to root via syscall
    } else {
        console_print_colored("root access denied\n", COLOR_YELLOW_ON_BLACK);
    }
}

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

    // Get disk stats via kernel function (not syscall - informational only)
    uint32_t total_disk_kb, used_disk_kb, free_disk_kb;
    fs_get_disk_stats(&total_disk_kb, &used_disk_kb, &free_disk_kb);

    console_print("Total Disk: "); int_to_str(total_disk_kb, num); console_print(num); console_print(" KB\n");
    console_print("Used Disk:  "); int_to_str(used_disk_kb, num); console_print(num); console_print(" KB\n");
    console_print("Free Disk:  "); int_to_str(free_disk_kb, num); console_print(num); console_print(" KB\n");

    console_print("\n");
    console_print_colored("=== Filesystem Cache ===\n", COLOR_GREEN_ON_BLACK);

    // Get cache stats
    uint32_t cache_size, cached_nodes, dirty_nodes;
    fs_get_cache_stats(&cache_size, &cached_nodes, &dirty_nodes);

    console_print("Cache Size:    "); int_to_str(cache_size, num); console_print(num); console_print(" slots\n");
    console_print("Cached Nodes:  "); int_to_str(cached_nodes, num); console_print(num); console_print("\n");
    console_print("Dirty Nodes:   "); int_to_str(dirty_nodes, num); console_print(num); console_print(" (pending write)\n");

    uint32_t cache_usage = (cached_nodes * 100) / cache_size;
    console_print("Cache Usage:   "); int_to_str(cache_usage, num); console_print(num); console_print("%\n");
}

void cmd_sysinfo() {
    console_print_colored("=== PUNIX System Information ===\n", COLOR_GREEN_ON_BLACK);
    console_print("\n");

    // Try to read version from /boot/version via syscall
    int fd = sys_open("/boot/version", O_RDONLY);
    if (fd >= 0) {
        char buffer[128];
        int bytes = sys_read(fd, buffer, 127);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            console_print(buffer);
        }
        sys_close(fd);
        console_print("\n");
    } else {
        console_print("PUNIX Kernel v1.04\n\n");
    }

    // Disk layout info
    console_print_colored("Disk Layout:\n", COLOR_YELLOW_ON_BLACK);
    console_print("  Sector 0:       Bootloader (512 bytes)\n");
    console_print("  Sectors 1-60:   Kernel binary (~30 KB)\n");
    console_print("  Sector 61:      Filesystem superblock\n");
    console_print("  Sectors 62+:    Filesystem data\n");
    console_print("\n");

    console_print_colored("Current User: ", COLOR_YELLOW_ON_BLACK);
    console_print(USERNAME);
    console_print("\n");
}

void cmd_motd() {
    int fd = sys_open("/etc/motd", O_RDONLY);
    if (fd < 0) {
        console_print_colored("No message of the day.\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    char buffer[256];
    int bytes = sys_read(fd, buffer, 255);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        console_print_colored(buffer, COLOR_GREEN_ON_BLACK);
    }
    sys_close(fd);
}

void cmd_sudo(char* args) {
    if (strlen(args) == 0) {
        console_print_colored("Usage: sudo <command>\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Parse command
    char cmd[40];
    char cmd_args[40];
    int i = 0;

    while (args[i] && args[i] != ' ') {
        cmd[i] = args[i];
        i++;
    }
    cmd[i] = '\0';

    int j = 0;
    if (args[i] == ' ') {
        i++;
        while (args[i] == ' ') i++;
        while (args[i]) {
            cmd_args[j++] = args[i++];
        }
    }
    cmd_args[j] = '\0';

    // Check if privileged command
    if (strcmp(cmd, "shutdown") != 0 &&
        strcmp(cmd, "chuser") != 0 &&
        strcmp(cmd, "chpasswd") != 0) {
        console_print_colored("sudo: only 'shutdown', 'chuser', and 'chpasswd' are supported\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    // Ask for password
    char pass[MAX_PASSWORD_LEN];
    console_print_colored("[sudo] password for ", COLOR_YELLOW_ON_BLACK);
    console_print(current_user);
    console_print(": ");
    read_line_with_display(pass, MAX_PASSWORD_LEN);

    if (strcmp(pass, ROOT_PASSWORD) == 0) {
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

void cmd_shutdown() {
    if (!ROOT_ACCESS_GRANTED) {
        console_print_colored("shutdown: permission denied (try 'sudo shutdown')\n", COLOR_LIGHT_RED);
        return;
    }

    console_clear_screen();
    console_print_colored("SHUTTING DOWN SYSTEM...\n", COLOR_LIGHT_RED);
    console_print_colored("Goodbye!\n", COLOR_GREEN_ON_BLACK);

    // QEMU shutdown
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x604));
    while(1) __asm__ volatile("hlt");
}

void cmd_chuser() {
    if (!ROOT_ACCESS_GRANTED) {
        console_print_colored("chuser: permission denied (try 'sudo chuser')\n", COLOR_LIGHT_RED);
        return;
    }
    auth_change_username(read_line_with_display);
}

void cmd_chpasswd() {
    if (!ROOT_ACCESS_GRANTED) {
        console_print_colored("chpasswd: permission denied (try 'sudo chpasswd')\n", COLOR_LIGHT_RED);
        return;
    }
    auth_change_password(read_line_with_display);
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
    console_print("  cat [file]    - Display file contents\n");
    console_print("  touch [file]  - Create empty file\n");
    console_print("  echo [text] [file] - Write text to file\n");
    console_print("  cp [src] [dst] - Copy file\n");
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

void cmd_exit() {
    if (ROOT_ACCESS_GRANTED) {
        ROOT_ACCESS_GRANTED = 0;
        console_print_colored("Exited root mode\n", COLOR_GREEN_ON_BLACK);
        sys_chdir("/a");  // Return to /a via syscall
    } else {
        console_print_colored("Not in root mode. Use 'shutdown' to power off.\n", COLOR_YELLOW_ON_BLACK);
    }
}

// History Functions (Stubs)
void history_save() {
    console_print_colored("History save not available.\n", COLOR_YELLOW_ON_BLACK);
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

        // Command routing using syscalls
        if (strcmp(cmd, "ls") == 0) cmd_ls();
        else if (strcmp(cmd, "pwd") == 0) cmd_pwd();
        else if (strcmp(cmd, "cd") == 0) cmd_cd(args);
        else if (strcmp(cmd, "mkdir") == 0) cmd_mkdir(args);
        else if (strcmp(cmd, "rmdir") == 0) cmd_rmdir(args);
        else if (strcmp(cmd, "cat") == 0) cmd_cat(args);
        else if (strcmp(cmd, "touch") == 0) cmd_touch(args);
        else if (strcmp(cmd, "echo") == 0) cmd_echo(args);
        else if (strcmp(cmd, "cp") == 0) cmd_cp(args);
        else if (strcmp(cmd, "help") == 0) cmd_help();
        else if (strcmp(cmd, "clear") == 0) cmd_clear();
        else if (strcmp(cmd, "mem") == 0) cmd_mem();
        else if (strcmp(cmd, "sysinfo") == 0) cmd_sysinfo();
        else if (strcmp(cmd, "motd") == 0) cmd_motd();
        else if (strcmp(cmd, "root") == 0) cmd_su();
        else if (strcmp(cmd, "exit") == 0) cmd_exit();
        else if (strcmp(cmd, "add") == 0) cmd_add(args);
        else if (strcmp(cmd, "text") == 0) text_editor(args);
        else if (strcmp(cmd, "sudo") == 0) cmd_sudo(args);
        else if (strcmp(cmd, "shutdown") == 0) cmd_shutdown();
        else if (strcmp(cmd, "sync") == 0) fs_sync();
        else if (strcmp(cmd, "chuser") == 0) cmd_chuser();
        else if (strcmp(cmd, "chpasswd") == 0) cmd_chpasswd();
        else {
            console_print(cmd);
            console_print_colored(": command not found\n", COLOR_YELLOW_ON_BLACK);
        }
    }
}
