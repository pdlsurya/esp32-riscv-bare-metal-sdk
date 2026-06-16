#pragma once

#include <stddef.h>
#include <stdio.h>

int usb_serial_write(const char *buf, size_t len);
char *usb_serial_read_string();
