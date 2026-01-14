#ifndef __CONSOLE_CONSOLE_H
#define __CONSOLE_CONSOLE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*console_rx_cb_t)(void);

int console_init(console_rx_cb_t rx_cb);
int console_read(char *buf, int max_len, int *out_len);
int console_write(const char *buf, size_t len);
int console_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
