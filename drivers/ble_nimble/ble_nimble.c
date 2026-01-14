#include <stddef.h>
#include <string.h>

#include "ble_nimble.h"
#include "delay.h"
#include "esp_hosted.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "store/ram/ble_store_ram.h"

static const char *TAG = "ble_nimble";

static ble_nimble_config_t s_cfg;
static bool s_initialized;
static bool s_synced;
static bool s_stop_requested;

static void ble_nimble_on_reset(int reason)
{
    s_synced = false;
    if (s_cfg.reset_cb != NULL)
    {
        s_cfg.reset_cb(reason);
        return;
    }

    ESP_LOGW(TAG, "host reset reason=%d\n", reason);
}

static void ble_nimble_on_sync(void)
{
    s_synced = true;
    if (s_cfg.sync_cb != NULL)
    {
        s_cfg.sync_cb();
    }
}

esp_err_t ble_nimble_init(const ble_nimble_config_t *config)
{
    esp_err_t rc;

    if (s_initialized)
    {
        return ESP_OK;
    }

    memset(&s_cfg, 0, sizeof(s_cfg));
    if (config != NULL)
    {
        s_cfg = *config;
    }
    else
    {
        s_cfg.init_gap_service = true;
        s_cfg.init_gatt_service = true;
        s_cfg.init_store_ram = true;
    }

    rc = nimble_port_init();
    if (rc != ESP_OK)
    {
        return rc;
    }

    ble_hs_cfg.reset_cb = ble_nimble_on_reset;
    ble_hs_cfg.sync_cb = ble_nimble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    if (s_cfg.init_store_ram)
    {
        ble_store_ram_init();
    }

    if (s_cfg.init_gap_service)
    {
        ble_svc_gap_init();
    }

    if (s_cfg.device_name != NULL)
    {
        ble_svc_gap_device_name_set(s_cfg.device_name);
    }

    if (s_cfg.init_gatt_service)
    {
        ble_svc_gatt_init();
    }

    s_synced = false;
    s_stop_requested = false;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t ble_nimble_deinit(void)
{
    s_stop_requested = true;
    s_synced = false;
    s_initialized = false;
    return nimble_port_deinit();
}

void ble_nimble_poll(void)
{
    if (!s_initialized)
    {
        return;
    }

    esp_hosted_poll();
    esp_hosted_dispatch_rx();
    nimble_port_run_once(0U);
}

void ble_nimble_run(void)
{
    while (!s_stop_requested)
    {
        ble_nimble_poll();
        delay_ms(1U);
    }
}

void ble_nimble_stop(void)
{
    s_stop_requested = true;
    nimble_port_stop();
}

bool ble_nimble_is_synced(void)
{
    return s_synced;
}
