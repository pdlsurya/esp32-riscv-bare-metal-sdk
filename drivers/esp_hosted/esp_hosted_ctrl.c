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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "delay.h"
#include "esp_hosted.h"
#include "esp_hosted_ctrl.h"
#include "usb_serial.h"

#define PB_WIRE_VARINT 0U
#define PB_WIRE_LEN 2U

#define PROTO_PSER_TLV_T_EPNAME 0x01U
#define PROTO_PSER_TLV_T_DATA 0x02U

#define RPC_EP_NAME_RSP "RPCRsp"
#define RPC_EP_NAME_EVT "RPCEvt"

#define RPC_TYPE_REQ 1U
#define RPC_TYPE_RESP 2U
#define RPC_TYPE_EVENT 3U

#define RPC_ID_REQ_SET_WIFI_MODE 260U
#define RPC_ID_REQ_GET_MAC_ADDRESS 257U
#define RPC_ID_REQ_WIFI_INIT 278U
#define RPC_ID_REQ_WIFI_START 280U
#define RPC_ID_REQ_WIFI_CONNECT 282U
#define RPC_ID_REQ_WIFI_DISCONNECT 283U
#define RPC_ID_REQ_WIFI_SET_CONFIG 284U
#define RPC_ID_REQ_GET_COPROCESSOR_FW_VERSION 350U
#define RPC_ID_REQ_FEATURE_CONTROL 387U

#define RPC_ID_RESP_GET_MAC_ADDRESS 513U
#define RPC_ID_RESP_SET_WIFI_MODE 516U
#define RPC_ID_RESP_WIFI_INIT 534U
#define RPC_ID_RESP_WIFI_START 536U
#define RPC_ID_RESP_WIFI_CONNECT 538U
#define RPC_ID_RESP_WIFI_DISCONNECT 539U
#define RPC_ID_RESP_WIFI_SET_CONFIG 540U
#define RPC_ID_RESP_GET_COPROCESSOR_FW_VERSION 606U
#define RPC_ID_RESP_FEATURE_CONTROL 643U

#define RPC_ID_EVENT_ESP_INIT 769U
#define RPC_ID_EVENT_STA_CONNECTED 775U
#define RPC_ID_EVENT_STA_DISCONNECTED 776U

#define RPC_FIELD_MSG_TYPE 1U
#define RPC_FIELD_MSG_ID 2U
#define RPC_FIELD_UID 3U
#define RPC_FIELD_REQ_GET_MAC_ADDRESS RPC_ID_REQ_GET_MAC_ADDRESS
#define RPC_FIELD_RESP_GET_MAC_ADDRESS RPC_ID_RESP_GET_MAC_ADDRESS
#define RPC_FIELD_REQ_SET_WIFI_MODE RPC_ID_REQ_SET_WIFI_MODE
#define RPC_FIELD_RESP_SET_WIFI_MODE RPC_ID_RESP_SET_WIFI_MODE
#define RPC_FIELD_REQ_WIFI_INIT RPC_ID_REQ_WIFI_INIT
#define RPC_FIELD_RESP_WIFI_INIT RPC_ID_RESP_WIFI_INIT
#define RPC_FIELD_REQ_WIFI_START RPC_ID_REQ_WIFI_START
#define RPC_FIELD_RESP_WIFI_START RPC_ID_RESP_WIFI_START
#define RPC_FIELD_REQ_WIFI_CONNECT RPC_ID_REQ_WIFI_CONNECT
#define RPC_FIELD_RESP_WIFI_CONNECT RPC_ID_RESP_WIFI_CONNECT
#define RPC_FIELD_REQ_WIFI_DISCONNECT RPC_ID_REQ_WIFI_DISCONNECT
#define RPC_FIELD_RESP_WIFI_DISCONNECT RPC_ID_RESP_WIFI_DISCONNECT
#define RPC_FIELD_REQ_WIFI_SET_CONFIG RPC_ID_REQ_WIFI_SET_CONFIG
#define RPC_FIELD_RESP_WIFI_SET_CONFIG RPC_ID_RESP_WIFI_SET_CONFIG
#define RPC_FIELD_REQ_GET_COPROCESSOR_FW_VERSION RPC_ID_REQ_GET_COPROCESSOR_FW_VERSION
#define RPC_FIELD_RESP_GET_COPROCESSOR_FW_VERSION RPC_ID_RESP_GET_COPROCESSOR_FW_VERSION
#define RPC_FIELD_REQ_FEATURE_CONTROL RPC_ID_REQ_FEATURE_CONTROL
#define RPC_FIELD_RESP_FEATURE_CONTROL RPC_ID_RESP_FEATURE_CONTROL
#define RPC_FIELD_EVENT_ESP_INIT RPC_ID_EVENT_ESP_INIT
#define RPC_FIELD_EVENT_STA_CONNECTED RPC_ID_EVENT_STA_CONNECTED
#define RPC_FIELD_EVENT_STA_DISCONNECTED RPC_ID_EVENT_STA_DISCONNECTED

#define FEATURE_FIELD_FEATURE 1U
#define FEATURE_FIELD_COMMAND 2U
#define FEATURE_FIELD_OPTION 3U

#define ESP_INIT_FIELD_CP_RESET_REASON 2U

#define RESP_FIELD_RESP 1U
#define RESP_FIELD_FEATURE 2U
#define RESP_FIELD_COMMAND 3U
#define RESP_FIELD_OPTION 4U

#define WIFI_GET_MAC_FIELD_MODE 1U
#define WIFI_SET_MODE_FIELD_MODE 1U
#define WIFI_SET_CONFIG_FIELD_IFACE 1U
#define WIFI_SET_CONFIG_FIELD_CFG 2U
#define WIFI_CONFIG_FIELD_STA 2U
#define WIFI_STA_CFG_FIELD_SSID 1U
#define WIFI_STA_CFG_FIELD_PASSWORD 2U
#define WIFI_STA_CFG_FIELD_THRESHOLD 9U
#define WIFI_STA_CFG_FIELD_PMF_CFG 10U
#define WIFI_STA_CFG_FIELD_SAE_PWE_H2E 12U
#define WIFI_SCAN_THRESHOLD_FIELD_RSSI 1U
#define WIFI_SCAN_THRESHOLD_FIELD_AUTHMODE 2U
#define WIFI_PMF_CFG_FIELD_CAPABLE 1U
#define WIFI_PMF_CFG_FIELD_REQUIRED 2U
#define WIFI_INIT_FIELD_CFG 1U
#define WIFI_EVENT_FIELD_RESP 1U
#define WIFI_EVENT_FIELD_DATA 2U
#define WIFI_EVENT_STA_SSID 1U
#define WIFI_EVENT_STA_SSID_LEN 2U
#define WIFI_EVENT_STA_BSSID 3U
#define WIFI_EVENT_STA_CHANNEL 4U
#define WIFI_EVENT_STA_AUTHMODE 5U
#define WIFI_EVENT_STA_AID 6U
#define WIFI_EVENT_STA_REASON 4U
#define WIFI_EVENT_STA_RSSI 5U
#define WIFI_MAC_RESP_FIELD_MAC 1U
#define WIFI_MAC_RESP_FIELD_RESP 2U

#define WIFI_INIT_DEFAULT_STATIC_RX_BUF_NUM 10U
#define WIFI_INIT_DEFAULT_DYNAMIC_RX_BUF_NUM 32U
#define WIFI_INIT_DEFAULT_TX_BUF_TYPE 1U
#define WIFI_INIT_DEFAULT_DYNAMIC_TX_BUF_NUM 32U
#define WIFI_INIT_DEFAULT_AMPDU_RX_ENABLE 1U
#define WIFI_INIT_DEFAULT_AMPDU_TX_ENABLE 1U
#define WIFI_INIT_DEFAULT_NVS_ENABLE 1U
#define WIFI_INIT_DEFAULT_RX_BA_WIN 6U
#define WIFI_INIT_DEFAULT_BEACON_MAX_LEN 752U
#define WIFI_INIT_DEFAULT_MGMT_SBUF_NUM 32U
#define WIFI_INIT_DEFAULT_RX_MGMT_BUF_TYPE 1U
#define WIFI_INIT_DEFAULT_TX_HETB_QUEUE_NUM 1U
#define WIFI_INIT_CONFIG_MAGIC 0x1F2F3F4FU
#define WIFI_AUTH_OPEN 0U
#define WIFI_AUTH_WPA2_PSK 3U
#define WIFI_SAE_PWE_UNSPECIFIED 0U

#ifndef ESP_HOSTED_CTRL_DEBUG
#define ESP_HOSTED_CTRL_DEBUG 0
#endif

#if ESP_HOSTED_CTRL_DEBUG
#define ESP_HOSTED_CTRL_LOG(...) printf(__VA_ARGS__)
#else
#define ESP_HOSTED_CTRL_LOG(...) ((void)0)
#endif

#define ESP_HOSTED_CTRL_ERR(...) printf("[hosted_ctrl] " __VA_ARGS__)

typedef enum
{
    ESP_HOSTED_CTRL_WAIT_NONE = 0,
    ESP_HOSTED_CTRL_WAIT_FEATURE_CONTROL,
    ESP_HOSTED_CTRL_WAIT_FW_VERSION,
    ESP_HOSTED_CTRL_WAIT_WIFI_INIT,
    ESP_HOSTED_CTRL_WAIT_WIFI_SET_MODE,
    ESP_HOSTED_CTRL_WAIT_WIFI_SET_CONFIG,
    ESP_HOSTED_CTRL_WAIT_WIFI_START,
    ESP_HOSTED_CTRL_WAIT_WIFI_CONNECT,
    ESP_HOSTED_CTRL_WAIT_WIFI_DISCONNECT,
    ESP_HOSTED_CTRL_WAIT_WIFI_GET_MAC,
} esp_hosted_ctrl_wait_kind_t;

typedef struct
{
    bool initialized;
    uint32_t next_uid;
    bool waiting;
    esp_hosted_ctrl_wait_kind_t wait_kind;
    bool response_seen;
    uint32_t wait_uid;
    uint32_t wait_msg_id;
    esp_hosted_ctrl_feature_t wait_feature;
    esp_hosted_ctrl_feature_command_t wait_command;
    int32_t response_code;
    uint32_t response_feature;
    uint32_t response_command;
    uint32_t response_major;
    uint32_t response_minor;
    uint32_t response_patch;
    uint8_t response_mac[6];
    bool ready;
    uint32_t cp_reset_reason;
    bool sta_connected;
    esp_hosted_wifi_event_cb_t wifi_event_cb;
    void *wifi_event_ctx;
} esp_hosted_ctrl_state_t;

typedef struct
{
    bool valid;
    uint32_t msg_type;
    uint32_t msg_id;
    uint32_t uid;
    uint32_t payload_field;
    const uint8_t *payload;
    size_t payload_len;
} esp_hosted_ctrl_rpc_msg_t;

typedef struct
{
    bool valid;
    int32_t resp;
    bool resp_present;
    uint32_t feature;
    bool feature_present;
    uint32_t command;
    bool command_present;
} esp_hosted_ctrl_feature_resp_t;

typedef struct
{
    bool valid;
    int32_t resp;
    bool resp_present;
    uint32_t major;
    bool major_present;
    uint32_t minor;
    bool minor_present;
    uint32_t patch;
    bool patch_present;
} esp_hosted_ctrl_fw_version_resp_t;

typedef struct
{
    bool valid;
    int32_t resp;
    bool resp_present;
} esp_hosted_ctrl_simple_resp_t;

typedef struct
{
    bool valid;
    int32_t resp;
    bool resp_present;
    uint8_t mac[6];
    bool mac_present;
} esp_hosted_ctrl_mac_resp_t;

static esp_hosted_ctrl_state_t s_ctrl;

static bool esp_hosted_ctrl_wrap_tlv(const uint8_t *payload,
                                     size_t payload_len,
                                     uint8_t *buf,
                                     size_t cap,
                                     size_t *out_len)
{
    const char *ep_name = RPC_EP_NAME_RSP;
    size_t ep_len = strlen(ep_name);
    size_t off = 0U;

    if ((payload == NULL && payload_len != 0U) || buf == NULL || out_len == NULL ||
        ep_len > 0xFFFFU || payload_len > 0xFFFFU)
    {
        return false;
    }

    if (cap < (1U + 2U + ep_len + 1U + 2U + payload_len))
    {
        return false;
    }

    buf[off++] = PROTO_PSER_TLV_T_EPNAME;
    buf[off++] = (uint8_t)(ep_len & 0xFFU);
    buf[off++] = (uint8_t)((ep_len >> 8U) & 0xFFU);
    memcpy(buf + off, ep_name, ep_len);
    off += ep_len;

    buf[off++] = PROTO_PSER_TLV_T_DATA;
    buf[off++] = (uint8_t)(payload_len & 0xFFU);
    buf[off++] = (uint8_t)((payload_len >> 8U) & 0xFFU);
    if (payload_len != 0U)
    {
        memcpy(buf + off, payload, payload_len);
        off += payload_len;
    }

    *out_len = off;
    return true;
}

static bool esp_hosted_ctrl_unwrap_tlv(const uint8_t *buf,
                                       size_t len,
                                       const uint8_t **out_payload,
                                       size_t *out_payload_len)
{
    size_t off = 0U;
    uint16_t ep_len;
    uint16_t data_len;

    if (buf == NULL || out_payload == NULL || out_payload_len == NULL || len < (1U + 2U + 1U + 2U))
    {
        return false;
    }

    if (buf[off++] != PROTO_PSER_TLV_T_EPNAME)
    {
        return false;
    }

    ep_len = (uint16_t)buf[off] | ((uint16_t)buf[off + 1U] << 8U);
    off += 2U;
    if ((len - off) < ep_len)
    {
        return false;
    }

    if (!((ep_len == strlen(RPC_EP_NAME_RSP) && memcmp(buf + off, RPC_EP_NAME_RSP, ep_len) == 0) ||
          (ep_len == strlen(RPC_EP_NAME_EVT) && memcmp(buf + off, RPC_EP_NAME_EVT, ep_len) == 0)))
    {
        return false;
    }
    off += ep_len;

    if ((len - off) < 3U || buf[off++] != PROTO_PSER_TLV_T_DATA)
    {
        return false;
    }

    data_len = (uint16_t)buf[off] | ((uint16_t)buf[off + 1U] << 8U);
    off += 2U;
    if ((len - off) < data_len)
    {
        return false;
    }

    *out_payload = buf + off;
    *out_payload_len = (size_t)data_len;
    return true;
}

static bool pb_read_varint(const uint8_t *buf, size_t len, size_t *off, uint64_t *out_value)
{
    uint64_t value = 0U;
    uint8_t shift = 0U;

    if (buf == NULL || off == NULL || out_value == NULL)
    {
        return false;
    }

    while (*off < len && shift < 64U)
    {
        uint8_t byte = buf[*off];
        (*off)++;
        value |= ((uint64_t)(byte & 0x7FU)) << shift;
        if ((byte & 0x80U) == 0U)
        {
            *out_value = value;
            return true;
        }
        shift = (uint8_t)(shift + 7U);
    }

    return false;
}

static bool pb_skip_field(const uint8_t *buf, size_t len, size_t *off, uint8_t wire_type)
{
    uint64_t value;

    if (buf == NULL || off == NULL)
    {
        return false;
    }

    switch (wire_type)
    {
        case PB_WIRE_VARINT:
            return pb_read_varint(buf, len, off, &value);
        case 1U:
            if ((len - *off) < 8U)
            {
                return false;
            }
            *off += 8U;
            return true;
        case PB_WIRE_LEN:
            if (!pb_read_varint(buf, len, off, &value) || value > (uint64_t)(len - *off))
            {
                return false;
            }
            *off += (size_t)value;
            return true;
        case 5U:
            if ((len - *off) < 4U)
            {
                return false;
            }
            *off += 4U;
            return true;
        default:
            return false;
    }
}

static bool pb_write_varint(uint8_t *buf, size_t cap, size_t *off, uint64_t value)
{
    if (buf == NULL || off == NULL)
    {
        return false;
    }

    do
    {
        uint8_t byte = (uint8_t)(value & 0x7FU);
        value >>= 7U;
        if (value != 0U)
        {
            byte |= 0x80U;
        }
        if (*off >= cap)
        {
            return false;
        }
        buf[*off] = byte;
        (*off)++;
    } while (value != 0U);

    return true;
}

static bool pb_write_key(uint8_t *buf, size_t cap, size_t *off, uint32_t field_number, uint8_t wire_type)
{
    uint64_t key = ((uint64_t)field_number << 3U) | (uint64_t)wire_type;
    return pb_write_varint(buf, cap, off, key);
}

static bool pb_write_varint_field(uint8_t *buf,
                                  size_t cap,
                                  size_t *off,
                                  uint32_t field_number,
                                  uint64_t value)
{
    return pb_write_key(buf, cap, off, field_number, PB_WIRE_VARINT) &&
           pb_write_varint(buf, cap, off, value);
}

static bool pb_write_bytes_field(uint8_t *buf,
                                 size_t cap,
                                 size_t *off,
                                 uint32_t field_number,
                                 const uint8_t *value,
                                 size_t value_len)
{
    if ((value == NULL && value_len != 0U) ||
        !pb_write_key(buf, cap, off, field_number, PB_WIRE_LEN) ||
        !pb_write_varint(buf, cap, off, value_len) ||
        (cap - *off) < value_len)
    {
        return false;
    }

    if (value_len != 0U)
    {
        memcpy(buf + *off, value, value_len);
        *off += value_len;
    }

    return true;
}

static int32_t pb_varint_to_i32(uint64_t value)
{
    if (value <= 0x7FFFFFFFULL)
    {
        return (int32_t)value;
    }

    {
        uint32_t raw = (uint32_t)value;
        int32_t signed_value;
        memcpy(&signed_value, &raw, sizeof(signed_value));
        return signed_value;
    }
}

static bool esp_hosted_ctrl_parse_rpc(const uint8_t *buf,
                                      size_t len,
                                      esp_hosted_ctrl_rpc_msg_t *out_msg)
{
    esp_hosted_ctrl_rpc_msg_t msg = {0};
    size_t off = 0U;

    if (buf == NULL || out_msg == NULL)
    {
        return false;
    }

    while (off < len)
    {
        uint64_t key;
        uint64_t value;
        uint32_t field_number;
        uint8_t wire_type;

        if (!pb_read_varint(buf, len, &off, &key))
        {
            return false;
        }

        field_number = (uint32_t)(key >> 3U);
        wire_type = (uint8_t)(key & 0x07U);

        if (field_number == RPC_FIELD_MSG_TYPE && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            msg.msg_type = (uint32_t)value;
        }
        else if (field_number == RPC_FIELD_MSG_ID && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            msg.msg_id = (uint32_t)value;
        }
        else if (field_number == RPC_FIELD_UID && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            msg.uid = (uint32_t)value;
        }
        else if (wire_type == PB_WIRE_LEN)
        {
            if (!pb_read_varint(buf, len, &off, &value) || value > (uint64_t)(len - off))
            {
                return false;
            }
            if (msg.payload == NULL)
            {
                msg.payload_field = field_number;
                msg.payload = buf + off;
                msg.payload_len = (size_t)value;
            }
            off += (size_t)value;
        }
        else if (!pb_skip_field(buf, len, &off, wire_type))
        {
            return false;
        }
    }

    msg.valid = true;
    *out_msg = msg;
    return true;
}

static bool esp_hosted_ctrl_parse_feature_resp(const uint8_t *buf,
                                               size_t len,
                                               esp_hosted_ctrl_feature_resp_t *out_resp)
{
    esp_hosted_ctrl_feature_resp_t resp = {0};
    size_t off = 0U;

    if (buf == NULL || out_resp == NULL)
    {
        return false;
    }

    while (off < len)
    {
        uint64_t key;
        uint64_t value;
        uint32_t field_number;
        uint8_t wire_type;

        if (!pb_read_varint(buf, len, &off, &key))
        {
            return false;
        }
        field_number = (uint32_t)(key >> 3U);
        wire_type = (uint8_t)(key & 0x07U);
        if (wire_type != PB_WIRE_VARINT)
        {
            if (!pb_skip_field(buf, len, &off, wire_type))
            {
                return false;
            }
            continue;
        }
        if (!pb_read_varint(buf, len, &off, &value))
        {
            return false;
        }

        if (field_number == RESP_FIELD_RESP)
        {
            resp.resp = pb_varint_to_i32(value);
            resp.resp_present = true;
        }
        else if (field_number == RESP_FIELD_FEATURE)
        {
            resp.feature = (uint32_t)value;
            resp.feature_present = true;
        }
        else if (field_number == RESP_FIELD_COMMAND)
        {
            resp.command = (uint32_t)value;
            resp.command_present = true;
        }
    }

    resp.valid = true;
    *out_resp = resp;
    return true;
}

static bool esp_hosted_ctrl_parse_fw_version_resp(const uint8_t *buf,
                                                  size_t len,
                                                  esp_hosted_ctrl_fw_version_resp_t *out_resp)
{
    esp_hosted_ctrl_fw_version_resp_t resp = {0};
    size_t off = 0U;

    if (buf == NULL || out_resp == NULL)
    {
        return false;
    }

    while (off < len)
    {
        uint64_t key;
        uint64_t value;
        uint32_t field_number;
        uint8_t wire_type;

        if (!pb_read_varint(buf, len, &off, &key))
        {
            return false;
        }
        field_number = (uint32_t)(key >> 3U);
        wire_type = (uint8_t)(key & 0x07U);
        if (wire_type != PB_WIRE_VARINT)
        {
            if (!pb_skip_field(buf, len, &off, wire_type))
            {
                return false;
            }
            continue;
        }
        if (!pb_read_varint(buf, len, &off, &value))
        {
            return false;
        }

        if (field_number == RESP_FIELD_RESP)
        {
            resp.resp = pb_varint_to_i32(value);
            resp.resp_present = true;
        }
        else if (field_number == 2U)
        {
            resp.major = (uint32_t)value;
            resp.major_present = true;
        }
        else if (field_number == 3U)
        {
            resp.minor = (uint32_t)value;
            resp.minor_present = true;
        }
        else if (field_number == 4U)
        {
            resp.patch = (uint32_t)value;
            resp.patch_present = true;
        }
    }

    resp.valid = true;
    *out_resp = resp;
    return true;
}

static bool esp_hosted_ctrl_parse_simple_resp(const uint8_t *buf,
                                              size_t len,
                                              esp_hosted_ctrl_simple_resp_t *out_resp)
{
    esp_hosted_ctrl_simple_resp_t resp = {0};
    size_t off = 0U;

    if (buf == NULL || out_resp == NULL)
    {
        return false;
    }

    while (off < len)
    {
        uint64_t key;
        uint64_t value;
        uint32_t field_number;
        uint8_t wire_type;

        if (!pb_read_varint(buf, len, &off, &key))
        {
            return false;
        }
        field_number = (uint32_t)(key >> 3U);
        wire_type = (uint8_t)(key & 0x07U);

        if (field_number == RESP_FIELD_RESP && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            resp.resp = pb_varint_to_i32(value);
            resp.resp_present = true;
        }
        else if (!pb_skip_field(buf, len, &off, wire_type))
        {
            return false;
        }
    }

    resp.valid = true;
    *out_resp = resp;
    return true;
}

static bool esp_hosted_ctrl_parse_mac_resp(const uint8_t *buf,
                                           size_t len,
                                           esp_hosted_ctrl_mac_resp_t *out_resp)
{
    esp_hosted_ctrl_mac_resp_t resp = {0};
    size_t off = 0U;

    if (buf == NULL || out_resp == NULL)
    {
        return false;
    }

    while (off < len)
    {
        uint64_t key;
        uint64_t value;
        uint32_t field_number;
        uint8_t wire_type;

        if (!pb_read_varint(buf, len, &off, &key))
        {
            return false;
        }
        field_number = (uint32_t)(key >> 3U);
        wire_type = (uint8_t)(key & 0x07U);

        if (field_number == WIFI_MAC_RESP_FIELD_MAC && wire_type == PB_WIRE_LEN)
        {
            if (!pb_read_varint(buf, len, &off, &value) || value > (uint64_t)(len - off))
            {
                return false;
            }
            if (value == 6U)
            {
                memcpy(resp.mac, buf + off, 6U);
                resp.mac_present = true;
            }
            off += (size_t)value;
        }
        else if (field_number == WIFI_MAC_RESP_FIELD_RESP && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            resp.resp = pb_varint_to_i32(value);
            resp.resp_present = true;
        }
        else if (!pb_skip_field(buf, len, &off, wire_type))
        {
            return false;
        }
    }

    resp.valid = true;
    *out_resp = resp;
    return true;
}

static bool esp_hosted_ctrl_parse_esp_init_event(const uint8_t *buf,
                                                 size_t len,
                                                 uint32_t *out_cp_reset_reason)
{
    size_t off = 0U;
    uint32_t cp_reset_reason = 0U;

    if (buf == NULL || out_cp_reset_reason == NULL)
    {
        return false;
    }

    while (off < len)
    {
        uint64_t key;
        uint64_t value;
        uint32_t field_number;
        uint8_t wire_type;

        if (!pb_read_varint(buf, len, &off, &key))
        {
            return false;
        }
        field_number = (uint32_t)(key >> 3U);
        wire_type = (uint8_t)(key & 0x07U);

        if (field_number == ESP_INIT_FIELD_CP_RESET_REASON && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            cp_reset_reason = (uint32_t)value;
        }
        else if (!pb_skip_field(buf, len, &off, wire_type))
        {
            return false;
        }
    }

    *out_cp_reset_reason = cp_reset_reason;
    return true;
}

static bool esp_hosted_ctrl_parse_wifi_event_wrapper(const uint8_t *buf,
                                                     size_t len,
                                                     int32_t *out_resp,
                                                     const uint8_t **out_data,
                                                     size_t *out_data_len)
{
    size_t off = 0U;
    int32_t resp = 0;
    const uint8_t *data = NULL;
    size_t data_len = 0U;

    if (buf == NULL || out_resp == NULL || out_data == NULL || out_data_len == NULL)
    {
        return false;
    }

    while (off < len)
    {
        uint64_t key;
        uint64_t value;
        uint32_t field_number;
        uint8_t wire_type;

        if (!pb_read_varint(buf, len, &off, &key))
        {
            return false;
        }
        field_number = (uint32_t)(key >> 3U);
        wire_type = (uint8_t)(key & 0x07U);

        if (field_number == WIFI_EVENT_FIELD_RESP && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            resp = pb_varint_to_i32(value);
        }
        else if (field_number == WIFI_EVENT_FIELD_DATA && wire_type == PB_WIRE_LEN)
        {
            if (!pb_read_varint(buf, len, &off, &value) || value > (uint64_t)(len - off))
            {
                return false;
            }
            data = buf + off;
            data_len = (size_t)value;
            off += (size_t)value;
        }
        else if (!pb_skip_field(buf, len, &off, wire_type))
        {
            return false;
        }
    }

    *out_resp = resp;
    *out_data = data;
    *out_data_len = data_len;
    return true;
}

static bool esp_hosted_ctrl_parse_sta_connected_event(const uint8_t *buf,
                                                      size_t len,
                                                      esp_hosted_wifi_sta_connected_t *out_evt)
{
    esp_hosted_wifi_sta_connected_t evt = {0};
    size_t off = 0U;
    size_t copied_ssid = 0U;

    if (buf == NULL || out_evt == NULL)
    {
        return false;
    }

    while (off < len)
    {
        uint64_t key;
        uint64_t value;
        uint32_t field_number;
        uint8_t wire_type;

        if (!pb_read_varint(buf, len, &off, &key))
        {
            return false;
        }
        field_number = (uint32_t)(key >> 3U);
        wire_type = (uint8_t)(key & 0x07U);

        if (field_number == WIFI_EVENT_STA_SSID && wire_type == PB_WIRE_LEN)
        {
            if (!pb_read_varint(buf, len, &off, &value) || value > (uint64_t)(len - off))
            {
                return false;
            }
            copied_ssid = (size_t)value;
            if (copied_ssid > sizeof(evt.ssid))
            {
                copied_ssid = sizeof(evt.ssid);
            }
            if (copied_ssid != 0U)
            {
                memcpy(evt.ssid, buf + off, copied_ssid);
            }
            off += (size_t)value;
        }
        else if (field_number == WIFI_EVENT_STA_SSID_LEN && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            evt.ssid_len = (uint8_t)value;
        }
        else if (field_number == WIFI_EVENT_STA_BSSID && wire_type == PB_WIRE_LEN)
        {
            if (!pb_read_varint(buf, len, &off, &value) || value > (uint64_t)(len - off))
            {
                return false;
            }
            if (value == 6U)
            {
                memcpy(evt.bssid, buf + off, 6U);
            }
            off += (size_t)value;
        }
        else if (field_number == WIFI_EVENT_STA_CHANNEL && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            evt.channel = (uint32_t)value;
        }
        else if (field_number == WIFI_EVENT_STA_AUTHMODE && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            evt.authmode = pb_varint_to_i32(value);
        }
        else if (field_number == WIFI_EVENT_STA_AID && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            evt.aid = (uint32_t)value;
        }
        else if (!pb_skip_field(buf, len, &off, wire_type))
        {
            return false;
        }
    }

    if (evt.ssid_len == 0U)
    {
        evt.ssid_len = (uint8_t)copied_ssid;
    }

    *out_evt = evt;
    return true;
}

static bool esp_hosted_ctrl_parse_sta_disconnected_event(const uint8_t *buf,
                                                         size_t len,
                                                         esp_hosted_wifi_sta_disconnected_t *out_evt)
{
    esp_hosted_wifi_sta_disconnected_t evt = {0};
    size_t off = 0U;
    size_t copied_ssid = 0U;

    if (buf == NULL || out_evt == NULL)
    {
        return false;
    }

    while (off < len)
    {
        uint64_t key;
        uint64_t value;
        uint32_t field_number;
        uint8_t wire_type;

        if (!pb_read_varint(buf, len, &off, &key))
        {
            return false;
        }
        field_number = (uint32_t)(key >> 3U);
        wire_type = (uint8_t)(key & 0x07U);

        if (field_number == WIFI_EVENT_STA_SSID && wire_type == PB_WIRE_LEN)
        {
            if (!pb_read_varint(buf, len, &off, &value) || value > (uint64_t)(len - off))
            {
                return false;
            }
            copied_ssid = (size_t)value;
            if (copied_ssid > sizeof(evt.ssid))
            {
                copied_ssid = sizeof(evt.ssid);
            }
            if (copied_ssid != 0U)
            {
                memcpy(evt.ssid, buf + off, copied_ssid);
            }
            off += (size_t)value;
        }
        else if (field_number == WIFI_EVENT_STA_SSID_LEN && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            evt.ssid_len = (uint8_t)value;
        }
        else if (field_number == WIFI_EVENT_STA_BSSID && wire_type == PB_WIRE_LEN)
        {
            if (!pb_read_varint(buf, len, &off, &value) || value > (uint64_t)(len - off))
            {
                return false;
            }
            if (value == 6U)
            {
                memcpy(evt.bssid, buf + off, 6U);
            }
            off += (size_t)value;
        }
        else if (field_number == WIFI_EVENT_STA_REASON && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            evt.reason = (uint32_t)value;
        }
        else if (field_number == WIFI_EVENT_STA_RSSI && wire_type == PB_WIRE_VARINT)
        {
            if (!pb_read_varint(buf, len, &off, &value))
            {
                return false;
            }
            evt.rssi = pb_varint_to_i32(value);
        }
        else if (!pb_skip_field(buf, len, &off, wire_type))
        {
            return false;
        }
    }

    if (evt.ssid_len == 0U)
    {
        evt.ssid_len = (uint8_t)copied_ssid;
    }

    *out_evt = evt;
    return true;
}

static void esp_hosted_ctrl_reset_wait_state(void)
{
    s_ctrl.waiting = false;
    s_ctrl.wait_kind = ESP_HOSTED_CTRL_WAIT_NONE;
}

static void esp_hosted_ctrl_reset_response_fields(void)
{
    s_ctrl.response_seen = false;
    s_ctrl.response_code = ESP_HOSTED_CTRL_ERR_TIMEOUT;
    s_ctrl.response_feature = 0U;
    s_ctrl.response_command = 0U;
    s_ctrl.response_major = 0U;
    s_ctrl.response_minor = 0U;
    s_ctrl.response_patch = 0U;
    memset(s_ctrl.response_mac, 0, sizeof(s_ctrl.response_mac));
}

static uint32_t esp_hosted_ctrl_next_uid(void)
{
    uint32_t uid = s_ctrl.next_uid++;
    if (s_ctrl.next_uid == 0U)
    {
        s_ctrl.next_uid = 1U;
    }
    return uid;
}

static bool esp_hosted_ctrl_encode_feature_request(uint32_t uid,
                                                   esp_hosted_ctrl_feature_t feature,
                                                   esp_hosted_ctrl_feature_command_t command,
                                                   esp_hosted_ctrl_feature_option_t option,
                                                   uint8_t *buf,
                                                   size_t cap,
                                                   size_t *out_len)
{
    uint8_t feature_buf[16];
    size_t feature_len = 0U;
    size_t msg_len = 0U;

    if (buf == NULL || out_len == NULL)
    {
        return false;
    }

    if (!pb_write_varint_field(feature_buf, sizeof(feature_buf), &feature_len, FEATURE_FIELD_FEATURE, (uint32_t)feature) ||
        !pb_write_varint_field(feature_buf, sizeof(feature_buf), &feature_len, FEATURE_FIELD_COMMAND, (uint32_t)command))
    {
        return false;
    }

    if (option != ESP_HOSTED_CTRL_FEATURE_OPTION_NONE &&
        !pb_write_varint_field(feature_buf, sizeof(feature_buf), &feature_len, FEATURE_FIELD_OPTION, (uint32_t)option))
    {
        return false;
    }

    if (!pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_TYPE, RPC_TYPE_REQ) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_ID, RPC_ID_REQ_FEATURE_CONTROL) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_UID, uid) ||
        !pb_write_bytes_field(buf, cap, &msg_len, RPC_FIELD_REQ_FEATURE_CONTROL, feature_buf, feature_len))
    {
        return false;
    }

    *out_len = msg_len;
    return true;
}

static bool esp_hosted_ctrl_encode_fw_version_request(uint32_t uid,
                                                      uint8_t *buf,
                                                      size_t cap,
                                                      size_t *out_len)
{
    size_t msg_len = 0U;

    if (buf == NULL || out_len == NULL)
    {
        return false;
    }

    if (!pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_TYPE, RPC_TYPE_REQ) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_ID, RPC_ID_REQ_GET_COPROCESSOR_FW_VERSION) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_UID, uid))
    {
        return false;
    }

    *out_len = msg_len;
    return true;
}

static bool esp_hosted_ctrl_encode_wifi_init_default_request(uint32_t uid,
                                                             uint8_t *buf,
                                                             size_t cap,
                                                             size_t *out_len)
{
    uint8_t cfg_buf[96];
    uint8_t req_buf[112];
    size_t cfg_len = 0U;
    size_t req_len = 0U;
    size_t msg_len = 0U;

    if (buf == NULL || out_len == NULL)
    {
        return false;
    }

    if (!pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 1U, WIFI_INIT_DEFAULT_STATIC_RX_BUF_NUM) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 2U, WIFI_INIT_DEFAULT_DYNAMIC_RX_BUF_NUM) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 3U, WIFI_INIT_DEFAULT_TX_BUF_TYPE) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 5U, WIFI_INIT_DEFAULT_DYNAMIC_TX_BUF_NUM) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 8U, WIFI_INIT_DEFAULT_AMPDU_RX_ENABLE) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 9U, WIFI_INIT_DEFAULT_AMPDU_TX_ENABLE) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 11U, WIFI_INIT_DEFAULT_NVS_ENABLE) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 13U, WIFI_INIT_DEFAULT_RX_BA_WIN) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 15U, WIFI_INIT_DEFAULT_BEACON_MAX_LEN) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 16U, WIFI_INIT_DEFAULT_MGMT_SBUF_NUM) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 21U, WIFI_INIT_DEFAULT_RX_MGMT_BUF_TYPE) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 23U, WIFI_INIT_DEFAULT_TX_HETB_QUEUE_NUM) ||
        !pb_write_varint_field(cfg_buf, sizeof(cfg_buf), &cfg_len, 20U, WIFI_INIT_CONFIG_MAGIC))
    {
        return false;
    }

    if (!pb_write_bytes_field(req_buf, sizeof(req_buf), &req_len, WIFI_INIT_FIELD_CFG, cfg_buf, cfg_len))
    {
        return false;
    }

    if (!pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_TYPE, RPC_TYPE_REQ) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_ID, RPC_ID_REQ_WIFI_INIT) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_UID, uid) ||
        !pb_write_bytes_field(buf, cap, &msg_len, RPC_FIELD_REQ_WIFI_INIT, req_buf, req_len))
    {
        return false;
    }

    *out_len = msg_len;
    return true;
}

static bool esp_hosted_ctrl_encode_wifi_set_mode_request(uint32_t uid,
                                                         esp_hosted_wifi_mode_t mode,
                                                         uint8_t *buf,
                                                         size_t cap,
                                                         size_t *out_len)
{
    uint8_t req_buf[16];
    size_t req_len = 0U;
    size_t msg_len = 0U;

    if (buf == NULL || out_len == NULL)
    {
        return false;
    }

    if (!pb_write_varint_field(req_buf, sizeof(req_buf), &req_len, WIFI_SET_MODE_FIELD_MODE, (uint32_t)mode))
    {
        return false;
    }

    if (!pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_TYPE, RPC_TYPE_REQ) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_ID, RPC_ID_REQ_SET_WIFI_MODE) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_UID, uid) ||
        !pb_write_bytes_field(buf, cap, &msg_len, RPC_FIELD_REQ_SET_WIFI_MODE, req_buf, req_len))
    {
        return false;
    }

    *out_len = msg_len;
    return true;
}

static bool esp_hosted_ctrl_encode_wifi_set_sta_config_request(uint32_t uid,
                                                               const char *ssid,
                                                               const char *password,
                                                               uint8_t *buf,
                                                               size_t cap,
                                                               size_t *out_len)
{
    uint8_t threshold_buf[16];
    uint8_t pmf_buf[16];
    uint8_t sta_buf[192];
    uint8_t cfg_buf[208];
    uint8_t req_buf[240];
    size_t ssid_raw_len;
    size_t pass_raw_len;
    size_t ssid_wire_len;
    size_t pass_wire_len;
    size_t threshold_len = 0U;
    size_t pmf_len = 0U;
    size_t sta_len = 0U;
    size_t cfg_len = 0U;
    size_t req_len = 0U;
    size_t msg_len = 0U;
    uint32_t authmode = WIFI_AUTH_OPEN;

    if (buf == NULL || out_len == NULL || ssid == NULL || password == NULL)
    {
        return false;
    }

    ssid_raw_len = strlen(ssid);
    pass_raw_len = strlen(password);
    if (ssid_raw_len == 0U || ssid_raw_len > 32U || pass_raw_len > 64U)
    {
        return false;
    }

    /*
     * Match esp-hosted host encoder semantics (RPC_REQ_COPY_STR):
     * encode STA strings as C-strings including '\0', clamped to max field size.
     */
    ssid_wire_len = (ssid_raw_len + 1U <= 32U) ? (ssid_raw_len + 1U) : 32U;
    pass_wire_len = (pass_raw_len + 1U <= 64U) ? (pass_raw_len + 1U) : 64U;
    if (pass_raw_len >= 8U)
    {
        authmode = WIFI_AUTH_WPA2_PSK;
    }

    if (!pb_write_varint_field(threshold_buf, sizeof(threshold_buf), &threshold_len, WIFI_SCAN_THRESHOLD_FIELD_AUTHMODE, authmode) ||
        !pb_write_varint_field(pmf_buf, sizeof(pmf_buf), &pmf_len, WIFI_PMF_CFG_FIELD_CAPABLE, 1U) ||
        !pb_write_varint_field(pmf_buf, sizeof(pmf_buf), &pmf_len, WIFI_PMF_CFG_FIELD_REQUIRED, 0U) ||
        !pb_write_bytes_field(sta_buf, sizeof(sta_buf), &sta_len, WIFI_STA_CFG_FIELD_SSID, (const uint8_t *)ssid, ssid_wire_len) ||
        !pb_write_bytes_field(sta_buf, sizeof(sta_buf), &sta_len, WIFI_STA_CFG_FIELD_PASSWORD, (const uint8_t *)password, pass_wire_len) ||
        !pb_write_bytes_field(sta_buf, sizeof(sta_buf), &sta_len, WIFI_STA_CFG_FIELD_THRESHOLD, threshold_buf, threshold_len) ||
        !pb_write_bytes_field(sta_buf, sizeof(sta_buf), &sta_len, WIFI_STA_CFG_FIELD_PMF_CFG, pmf_buf, pmf_len) ||
        !pb_write_varint_field(sta_buf, sizeof(sta_buf), &sta_len, WIFI_STA_CFG_FIELD_SAE_PWE_H2E, WIFI_SAE_PWE_UNSPECIFIED) ||
        !pb_write_bytes_field(cfg_buf, sizeof(cfg_buf), &cfg_len, WIFI_CONFIG_FIELD_STA, sta_buf, sta_len) ||
        !pb_write_varint_field(req_buf, sizeof(req_buf), &req_len, WIFI_SET_CONFIG_FIELD_IFACE, ESP_HOSTED_WIFI_IF_STA) ||
        !pb_write_bytes_field(req_buf, sizeof(req_buf), &req_len, WIFI_SET_CONFIG_FIELD_CFG, cfg_buf, cfg_len) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_TYPE, RPC_TYPE_REQ) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_ID, RPC_ID_REQ_WIFI_SET_CONFIG) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_UID, uid) ||
        !pb_write_bytes_field(buf, cap, &msg_len, RPC_FIELD_REQ_WIFI_SET_CONFIG, req_buf, req_len))
    {
        return false;
    }

    *out_len = msg_len;
    return true;
}

static bool esp_hosted_ctrl_encode_empty_request(uint32_t uid,
                                                 uint32_t msg_id,
                                                 uint32_t payload_field,
                                                 uint8_t *buf,
                                                 size_t cap,
                                                 size_t *out_len)
{
    size_t msg_len = 0U;

    if (buf == NULL || out_len == NULL)
    {
        return false;
    }

    if (!pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_TYPE, RPC_TYPE_REQ) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_ID, msg_id) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_UID, uid) ||
        !pb_write_bytes_field(buf, cap, &msg_len, payload_field, NULL, 0U))
    {
        return false;
    }

    *out_len = msg_len;
    return true;
}

static bool esp_hosted_ctrl_encode_get_mac_request(uint32_t uid,
                                                   esp_hosted_wifi_interface_t iface,
                                                   uint8_t *buf,
                                                   size_t cap,
                                                   size_t *out_len)
{
    uint8_t req_buf[16];
    size_t req_len = 0U;
    size_t msg_len = 0U;

    if (buf == NULL || out_len == NULL)
    {
        return false;
    }

    if (!pb_write_varint_field(req_buf, sizeof(req_buf), &req_len, WIFI_GET_MAC_FIELD_MODE, (uint32_t)iface) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_TYPE, RPC_TYPE_REQ) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_MSG_ID, RPC_ID_REQ_GET_MAC_ADDRESS) ||
        !pb_write_varint_field(buf, cap, &msg_len, RPC_FIELD_UID, uid) ||
        !pb_write_bytes_field(buf, cap, &msg_len, RPC_FIELD_REQ_GET_MAC_ADDRESS, req_buf, req_len))
    {
        return false;
    }

    *out_len = msg_len;
    return true;
}

static void esp_hosted_ctrl_notify_wifi_event(esp_hosted_wifi_event_t event,
                                              const void *event_data)
{
    if (s_ctrl.wifi_event_cb != NULL)
    {
        s_ctrl.wifi_event_cb(event, event_data, s_ctrl.wifi_event_ctx);
    }
}

static void esp_hosted_ctrl_rx(const esp_hosted_frame_info_t *info,
                               const uint8_t *payload,
                               size_t len,
                               void *ctx)
{
    const uint8_t *rpc_payload = NULL;
    size_t rpc_len = 0U;
    esp_hosted_ctrl_rpc_msg_t msg;

    (void)info;
    (void)ctx;

    if (payload == NULL || len == 0U)
    {
        return;
    }

    if (!esp_hosted_ctrl_unwrap_tlv(payload, len, &rpc_payload, &rpc_len))
    {
        ESP_HOSTED_CTRL_ERR("unexpected serial payload len=%u\n", (unsigned)len);
        return;
    }

    ESP_HOSTED_CTRL_LOG("[hosted_ctrl] rx tlv len=%u rpc_len=%u\n",
                        (unsigned)len,
                        (unsigned)rpc_len);

    if (!esp_hosted_ctrl_parse_rpc(rpc_payload, rpc_len, &msg))
    {
        ESP_HOSTED_CTRL_ERR("rpc parse failed\n");
        return;
    }

    if (msg.msg_type == RPC_TYPE_EVENT && msg.payload != NULL)
    {
        if (msg.msg_id == RPC_ID_EVENT_ESP_INIT && msg.payload_field == RPC_FIELD_EVENT_ESP_INIT)
        {
            uint32_t cp_reset_reason = 0U;
            if (esp_hosted_ctrl_parse_esp_init_event(msg.payload, msg.payload_len, &cp_reset_reason))
            {
                s_ctrl.ready = true;
                s_ctrl.cp_reset_reason = cp_reset_reason;
                ESP_HOSTED_CTRL_LOG("[hosted_ctrl] event esp_init reason=%lu\n",
                                    (unsigned long)cp_reset_reason);
            }
            return;
        }
        if (msg.msg_id == RPC_ID_EVENT_STA_CONNECTED && msg.payload_field == RPC_FIELD_EVENT_STA_CONNECTED)
        {
            int32_t resp = 0;
            const uint8_t *event_data = NULL;
            size_t event_len = 0U;
            esp_hosted_wifi_sta_connected_t evt;

            if (esp_hosted_ctrl_parse_wifi_event_wrapper(msg.payload, msg.payload_len, &resp, &event_data, &event_len) &&
                resp == 0 && event_data != NULL &&
                esp_hosted_ctrl_parse_sta_connected_event(event_data, event_len, &evt))
            {
                s_ctrl.sta_connected = true;
                esp_hosted_ctrl_notify_wifi_event(ESP_HOSTED_WIFI_EVENT_STA_CONNECTED, &evt);
            }
            return;
        }
        if (msg.msg_id == RPC_ID_EVENT_STA_DISCONNECTED && msg.payload_field == RPC_FIELD_EVENT_STA_DISCONNECTED)
        {
            int32_t resp = 0;
            const uint8_t *event_data = NULL;
            size_t event_len = 0U;
            esp_hosted_wifi_sta_disconnected_t evt;

            if (esp_hosted_ctrl_parse_wifi_event_wrapper(msg.payload, msg.payload_len, &resp, &event_data, &event_len) &&
                event_data != NULL &&
                esp_hosted_ctrl_parse_sta_disconnected_event(event_data, event_len, &evt))
            {
                s_ctrl.sta_connected = false;
                esp_hosted_ctrl_notify_wifi_event(ESP_HOSTED_WIFI_EVENT_STA_DISCONNECTED, &evt);
            }
            return;
        }
    }

    if (!s_ctrl.waiting ||
        msg.msg_type != RPC_TYPE_RESP ||
        msg.msg_id != s_ctrl.wait_msg_id ||
        msg.uid != s_ctrl.wait_uid ||
        msg.payload == NULL ||
        msg.payload_field != s_ctrl.wait_msg_id)
    {
        return;
    }

    s_ctrl.response_seen = true;
    s_ctrl.response_code = ESP_HOSTED_CTRL_ERR_PROTO;

    if (s_ctrl.wait_kind == ESP_HOSTED_CTRL_WAIT_FEATURE_CONTROL)
    {
        esp_hosted_ctrl_feature_resp_t feature_resp;
        if (!esp_hosted_ctrl_parse_feature_resp(msg.payload, msg.payload_len, &feature_resp))
        {
            ESP_HOSTED_CTRL_ERR("malformed feature response uid=%lu\n", (unsigned long)msg.uid);
            return;
        }
        s_ctrl.response_code = feature_resp.resp;
        s_ctrl.response_feature = feature_resp.feature;
        s_ctrl.response_command = feature_resp.command;
        return;
    }

    if (s_ctrl.wait_kind == ESP_HOSTED_CTRL_WAIT_FW_VERSION)
    {
        esp_hosted_ctrl_fw_version_resp_t fw_resp;
        if (!esp_hosted_ctrl_parse_fw_version_resp(msg.payload, msg.payload_len, &fw_resp))
        {
            ESP_HOSTED_CTRL_ERR("malformed fw version response uid=%lu\n", (unsigned long)msg.uid);
            return;
        }
        s_ctrl.response_code = fw_resp.resp;
        s_ctrl.response_major = fw_resp.major;
        s_ctrl.response_minor = fw_resp.minor;
        s_ctrl.response_patch = fw_resp.patch;
        return;
    }

    if (s_ctrl.wait_kind == ESP_HOSTED_CTRL_WAIT_WIFI_GET_MAC)
    {
        esp_hosted_ctrl_mac_resp_t mac_resp;
        if (!esp_hosted_ctrl_parse_mac_resp(msg.payload, msg.payload_len, &mac_resp))
        {
            ESP_HOSTED_CTRL_ERR("malformed mac response uid=%lu\n", (unsigned long)msg.uid);
            return;
        }
        s_ctrl.response_code = mac_resp.resp;
        if (mac_resp.mac_present)
        {
            memcpy(s_ctrl.response_mac, mac_resp.mac, sizeof(s_ctrl.response_mac));
        }
        return;
    }

    {
        esp_hosted_ctrl_simple_resp_t simple_resp;
        if (!esp_hosted_ctrl_parse_simple_resp(msg.payload, msg.payload_len, &simple_resp))
        {
            ESP_HOSTED_CTRL_ERR("malformed response uid=%lu msg=%lu\n",
                                (unsigned long)msg.uid,
                                (unsigned long)msg.msg_id);
            return;
        }
        s_ctrl.response_code = simple_resp.resp;
    }
}

static int esp_hosted_ctrl_request_common(esp_hosted_ctrl_wait_kind_t wait_kind,
                                          uint32_t resp_msg_id,
                                          uint32_t uid,
                                          const uint8_t *req_buf,
                                          size_t req_len,
                                          uint32_t timeout_ms,
                                          const char *log_name)
{
    uint8_t tlv_buf[256];
    size_t tlv_len = 0U;
    size_t written;
    uint32_t waited = 0U;

    if (!esp_hosted_ctrl_init() || req_buf == NULL || log_name == NULL)
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    if (!esp_hosted_ctrl_wrap_tlv(req_buf, req_len, tlv_buf, sizeof(tlv_buf), &tlv_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    s_ctrl.waiting = true;
    s_ctrl.wait_kind = wait_kind;
    s_ctrl.wait_uid = uid;
    s_ctrl.wait_msg_id = resp_msg_id;
    esp_hosted_ctrl_reset_response_fields();

    written = esp_hosted_ctrl_tx(tlv_buf, tlv_len);
    if (written != tlv_len)
    {
        esp_hosted_ctrl_reset_wait_state();
        return ESP_HOSTED_CTRL_ERR_TX;
    }

    ESP_HOSTED_CTRL_LOG("[hosted_ctrl] request uid=%lu %s\n", (unsigned long)uid, log_name);
    while (waited < timeout_ms)
    {
        esp_hosted_poll();
        esp_hosted_dispatch_rx();
        if (s_ctrl.response_seen)
        {
            int rc = s_ctrl.response_code;
            esp_hosted_ctrl_reset_wait_state();
            return rc;
        }
        delay_ms(1U);
        waited++;
    }

    esp_hosted_ctrl_reset_wait_state();
    ESP_HOSTED_CTRL_LOG("[hosted_ctrl] request timeout uid=%lu %s\n", (unsigned long)uid, log_name);
    return ESP_HOSTED_CTRL_ERR_TIMEOUT;
}

bool esp_hosted_ctrl_init(void)
{
    if (!esp_hosted_is_initialized())
    {
        return false;
    }

    if (!s_ctrl.initialized)
    {
        memset(&s_ctrl, 0, sizeof(s_ctrl));
        s_ctrl.next_uid = 1U;
        s_ctrl.initialized = true;
    }

    esp_hosted_ctrl_set_rx_cb(esp_hosted_ctrl_rx, NULL);
    return true;
}

void esp_hosted_ctrl_deinit(void)
{
    esp_hosted_ctrl_set_rx_cb(NULL, NULL);
    memset(&s_ctrl, 0, sizeof(s_ctrl));
}

bool esp_hosted_ctrl_is_ready(void)
{
    return s_ctrl.ready;
}

int esp_hosted_ctrl_feature_control(esp_hosted_ctrl_feature_t feature,
                                    esp_hosted_ctrl_feature_command_t command,
                                    esp_hosted_ctrl_feature_option_t option,
                                    uint32_t timeout_ms)
{
    uint8_t req_buf[32];
    size_t req_len = 0U;
    uint32_t uid;
    int rc;

    if (!esp_hosted_ctrl_init())
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    uid = esp_hosted_ctrl_next_uid();
    if (!esp_hosted_ctrl_encode_feature_request(uid, feature, command, option, req_buf, sizeof(req_buf), &req_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    s_ctrl.wait_feature = feature;
    s_ctrl.wait_command = command;
    rc = esp_hosted_ctrl_request_common(ESP_HOSTED_CTRL_WAIT_FEATURE_CONTROL,
                                        RPC_ID_RESP_FEATURE_CONTROL,
                                        uid,
                                        req_buf,
                                        req_len,
                                        timeout_ms,
                                        "feature_control");
    if (rc == ESP_HOSTED_CTRL_OK)
    {
        bool mismatch = (s_ctrl.response_feature != (uint32_t)feature) ||
                        (s_ctrl.response_command != (uint32_t)command);
        if (mismatch)
        {
            return ESP_HOSTED_CTRL_ERR_MISMATCH;
        }
    }
    return rc;
}

int esp_hosted_ctrl_get_coprocessor_fw_version(uint32_t timeout_ms,
                                               uint32_t *major,
                                               uint32_t *minor,
                                               uint32_t *patch)
{
    uint8_t req_buf[16];
    size_t req_len = 0U;
    uint32_t uid;
    int rc;

    if (!esp_hosted_ctrl_init())
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    uid = esp_hosted_ctrl_next_uid();
    if (!esp_hosted_ctrl_encode_fw_version_request(uid, req_buf, sizeof(req_buf), &req_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    rc = esp_hosted_ctrl_request_common(ESP_HOSTED_CTRL_WAIT_FW_VERSION,
                                        RPC_ID_RESP_GET_COPROCESSOR_FW_VERSION,
                                        uid,
                                        req_buf,
                                        req_len,
                                        timeout_ms,
                                        "fw_version");
    if (rc == ESP_HOSTED_CTRL_OK)
    {
        if (major != NULL)
        {
            *major = s_ctrl.response_major;
        }
        if (minor != NULL)
        {
            *minor = s_ctrl.response_minor;
        }
        if (patch != NULL)
        {
            *patch = s_ctrl.response_patch;
        }
    }
    return rc;
}

int esp_hosted_ctrl_bt_init(uint32_t timeout_ms)
{
    return esp_hosted_ctrl_feature_control(ESP_HOSTED_CTRL_FEATURE_BLUETOOTH,
                                           ESP_HOSTED_CTRL_FEATURE_COMMAND_BT_INIT,
                                           ESP_HOSTED_CTRL_FEATURE_OPTION_NONE,
                                           timeout_ms);
}

int esp_hosted_ctrl_bt_enable(uint32_t timeout_ms)
{
    return esp_hosted_ctrl_feature_control(ESP_HOSTED_CTRL_FEATURE_BLUETOOTH,
                                           ESP_HOSTED_CTRL_FEATURE_COMMAND_BT_ENABLE,
                                           ESP_HOSTED_CTRL_FEATURE_OPTION_NONE,
                                           timeout_ms);
}

void esp_hosted_wifi_set_event_cb(esp_hosted_wifi_event_cb_t cb, void *ctx)
{
    if (esp_hosted_ctrl_init())
    {
        s_ctrl.wifi_event_cb = cb;
        s_ctrl.wifi_event_ctx = ctx;
    }
}

bool esp_hosted_wifi_sta_is_connected(void)
{
    return s_ctrl.sta_connected;
}

int esp_hosted_wifi_init_default(uint32_t timeout_ms)
{
    uint8_t req_buf[160];
    size_t req_len = 0U;
    uint32_t uid;

    if (!esp_hosted_ctrl_init())
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    uid = esp_hosted_ctrl_next_uid();
    if (!esp_hosted_ctrl_encode_wifi_init_default_request(uid, req_buf, sizeof(req_buf), &req_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    return esp_hosted_ctrl_request_common(ESP_HOSTED_CTRL_WAIT_WIFI_INIT,
                                          RPC_ID_RESP_WIFI_INIT,
                                          uid,
                                          req_buf,
                                          req_len,
                                          timeout_ms,
                                          "wifi_init");
}

int esp_hosted_wifi_set_mode(esp_hosted_wifi_mode_t mode, uint32_t timeout_ms)
{
    uint8_t req_buf[32];
    size_t req_len = 0U;
    uint32_t uid;

    if (!esp_hosted_ctrl_init())
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    if (mode < ESP_HOSTED_WIFI_MODE_NULL || mode > ESP_HOSTED_WIFI_MODE_APSTA)
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    uid = esp_hosted_ctrl_next_uid();
    if (!esp_hosted_ctrl_encode_wifi_set_mode_request(uid, mode, req_buf, sizeof(req_buf), &req_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    return esp_hosted_ctrl_request_common(ESP_HOSTED_CTRL_WAIT_WIFI_SET_MODE,
                                          RPC_ID_RESP_SET_WIFI_MODE,
                                          uid,
                                          req_buf,
                                          req_len,
                                          timeout_ms,
                                          "wifi_set_mode");
}

int esp_hosted_wifi_set_sta_config(const char *ssid,
                                   const char *password,
                                   uint32_t timeout_ms)
{
    uint8_t req_buf[224];
    size_t req_len = 0U;
    uint32_t uid;

    if (!esp_hosted_ctrl_init())
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    uid = esp_hosted_ctrl_next_uid();
    if (!esp_hosted_ctrl_encode_wifi_set_sta_config_request(uid,
                                                            ssid,
                                                            password,
                                                            req_buf,
                                                            sizeof(req_buf),
                                                            &req_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    return esp_hosted_ctrl_request_common(ESP_HOSTED_CTRL_WAIT_WIFI_SET_CONFIG,
                                          RPC_ID_RESP_WIFI_SET_CONFIG,
                                          uid,
                                          req_buf,
                                          req_len,
                                          timeout_ms,
                                          "wifi_set_config");
}

int esp_hosted_wifi_start(uint32_t timeout_ms)
{
    uint8_t req_buf[16];
    size_t req_len = 0U;
    uint32_t uid;

    if (!esp_hosted_ctrl_init())
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    uid = esp_hosted_ctrl_next_uid();
    if (!esp_hosted_ctrl_encode_empty_request(uid,
                                              RPC_ID_REQ_WIFI_START,
                                              RPC_FIELD_REQ_WIFI_START,
                                              req_buf,
                                              sizeof(req_buf),
                                              &req_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    return esp_hosted_ctrl_request_common(ESP_HOSTED_CTRL_WAIT_WIFI_START,
                                          RPC_ID_RESP_WIFI_START,
                                          uid,
                                          req_buf,
                                          req_len,
                                          timeout_ms,
                                          "wifi_start");
}

int esp_hosted_wifi_connect(uint32_t timeout_ms)
{
    uint8_t req_buf[16];
    size_t req_len = 0U;
    uint32_t uid;

    if (!esp_hosted_ctrl_init())
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    uid = esp_hosted_ctrl_next_uid();
    if (!esp_hosted_ctrl_encode_empty_request(uid,
                                              RPC_ID_REQ_WIFI_CONNECT,
                                              RPC_FIELD_REQ_WIFI_CONNECT,
                                              req_buf,
                                              sizeof(req_buf),
                                              &req_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    return esp_hosted_ctrl_request_common(ESP_HOSTED_CTRL_WAIT_WIFI_CONNECT,
                                          RPC_ID_RESP_WIFI_CONNECT,
                                          uid,
                                          req_buf,
                                          req_len,
                                          timeout_ms,
                                          "wifi_connect");
}

int esp_hosted_wifi_disconnect(uint32_t timeout_ms)
{
    uint8_t req_buf[16];
    size_t req_len = 0U;
    uint32_t uid;

    if (!esp_hosted_ctrl_init())
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    uid = esp_hosted_ctrl_next_uid();
    if (!esp_hosted_ctrl_encode_empty_request(uid,
                                              RPC_ID_REQ_WIFI_DISCONNECT,
                                              RPC_FIELD_REQ_WIFI_DISCONNECT,
                                              req_buf,
                                              sizeof(req_buf),
                                              &req_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    s_ctrl.sta_connected = false;
    return esp_hosted_ctrl_request_common(ESP_HOSTED_CTRL_WAIT_WIFI_DISCONNECT,
                                          RPC_ID_RESP_WIFI_DISCONNECT,
                                          uid,
                                          req_buf,
                                          req_len,
                                          timeout_ms,
                                          "wifi_disconnect");
}

int esp_hosted_wifi_get_mac(esp_hosted_wifi_interface_t iface,
                            uint8_t mac[6],
                            uint32_t timeout_ms)
{
    uint8_t req_buf[32];
    size_t req_len = 0U;
    uint32_t uid;
    int rc;

    if (!esp_hosted_ctrl_init() || mac == NULL)
    {
        return ESP_HOSTED_CTRL_ERR_STATE;
    }

    uid = esp_hosted_ctrl_next_uid();
    if (!esp_hosted_ctrl_encode_get_mac_request(uid, iface, req_buf, sizeof(req_buf), &req_len))
    {
        return ESP_HOSTED_CTRL_ERR_PROTO;
    }

    rc = esp_hosted_ctrl_request_common(ESP_HOSTED_CTRL_WAIT_WIFI_GET_MAC,
                                        RPC_ID_RESP_GET_MAC_ADDRESS,
                                        uid,
                                        req_buf,
                                        req_len,
                                        timeout_ms,
                                        "wifi_get_mac");
    if (rc == ESP_HOSTED_CTRL_OK)
    {
        memcpy(mac, s_ctrl.response_mac, 6U);
    }
    return rc;
}
