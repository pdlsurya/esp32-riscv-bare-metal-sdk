#pragma once

#include <stddef.h>
#include <stdio.h>

int serial_write(const char *buf, size_t len);
char *serial_read_string();
