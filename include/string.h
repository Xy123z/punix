// include/string.h - String utility functions

#ifndef STRING_H
#define STRING_H

#include "types.h"

// String functions
int strcmp(const char* str1, const char* str2);
void strcpy(char* dest, const char* src);
int strlen(const char* str);
void int_to_str(int num, char* str);
int str_to_int(const char* str);

#endif // STRING_H
