#ifndef __ESP_NIMBLE_MEM_H
#define __ESP_NIMBLE_MEM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *nimble_platform_mem_malloc(size_t size);
void *nimble_platform_mem_calloc(size_t n, size_t size);
void *nimble_platform_mem_realloc(void *ptr, size_t size);
void nimble_platform_mem_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
