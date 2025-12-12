#pragma once

#include <stdint.h>

#define MTIMER_TICK_FREQUENCY 360000000

#define MTIMER_IRQn 7

#define MTIMER ((volatile mtimer_dev_t *)(0x20004000))

#define MTIME ((volatile mtime_reg_t *)(0x2000BFF8))

#define MTIMER_MS_TO_TICKS(ms) ((uint64_t)(ms * (MTIMER_TICK_FREQUENCY / 1000))) // Macro to convert milliseconds into number of MTIME ticks

#define MTIMER_US_TO_TICKS(us) ((uint64_t)(us * (MTIMER_TICK_FREQUENCY / 1000000))) // Macro to convert microseconds into number of MTIME ticks

#define SET_MTIMECMP(val)                                               \
    do                                                                  \
    {                                                                   \
        MTIMER->mtimecmp.mtimecmp_lo = (uint32_t)val;                   \
        MTIMER->mtimecmp.mtimecmp_hi = (uint32_t)((uint64_t)val >> 32); \
    } while (0)

#define GET_MTIME() ((uint64_t)(((uint64_t)MTIME->mtime_hi << 32) + (MTIME->mtime_lo)))

#define MTIMER_TICKS_TO_MS(ticks) ((uint64_t)((ticks * 1000) / MTIMER_TICK_FREQUENCY))

typedef void (*mtimer_cb_t)(void);

typedef struct
{
    uint32_t mtime_en : 3;  // counter enable
    uint32_t mtime_ovf : 1; // overflow
    uint32_t mtime_sam : 2; // sampling mode
    uint32_t reserved : 26;
} mtimectl_reg_t;

typedef struct
{

    uint32_t mtime_lo;
    uint32_t mtime_hi;

} mtime_reg_t;

typedef struct
{

    uint32_t mtimecmp_lo;
    uint32_t mtimecmp_hi;

} mtimecmp_reg_t;

typedef struct
{

    uint32_t mtimeload_lo;
    uint32_t mtimeload_hi;

} mtimeload_reg_t;

typedef struct
{
    volatile mtimecmp_reg_t mtimecmp;
    volatile mtimeload_reg_t mtimeload;
    volatile mtimectl_reg_t mtime_ctl;

} mtimer_dev_t;

typedef struct
{
    uint64_t period_ticks;
    mtimer_cb_t cb_function;
} mtimer_cb_init_t;

__attribute__((always_inline)) static inline void mtimer_enable()
{
    MTIMER->mtime_ctl.mtime_en = 1;   
}

__attribute__((always_inline)) static inline void mtimer_disable()
{
    MTIMER->mtime_ctl.mtime_en = 0;
}

__attribute__((always_inline)) static inline uint32_t uptime_ms()
{
    return MTIMER_TICKS_TO_MS(GET_MTIME());
}

void mtimer_callback_init(mtimer_cb_init_t *p_mtimer_cb_init);
