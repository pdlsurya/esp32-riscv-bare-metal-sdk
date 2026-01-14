#ifndef _NIMBLE_NPL_OS_H_
#define _NIMBLE_NPL_OS_H_

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "nimble/os_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_NPL_OS_ALIGNMENT    (4)
#define BLE_NPL_TIME_FOREVER    INT32_MAX

struct ble_npl_eventq *ble_npl_eventq_dflt_get(void);
void ble_npl_event_deinit(struct ble_npl_event *ev);
void ble_npl_event_reset(struct ble_npl_event *ev);
void ble_npl_callout_deinit(struct ble_npl_callout *co);
void ble_npl_callout_mem_reset(struct ble_npl_callout *co);

int ble_npl_task_init(struct ble_npl_task *t, const char *name,
                      ble_npl_task_func_t func, void *arg, uint8_t prio,
                      ble_npl_time_t sanity_itvl, ble_npl_stack_t *stack_bottom,
                      uint16_t stack_size);
int ble_npl_task_remove(struct ble_npl_task *t);
uint8_t ble_npl_task_count(void);
void ble_npl_task_yield(void);

#ifdef __cplusplus
}
#endif

#endif
