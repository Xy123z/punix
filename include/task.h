#ifndef TASK_H
#define TASK_H

#include "types.h"
#include "paging.h"

#define KERNEL_STACK_SIZE 8192

/**
 * @brief Process register state (saved during interrupts)
 */
typedef struct {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t eip, eflags;
    uint32_t cs, ss, ds, es, fs, gs;
} registers_t;

/**
 * @brief Task structure (Minimal Process Control Block)
 */
typedef struct task {
    uint32_t id;
    uint32_t uid;
    uint32_t gid;
    page_directory_t* page_directory;
    uint32_t kernel_stack;
    uint32_t user_stack_top;
    registers_t regs;
    struct task* next;
} task_t;

extern task_t* current_task;

void task_init();
void enter_user_mode(uint32_t target_eip, uint32_t target_esp);

#endif
