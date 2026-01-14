#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "esp_hosted.h"
#include "nimble/nimble_npl.h"
#include "os/os.h"
#include "delay.h"
#include "mtimer.h"
#include "riscv/csr.h"
#include "riscv/encoding.h"

static struct ble_npl_callout *s_callout_head;
static uint32_t s_critical_nesting;

static void ble_npl_callout_remove(struct ble_npl_callout *co)
{
    struct ble_npl_callout **link = &s_callout_head;

    while (*link != NULL)
    {
        if (*link == co)
        {
            *link = co->next;
            co->next = NULL;
            co->c_active = false;
            return;
        }
        link = &(*link)->next;
    }
}

static void ble_npl_callout_insert(struct ble_npl_callout *co)
{
    struct ble_npl_callout **link = &s_callout_head;

    while (*link != NULL)
    {
        ble_npl_stime_t delta = (ble_npl_stime_t)((*link)->c_ticks - co->c_ticks);
        if (delta > 0)
        {
            break;
        }
        link = &(*link)->next;
    }

    co->next = *link;
    *link = co;
    co->c_active = true;
}

static void ble_npl_process_callouts(void)
{
    ble_npl_time_t now = ble_npl_time_get();

    while (s_callout_head != NULL)
    {
        struct ble_npl_callout *co = s_callout_head;
        ble_npl_stime_t delta = (ble_npl_stime_t)(co->c_ticks - now);
        if (delta > 0)
        {
            break;
        }

        s_callout_head = co->next;
        co->next = NULL;
        co->c_active = false;
        ble_npl_eventq_put(co->c_evq, &co->c_ev);
    }
}

static void ble_npl_service_background(void)
{
    /*
     * Bare-metal NimBLE performs synchronous waits for HCI command acks.
     * Service the hosted transport while waiting so RX packets can be
     * dispatched into NimBLE and release those waits.
     */
    esp_hosted_poll();
    esp_hosted_dispatch_rx();
    ble_npl_process_callouts();
}

bool ble_npl_os_started(void)
{
    return true;
}

void *ble_npl_get_current_task_id(void)
{
    return (void *)1;
}

void ble_npl_eventq_init(struct ble_npl_eventq *evq)
{
    if (evq == NULL)
    {
        return;
    }

    evq->eventq = evq;
    evq->head = NULL;
    evq->tail = NULL;
}

void ble_npl_eventq_deinit(struct ble_npl_eventq *evq)
{
    if (evq == NULL)
    {
        return;
    }

    evq->eventq = NULL;
    evq->head = NULL;
    evq->tail = NULL;
}

struct ble_npl_event *ble_npl_eventq_get(struct ble_npl_eventq *evq,
                                         ble_npl_time_t tmo)
{
    ble_npl_time_t start = ble_npl_time_get();

    if (evq == NULL)
    {
        return NULL;
    }

    while (true)
    {
        struct ble_npl_event *ev;
        uint32_t sr;

        ble_npl_service_background();

        OS_ENTER_CRITICAL(sr);
        ev = evq->head;
        if (ev != NULL)
        {
            evq->head = ev->next;
            if (evq->head == NULL)
            {
                evq->tail = NULL;
            }
            ev->next = NULL;
            ev->ev_queued = 0U;
            ev->evq = NULL;
            OS_EXIT_CRITICAL(sr);
            return ev;
        }
        OS_EXIT_CRITICAL(sr);

        if (tmo == 0U)
        {
            return NULL;
        }

        if (tmo != BLE_NPL_TIME_FOREVER)
        {
            if ((ble_npl_time_get() - start) >= tmo)
            {
                return NULL;
            }
        }

        delay_us(100);
    }
}

void ble_npl_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev)
{
    uint32_t sr;

    if (evq == NULL || ev == NULL)
    {
        return;
    }

    OS_ENTER_CRITICAL(sr);

    if (ev->ev_queued)
    {
        OS_EXIT_CRITICAL(sr);
        return;
    }

    ev->next = NULL;
    ev->ev_queued = 1U;
    ev->evq = evq;

    if (evq->tail != NULL)
    {
        evq->tail->next = ev;
    }
    else
    {
        evq->head = ev;
    }
    evq->tail = ev;

    OS_EXIT_CRITICAL(sr);
}

void ble_npl_eventq_remove(struct ble_npl_eventq *evq, struct ble_npl_event *ev)
{
    struct ble_npl_event *prev = NULL;
    struct ble_npl_event *cur;
    uint32_t sr;

    if (evq == NULL || ev == NULL)
    {
        return;
    }

    OS_ENTER_CRITICAL(sr);

    cur = evq->head;
    while (cur != NULL)
    {
        if (cur == ev)
        {
            if (prev != NULL)
            {
                prev->next = cur->next;
            }
            else
            {
                evq->head = cur->next;
            }

            if (evq->tail == cur)
            {
                evq->tail = prev;
            }

            cur->next = NULL;
            cur->ev_queued = 0U;
            cur->evq = NULL;
            break;
        }
        prev = cur;
        cur = cur->next;
    }

    OS_EXIT_CRITICAL(sr);
}

void ble_npl_event_init(struct ble_npl_event *ev, ble_npl_event_fn *fn, void *arg)
{
    if (ev == NULL)
    {
        return;
    }

    ev->event = ev;
    ev->ev_queued = 0U;
    ev->ev_cb = fn;
    ev->ev_arg = arg;
    ev->next = NULL;
    ev->evq = NULL;
}

void ble_npl_event_deinit(struct ble_npl_event *ev)
{
    if (ev == NULL)
    {
        return;
    }

    if (ev->evq != NULL)
    {
        ble_npl_eventq_remove(ev->evq, ev);
    }
    ble_npl_event_reset(ev);
}

void ble_npl_event_reset(struct ble_npl_event *ev)
{
    if (ev == NULL)
    {
        return;
    }

    ev->ev_queued = 0U;
    ev->event = ev;
    ev->next = NULL;
    ev->evq = NULL;
}

bool ble_npl_event_is_queued(struct ble_npl_event *ev)
{
    return ev != NULL && ev->ev_queued != 0U;
}

void *ble_npl_event_get_arg(struct ble_npl_event *ev)
{
    return ev != NULL ? ev->ev_arg : NULL;
}

void ble_npl_event_set_arg(struct ble_npl_event *ev, void *arg)
{
    if (ev != NULL)
    {
        ev->ev_arg = arg;
    }
}

bool ble_npl_eventq_is_empty(struct ble_npl_eventq *evq)
{
    return evq == NULL || evq->head == NULL;
}

void ble_npl_event_run(struct ble_npl_event *ev)
{
    if (ev != NULL && ev->ev_cb != NULL)
    {
        ev->ev_cb(ev);
    }
}

ble_npl_error_t ble_npl_mutex_init(struct ble_npl_mutex *mu)
{
    if (mu == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    mu->depth = 0U;
    mu->mutex = mu;
    mu->owner = NULL;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout)
{
    ble_npl_time_t start = ble_npl_time_get();
    void *owner;

    if (mu == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    owner = ble_npl_get_current_task_id();

    while (true)
    {
        uint32_t sr;
        bool acquired = false;

        ble_npl_service_background();

        OS_ENTER_CRITICAL(sr);
        if (mu->owner == NULL || mu->owner == owner)
        {
            mu->owner = owner;
            mu->depth++;
            acquired = true;
        }
        OS_EXIT_CRITICAL(sr);

        if (acquired)
        {
            return BLE_NPL_OK;
        }

        if (timeout == 0U)
        {
            return BLE_NPL_TIMEOUT;
        }

        if (timeout != BLE_NPL_TIME_FOREVER &&
            (ble_npl_time_get() - start) >= timeout)
        {
            return BLE_NPL_TIMEOUT;
        }

        delay_us(100);
    }
}

ble_npl_error_t ble_npl_mutex_release(struct ble_npl_mutex *mu)
{
    uint32_t sr;

    if (mu == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    OS_ENTER_CRITICAL(sr);
    if (mu->owner == ble_npl_get_current_task_id() && mu->depth > 0U)
    {
        mu->depth--;
        if (mu->depth == 0U)
        {
            mu->owner = NULL;
        }
        OS_EXIT_CRITICAL(sr);
        return BLE_NPL_OK;
    }
    OS_EXIT_CRITICAL(sr);

    return BLE_NPL_BAD_MUTEX;
}

ble_npl_error_t ble_npl_mutex_deinit(struct ble_npl_mutex *mu)
{
    return ble_npl_mutex_init(mu);
}

ble_npl_error_t ble_npl_sem_init(struct ble_npl_sem *sem, uint16_t tokens)
{
    if (sem == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    sem->tokens = tokens;
    sem->sem = sem;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout)
{
    ble_npl_time_t start = ble_npl_time_get();

    if (sem == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    while (true)
    {
        uint32_t sr;
        bool acquired = false;

        ble_npl_service_background();

        OS_ENTER_CRITICAL(sr);
        if (sem->tokens > 0U)
        {
            sem->tokens--;
            acquired = true;
        }
        OS_EXIT_CRITICAL(sr);

        if (acquired)
        {
            return BLE_NPL_OK;
        }

        if (timeout == 0U)
        {
            return BLE_NPL_TIMEOUT;
        }

        if (timeout != BLE_NPL_TIME_FOREVER &&
            (ble_npl_time_get() - start) >= timeout)
        {
            return BLE_NPL_TIMEOUT;
        }

        delay_us(100);
    }
}

ble_npl_error_t ble_npl_sem_release(struct ble_npl_sem *sem)
{
    uint32_t sr;

    if (sem == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    OS_ENTER_CRITICAL(sr);
    sem->tokens++;
    OS_EXIT_CRITICAL(sr);
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_sem_deinit(struct ble_npl_sem *sem)
{
    return ble_npl_sem_init(sem, 0U);
}

uint16_t ble_npl_sem_get_count(struct ble_npl_sem *sem)
{
    return sem != NULL ? sem->tokens : 0U;
}

int ble_npl_callout_init(struct ble_npl_callout *co, struct ble_npl_eventq *evq,
                         ble_npl_event_fn *ev_cb, void *ev_arg)
{
    if (co == NULL || evq == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    ble_npl_event_init(&co->c_ev, ev_cb, ev_arg);
    co->co = co;
    co->c_evq = evq;
    co->c_ticks = 0U;
    co->c_active = false;
    co->next = NULL;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_callout_reset(struct ble_npl_callout *co, ble_npl_time_t ticks)
{
    uint32_t sr;

    if (co == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    OS_ENTER_CRITICAL(sr);
    ble_npl_callout_remove(co);
    co->c_ticks = ble_npl_time_get() + ticks;
    ble_npl_callout_insert(co);
    OS_EXIT_CRITICAL(sr);
    return BLE_NPL_OK;
}

void ble_npl_callout_stop(struct ble_npl_callout *co)
{
    uint32_t sr;

    if (co == NULL)
    {
        return;
    }

    OS_ENTER_CRITICAL(sr);
    ble_npl_callout_remove(co);
    OS_EXIT_CRITICAL(sr);
}

void ble_npl_callout_deinit(struct ble_npl_callout *co)
{
    if (co == NULL)
    {
        return;
    }

    ble_npl_callout_stop(co);
    ble_npl_event_deinit(&co->c_ev);
    co->co = NULL;
    co->c_evq = NULL;
    co->c_ticks = 0U;
}

void ble_npl_callout_mem_reset(struct ble_npl_callout *co)
{
    if (co != NULL)
    {
        ble_npl_callout_stop(co);
        co->co = co;
        co->c_ticks = 0U;
    }
}

bool ble_npl_callout_is_active(struct ble_npl_callout *co)
{
    return co != NULL && co->c_active;
}

ble_npl_time_t ble_npl_callout_get_ticks(struct ble_npl_callout *co)
{
    return co != NULL ? co->c_ticks : 0U;
}

ble_npl_time_t ble_npl_callout_remaining_ticks(struct ble_npl_callout *co,
                                               ble_npl_time_t time)
{
    if (co == NULL || !co->c_active)
    {
        return 0U;
    }

    if ((ble_npl_stime_t)(co->c_ticks - time) <= 0)
    {
        return 0U;
    }

    return co->c_ticks - time;
}

void ble_npl_callout_set_arg(struct ble_npl_callout *co, void *arg)
{
    if (co != NULL)
    {
        ble_npl_event_set_arg(&co->c_ev, arg);
    }
}

ble_npl_time_t ble_npl_time_get(void)
{
    return uptime_ms();
}

ble_npl_error_t ble_npl_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks)
{
    if (out_ticks == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    *out_ticks = ms;
    return BLE_NPL_OK;
}

ble_npl_error_t ble_npl_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms)
{
    if (out_ms == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    *out_ms = ticks;
    return BLE_NPL_OK;
}

ble_npl_time_t ble_npl_time_ms_to_ticks32(uint32_t ms)
{
    return ms;
}

uint32_t ble_npl_time_ticks_to_ms32(ble_npl_time_t ticks)
{
    return ticks;
}

void ble_npl_time_delay(ble_npl_time_t ticks)
{
    delay_ms(ticks);
}

uint32_t ble_npl_hw_enter_critical(void)
{
    uint32_t old_mstatus = (uint32_t)RV_CLEAR_CSR(mstatus, MSTATUS_MIE);
    s_critical_nesting++;
    return old_mstatus & MSTATUS_MIE;
}

void ble_npl_hw_exit_critical(uint32_t ctx)
{
    if (s_critical_nesting > 0U)
    {
        s_critical_nesting--;
    }

    if ((ctx & MSTATUS_MIE) != 0U)
    {
        RV_SET_CSR(mstatus, MSTATUS_MIE);
    }
}

bool ble_npl_hw_is_in_critical(void)
{
    return s_critical_nesting != 0U;
}

int ble_npl_task_init(struct ble_npl_task *t, const char *name,
                      ble_npl_task_func_t func, void *arg, uint8_t prio,
                      ble_npl_time_t sanity_itvl, ble_npl_stack_t *stack_bottom,
                      uint16_t stack_size)
{
    (void)prio;
    (void)sanity_itvl;
    (void)stack_bottom;
    (void)stack_size;

    if (t == NULL || func == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    t->name = name;
    t->func = func;
    t->arg = arg;
    return BLE_NPL_OK;
}

int ble_npl_task_remove(struct ble_npl_task *t)
{
    if (t == NULL)
    {
        return BLE_NPL_EINVAL;
    }

    t->name = NULL;
    t->func = NULL;
    t->arg = NULL;
    return BLE_NPL_OK;
}

uint8_t ble_npl_task_count(void)
{
    return 1U;
}

void ble_npl_task_yield(void)
{
    ble_npl_process_callouts();
}
