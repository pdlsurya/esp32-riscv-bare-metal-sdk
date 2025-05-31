#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "console/console.h"
#include "esp_nimble_mem.h"
#include "usb_serial.h"

static console_rx_cb_t s_console_rx_cb;

int console_init(console_rx_cb_t rx_cb)
{
    s_console_rx_cb = rx_cb;
    return 0;
}

int console_read(char *buf, int max_len, int *out_len)
{
    (void)buf;
    (void)max_len;
    if (out_len != NULL)
    {
        *out_len = 0;
    }
    return s_console_rx_cb != NULL ? s_console_rx_cb() : 0;
}

int console_write(const char *buf, size_t len)
{
    if (buf == NULL)
    {
        return -1;
    }

    return serial_write(buf, len);
}

int console_printf(const char *fmt, ...)
{
    int rc;
    va_list ap;

    va_start(ap, fmt);
    rc = serial_vprintf(fmt, ap);
    va_end(ap);
    return rc;
}

void *nimble_platform_mem_malloc(size_t size)
{
    return malloc(size);
}

void *nimble_platform_mem_calloc(size_t n, size_t size)
{
    return calloc(n, size);
}

void *nimble_platform_mem_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void nimble_platform_mem_free(void *ptr)
{
    free(ptr);
}
