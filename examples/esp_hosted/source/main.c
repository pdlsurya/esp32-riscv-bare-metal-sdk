#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ble_nimble.h"
#include "delay.h"
#include "esp_hosted.h"
#include "esp_hosted_ctrl.h"
#include "esp_hosted_sdio.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/tcp.h"
#include "lwip_hosted.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"
#include "usb_serial.h"

#define BLE_DEVICE_NAME "ESP32-P4 Hosted BLE"
#define BLE_DEMO_SERVICE_UUID 0xFFF0U
#define BLE_DEMO_TEXT_CHAR_UUID 0xFFF1U
#define BLE_DEMO_VALUE_MAX_LEN 63U
#define BLE_ADV_RESTART_DELAY_MS 100U
#define HOSTED_PRIV_PKT_EVENT 0x33U
#define HOSTED_PRIV_EVENT_INIT 0x22U
#define HOSTED_PRIV_CAPABILITY 0x11U
#define HOSTED_PRIV_FIRMWARE_CHIP_ID 0x12U
#define HOSTED_PRIV_CAP_EXT 0x16U
#define HOSTED_PRIV_TRANS_SDIO_MODE 0x18U
#define HOSTED_HOST_CAPABILITIES 0x44U
#define HOSTED_HOST_SLAVE_CHIP_ID 0x45U
#define HOSTED_HOST_RAW_TP 0x46U
#define HOSTED_HOST_THROTTLE_HIGH 0x47U
#define HOSTED_HOST_THROTTLE_LOW 0x48U
#define WIFI_STA_SSID "Surya_303184"
#define WIFI_STA_PASSWORD "ptk0405GJH@303184"
#define WIFI_HOSTNAME "esp32-p4"
#define WIFI_CTRL_TIMEOUT_MS 4000U
#define TCP_RX_PORT 3333U
#define TCP_RX_MAX_PRINT 128U

static bool s_ble_adv_start_pending = true;
static bool s_ble_adv_started;
static bool s_ble_sync_announced;
static uint8_t s_ble_addr_type;
static uint16_t s_ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_ble_adv_retry_delay_ms;
static char s_demo_text[BLE_DEMO_VALUE_MAX_LEN + 1U] = "hello from esp32-p4";
static uint16_t s_demo_text_len = (uint16_t)(sizeof("hello from esp32-p4") - 1U);
static uint16_t s_demo_text_val_handle;
static bool s_hosted_init_seen;
static bool s_hosted_init_replied;
static uint8_t s_hosted_slave_cap;
static uint8_t s_hosted_slave_chip_id = 0xFFU;
static uint32_t s_hosted_slave_ext_cap;
static uint8_t s_hosted_slave_sdio_mode;
static bool s_wifi_enabled;
static bool s_wifi_started;
static bool s_wifi_ip_announced;
static ip4_addr_t s_wifi_last_addr;
static bool s_tcp_server_started;
static struct tcp_pcb *s_tcp_listen_pcb;
static struct tcp_pcb *s_tcp_client_pcb;
static ble_uuid16_t s_demo_service_uuid = BLE_UUID16_INIT(BLE_DEMO_SERVICE_UUID);
static ble_uuid16_t s_demo_text_uuid = BLE_UUID16_INIT(BLE_DEMO_TEXT_CHAR_UUID);

static int ble_demo_text_access(uint16_t conn_handle,
                                uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt,
                                void *arg);
static bool tcp_rx_server_start(void);
static void tcp_rx_server_stop(void);

static void tcp_client_release(struct tcp_pcb *pcb)
{
    if (pcb == NULL)
    {
        return;
    }

    tcp_arg(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_err(pcb, NULL);
    tcp_poll(pcb, NULL, 0U);
}

static void tcp_client_close(struct tcp_pcb *pcb)
{
    err_t rc;

    if (pcb == NULL)
    {
        return;
    }

    tcp_client_release(pcb);
    rc = tcp_close(pcb);
    if (rc != ERR_OK)
    {
        tcp_abort(pcb);
    }
}

static void tcp_client_err_cb(void *arg, err_t err)
{
    (void)arg;
    (void)err;
    s_tcp_client_pcb = NULL;
    serial_printf("[tcp] client closed\n");
}

static err_t tcp_client_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    uint8_t buf[TCP_RX_MAX_PRINT + 1U];
    u16_t copy_len;

    (void)arg;

    if (err != ERR_OK)
    {
        if (p != NULL)
        {
            pbuf_free(p);
        }
        return err;
    }

    if (p == NULL)
    {
        if (s_tcp_client_pcb == tpcb)
        {
            s_tcp_client_pcb = NULL;
        }
        tcp_client_close(tpcb);
        serial_printf("[tcp] client disconnected\n");
        return ERR_OK;
    }

    copy_len = p->tot_len;
    if (copy_len > TCP_RX_MAX_PRINT)
    {
        copy_len = TCP_RX_MAX_PRINT;
    }

    if (copy_len > 0U)
    {
        pbuf_copy_partial(p, buf, copy_len, 0U);
    }
    buf[copy_len] = '\0';
    serial_printf("[tcp] rx len=%u msg=\"%s\"\n", (unsigned)p->tot_len, buf);

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_server_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    (void)arg;

    if (err != ERR_OK || newpcb == NULL)
    {
        return ERR_VAL;
    }

    if (s_tcp_client_pcb != NULL)
    {
        tcp_client_close(s_tcp_client_pcb);
        s_tcp_client_pcb = NULL;
    }

    s_tcp_client_pcb = newpcb;
    tcp_arg(newpcb, NULL);
    tcp_recv(newpcb, tcp_client_recv_cb);
    tcp_err(newpcb, tcp_client_err_cb);
    serial_printf("[tcp] client connected\n");
    return ERR_OK;
}

static bool tcp_rx_server_start(void)
{
    struct tcp_pcb *pcb;
    struct tcp_pcb *listen_pcb;
    err_t rc;

    if (s_tcp_server_started)
    {
        return true;
    }

    pcb = tcp_new();
    if (pcb == NULL)
    {
        serial_printf("[tcp] alloc failed\n");
        return false;
    }

    rc = tcp_bind(pcb, IP_ADDR_ANY, TCP_RX_PORT);
    if (rc != ERR_OK)
    {
        tcp_close(pcb);
        serial_printf("[tcp] bind failed rc=%d\n", (int)rc);
        return false;
    }

    listen_pcb = tcp_listen_with_backlog(pcb, 1U);
    if (listen_pcb == NULL)
    {
        tcp_abort(pcb);
        serial_printf("[tcp] listen failed\n");
        return false;
    }

    s_tcp_listen_pcb = listen_pcb;
    tcp_accept(s_tcp_listen_pcb, tcp_server_accept_cb);
    s_tcp_server_started = true;
    serial_printf("[tcp] listening on port %u\n", (unsigned)TCP_RX_PORT);
    return true;
}

static void tcp_rx_server_stop(void)
{
    if (s_tcp_client_pcb != NULL)
    {
        tcp_client_close(s_tcp_client_pcb);
        s_tcp_client_pcb = NULL;
    }

    if (s_tcp_listen_pcb != NULL)
    {
        tcp_accept(s_tcp_listen_pcb, NULL);
        tcp_close(s_tcp_listen_pcb);
        s_tcp_listen_pcb = NULL;
    }

    s_tcp_server_started = false;
}

static const struct ble_gatt_chr_def s_demo_chrs[] = {
    {
        .uuid = &s_demo_text_uuid.u,
        .access_cb = ble_demo_text_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        .val_handle = &s_demo_text_val_handle,
    },
    {0}
};

static const struct ble_gatt_svc_def s_demo_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_demo_service_uuid.u,
        .characteristics = s_demo_chrs,
    },
    {0}
};

static void ble_schedule_adv_restart(uint16_t delay_ms)
{
    s_ble_adv_start_pending = true;
    s_ble_adv_started = false;
    s_ble_adv_retry_delay_ms = delay_ms;
}

static void ble_on_host_reset(int reason)
{
    s_ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_ble_sync_announced = false;
    ble_schedule_adv_restart(BLE_ADV_RESTART_DELAY_MS);
    serial_printf("[ble] host reset reason=%d\n", reason);
}

static void ble_on_host_sync(void)
{
    s_ble_sync_announced = false;
    ble_schedule_adv_restart(0U);
}

static bool hosted_version_at_least(uint32_t major,
                                    uint32_t minor,
                                    uint32_t patch,
                                    uint32_t need_major,
                                    uint32_t need_minor,
                                    uint32_t need_patch)
{
    if (major != need_major)
    {
        return major > need_major;
    }
    if (minor != need_minor)
    {
        return minor > need_minor;
    }
    return patch >= need_patch;
}

static int hosted_probe_fw_version_with_retry(uint32_t *major,
                                              uint32_t *minor,
                                              uint32_t *patch)
{
    uint32_t attempt;

    for (attempt = 0; attempt < 3U; attempt++)
    {
        int rc;
        uint32_t settle_ms = (attempt == 0U) ? 250U : 500U;
        uint32_t i;

        for (i = 0; i < settle_ms; i++)
        {
            esp_hosted_poll();
            esp_hosted_dispatch_rx();
            delay_ms(1U);
        }

        rc = esp_hosted_ctrl_get_coprocessor_fw_version(1500U, major, minor, patch);
        if (rc == ESP_HOSTED_CTRL_OK)
        {
            return rc;
        }
    }

    return ESP_HOSTED_CTRL_ERR_TIMEOUT;
}

static int hosted_bt_feature_with_retry(bool enable)
{
    uint32_t attempt;

    for (attempt = 0; attempt < 3U; attempt++)
    {
        int rc;
        uint32_t settle_ms = (attempt == 0U) ? 150U : 400U;
        uint32_t i;

        for (i = 0; i < settle_ms; i++)
        {
            esp_hosted_poll();
            esp_hosted_dispatch_rx();
            delay_ms(1U);
        }

        rc = enable ? esp_hosted_ctrl_bt_enable(3000U) : esp_hosted_ctrl_bt_init(3000U);
        if (rc == ESP_HOSTED_CTRL_OK)
        {
            return rc;
        }
    }

    return ESP_HOSTED_CTRL_ERR_TIMEOUT;
}

static bool wifi_demo_requested(void)
{
    return WIFI_STA_SSID[0] != '\0';
}

static void wifi_netif_status_cb(struct netif *netif)
{
    char ip_buf[20];

    if (netif == NULL)
    {
        return;
    }

    if (!ip4_addr_isany_val(*netif_ip4_addr(netif)))
    {
        if (!s_wifi_ip_announced || !ip4_addr_cmp(netif_ip4_addr(netif), &s_wifi_last_addr))
        {
            ip4_addr_copy(s_wifi_last_addr, *netif_ip4_addr(netif));
            ip4addr_ntoa_r(netif_ip4_addr(netif), ip_buf, sizeof(ip_buf));
            serial_printf("[wifi] ip=%s\n", ip_buf);
            (void)tcp_rx_server_start();
            s_wifi_ip_announced = true;
        }
    }
    else if (s_wifi_ip_announced)
    {
        ip4_addr_set_zero(&s_wifi_last_addr);
        s_wifi_ip_announced = false;
        tcp_rx_server_stop();
        serial_printf("[wifi] ip cleared\n");
    }
}

static void hosted_wifi_event_cb(esp_hosted_wifi_event_t event, const void *event_data, void *ctx)
{
    (void)ctx;

    switch (event)
    {
        case ESP_HOSTED_WIFI_EVENT_STA_CONNECTED:
        {
            const esp_hosted_wifi_sta_connected_t *connected = (const esp_hosted_wifi_sta_connected_t *)event_data;
            serial_printf("[wifi] sta connected ssid=\"%.*s\" channel=%lu aid=%lu\n",
                          connected != NULL ? (int)connected->ssid_len : 0,
                          connected != NULL ? (const char *)connected->ssid : "",
                          connected != NULL ? (unsigned long)connected->channel : 0UL,
                          connected != NULL ? (unsigned long)connected->aid : 0UL);
            lwip_hosted_set_link_up();
            break;
        }

        case ESP_HOSTED_WIFI_EVENT_STA_DISCONNECTED:
        {
            const esp_hosted_wifi_sta_disconnected_t *disconnected = (const esp_hosted_wifi_sta_disconnected_t *)event_data;
            serial_printf("[wifi] sta disconnected reason=%lu rssi=%ld\n",
                          disconnected != NULL ? (unsigned long)disconnected->reason : 0UL,
                          disconnected != NULL ? (long)disconnected->rssi : 0L);
            lwip_hosted_set_link_down();
            break;
        }

        default:
            break;
    }
}

static bool hosted_wifi_start(void)
{
    lwip_hosted_config_t lwip_cfg;
    struct netif *netif;
    uint8_t mac[6];
    bool lwip_ready = false;
    int rc;

    esp_hosted_wifi_set_event_cb(hosted_wifi_event_cb, NULL);

    /*
     * Give co-processor control path a short settle window after init
     * handshake before first Wi-Fi RPC.
     */
    {
        uint32_t settle_ms = 250U;
        while (settle_ms-- > 0U)
        {
            esp_hosted_poll();
            esp_hosted_dispatch_rx();
            delay_ms(1U);
        }
    }

    lwip_hosted_get_default_config(&lwip_cfg);
    lwip_cfg.hostname = WIFI_HOSTNAME;
    lwip_cfg.hwaddr_valid = false;

    if (!lwip_hosted_init(&lwip_cfg))
    {
        serial_printf("[wifi] lwip init failed\n");
        return false;
    }
    lwip_ready = true;

    netif = lwip_hosted_get_netif();
    if (netif != NULL)
    {
        netif_set_status_callback(netif, wifi_netif_status_cb);
    }
    ip4_addr_set_zero(&s_wifi_last_addr);
    s_wifi_ip_announced = false;

    rc = esp_hosted_wifi_init_default(WIFI_CTRL_TIMEOUT_MS);
    if (rc != ESP_HOSTED_CTRL_OK)
    {
        serial_printf("[wifi] wifi init failed rc=%d\n", rc);
        goto fail;
    }

    rc = esp_hosted_wifi_set_mode(ESP_HOSTED_WIFI_MODE_STA, WIFI_CTRL_TIMEOUT_MS);
    if (rc != ESP_HOSTED_CTRL_OK)
    {
        serial_printf("[wifi] set mode failed rc=%d\n", rc);
        goto fail;
    }

    rc = esp_hosted_wifi_get_mac(ESP_HOSTED_WIFI_IF_STA, mac, 1500U);
    if (rc == ESP_HOSTED_CTRL_OK)
    {
        serial_printf("[wifi] sta mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        lwip_hosted_set_mac(mac);
    }
    else
    {
        serial_printf("[wifi] get mac failed rc=%d (0x%08lx)\n",
                      rc,
                      (unsigned long)(uint32_t)rc);
    }

    rc = esp_hosted_wifi_set_sta_config(WIFI_STA_SSID, WIFI_STA_PASSWORD, WIFI_CTRL_TIMEOUT_MS);
    if (rc != ESP_HOSTED_CTRL_OK)
    {
        serial_printf("[wifi] set config failed rc=%d\n", rc);
        goto fail;
    }

    rc = esp_hosted_wifi_start(WIFI_CTRL_TIMEOUT_MS);
    if (rc != ESP_HOSTED_CTRL_OK)
    {
        serial_printf("[wifi] wifi start failed rc=%d\n", rc);
        goto fail;
    }

    rc = esp_hosted_wifi_connect(WIFI_CTRL_TIMEOUT_MS);
    if (rc != ESP_HOSTED_CTRL_OK)
    {
        serial_printf("[wifi] wifi connect failed rc=%d\n", rc);
        goto fail;
    }

    serial_printf("[wifi] connecting to \"%s\" with lwIP DHCP\n", WIFI_STA_SSID);
    return true;

fail:
    if (lwip_ready)
    {
        lwip_hosted_deinit();
    }
    return false;
}

static bool sdio_try_attach(esp_hosted_sdio_config_t *cfg, const char *label)
{
    if (cfg == NULL)
    {
        return false;
    }

    serial_printf("[sdio] try %s slot=%u width=%u clk=%u kHz pullups=%u\n",
                  label,
                  (unsigned)cfg->slot,
                  (unsigned)cfg->bus_width,
                  (unsigned)cfg->clock_khz,
                  (unsigned)cfg->use_internal_pullups);
    return esp_hosted_sdio_attach(cfg);
}

static bool hosted_send_init_response(uint8_t chip_id)
{
    uint8_t buf[17];
    size_t written;

    memset(buf, 0, sizeof(buf));
    buf[0] = HOSTED_PRIV_EVENT_INIT;
    buf[1] = 15U;
    buf[2] = HOSTED_HOST_CAPABILITIES;
    buf[3] = 1U;
    buf[4] = 0U;
    buf[5] = HOSTED_HOST_SLAVE_CHIP_ID;
    buf[6] = 1U;
    buf[7] = chip_id;
    buf[8] = HOSTED_HOST_RAW_TP;
    buf[9] = 1U;
    buf[10] = 0U;
    buf[11] = HOSTED_HOST_THROTTLE_HIGH;
    buf[12] = 1U;
    buf[13] = 0U;
    buf[14] = HOSTED_HOST_THROTTLE_LOW;
    buf[15] = 1U;
    buf[16] = 0U;

    written = esp_hosted_priv_tx(HOSTED_PRIV_PKT_EVENT, buf, sizeof(buf));
    if (written != sizeof(buf))
    {
        serial_printf("[hosted] init response failed wrote=%u need=%u\n",
                      (unsigned)written,
                      (unsigned)sizeof(buf));
        return false;
    }

    serial_printf("[hosted] init response sent chip=0x%02x\n", chip_id);
    return true;
}

static void hosted_priv_rx(const esp_hosted_frame_info_t *info,
                           const uint8_t *payload,
                           size_t len,
                           void *ctx)
{
    const uint8_t *pos;
    size_t remaining;

    (void)info;
    (void)ctx;

    if (payload == NULL || len < 2U)
    {
        return;
    }

    if (payload[0] != HOSTED_PRIV_EVENT_INIT)
    {
        serial_printf("[hosted] priv event type=0x%02x len=%u\n",
                      (unsigned)payload[0],
                      (unsigned)len);
        return;
    }

    if (len < (size_t)(2U + payload[1]))
    {
        serial_printf("[hosted] short init event len=%u need=%u\n",
                      (unsigned)len,
                      (unsigned)(2U + payload[1]));
        return;
    }

    s_hosted_init_seen = true;
    pos = payload + 2U;
    remaining = payload[1];

    while (remaining >= 2U)
    {
        uint8_t tag = pos[0];
        uint8_t tag_len = pos[1];

        if ((size_t)(tag_len + 2U) > remaining)
        {
            break;
        }

        if (tag == HOSTED_PRIV_CAPABILITY && tag_len >= 1U)
        {
            s_hosted_slave_cap = pos[2];
        }
        else if (tag == HOSTED_PRIV_FIRMWARE_CHIP_ID && tag_len >= 1U)
        {
            s_hosted_slave_chip_id = pos[2];
        }
        else if (tag == HOSTED_PRIV_CAP_EXT && tag_len >= 4U)
        {
            s_hosted_slave_ext_cap = ((uint32_t)pos[2]) |
                                     ((uint32_t)pos[3] << 8) |
                                     ((uint32_t)pos[4] << 16) |
                                     ((uint32_t)pos[5] << 24);
        }
        else if (tag == HOSTED_PRIV_TRANS_SDIO_MODE && tag_len >= 1U)
        {
            s_hosted_slave_sdio_mode = pos[2];
        }

        pos += (size_t)tag_len + 2U;
        remaining -= (size_t)tag_len + 2U;
    }

    serial_printf("[hosted] init cap=0x%02x ext=0x%08lx chip=0x%02x sdio=%s\n",
                  (unsigned)s_hosted_slave_cap,
                  (unsigned long)s_hosted_slave_ext_cap,
                  (unsigned)s_hosted_slave_chip_id,
                  s_hosted_slave_sdio_mode ? "streaming" : "packet");

    if (!s_hosted_init_replied)
    {
        s_hosted_init_replied = hosted_send_init_response(s_hosted_slave_chip_id);
    }
}

static int ble_demo_text_access(uint16_t conn_handle,
                                uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt,
                                void *arg)
{
    uint16_t value_len = 0U;
    int rc;

    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt == NULL || ctxt->om == NULL)
    {
        return BLE_ATT_ERR_UNLIKELY;
    }

    switch (ctxt->op)
    {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            rc = os_mbuf_append(ctxt->om, s_demo_text, s_demo_text_len);
            return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            if (OS_MBUF_PKTLEN(ctxt->om) > BLE_DEMO_VALUE_MAX_LEN)
            {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            rc = ble_hs_mbuf_to_flat(ctxt->om, s_demo_text, BLE_DEMO_VALUE_MAX_LEN, &value_len);
            if (rc != 0)
            {
                return BLE_ATT_ERR_UNLIKELY;
            }

            s_demo_text[value_len] = '\0';
            s_demo_text_len = value_len;
            serial_printf("[gatt] text=\"%s\"\n", s_demo_text);
            return 0;

        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

static bool ble_demo_gatt_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(s_demo_svcs);
    if (rc != 0)
    {
        serial_printf("[gatt] count cfg failed rc=%d\n", rc);
        return false;
    }

    rc = ble_gatts_add_svcs(s_demo_svcs);
    if (rc != 0)
    {
        serial_printf("[gatt] add svcs failed rc=%d\n", rc);
        return false;
    }

    serial_printf("[gatt] service=0x%04x char=0x%04x read/write text\n",
                  (unsigned)BLE_DEMO_SERVICE_UUID,
                  (unsigned)BLE_DEMO_TEXT_CHAR_UUID);
    return true;
}

static int ble_gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    if (event == NULL)
    {
        return 0;
    }

    switch (event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0)
            {
                s_ble_conn_handle = event->connect.conn_handle;
                s_ble_adv_start_pending = false;
                s_ble_adv_started = false;
                s_ble_adv_retry_delay_ms = 0U;
                serial_printf("[ble] connected handle=%u\n", event->connect.conn_handle);
            }
            else
            {
                s_ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
                serial_printf("[ble] connect failed status=%d\n", event->connect.status);
                ble_schedule_adv_restart(BLE_ADV_RESTART_DELAY_MS);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            s_ble_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            serial_printf("[ble] disconnected reason=%d\n", event->disconnect.reason);
            ble_schedule_adv_restart(BLE_ADV_RESTART_DELAY_MS);
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            s_ble_adv_started = false;
            if (s_ble_conn_handle == BLE_HS_CONN_HANDLE_NONE)
            {
                serial_printf("[ble] adv complete reason=%d\n", event->adv_complete.reason);
                ble_schedule_adv_restart(BLE_ADV_RESTART_DELAY_MS);
            }
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            serial_printf("[ble] conn update status=%d handle=%u\n",
                          event->conn_update.status,
                          event->conn_update.conn_handle);
            break;

        case BLE_GAP_EVENT_MTU:
            serial_printf("[ble] mtu=%u handle=%u\n",
                          event->mtu.value,
                          event->mtu.conn_handle);
            break;

        default:
            break;
    }

    return 0;
}

static bool ble_start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    struct ble_gap_adv_params adv_params = {0};
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0)
    {
        serial_printf("[ble] ensure addr failed rc=%d\n", rc);
        return false;
    }

    rc = ble_hs_id_infer_auto(0, &s_ble_addr_type);
    if (rc != 0)
    {
        serial_printf("[ble] infer addr type failed rc=%d\n", rc);
        return false;
    }

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids16 = &s_demo_service_uuid;
    fields.num_uuids16 = 1U;
    fields.uuids16_is_complete = 1;
    fields.name = (const uint8_t *)BLE_DEVICE_NAME;
    fields.name_len = (uint8_t)strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        serial_printf("[ble] adv fields failed rc=%d\n", rc);
        return false;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_ble_addr_type,
                           NULL,
                           BLE_HS_FOREVER,
                           &adv_params,
                           ble_gap_event_cb,
                           NULL);
    if (rc != 0)
    {
        serial_printf("[ble] adv start failed rc=%d\n", rc);
        s_ble_adv_retry_delay_ms = 250U;
        return false;
    }

    serial_printf("[ble] advertising as \"%s\"\n", BLE_DEVICE_NAME);
    s_ble_adv_started = true;
    s_ble_adv_start_pending = false;
    s_ble_adv_retry_delay_ms = 0U;
    return true;
}

int main(void)
{
    esp_hosted_sdio_config_t sdio_cfg;
    uint32_t hosted_wait_ms = 5000U;
    ble_nimble_config_t ble_cfg = {0};
    bool need_bt_control = false;
    uint32_t fw_major = 0U;
    uint32_t fw_minor = 0U;
    uint32_t fw_patch = 0U;
    int rc;

    delay_ms(2000);
    serial_printf("ESP32-P4 esp-hosted lwIP example\n");
    if (wifi_demo_requested())
    {
        serial_printf("[wifi] enabled ssid=\"%s\"\n", WIFI_STA_SSID);
        s_wifi_enabled = true;
    }
    else
    {
        serial_printf("[wifi] disabled, set WIFI_STA_SSID/WIFI_STA_PASSWORD in main.c to enable lwIP\n");
    }

    /*
     * Keep this example dedicated to hosted BLE. The current sdFat32 path and
     * esp_hosted_sdio both control the SDMMC peripheral directly.
     */
    esp_hosted_sdio_get_default_config(&sdio_cfg);
    if (!sdio_try_attach(&sdio_cfg, "default"))
    {
        sdio_cfg.use_internal_pullups = true;
        if (!sdio_try_attach(&sdio_cfg, "pullups"))
        {
            sdio_cfg.bus_width = 1U;
            sdio_cfg.clock_khz = 5000U;
            if (!sdio_try_attach(&sdio_cfg, "safe-1bit"))
            {
                serial_printf("[sdio] attach failed\n");
                while (1)
                {
                    delay_ms(1000);
                }
            }
        }
    }

    serial_printf("[sdio] attached slot=%u width=%u clk=%u kHz\n",
                  (unsigned)sdio_cfg.slot,
                  (unsigned)sdio_cfg.bus_width,
                  (unsigned)sdio_cfg.clock_khz);

    if (!esp_hosted_ctrl_init())
    {
        serial_printf("[hosted] ctrl init failed\n");
        while (1)
        {
            delay_ms(1000);
        }
    }

    esp_hosted_priv_set_rx_cb(hosted_priv_rx, NULL);
    while (hosted_wait_ms-- > 0U && (!s_hosted_init_replied || !esp_hosted_ctrl_is_ready()))
    {
        esp_hosted_poll();
        esp_hosted_dispatch_rx();
        delay_ms(1U);
    }
    if (!s_hosted_init_replied || !esp_hosted_ctrl_is_ready())
    {
        serial_printf("[hosted] init/control timeout seen=%u replied=%u ctrl_ready=%u\n",
                      (unsigned)s_hosted_init_seen,
                      (unsigned)s_hosted_init_replied,
                      (unsigned)esp_hosted_ctrl_is_ready());
        while (1)
        {
            delay_ms(1000);
        }
    }

    rc = hosted_probe_fw_version_with_retry(&fw_major, &fw_minor, &fw_patch);
    if (rc == ESP_HOSTED_CTRL_OK)
    {
        serial_printf("[hosted] fw version %lu.%lu.%lu\n",
                      (unsigned long)fw_major,
                      (unsigned long)fw_minor,
                      (unsigned long)fw_patch);
        need_bt_control = hosted_version_at_least(fw_major, fw_minor, fw_patch, 2U, 5U, 2U);
    }
    else
    {
        serial_printf("[hosted] fw version unavailable rc=%d\n", rc);
        serial_printf("[hosted] assuming legacy slave firmware, BT controller assumed enabled\n");
    }

    if (need_bt_control)
    {
        rc = hosted_bt_feature_with_retry(false);
        if (rc != ESP_HOSTED_CTRL_OK)
        {
            serial_printf("[hosted] bt init failed rc=%d\n", rc);
            while (1)
            {
                delay_ms(1000);
            }
        }
        serial_printf("[hosted] bt init ok\n");

        rc = hosted_bt_feature_with_retry(true);
        if (rc != ESP_HOSTED_CTRL_OK)
        {
            serial_printf("[hosted] bt enable failed rc=%d\n", rc);
            while (1)
            {
                delay_ms(1000);
            }
        }
        serial_printf("[hosted] bt enable ok\n");
    }

    ble_cfg.reset_cb = ble_on_host_reset;
    ble_cfg.sync_cb = ble_on_host_sync;
    rc = ble_nimble_init(&ble_cfg);
    if (rc != ESP_OK)
    {
        serial_printf("[ble] init failed rc=%d\n", rc);
        while (1)
        {
            delay_ms(1000);
        }
    }
    if (!ble_demo_gatt_init())
    {
        serial_printf("[ble] gatt init failed\n");
        while (1)
        {
            delay_ms(1000);
        }
    }
    serial_printf("[ble] waiting for controller sync...\n");

    if (s_wifi_enabled)
    {
        s_wifi_started = hosted_wifi_start();
        if (!s_wifi_started)
        {
            serial_printf("[wifi] hosted Wi-Fi bring-up failed\n");
            serial_printf("[wifi] lwIP runtime disabled\n");
        }
    }

    while (1)
    {
        if (s_wifi_started)
        {
            lwip_hosted_poll();
        }
        else
        {
            esp_hosted_poll();
            esp_hosted_dispatch_rx();
        }

        ble_nimble_poll();
        if (ble_nimble_is_synced())
        {
            if (!s_ble_sync_announced)
            {
                serial_printf("[ble] synced\n");
                s_ble_sync_announced = true;
            }

            if (s_ble_adv_start_pending)
            {
                if (s_ble_adv_retry_delay_ms != 0U)
                {
                    s_ble_adv_retry_delay_ms--;
                }
                else
                {
                    (void)ble_start_advertising();
                }
            }
        }

        delay_ms(1U);
    }

    return 0;
}
