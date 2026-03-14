#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_hosted.h"
#include "esp_log.h"
#include "esp_nimble_mem.h"
#include "host/ble_hs_mbuf.h"
#include "nimble/hci_common.h"
#include "nimble/transport.h"
#include "os/os_mbuf.h"

static const char *TAG = "ble_hci_hosted";
static bool s_transport_ready;

static bool ble_hci_is_adv_report(const uint8_t *data, size_t len)
{
    if (len < 4U)
    {
        return false;
    }

    if (data[0] != BLE_HCI_EVCODE_LE_META)
    {
        return false;
    }

    return data[2] == BLE_HCI_LE_SUBEV_ADV_RPT ||
           data[2] == BLE_HCI_LE_SUBEV_EXT_ADV_RPT;
}

static void ble_hci_evt_rx(const esp_hosted_frame_info_t *info,
                           const uint8_t *payload,
                           size_t len,
                           void *ctx)
{
    uint8_t *evt_buf;
    size_t total_len;

    (void)info;
    (void)ctx;

    if (payload == NULL || len < 2U)
    {
        return;
    }

    total_len = 2U + payload[1];
    if (len < total_len)
    {
        ESP_LOGW(TAG, "short HCI event len=%u need=%u\n",
                 (unsigned)len, (unsigned)total_len);
        return;
    }

    if (payload[0] == BLE_HCI_EVCODE_HW_ERROR)
    {
        ESP_LOGE(TAG, "controller reported HW error\n");
        return;
    }

    evt_buf = ble_transport_alloc_evt(ble_hci_is_adv_report(payload, total_len) ? 1 : 0);
    if (evt_buf == NULL)
    {
        ESP_LOGW(TAG, "no NimBLE event buffer\n");
        return;
    }

    memcpy(evt_buf, payload, total_len);
    if (ble_transport_to_hs_evt(evt_buf) != 0)
    {
        ESP_LOGW(TAG, "ble_transport_to_hs_evt failed\n");
        ble_transport_free(evt_buf);
    }
}

static void ble_hci_acl_rx(const esp_hosted_frame_info_t *info,
                           const uint8_t *payload,
                           size_t len,
                           void *ctx)
{
    struct os_mbuf *om;

    (void)info;
    (void)ctx;

    if (payload == NULL || len == 0U)
    {
        return;
    }

    om = ble_transport_alloc_acl_from_ll();
    if (om == NULL)
    {
        ESP_LOGW(TAG, "no NimBLE ACL buffer\n");
        return;
    }

    if (os_mbuf_append(om, payload, (uint16_t)len) != 0)
    {
        ESP_LOGW(TAG, "os_mbuf_append failed\n");
        os_mbuf_free_chain(om);
        return;
    }

    if (ble_transport_to_hs_acl(om) != 0)
    {
        ESP_LOGW(TAG, "ble_transport_to_hs_acl failed\n");
        os_mbuf_free_chain(om);
    }
}

void hci_drv_init(void)
{
}

void ble_transport_ll_init(void)
{
    esp_hosted_ble_set_evt_cb(ble_hci_evt_rx, NULL);
    esp_hosted_ble_set_acl_cb(ble_hci_acl_rx, NULL);
    s_transport_ready = true;
}

void ble_transport_ll_deinit(void)
{
    esp_hosted_ble_set_evt_cb(NULL, NULL);
    esp_hosted_ble_set_acl_cb(NULL, NULL);
    s_transport_ready = false;
}

int ble_transport_to_ll_cmd_impl(void *buf)
{
    uint8_t *cmd = (uint8_t *)buf;
    size_t len;
    size_t written;

    if (cmd == NULL)
    {
        return -1;
    }

    len = 3U + cmd[2];
    written = s_transport_ready ? esp_hosted_ble_send_hci_cmd(cmd, len) : 0U;
    ble_transport_free(buf);

    return written == len ? 0 : -1;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om)
{
    uint8_t *flat;
    uint16_t len;
    size_t written;
    int rc;

    if (om == NULL)
    {
        return -1;
    }

    len = OS_MBUF_PKTLEN(om);
    flat = nimble_platform_mem_malloc(len);
    if (flat == NULL)
    {
        os_mbuf_free_chain(om);
        return -1;
    }

    rc = ble_hs_mbuf_to_flat(om, flat, len, NULL);
    os_mbuf_free_chain(om);
    if (rc != 0)
    {
        nimble_platform_mem_free(flat);
        return rc;
    }

    written = s_transport_ready ? esp_hosted_ble_send_acl(flat, len) : 0U;
    nimble_platform_mem_free(flat);
    return written == len ? 0 : -1;
}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om)
{
    if (om != NULL)
    {
        os_mbuf_free_chain(om);
    }
    return 0;
}
