#pragma once

#include <stddef.h>
#include <stdarg.h>

int serial_write(const char *buf, size_t len);
int serial_vprintf(const char *format, va_list args);
int serial_printf(const char *format, ...);

char *serial_read_string();
