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
#include "../include/ata.h"

#include "../include/auth.h"
#include "../include/task.h"
uint32_t kernel_esp_saved;
extern void kernel_after_user(void);
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
__attribute__((noreturn))
void sys_exit_impl(uint32_t status) {
    console_print_colored("\nProcess exited with code: ", COLOR_LIGHT_CYAN);
    char status_str[12];
    int_to_str(status, status_str);
    console_print_colored(status_str, COLOR_LIGHT_CYAN);
    console_print("\nreturned to kernel context\n");

    __asm__ volatile(
         "mov %0, %%esp\n"
        "jmp kernel_after_user\n"
        :
        : "m"(kernel_esp_saved)
        : "memory"
    );

    __builtin_unreachable();
}
/**
 * @brief Initialize file descriptor table and task management
 */
void syscall_init() {
    // Clear FD table
    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i].in_use = 0;
    }
    
    // Initialize task management
    task_init();
    
    // Default to root if not set
    current_cwd = fs_root_id;
}

void syscall_set_cwd(uint32_t id) {
    current_cwd = id;
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
uint32_t syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx,
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

        case SYS_EXIT: {
            // sys_exit(int status)
           sys_exit_impl(ebx);
        }

        case SYS_GETCHAR: {
            ret = keyboard_read();
            break;
        }

        case SYS_PUTCHAR: {
            console_putchar((char)ebx, COLOR_WHITE_ON_BLACK);
            ret = 0;
            break;
        }

        case SYS_PRINT_COLORED: {
            console_print_colored((const char*)ebx, (uint8_t)ecx);
            ret = 0;
            break;
        }

        case SYS_CLEAR_SCREEN: {
            console_clear_screen();
            ret = 0;
            break;
        }

        case SYS_GET_DISK_STATS: {
            fs_get_disk_stats((uint32_t*)ebx, (uint32_t*)ecx, (uint32_t*)edx);
            ret = 0;
            break;
        }

        case SYS_GET_CACHE_STATS: {
            fs_get_cache_stats((uint32_t*)ebx, (uint32_t*)ecx, (uint32_t*)edx);
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

            uint32_t offset = fd_table[fd].offset;
            uint32_t bytes_read = fs_read(node, offset, count, (uint8_t*)buf);

            fd_table[fd].offset += bytes_read;
            ret = bytes_read;
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

            uint32_t offset = fd_table[fd].offset;
            uint32_t bytes_written = fs_write(node, offset, count, (uint8_t*)buf);

            fd_table[fd].offset += bytes_written;
            ret = bytes_written;
            break;
        }

        case SYS_CLOSE: {
            // sys_close(int fd)
            int fd = (int)ebx;
            free_fd(fd);
            ret = 0;
            break;
        }

        case SYS_SYNC: {
            fs_sync();
            ret = 0;
            break;
        }

        case SYS_CHUSER: {
            // sys_chuser(const char* username)
            if (current_task->uid != 0) {
                ret = -1; // Permission denied
                break;
            }
            const char* username = (const char*)ebx;
            auth_set_username(username);
            ret = 0;
            break;
        }

        case SYS_CHPASS: {
            // sys_chpass(const char* password)
            if (current_task->uid != 0) {
                ret = -1; // Permission denied
                break;
            }
            const char* password = (const char*)ebx;
            auth_set_password(password);
            ret = 0;
            break;
        }

        case SYS_GETUID: {
            ret = current_task->uid;
            break;
        }

        case SYS_SETUID: {
            // sys_setuid(uint32_t uid)
            // Only root can set uid to something else
            if (current_task->uid != 0) {
                ret = -1;
                break;
            }
            current_task->uid = ebx;
            ret = 0;
            break;
        }

        case SYS_AUTHENTICATE: {
            // sys_authenticate(const char* password)
            // Verifies password and sets uid to 0 on success
            extern char ROOT_PASSWORD[MAX_PASSWORD_LEN];
            const char* pass = (const char*)ebx;
            if (strcmp(pass, ROOT_PASSWORD) == 0) {
                current_task->uid = 0;
                ret = 0;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_SHUTDOWN: {
            console_print_colored("\nSHUTTING DOWN SYSTEM...\n", COLOR_YELLOW_ON_BLACK);
            
            // QEMU/Bochs ACPI Shutdown
            __asm__ volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
            
            // VirtualBox Shutdown (Modern ACPI)
            __asm__ volatile("outw %0, %1" : : "a"((uint16_t)0x3400), "Nd"((uint16_t)0x4004));
            
            // QEMU Debug Exit (if enabled)
            __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x0), "Nd"((uint16_t)0x501));

            // If all else fails, halt
            while(1) __asm__ volatile("cli; hlt");
            break;
        }

        case SYS_RESTART: {
            console_print_colored("\nRESTARTING SYSTEM...\n", COLOR_YELLOW_ON_BLACK);
            
            // 8042 Keyboard Controller Reset
            uint8_t good = 0x02;
            while (good & 0x02) {
                __asm__ volatile("inb $0x64, %0" : "=a"(good));
            }
            __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
            
            // Fallback: Triple Fault
            __asm__ volatile("lidt (%0)" : : "r" (0));
            __asm__ volatile("int $3");
            
            while(1) __asm__ volatile("cli; hlt");
            break;
        }

        case SYS_GETCWD: {
            char* buf = (char*)ebx;
            if (!buf) { ret = -1; break; }

            char name[FS_MAX_NAME];
            if (fs_get_inode_name(current_cwd, name)) {
                if (current_cwd == fs_root_id) {
                    strcpy(buf, "/");
                } else {
                    strcpy(buf, "/");
                    strcat(buf, name);
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
            char parent_path[128];
            char name[FS_MAX_NAME];
            
            // Resolve parent and name
            char* last_slash = strrchr(path, '/');
            if (last_slash) {
                int len = last_slash - path;
                if (len == 0) { // Root mkdir
                    strcpy(parent_path, "/");
                } else {
                    strncpy(parent_path, path, len);
                    parent_path[len] = '\0';
                }
                strcpy(name, last_slash + 1);
            } else {
                strcpy(parent_path, ".");
                strcpy(name, path);
            }

            fs_node_t* parent = fs_find_node(parent_path, current_cwd);
            if (parent && parent->type == FS_TYPE_DIRECTORY) {
                if (fs_create_node(parent->id, name, FS_TYPE_DIRECTORY)) {
                    ret = 0;
                } else {
                    ret = -1;
                }
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
            char parent_path[128];
            char name[FS_MAX_NAME];

            char* last_slash = strrchr(path, '/');
            if (last_slash) {
                int len = last_slash - path;
                if (len == 0) {
                    strcpy(parent_path, "/");
                } else {
                    strncpy(parent_path, path, len);
                    parent_path[len] = '\0';
                }
                strcpy(name, last_slash + 1);
            } else {
                strcpy(parent_path, ".");
                strcpy(name, path);
            }

            fs_node_t* parent = fs_find_node(parent_path, current_cwd);
            if (parent && parent->type == FS_TYPE_DIRECTORY) {
                if (fs_create_node(parent->id, name, FS_TYPE_FILE)) {
                    ret = 0;
                } else {
                    ret = -1;
                }
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_GETDENTS: {
            char* path = (char*)ebx;
            struct dirent* dirents = (struct dirent*)ecx;
            int max_count = (int)edx;

            fs_node_t* dir = fs_find_node(path, current_cwd);
            if (!dir || dir->type != FS_TYPE_DIRECTORY) {
                ret = -1;
                break;
            }

            int count = 0;
            fs_dirent_t entries[SECTOR_SIZE / sizeof(fs_dirent_t)];

            for (int i = 0; i < 12 && count < max_count; i++) {
                if (dir->blocks[i] == 0) continue;
                // Note: In syscall layer, we should probably use a safer read
                // but direct ATA read is used for simplicity in this kernel stage.
                extern int ata_read_sectors(uint32_t lba, uint8_t count, void* buffer);
                ata_read_sectors(dir->blocks[i], 1, entries);

                for (int j = 0; j < (SECTOR_SIZE / sizeof(fs_dirent_t)) && count < max_count; j++) {
                    if (entries[j].inode_id != 0) {
                        dirents[count].d_ino = entries[j].inode_id;
                        inode_t* child = fs_get_node(entries[j].inode_id);
                        dirents[count].d_type = child ? child->type : 0;
                        strcpy(dirents[count].d_name, entries[j].name);
                        count++;
                    }
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

    // Return value naturally goes in EAX via C calling convention
    return ret;
}

// Assembly wrapper for system call interrupt
// CRITICAL: We must NOT save/restore EAX because it holds the return value
__asm__(
    ".global syscall_interrupt_wrapper\n"
    "syscall_interrupt_wrapper:\n"
    // Save registers that we'll clobber (but NOT EAX - it will hold return value)
    "   push %ebx\n"
    "   push %ecx\n"
    "   push %edx\n"
    "   push %esi\n"
    "   push %edi\n"
    "   push %ebp\n"
    
    // Push arguments for syscall_handler in REVERSE order (cdecl convention)
    // syscall_handler(eax, ebx, ecx, edx, esi, edi)
    "   push %edi\n"                // arg6
    "   push %esi\n"                // arg5
    "   push %edx\n"                // arg4
    "   push %ecx\n"                // arg3
    "   push %ebx\n"                // arg2
    "   push %eax\n"                // arg1 (syscall number)
    
    "   call syscall_handler\n"     // Call C handler, return value in EAX
    "   add $24, %esp\n"            // Clean up 6 arguments (6 * 4 = 24 bytes)
    
    // Restore registers (but NOT EAX - it has the return value!)
    "   pop %ebp\n"
    "   pop %edi\n"
    "   pop %esi\n"
    "   pop %edx\n"
    "   pop %ecx\n"
    "   pop %ebx\n"
    
    "   iret\n"                     // Return to user code with EAX = return value
);
