// src/string.c - String utility functions
#include "../include/string.h"

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

void int_to_str(int num, char* str) {
    int i = 0;
    int is_negative = 0;

    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    if (num == 0) {
        str[i++] = '0';
    } else {
        char temp[12];
        int j = 0;

        while (num > 0) {
            temp[j++] = (num % 10) + '0';
            num /= 10;
        }

        if (is_negative) {
            str[i++] = '-';
        }

        while (j > 0) {
            str[i++] = temp[--j];
        }
    }

    str[i] = '\0';
}

int str_to_int(const char* str) {
    int result = 0;
    int sign = 1;
    int i = 0;

    if (str[0] == '-') {
        sign = -1;
        i = 1;
    }

    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return result * sign;
}
