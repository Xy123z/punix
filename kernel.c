// kernel.c - Main kernel entry point
#include "include/types.h"
#include "include/vga.h"
#include "include/memory.h"
#include "include/interrupt.h"
#include "include/shell.h"
#include "include/text.h"
#include "include/fs.h"
#include "include/mouse.h"
#include "include/string.h"
#include "include/console.h"
void _start() {
    // Test marker
    char* vga = (char*)0xB8000;
    vga[0] = 'T';
    vga[1] = 0x0F;
    vga[2] = 'E';
    vga[3] = 0x0F;
    vga[4] = 'S';
    vga[5] = 0x0F;
    vga[6] = 'T';
    vga[7] = 0x0F;

    void kernel_main();
    void kernel_main() __attribute__((section(".text.startup")));
}

void kernel_main() {
    // Initialize VGA
    console_init();
    console_clear_screen();
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Initializing kernel...\n", COLOR_GREEN_ON_BLACK);
 for (volatile int i = 0; i < 100000000; i++);
    // Initialize memory
  console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Setting up memory manager...\n", COLOR_YELLOW_ON_BLACK);
    pmm_init();
    heap_init();
    fs_init();
 for (volatile int i = 0; i < 100000000; i++);
    // Test memory
    void* test_page = pmm_alloc_page();
    if (test_page) {
        pmm_free_page(test_page);
         console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
        console_print_colored("Memory manager ready!\n", COLOR_GREEN_ON_BLACK);
         for (volatile int i = 0; i < 100000000; i++);
    }

    // Initialize interrupts
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Setting up IDT...\n", COLOR_YELLOW_ON_BLACK);
    idt_init();
 for (volatile int i = 0; i < 100000000; i++);
  console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Configuring PIC...\n", COLOR_YELLOW_ON_BLACK);
    pic_init();
    for (volatile int i = 0; i < 100000000; i++);
  console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Configuring mouse driver...\n", COLOR_YELLOW_ON_BLACK);
    mouse_init();
 for (volatile int i = 0; i < 100000000; i++);
  console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Enabling interrupts...\n", COLOR_YELLOW_ON_BLACK);
    __asm__ volatile("sti");
 for (volatile int i = 0; i < 100000000; i++);
  console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Kernel ready!\n\n", COLOR_GREEN_ON_BLACK);

    // Security setup
    char pass[MAX_PASSWORD_LEN];
    char pass_conf[MAX_PASSWORD_LEN];
    char user[MAX_USERNAME_LEN];
     console_print_colored("enter username(within 39 characters): ", COLOR_GREEN_ON_BLACK);
     read_line_with_display(user,MAX_USERNAME_LEN);
     strcpy(USERNAME,user);
    do{
    console_print_colored("enter root password(within 39 characters): ", COLOR_GREEN_ON_BLACK);
    read_line_with_display(pass,MAX_PASSWORD_LEN);
     console_print_colored("confirm password: ", COLOR_GREEN_ON_BLACK);
    read_line_with_display(pass_conf,MAX_PASSWORD_LEN);
    if(strcmp(pass,pass_conf) == 0){
      strcpy(ROOT_PASSWORD, pass_conf);
      break;
    }
    console_print_colored("Passwords didn't match, please try again.\n", COLOR_YELLOW_ON_BLACK);
    }
    while(strcmp(pass,pass_conf));

    // --- NEW: Change initial directory to /a (the 'home' directory) ---
    if (fs_change_dir("a") == 0) {
        console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
        console_print_colored("Initial working directory set to /a.\n", COLOR_GREEN_ON_BLACK);
    } else {
        // Fallback or error message if /a wasn't created, though it should be.
        console_print_colored("[warn] ", COLOR_YELLOW_ON_BLACK);
        console_print_colored("Could not set initial working directory to /a, starting in /.\n", COLOR_YELLOW_ON_BLACK);
    }
    // -------------------------------------------------------------------

    // --- UPDATED BOOT DELAY ---
    for (volatile int i = 0; i < 100000000; i++);
    // --- END UPDATED BOOT DELAY ---

    // Start shell
    console_clear_screen();
    shell_init();
    shell_run();

    // Should never reach here
    while(1) {
        __asm__ volatile("hlt");
    }
}
