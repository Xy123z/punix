// include/syscall.h - System Call Interface

#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

// Directory entry structure
struct dirent {
    uint32_t d_ino;           // Inode number
    uint8_t  d_type;          // File type
    char     d_name[64];      // Filename
};

// System call numbers (for reference)
#define SYS_READ         0
#define SYS_WRITE        1
#define SYS_OPEN         2
#define SYS_CLOSE        3
#define SYS_GETDENTS     4
#define SYS_CHDIR        5
#define SYS_GETCWD       6
#define SYS_MKDIR        7
#define SYS_RMDIR        8
#define SYS_UNLINK       9
#define SYS_STAT         10
#define SYS_EXIT         11
#define SYS_GETPID       12
#define SYS_MALLOC       13
#define SYS_FREE         14
#define SYS_PRINT        15
#define SYS_CREATE_FILE  16

// Open flags
#define O_RDONLY  0x00
#define O_WRONLY  0x01
#define O_RDWR    0x02
#define O_CREAT   0x04

// Kernel-side functions
void syscall_init();
void syscall_handler(uint32_t eax, uint32_t ebx, uint32_t ecx,
                     uint32_t edx, uint32_t esi, uint32_t edi);
extern void syscall_interrupt_wrapper();

// User-space system call wrappers
// These functions can be called from user programs

static inline int sys_print(const char* str) {
    int ret;
    __asm__ volatile(
        "mov $15, %%eax\n"      // SYS_PRINT
        "mov %1, %%ebx\n"       // str
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(str)
        : "eax", "ebx"
    );
    return ret;
}

static inline int sys_open(const char* path, int flags) {
    int ret;
    __asm__ volatile(
        "mov $2, %%eax\n"       // SYS_OPEN
        "mov %1, %%ebx\n"       // path
        "mov %2, %%ecx\n"       // flags
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(path), "r"(flags)
        : "eax", "ebx", "ecx"
    );
    return ret;
}

static inline int sys_read(int fd, void* buf, uint32_t count) {
    int ret;
    __asm__ volatile(
        "mov $0, %%eax\n"       // SYS_READ
        "mov %1, %%ebx\n"       // fd
        "mov %2, %%ecx\n"       // buf
        "mov %3, %%edx\n"       // count
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(fd), "r"(buf), "r"(count)
        : "eax", "ebx", "ecx", "edx"
    );
    return ret;
}

static inline int sys_write(int fd, const void* buf, uint32_t count) {
    int ret;
    __asm__ volatile(
        "mov $1, %%eax\n"       // SYS_WRITE
        "mov %1, %%ebx\n"       // fd
        "mov %2, %%ecx\n"       // buf
        "mov %3, %%edx\n"       // count
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(fd), "r"(buf), "r"(count)
        : "eax", "ebx", "ecx", "edx"
    );
    return ret;
}

static inline int sys_close(int fd) {
    int ret;
    __asm__ volatile(
        "mov $3, %%eax\n"       // SYS_CLOSE
        "mov %1, %%ebx\n"       // fd
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(fd)
        : "eax", "ebx"
    );
    return ret;
}

static inline int sys_getcwd(char* buf, uint32_t size) {
    int ret;
    __asm__ volatile(
        "mov $6, %%eax\n"       // SYS_GETCWD
        "mov %1, %%ebx\n"       // buf
        "mov %2, %%ecx\n"       // size
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(buf), "r"(size)
        : "eax", "ebx", "ecx"
    );
    return ret;
}

static inline int sys_chdir(const char* path) {
    int ret;
    __asm__ volatile(
        "mov $5, %%eax\n"       // SYS_CHDIR
        "mov %1, %%ebx\n"       // path
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(path)
        : "eax", "ebx"
    );
    return ret;
}

static inline int sys_mkdir(const char* path) {
    int ret;
    __asm__ volatile(
        "mov $7, %%eax\n"       // SYS_MKDIR
        "mov %1, %%ebx\n"       // path
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(path)
        : "eax", "ebx"
    );
    return ret;
}

static inline int sys_rmdir(const char* path) {
    int ret;
    __asm__ volatile(
        "mov $8, %%eax\n"       // SYS_RMDIR
        "mov %1, %%ebx\n"       // path
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(path)
        : "eax", "ebx"
    );
    return ret;
}

static inline int sys_create_file(const char* path) {
    int ret;
    __asm__ volatile(
        "mov $16, %%eax\n"      // SYS_CREATE_FILE
        "mov %1, %%ebx\n"       // path
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(path)
        : "eax", "ebx"
    );
    return ret;
}

static inline int sys_getdents(const char* path, struct dirent* buf, int count) {
    int ret;
    __asm__ volatile(
        "mov $4, %%eax\n"       // SYS_GETDENTS
        "mov %1, %%ebx\n"       // path
        "mov %2, %%ecx\n"       // buf
        "mov %3, %%edx\n"       // count
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(ret)
        : "r"(path), "r"(buf), "r"(count)
        : "eax", "ebx", "ecx", "edx"
    );
    return ret;
}

#endif // SYSCALL_H
