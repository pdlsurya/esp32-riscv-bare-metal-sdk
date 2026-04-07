#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#include <stdint.h>
#include <stdio.h>

#include "usb_serial.h"

#define BYTE_ORDER LITTLE_ENDIAN

typedef uint8_t u8_t;
typedef int8_t s8_t;
typedef uint16_t u16_t;
typedef int16_t s16_t;
typedef uint32_t u32_t;
typedef int32_t s32_t;
typedef uintptr_t mem_ptr_t;

typedef int sys_prot_t;

u32_t lwip_hosted_rand(void);

#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PLATFORM_DIAG(x) printf x
#define LWIP_RAND lwip_hosted_rand
#define LWIP_PLATFORM_ASSERT(x)                             \
    do                                                      \
    {                                                       \
        printf("[lwip] assert: %s\n", (x));         \
        while (1)                                           \
        {                                                   \
        }                                                   \
    } while (0)

#endif
