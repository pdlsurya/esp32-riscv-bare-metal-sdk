#ifndef __BLE_NIMBLE_H
#define __BLE_NIMBLE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ble_nimble_reset_cb_t)(int reason);
typedef void (*ble_nimble_sync_cb_t)(void);

typedef struct
{
    const char *device_name;
    ble_nimble_reset_cb_t reset_cb;
    ble_nimble_sync_cb_t sync_cb;
    bool init_gap_service;
    bool init_gatt_service;
    bool init_store_ram;
} ble_nimble_config_t;

esp_err_t ble_nimble_init(const ble_nimble_config_t *config);
esp_err_t ble_nimble_deinit(void);
void ble_nimble_poll(void);
void ble_nimble_run(void);
void ble_nimble_stop(void);
bool ble_nimble_is_synced(void);

#ifdef __cplusplus
}
#endif

#endif
