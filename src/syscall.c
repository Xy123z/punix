/**
 * src/syscall.c - System Call Interface
 * Provides the bridge between user space and kernel space
 */

#include "../include/syscall.h"
#include "../include/types.h"
#include "../include/interrupt.h"
#include "../include/console.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/memory.h"


// File descriptor table (simplified - single process for now)
#define MAX_FDS 16
typedef struct {
    uint32_t node_id;        // Filesystem node ID
    uint32_t offset;         // Current read/write position
    uint8_t  flags;          // Open flags
    uint8_t  in_use;         // 1 if FD is allocated
} file_descriptor_t;

static file_descriptor_t fd_table[MAX_FDS];

// Current working directory (global for now)
static uint32_t current_cwd = 0;

/**
 * @brief Initialize file descriptor table
 */
void syscall_init() {
    // Clear FD table
    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i].in_use = 0;
    }

    // Set initial working directory to /a
    fs_node_t* dir_a = fs_find_node("a", fs_root_id);
    if (dir_a) {
        current_cwd = dir_a->id;
    } else {
        current_cwd = fs_root_id;
    }
}

/**
 * @brief Allocate a file descriptor
 */
static int allocate_fd(uint32_t node_id, uint8_t flags) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            fd_table[i].node_id = node_id;
            fd_table[i].offset = 0;
            fd_table[i].flags = flags;
            fd_table[i].in_use = 1;
            return i;
        }
    }
    return -1;  // No free FDs
}

/**
 * @brief Free a file descriptor
 */
static void free_fd(int fd) {
    if (fd >= 0 && fd < MAX_FDS) {
        fd_table[fd].in_use = 0;
    }
}

/**
 * @brief System call handler
 * Called when user code executes "int 0x80"
 *
 * Registers on entry:
 *   EAX = syscall number
 *   EBX = arg1
 *   ECX = arg2
 *   EDX = arg3
 *   ESI = arg4
 *   EDI = arg5
 *
 * Return value in EAX
 */
void syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx,
                     uint32_t edx, uint32_t esi, uint32_t edi) {
    uint32_t syscall_num = eax;
    uint32_t ret = 0;

    switch (syscall_num) {
        case SYS_PRINT: {
            // sys_print(const char* str)
            char* str = (char*)ebx;
            console_print(str);
            ret = 0;
            break;
        }

        case SYS_OPEN: {
            // sys_open(const char* path, int flags)
            char* path = (char*)ebx;
            uint8_t flags = (uint8_t)ecx;

            fs_node_t* node = fs_find_node(path, current_cwd);
            if (!node) {
                ret = -1;  // File not found
            } else {
                int fd = allocate_fd(node->id, flags);
                ret = fd;
            }
            break;
        }

        case SYS_READ: {
            // sys_read(int fd, void* buf, size_t count)
            int fd = (int)ebx;
            char* buf = (char*)ecx;
            uint32_t count = edx;

            if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
                ret = -1;
                break;
            }

            fs_node_t* node = fs_get_node(fd_table[fd].node_id);
            if (!node) {
                ret = -1;
                break;
            }

            // Read from file's padding area (simple implementation)
            uint32_t offset = fd_table[fd].offset;
            uint32_t bytes_to_read = count;
            if (offset + bytes_to_read > node->size) {
                bytes_to_read = node->size - offset;
            }

            // Copy data
            char* file_data = (char*)node->padding;
            for (uint32_t i = 0; i < bytes_to_read; i++) {
                buf[i] = file_data[offset + i];
            }

            fd_table[fd].offset += bytes_to_read;
            ret = bytes_to_read;
            break;
        }

        case SYS_WRITE: {
            // sys_write(int fd, const void* buf, size_t count)
            int fd = (int)ebx;
            const char* buf = (const char*)ecx;
            uint32_t count = edx;

            if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
                ret = -1;
                break;
            }

            fs_node_t* node = fs_get_node(fd_table[fd].node_id);
            if (!node) {
                ret = -1;
                break;
            }

            // Write to file's padding area
            uint32_t offset = fd_table[fd].offset;
            uint32_t bytes_to_write = count;
            if (offset + bytes_to_write > 400) {  // Max padding size
                bytes_to_write = 400 - offset;
            }

            char* file_data = (char*)node->padding;
            for (uint32_t i = 0; i < bytes_to_write; i++) {
                file_data[offset + i] = buf[i];
            }

            if (offset + bytes_to_write > node->size) {
                node->size = offset + bytes_to_write;
            }

            fd_table[fd].offset += bytes_to_write;
            fs_update_node(node);
            ret = bytes_to_write;
            break;
        }

        case SYS_CLOSE: {
            // sys_close(int fd)
            int fd = (int)ebx;
            free_fd(fd);
            ret = 0;
            break;
        }

        case SYS_GETCWD: {
            // sys_getcwd(char* buf, size_t size)
            char* buf = (char*)ebx;
            uint32_t size = ecx;

            // Build path string
            // For simplicity, just return the current directory name
            fs_node_t* cwd = fs_get_node(current_cwd);
            if (cwd) {
                if (current_cwd == fs_root_id) {
                    strcpy(buf, "/");
                } else {
                    strcpy(buf, "/");
                    strcat(buf, cwd->name);
                }
                ret = 0;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_CHDIR: {
            // sys_chdir(const char* path)
            char* path = (char*)ebx;

            fs_node_t* target = fs_find_node(path, current_cwd);
            if (target && target->type == FS_TYPE_DIRECTORY) {
                current_cwd = target->id;
                ret = 0;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_MKDIR: {
            // sys_mkdir(const char* path)
            char* path = (char*)ebx;

            // Parse parent directory and new dir name
            // Simplified: assume path is just the name in current directory
            if (fs_create_node(current_cwd, path, FS_TYPE_DIRECTORY)) {
                ret = 0;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_RMDIR: {
            // sys_rmdir(const char* path)
            char* path = (char*)ebx;

            fs_node_t* target = fs_find_node(path, current_cwd);
            if (target && target->type == FS_TYPE_DIRECTORY) {
                if (fs_delete_node(target->id)) {
                    ret = 0;
                } else {
                    ret = -1;
                }
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_CREATE_FILE: {
            // sys_create_file(const char* path)
            char* path = (char*)ebx;

            if (fs_create_node(current_cwd, path, FS_TYPE_FILE)) {
                ret = 0;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_GETDENTS: {
            // sys_getdents(const char* path, struct dirent* buf, int count)
            char* path = (char*)ebx;
            struct dirent* dirents = (struct dirent*)ecx;
            int max_count = (int)edx;

            fs_node_t* dir = fs_find_node(path, current_cwd);
            if (!dir || dir->type != FS_TYPE_DIRECTORY) {
                ret = -1;
                break;
            }

            // Copy directory entries
            int count = dir->child_count;
            if (count > max_count) count = max_count;

            for (int i = 0; i < count; i++) {
                uint32_t child_id = dir->child_ids[i];
                fs_node_t* child = fs_get_node(child_id);
                if (child) {
                    dirents[i].d_ino = child->id;
                    dirents[i].d_type = child->type;
                    strcpy(dirents[i].d_name, child->name);
                }
            }

            ret = count;
            break;
        }

        case SYS_MALLOC: {
            // sys_malloc(size_t size)
            uint32_t size = ebx;
            void* ptr = kmalloc(size);
            ret = (uint32_t)ptr;
            break;
        }

        case SYS_FREE: {
            // sys_free(void* ptr)
            void* ptr = (void*)ebx;
            kfree(ptr);
            ret = 0;
            break;
        }

        default:
            console_print("Unknown syscall: ");
            char num[12];
            int_to_str(syscall_num, num);
            console_print(num);
            console_print("\n");
            ret = -1;
            break;
    }

    // Return value is passed back in EAX
    // This will be handled by the assembly wrapper
    __asm__ volatile("mov %0, %%eax" : : "r"(ret));
}

// Assembly wrapper for system call interrupt
__asm__(
    ".global syscall_interrupt_wrapper\n"
    "syscall_interrupt_wrapper:\n"
    "   pusha\n"                    // Save all registers
    "   push %edi\n"                // Push args in reverse order
    "   push %esi\n"
    "   push %edx\n"
    "   push %ecx\n"
    "   push %ebx\n"
    "   push %eax\n"
    "   call syscall_handler\n"
    "   add $24, %esp\n"            // Clean up stack (6 args * 4 bytes)
    "   popa\n"                     // Restore registers (includes EAX with return value)
    "   iret\n"
);
