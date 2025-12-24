// kernel.c - Main kernel entry point
#include "include/types.h"
#include "include/vga.h"
#include "include/memory.h"
#include "include/paging.h"
#include "include/interrupt.h"
#include "include/shell.h"
#include "include/text.h"
#include "include/fs.h"
#include "include/mouse.h"
#include "include/string.h"
#include "include/console.h"
#include "include/ata.h"
#include "include/auth.h"
#include "include/gdt.h"
#include "include/task.h"
#include "include/syscall.h"

void kernel_main() {
    // Initialize VGA
    console_init();
    console_clear_screen();
    
    // 1. Core CPU Architecture
    gdt_init();
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("GDT and TSS initialized.\n", COLOR_GREEN_ON_BLACK);

    // 2. Memory Management (Identity mapping environment)
    pmm_init();
    heap_init();
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Memory manager initialized.\n", COLOR_GREEN_ON_BLACK);

    // 3. System Call & Task Management (Requires Heap)
    syscall_init();
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("System calls and initial task ready.\n", COLOR_GREEN_ON_BLACK);

    // 4. Paging Setup
    paging_init();
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Page tables prepared.\n", COLOR_GREEN_ON_BLACK);

    // 5. Interrupts (MUST be before paging_enable to catch faults)
    idt_init();
    pic_init();
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("IDT and PIC configured.\n", COLOR_GREEN_ON_BLACK);

    // 6. Enable Paging
    paging_enable();
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Paging enabled.\n", COLOR_GREEN_ON_BLACK);

    // 7. Hardware & Filesystem
    ata_init();
    fs_init();
    syscall_set_cwd(fs_current_dir_id);
    mouse_init();
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Hardware and Filesystem ready.\n", COLOR_GREEN_ON_BLACK);

    // 8. Finalize Kernel Space
    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Enabling interrupts...\n", COLOR_YELLOW_ON_BLACK);
    __asm__ volatile("sti");

    console_print_colored("[ ok ] ", COLOR_GREEN_ON_BLACK);
    console_print_colored("Kernel ready!\n\n", COLOR_GREEN_ON_BLACK);

    // Authentication setup (replaces old manual password entry)
    // auth_init(read_line_with_display);
    console_print("launching authentication system\n");
    auth_init(read_line_with_display);

    // Note: fs_init() already set the working directory to /a
    // No need to do it again here

    // Boot delay
    for (volatile int i = 0; i < 100000000; i++);

    // Start shell
    console_print("DEBUG: Clearing screen...\n");
    console_clear_screen();
    console_print("DEBUG: shell_init()...\n");
    // Start shell in User Mode (Ring 3)
    console_print("DEBUG: Switching to User Mode (Ring 3)...\n");
    
    // Allocate a user stack
    void* user_stack_phys = pmm_alloc_page();
    uint32_t user_stack_ext = 0xB0000000; // Arbitrary user stack address
    paging_map_page(current_page_directory, user_stack_ext, (uint32_t)user_stack_phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    
    // Ensure the shell code/data (currently part of kernel) is user-reachable
    // For now, we'll mark the first 4MB as user-reachable so the shell can run.
    paging_map_range(current_page_directory, 0x0, 0x0, 0x400000, PAGE_PRESENT | PAGE_RW | PAGE_USER);

    enter_user_mode((uint32_t)shell_run, user_stack_ext + PAGE_SIZE);

    // Should never reach here
    while(1) {
        __asm__ volatile("hlt");
    }
}

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

    kernel_main();
}
