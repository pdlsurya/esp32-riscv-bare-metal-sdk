/*
 * MIT License
 *
 * Copyright (c) 2025 Surya Poudel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __ESP_HOSTED_H
#define __ESP_HOSTED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ESP_HOSTED_RX_QUEUE_DEPTH
#define ESP_HOSTED_RX_QUEUE_DEPTH 8U
#endif

#ifndef ESP_HOSTED_RX_PAYLOAD_MAX
#define ESP_HOSTED_RX_PAYLOAD_MAX 1600U
#endif

/*
 * ESP-Hosted message interface types. These values match the public
 * ESP-Hosted-MCU documentation so the future SDIO/SPI transport can forward
 * the right channel metadata into this layer.
 */
typedef enum
{
    ESP_HOSTED_IF_INVALID = 0,
    ESP_HOSTED_IF_STA = 1,
    ESP_HOSTED_IF_AP = 2,
    ESP_HOSTED_IF_SERIAL = 3,
    ESP_HOSTED_IF_HCI = 4,
    ESP_HOSTED_IF_PRIV = 5,
    ESP_HOSTED_IF_TEST = 6,
    ESP_HOSTED_IF_ETH = 7,
    ESP_HOSTED_IF_MAX = 8,
} esp_hosted_if_t;

typedef enum
{
    ESP_HOSTED_HCI_PKT_NONE = 0,
    ESP_HOSTED_HCI_PKT_CMD = 1,
    ESP_HOSTED_HCI_PKT_ACL = 2,
    ESP_HOSTED_HCI_PKT_SCO = 3,
    ESP_HOSTED_HCI_PKT_EVT = 4,
} esp_hosted_hci_pkt_type_t;

/*
 * This is transport metadata, not the packed on-wire header. The SDIO/SPI
 * driver should parse the hosted header and fill this structure before passing
 * data upward through esp_hosted_rx_submit().
 */
typedef struct
{
    esp_hosted_if_t if_type;
    uint8_t if_num;
    uint8_t flags;
    uint16_t payload_len;
    uint16_t payload_offset;
    uint16_t checksum;
    uint16_t seq_num;
    uint8_t throttle_cmd;
    uint8_t hci_pkt_type;
    uint8_t priv_pkt_type;
} esp_hosted_frame_info_t;

typedef struct
{
    esp_hosted_frame_info_t info;
    size_t len;
    uint8_t payload[ESP_HOSTED_RX_PAYLOAD_MAX];
} esp_hosted_packet_t;

typedef size_t (*esp_hosted_transport_tx_fn)(const esp_hosted_frame_info_t *info,
                                             const void *payload,
                                             size_t len,
                                             void *ctx);
typedef void (*esp_hosted_transport_poll_fn)(void *ctx);

typedef struct
{
    esp_hosted_transport_tx_fn tx;
    esp_hosted_transport_poll_fn poll;
    void *ctx;
} esp_hosted_transport_t;

typedef void (*esp_hosted_rx_cb_t)(const esp_hosted_frame_info_t *info,
                                   const uint8_t *payload,
                                   size_t len,
                                   void *ctx);

bool esp_hosted_init(const esp_hosted_transport_t *transport);
void esp_hosted_deinit(void);
bool esp_hosted_is_initialized(void);
bool esp_hosted_set_transport(const esp_hosted_transport_t *transport);

/*
 * Poll the low-level transport. This does not consume queued RX packets.
 * Use esp_hosted_dispatch_rx() for callback mode or esp_hosted_rx_pop() for
 * manual polling mode.
 */
void esp_hosted_poll(void);

/*
 * Called by the future SDIO/SPI transport whenever one complete hosted frame
 * has been received and decoded.
 */
bool esp_hosted_rx_submit(const esp_hosted_frame_info_t *info, const void *payload, size_t len);

/*
 * Manual polling interface for applications that do not want callbacks.
 */
bool esp_hosted_rx_pop(esp_hosted_packet_t *out_packet);
size_t esp_hosted_rx_pending(void);
size_t esp_hosted_rx_dropped(void);

/*
 * Callback-driven interface for higher layers.
 */
void esp_hosted_lwip_set_input_cb(esp_hosted_rx_cb_t cb, void *ctx);
void esp_hosted_ctrl_set_rx_cb(esp_hosted_rx_cb_t cb, void *ctx);
void esp_hosted_ble_set_evt_cb(esp_hosted_rx_cb_t cb, void *ctx);
void esp_hosted_ble_set_acl_cb(esp_hosted_rx_cb_t cb, void *ctx);
void esp_hosted_priv_set_rx_cb(esp_hosted_rx_cb_t cb, void *ctx);
void esp_hosted_dispatch_rx(void);

/*
 * Outbound helpers for the three main paths we need on ESP32-P4:
 * - Wi-Fi data path for lwIP/netif
 * - control/RPC path for Wi-Fi remote control
 * - BLE HCI path for the host BLE stack
 */
size_t esp_hosted_lwip_tx(const void *frame, size_t len);
size_t esp_hosted_lwip_tx_to_if(esp_hosted_if_t if_type, uint8_t if_num, const void *frame, size_t len);
size_t esp_hosted_ctrl_tx(const void *buf, size_t len);
size_t esp_hosted_ble_send_hci_cmd(const void *buf, size_t len);
size_t esp_hosted_ble_send_acl(const void *buf, size_t len);
size_t esp_hosted_priv_tx(uint8_t priv_pkt_type, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
