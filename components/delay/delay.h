#pragma once

#include <stdint.h>
#include "mtimer.h"

__attribute__((always_inline)) static inline void delay_us(uint32_t us)
{
    uint64_t initial_time_ticks = GET_MTIME();
    uint64_t delay_ticks = MTIMER_US_TO_TICKS(us);
    while ((GET_MTIME() - initial_time_ticks) < delay_ticks)
        ;
}

__attribute__((always_inline)) static inline void delay_ms(uint32_t ms)
{
    uint64_t initial_time_ticks = GET_MTIME();
    uint64_t delay_ticks = MTIMER_MS_TO_TICKS(ms);
    while ((GET_MTIME() - initial_time_ticks) < delay_ticks)
        ;
}
