// include/shell.h - Shell interface

#ifndef SHELL_H
#define SHELL_H

#include "types.h"

#define MAX_HISTORY 50
#define MAX_RESULT_LEN 80
#define MAX_PASSWORD_LEN 40
#define MAX_USERNAME_LEN 40
// Shell functions
void shell_init();
void shell_run();

// Directory system
extern int ROOT_ACCESS_GRANTED;
extern int current_directory; // 0=/, 1=/a, 2=/h
extern char ROOT_PASSWORD[MAX_PASSWORD_LEN];
extern char USERNAME[MAX_USERNAME_LEN];
// History management
void history_save();
void history_delete(int index);
void history_show();

// Command handlers
void cmd_pwd();
void cmd_ls();
void cmd_cd(char* dir);
void cmd_add();
void cmd_mem();
void cmd_help();
void cmd_clear();
void cmd_exit();
void read_line_with_display(char* buffer, int max_len);
#endif // SHELL_H
