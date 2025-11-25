// kernel.c - Main kernel entry point
#include "include/types.h"
#include "include/vga.h"
#include "include/memory.h"
#include "include/interrupt.h"
#include "include/shell.h"
#include "include/text.h"
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
    vga_init();
    vga_clear_screen();
    vga_print_colored("[*] ", COLOR_GREEN_ON_BLACK);
    vga_print_colored("Initializing kernel...\n", COLOR_GREEN_ON_BLACK);
 for (volatile int i = 0; i < 100000000; i++);
    // Initialize memory
  vga_print_colored("[*] ", COLOR_GREEN_ON_BLACK);
    vga_print_colored("Setting up memory manager...\n", COLOR_YELLOW_ON_BLACK);
    pmm_init();
    heap_init();
 for (volatile int i = 0; i < 100000000; i++);
    // Test memory
    void* test_page = pmm_alloc_page();
    if (test_page) {
        pmm_free_page(test_page);
         vga_print_colored("[*] ", COLOR_GREEN_ON_BLACK);
        vga_print_colored("Memory manager ready!\n", COLOR_GREEN_ON_BLACK);
         for (volatile int i = 0; i < 100000000; i++);
    }

    // Initialize interrupts
     vga_print_colored("[*] ", COLOR_GREEN_ON_BLACK);
    vga_print_colored("Setting up IDT...\n", COLOR_YELLOW_ON_BLACK);
    idt_init();
 for (volatile int i = 0; i < 100000000; i++);
  vga_print_colored("[*] ", COLOR_GREEN_ON_BLACK);
    vga_print_colored("Configuring PIC...\n", COLOR_YELLOW_ON_BLACK);
    pic_init();
 for (volatile int i = 0; i < 100000000; i++);
  vga_print_colored("[*] ", COLOR_GREEN_ON_BLACK);
    vga_print_colored("Enabling interrupts...\n", COLOR_YELLOW_ON_BLACK);
    __asm__ volatile("sti");
 for (volatile int i = 0; i < 100000000; i++);
  vga_print_colored("[*] ", COLOR_GREEN_ON_BLACK);
    vga_print_colored("Kernel ready!\n\n", COLOR_GREEN_ON_BLACK);

    // --- UPDATED BOOT DELAY ---
    // Pause briefly so the user can see the boot logs before the screen clears.
    // The previous loop count was 10 million (10000000). I'm increasing it slightly
    // to ensure visibility, as execution speed can vary greatly.
    for (volatile int i = 0; i < 100000000; i++);
    // --- END UPDATED BOOT DELAY ---

    // Start shell
    vga_clear_screen();
    shell_init();
    shell_run();

    // Should never reach here
    while(1) {
        __asm__ volatile("hlt");
    }
}
