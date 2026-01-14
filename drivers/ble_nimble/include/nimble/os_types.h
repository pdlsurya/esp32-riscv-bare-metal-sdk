#ifndef _NPL_OS_TYPES_H
#define _NPL_OS_TYPES_H

#include <stdbool.h>
#include <stdint.h>

struct ble_npl_event;
struct ble_npl_eventq;
struct ble_npl_callout;
struct ble_npl_mutex;
struct ble_npl_sem;

typedef uint32_t ble_npl_time_t;
typedef int32_t ble_npl_stime_t;
typedef int ble_npl_stack_t;

typedef void (*ble_npl_task_func_t)(void *arg);

typedef void ble_npl_event_fn(struct ble_npl_event *ev);

struct ble_npl_event {
    void *event;
    uint8_t ev_queued;
    ble_npl_event_fn *ev_cb;
    void *ev_arg;
    struct ble_npl_event *next;
    struct ble_npl_eventq *evq;
};

struct ble_npl_eventq {
    void *eventq;
    struct ble_npl_event *head;
    struct ble_npl_event *tail;
};

struct ble_npl_callout {
    void *co;
    struct ble_npl_event c_ev;
    struct ble_npl_eventq *c_evq;
    ble_npl_time_t c_ticks;
    bool c_active;
    struct ble_npl_callout *next;
};

struct ble_npl_mutex {
    void *mutex;
    uint16_t depth;
    void *owner;
};

struct ble_npl_sem {
    void *sem;
    uint16_t tokens;
};

struct ble_npl_task {
    const char *name;
    ble_npl_task_func_t func;
    void *arg;
};

#endif
