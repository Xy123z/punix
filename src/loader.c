#include "../include/loader.h"
#include "../include/console.h"
#include "../include/fs.h"
#include "../include/memory.h"
#include "../include/paging.h"
#include "../include/string.h"
#include "../include/task.h"

/**
 * @brief Loads a flat binary program from disk sectors into user memory.
 * Addresses architectural faults:
 * 1. Prevents kernel overwrite by specific mapping.
 * 2. Handles multi-page programs safely.
 * 3. Clears memory to prevent data leaks.
 */
void load_user_program(char* path) {
    console_print("LOADING EXTERNAL PROGRAM: ");
    console_print(path);
    console_print("\n");

    // 1. Find the file
    fs_node_t* node = fs_find_node(path, fs_root_id);
    if (!node) {
        console_print_colored("FATAL: File not found!\n", COLOR_LIGHT_RED);
        return;
    }

    uint32_t user_virt_base = 0x00400000;
    uint32_t total_bytes = node->size;
    uint32_t pages_needed = (total_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    // 2. Allocate and Map Pages
    for (uint32_t i = 0; i < pages_needed; i++) {
        void* phys = pmm_alloc_page();
        if (!phys) {
            console_print_colored("FATAL: Out of memory during load!\n", COLOR_LIGHT_RED);
            return;
        }
        memset(phys, 0, PAGE_SIZE); // Security: Clear memory
        
        // Map the page to user space (RW)
        paging_map_page(current_page_directory, user_virt_base + (i * PAGE_SIZE),
                        (uint32_t)phys, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    }

    // 3. Read file content directly into user memory
    // Since we just mapped the pages in the CURRENT directory, we can write to them.
    int bytes_read = fs_read(node, 0, node->size, (uint8_t*)user_virt_base);
    if (bytes_read != node->size) {
        console_print_colored("FATAL: Failed to read complete file!\n", COLOR_LIGHT_RED);
        return;
    }

    // 2. Map VGA Buffer (User-reachable for console output)
    // Map only the necessary range instead of the whole first 4MB
    paging_map_range(current_page_directory, 0xB8000, 0xB8000, 0x8000,
                    PAGE_PRESENT | PAGE_RW | PAGE_USER);

    // 3. Allocate and map user stack (High memory - 0xB0000000)
    void* user_stack_phys = pmm_alloc_page();
    uint32_t user_stack_ext = 0xB0000000;
    paging_map_page(current_page_directory, user_stack_ext, (uint32_t)user_stack_phys,
                    PAGE_PRESENT | PAGE_RW | PAGE_USER);

    // 4. Jump to entry point!
    console_print("SWITCHING TO USER MODE...\n");
    enter_user_mode(user_virt_base, user_stack_ext + PAGE_SIZE);
}
