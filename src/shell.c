// src/shell.c - Shell implementation
#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/memory.h"
#include "../include/interrupt.h"
#include "../include/string.h"
#include "../include/text.h"
// Directory system
int current_directory = 1; // 0=/, 1=/a, 2=/h

// History storage (dynamic)
static char** history = 0;
static int history_count = 0;
static int history_capacity = 0;
static char last_result[MAX_RESULT_LEN] = "";
static int has_result = 0;

// Constants
static const char* current_user = "root";
static const char* kernel_name = "simplekernel";

// Helper: Read line with visual feedback
static void read_line_with_display(char* buffer, int max_len) {
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
                   c == ' ' || c == '.') {
            buffer[i++] = c;
            vga_putchar(c, COLOR_WHITE_ON_BLACK);
        }
    }

    buffer[i] = '\0';
}

// Show prompt
static void show_prompt() {
    vga_print_colored(current_user, COLOR_GREEN_ON_BLACK);
    vga_print_colored("@", COLOR_WHITE_ON_BLACK);
    vga_print_colored(kernel_name, COLOR_GREEN_ON_BLACK);
    vga_print_colored(":", COLOR_WHITE_ON_BLACK);

    if (current_directory == 0) {
        vga_print_colored("/", COLOR_YELLOW_ON_BLACK);
    } else if (current_directory == 1) {
        vga_print_colored("~", COLOR_YELLOW_ON_BLACK);
    } else {
        vga_print_colored("/h", COLOR_YELLOW_ON_BLACK);
    }

    vga_print_colored("# ", COLOR_WHITE_ON_BLACK);
}

void shell_init() {
    vga_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    vga_print_colored("|         SIMPLE KERNEL - CALCULATOR            |\n", COLOR_GREEN_ON_BLACK);
    vga_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    vga_print("\n");
}

// Command implementations
void cmd_pwd() {
    if (current_directory == 0) {
        vga_print_colored("/\n", COLOR_GREEN_ON_BLACK);
    } else if (current_directory == 1) {
        vga_print_colored("/a\n", COLOR_GREEN_ON_BLACK);
    } else {
        vga_print_colored("/h\n", COLOR_GREEN_ON_BLACK);
    }
}

void cmd_ls() {
    if (current_directory == 0) {
        vga_print_colored("a/\n", COLOR_YELLOW_ON_BLACK);
        vga_print_colored("h/\n", COLOR_YELLOW_ON_BLACK);
    } else if (current_directory == 2) {
        history_show();
    } else {
        vga_print_colored("ls: cannot list directory\n", COLOR_YELLOW_ON_BLACK);
    }
}

void cmd_cd(char* dir) {
    if (strcmp(dir, "..") == 0) {
        if (current_directory != 0) {
            current_directory = 0;
            vga_print_colored("Moved to root directory\n", COLOR_GREEN_ON_BLACK);
        }
    } else if (strcmp(dir, "/") == 0) {
        current_directory = 0;
        vga_print_colored("Moved to root directory\n", COLOR_GREEN_ON_BLACK);
    } else if (strcmp(dir, "a") == 0) {
        current_directory = 1;
        vga_print_colored("Moved to addition directory\n", COLOR_GREEN_ON_BLACK);
    } else if (strcmp(dir, "h") == 0) {
        current_directory = 2;
        vga_print_colored("Moved to history directory\n", COLOR_GREEN_ON_BLACK);
    } else {
        vga_print_colored("cd: no such directory\n", COLOR_YELLOW_ON_BLACK);
    }
}

void cmd_add() {
    if (current_directory != 1) {
        vga_print_colored("Mount /a for executing this command\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    char input[40];

    vga_print_colored("Enter first number: ", COLOR_YELLOW_ON_BLACK);
    read_line_with_display(input, 40);
    int num1 = str_to_int(input);

    vga_print_colored("Enter second number: ", COLOR_YELLOW_ON_BLACK);
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

    vga_print(last_result);
    vga_print("\n");
}

void cmd_mem() {
    vga_print_colored("=== Memory Statistics ===\n", COLOR_GREEN_ON_BLACK);

    uint32_t total, used, free;
    pmm_get_stats(&total, &used, &free);

    uint32_t total_kb = (total * PAGE_SIZE) / 1024;
    uint32_t used_kb = (used * PAGE_SIZE) / 1024;
    uint32_t free_kb = (free * PAGE_SIZE) / 1024;

    vga_print("Total memory: ");
    char num[16];
    int_to_str(total_kb, num);
    vga_print(num);
    vga_print(" KB\n");

    vga_print("Used memory:  ");
    int_to_str(used_kb, num);
    vga_print(num);
    vga_print(" KB\n");

    vga_print("Free memory:  ");
    int_to_str(free_kb, num);
    vga_print(num);
    vga_print(" KB\n");

    uint32_t percent_used = total > 0 ? (used * 100) / total : 0;
    vga_print("Usage: ");
    int_to_str(percent_used, num);
    vga_print(num);
    vga_print("%\n");
}

void cmd_help() {
    vga_clear_screen();
    vga_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    vga_print_colored("|   CALCULATOR KERNEL - Command Reference       |\n", COLOR_GREEN_ON_BLACK);
    vga_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    vga_print("\n");

    if (current_directory == 0) {
        vga_print_colored("Root Directory Commands:\n", COLOR_YELLOW_ON_BLACK);
        vga_print("  ls         - List directories\n");
        vga_print("  cd [dir]   - Change directory (a or h)\n");
        vga_print("  pwd        - Print working directory\n");
        vga_print("  mem        - Show memory statistics\n");
        vga_print("  clear      - Clear screen\n");
        vga_print("  exit       - Quit (shutdown)\n");
        vga_print("  help       - Show this help\n");
    } else if (current_directory == 1) {
        vga_print_colored("Addition Directory (/a) Commands:\n", COLOR_YELLOW_ON_BLACK);
        vga_print("  add        - Add two numbers\n");
        vga_print("  s          - Save last result to history\n");
        vga_print("  cd [dir]   - Change directory (/ or h)\n");
        vga_print("  cd ..      - Go to parent directory (/)\n");
        vga_print("  pwd        - Print working directory\n");
        vga_print("  mem        - Show memory statistics\n");
        vga_print("  clear      - Clear screen\n");
        vga_print("  exit       - Quit (shutdown)\n");
        vga_print("  help       - Show this help\n");
        vga_print("  text       - use simple text editor\n");
    } else {
        vga_print_colored("History Directory (/h) Commands:\n", COLOR_YELLOW_ON_BLACK);
        vga_print("  ls         - List saved calculations\n");
        vga_print("  del [n]    - Delete history entry number n\n");
        vga_print("  cd [dir]   - Change directory (/ or a)\n");
        vga_print("  cd ..      - Go to parent directory (/)\n");
        vga_print("  pwd        - Print working directory\n");
        vga_print("  mem        - Show memory statistics\n");
        vga_print("  clear      - Clear screen\n");
        vga_print("  exit       - Quit (shutdown)\n");
        vga_print("  help       - Show this help\n");
    }

    vga_print("\nPress any key to continue...");
    keyboard_read();
    vga_clear_screen();
    shell_init();
}

void cmd_clear() {
    vga_clear_screen();
    shell_init();
}

void cmd_exit() {
    vga_clear_screen();
    vga_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    vga_print_colored("|              SHUTTING DOWN...                  |\n", COLOR_GREEN_ON_BLACK);
    vga_print_colored("+================================================+\n", COLOR_GREEN_ON_BLACK);
    vga_print("\n");
    vga_print("Thank you for using Calculator Kernel!\n");
    vga_print("Goodbye!\n\n");
    vga_print("It is now safe to close this window.\n");

    // QEMU shutdown
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x604));
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x605));

    __asm__ volatile("cli; hlt");
    while(1) __asm__ volatile("hlt");
}

// History functions
void history_save() {
    if (!has_result) {
        vga_print_colored("Nothing to save!\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    if (history_count >= history_capacity) {
        int new_capacity = history_capacity == 0 ? 10 : history_capacity * 2;
        if (new_capacity > MAX_HISTORY) new_capacity = MAX_HISTORY;

        char** new_history = (char**)kmalloc(new_capacity * sizeof(char*));

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
        int len = strlen(last_result);
        history[history_count] = (char*)kmalloc(len + 1);
        strcpy(history[history_count], last_result);

        history_count++;
        vga_print_colored("Saved to history! (Memory used)\n", COLOR_GREEN_ON_BLACK);
    } else {
        vga_print_colored("History full! (max 50 entries)\n", COLOR_YELLOW_ON_BLACK);
    }
}

void history_delete(int index) {
    if (index < 1 || index > history_count) {
        vga_print_colored("Invalid index for the history!\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    int array_index = index - 1;

    kfree(history[array_index]);

    for (int i = array_index; i < history_count - 1; i++) {
        history[i] = history[i + 1];
    }

    history_count--;
    vga_print_colored("History entry deleted! (Memory freed)\n", COLOR_GREEN_ON_BLACK);
}

void history_show() {
    if (history_count == 0) {
        vga_print_colored("No history yet!\n", COLOR_YELLOW_ON_BLACK);
        return;
    }

    vga_print_colored("=== Calculation History ===\n", COLOR_GREEN_ON_BLACK);
    for (int i = 0; i < history_count; i++) {
        char num[12];
        int_to_str(i + 1, num);
        vga_print(num);
        vga_print(". ");
        vga_print(history[i]);
        vga_print("\n");
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
        else if (strcmp(cmd, "add") == 0 || strcmp(cmd, "ADD") == 0) {
            cmd_add();
        }
        else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "LS") == 0) {
            cmd_ls();
        }
        else if (strcmp(cmd, "text") == 0 || strcmp(cmd, "TEXT") == 0) {
            text_editor(args);
        }
         else if (strcmp(cmd, "show") == 0 || strcmp(cmd, "SHOW") == 0) {
            show_files();
        }
        else if ((strcmp(cmd, "s") == 0 || strcmp(cmd, "S") == 0) && current_directory == 1) {
            history_save();
        }
        else if (strcmp(cmd, "del") == 0 || strcmp(cmd, "DEL") == 0) {
            if (current_directory == 2) {
                int index = str_to_int(args);
                history_delete(index);
            } else {
                vga_print(cmd);
                vga_print_colored(": command not found\n", COLOR_YELLOW_ON_BLACK);
            }
        }
        else {
            vga_print(cmd);
            vga_print_colored(": command not found\n", COLOR_YELLOW_ON_BLACK);
        }
    }
}
