#pragma once

#include <stdint.h>
#include "hal/systimer_ll.h"

/* ESP32-C3 has no CPU-local MTIME; use SYSTIMER unit 0 and target 0. */
#define MTIMER_TICK_FREQUENCY 16000000
#define MTIMER_IRQn 7

#define MTIMER_MS_TO_TICKS(ms) ((uint64_t)(ms) * (MTIMER_TICK_FREQUENCY / 1000))
#define MTIMER_US_TO_TICKS(us) ((uint64_t)(us) * (MTIMER_TICK_FREQUENCY / 1000000))
#define MTIMER_TICKS_TO_MS(ticks) ((uint64_t)(ticks) / (MTIMER_TICK_FREQUENCY / 1000))

typedef void (*mtimer_cb_t)(void);

typedef struct
{
    uint64_t period_ticks;
    mtimer_cb_t cb_function;
} mtimer_cb_init_t;

__attribute__((always_inline)) static inline uint64_t GET_MTIME(void)
{
    systimer_ll_counter_snapshot(&SYSTIMER, 0);
    while (!systimer_ll_is_counter_value_valid(&SYSTIMER, 0))
    {
    }
    uint32_t lo = systimer_ll_get_counter_value_low(&SYSTIMER, 0);
    uint32_t hi = systimer_ll_get_counter_value_high(&SYSTIMER, 0);
    return ((uint64_t)hi << 32) | lo;
}

__attribute__((always_inline)) static inline void SET_MTIMECMP(uint64_t value)
{
    systimer_ll_enable_alarm(&SYSTIMER, 0, false);
    systimer_ll_set_alarm_target(&SYSTIMER, 0, value);
    systimer_ll_apply_alarm_value(&SYSTIMER, 0);
    systimer_ll_enable_alarm(&SYSTIMER, 0, true);
}

__attribute__((always_inline)) static inline void mtimer_enable(void)
{
    /* Use the raw LL entry point, as in the other bare-metal drivers. */
    (systimer_ll_enable_bus_clock)(true);
    systimer_ll_enable_clock(&SYSTIMER, true);
    systimer_ll_enable_alarm_int(&SYSTIMER, 0, false);
    systimer_ll_enable_alarm(&SYSTIMER, 0, false);
    systimer_ll_clear_alarm_int(&SYSTIMER, 0);
    systimer_ll_enable_counter(&SYSTIMER, 0, false);
    systimer_ll_set_counter_value(&SYSTIMER, 0, 0);
    systimer_ll_apply_counter_value(&SYSTIMER, 0);
    systimer_ll_counter_can_stall_by_cpu(&SYSTIMER, 0, 0, false);
    systimer_ll_connect_alarm_counter(&SYSTIMER, 0, 0);
    systimer_ll_enable_alarm_oneshot(&SYSTIMER, 0);
    systimer_ll_enable_counter(&SYSTIMER, 0, true);
}

__attribute__((always_inline)) static inline uint32_t uptime_ms(void)
{
    return MTIMER_TICKS_TO_MS(GET_MTIME());
}

void mtimer_callback_init(mtimer_cb_init_t *p_mtimer_cb_init);
