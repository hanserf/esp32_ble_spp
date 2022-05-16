#pragma once
#include "stdbool.h"
#include <stddef.h>
#include <stdint.h>
#define GETC_NO_BLOCK (false)
#define GETC_BLOCK (true)
void console_ll_init();
char console_ll_getc(bool block);
void console_printf(const char *str, ...);
void console_ll_putc(char c);
int console_ll_getline(char *data, size_t size);
int console_ll_putline(char *data, size_t size);
