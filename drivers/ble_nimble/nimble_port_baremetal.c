#include <stdbool.h>

#include "nimble/nimble_port.h"
#include "nimble/transport.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "esp_log.h"
#include "os/os.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "nimble_port";

static struct ble_npl_eventq s_default_eventq;
static volatile bool s_stop_requested;
static bool s_initialized;

extern void os_msys_init(void);
extern void os_msys_reset(void);
extern void os_mempool_module_init(void);
extern void ble_hs_deinit(void);

struct ble_npl_eventq *ble_npl_eventq_dflt_get(void)
{
    return &s_default_eventq;
}

esp_err_t esp_nimble_init(void)
{
    return nimble_port_init();
}

esp_err_t esp_nimble_deinit(void)
{
    return nimble_port_deinit();
}

esp_err_t nimble_port_init(void)
{
    esp_err_t rc;

    if (s_initialized)
    {
        return ESP_OK;
    }

    os_mempool_module_init();
    ble_npl_eventq_init(&s_default_eventq);

    rc = ble_buf_alloc();
    if (rc != ESP_OK)
    {
        ESP_LOGE(TAG, "ble_buf_alloc failed rc=%d\n", (int)rc);
        return rc;
    }

    ble_transport_init();
    os_msys_init();
    ble_transport_hs_init();
    ble_transport_ll_init();

    s_stop_requested = false;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t nimble_port_deinit(void)
{
    if (!s_initialized)
    {
        return ESP_OK;
    }

    ble_transport_ll_deinit();
    ble_hs_deinit();
    ble_transport_deinit();
    ble_buf_free();
    os_msys_reset();
    ble_npl_eventq_deinit(&s_default_eventq);

    s_initialized = false;
    s_stop_requested = false;
    return ESP_OK;
}

void nimble_port_run_once(uint32_t timeout_ms)
{
    struct ble_npl_event *ev;

    if (!s_initialized)
    {
        return;
    }

    ev = ble_npl_eventq_get(&s_default_eventq, timeout_ms);
    if (ev != NULL)
    {
        ble_npl_event_run(ev);
    }
}

void nimble_port_run(void)
{
    while (!s_stop_requested)
    {
        nimble_port_run_once(BLE_NPL_TIME_FOREVER);
    }
}

int nimble_port_stop(void)
{
    s_stop_requested = true;
    return 0;
}

struct ble_npl_eventq *nimble_port_get_dflt_eventq(void)
{
    return &s_default_eventq;
}
