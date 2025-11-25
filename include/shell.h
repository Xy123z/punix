// include/shell.h - Shell interface

#ifndef SHELL_H
#define SHELL_H

#include "types.h"

#define MAX_HISTORY 50
#define MAX_RESULT_LEN 80

// Shell functions
void shell_init();
void shell_run();

// Directory system
extern int current_directory; // 0=/, 1=/a, 2=/h

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

#endif // SHELL_H
