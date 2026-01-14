#ifndef _NIMBLE_PORT_H
#define _NIMBLE_PORT_H

#include <stdint.h>
#include "esp_err.h"
#include "nimble/nimble_npl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nimble_port_init(void);
esp_err_t nimble_port_deinit(void);

esp_err_t esp_nimble_init(void);
esp_err_t esp_nimble_deinit(void);

void nimble_port_run(void);
void nimble_port_run_once(uint32_t timeout_ms);
int nimble_port_stop(void);
struct ble_npl_eventq *nimble_port_get_dflt_eventq(void);

#ifdef __cplusplus
}
#endif

#endif
