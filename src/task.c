#include "../include/task.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/gdt.h"
#include "../include/paging.h"

task_t* current_task = 0;

/**
 * @brief Initialize the first task (the kernel process)
 */
void task_init() {
    current_task = (task_t*)kmalloc(sizeof(task_t));
    memset(current_task, 0, sizeof(task_t));

    current_task->id = 1;
    current_task->uid = 0; // Root by default
    current_task->gid = 0;
    current_task->page_directory = current_page_directory;
    current_task->kernel_stack = 0x90000; // Original kernel stack
    current_task->next = 0;

    // Set the TSS kernel stack to the end of our kernel stack
    tss_set_stack(current_task->kernel_stack);
}
