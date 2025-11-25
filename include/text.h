#ifndef TEXT_H
#define TEXT_H
#include "interrupt.h"
#include "memory.h"
#include "shell.h"
#include "string.h"
#include "types.h"
#include "vga.h"
#define MAX_FILE_SIZE 8192

void text_editor(const char* edit_filename);
void show_files();
typedef struct file_entry {
    char name[40];
    char* content;
    size_t size;
    struct file_entry* next;
} file_entry_t;

#endif
