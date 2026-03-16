/*
 * MIT License
 *
 * Copyright (c) 2025 Surya Poudel
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_hosted.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/ip4_addr.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "lwip_hosted.h"
#include "mtimer.h"
#include "netif/ethernet.h"
#include "usb_serial.h"

#ifndef LWIP_HOSTED_FRAME_MAX
#define LWIP_HOSTED_FRAME_MAX 1600U
#endif

typedef struct
{
    struct netif netif;
    bool initialized;
    bool dhcp_enabled;
    bool dhcp_running;
    bool link_up;
    esp_hosted_if_t if_type;
    uint8_t if_num;
    uint16_t mtu;
    char hostname[32];
} lwip_hosted_state_t;

static lwip_hosted_state_t s_lwip;
static u32_t s_lwip_rand_state = 1U;

static err_t lwip_hosted_linkoutput(struct netif *netif, struct pbuf *p)
{
    uint8_t frame[LWIP_HOSTED_FRAME_MAX];
    size_t tx_len;

    if (netif == NULL || p == NULL || p->tot_len > sizeof(frame))
    {
        return ERR_ARG;
    }

    if (pbuf_copy_partial(p, frame, p->tot_len, 0U) != p->tot_len)
    {
        return ERR_MEM;
    }

    tx_len = esp_hosted_lwip_tx_to_if(s_lwip.if_type, s_lwip.if_num, frame, p->tot_len);
    if (tx_len != p->tot_len)
    {
        return ERR_IF;
    }

    return ERR_OK;
}

static err_t lwip_hosted_netif_init_cb(struct netif *netif)
{
    if (netif == NULL)
    {
        return ERR_ARG;
    }

    netif->name[0] = 'w';
    netif->name[1] = 'h';
    netif->mtu = s_lwip.mtu;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif->output = etharp_output;
    netif->linkoutput = lwip_hosted_linkoutput;
#if LWIP_NETIF_HOSTNAME
    netif->hostname = s_lwip.hostname[0] != '\0' ? s_lwip.hostname : NULL;
#endif
    return ERR_OK;
}

static void lwip_hosted_rx_input(const esp_hosted_frame_info_t *info,
                                 const uint8_t *payload,
                                 size_t len,
                                 void *ctx)
{
    struct pbuf *p;
    err_t rc;

    (void)ctx;

    if (!s_lwip.initialized || info == NULL || payload == NULL || len == 0U)
    {
        return;
    }

    if (info->if_type != s_lwip.if_type || info->if_num != s_lwip.if_num)
    {
        return;
    }

    p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
    if (p == NULL)
    {
        serial_printf("[lwip] rx drop: pbuf alloc len=%u\n", (unsigned)len);
        return;
    }

    if (pbuf_take(p, payload, len) != ERR_OK)
    {
        pbuf_free(p);
        serial_printf("[lwip] rx drop: pbuf take len=%u\n", (unsigned)len);
        return;
    }

    rc = s_lwip.netif.input(p, &s_lwip.netif);
    if (rc != ERR_OK)
    {
        pbuf_free(p);
        serial_printf("[lwip] rx drop: input rc=%d len=%u\n", (int)rc, (unsigned)len);
    }
}

static void lwip_hosted_clear_addrs(void)
{
    ip4_addr_t zero;
    ip4_addr_set_zero(&zero);
    netif_set_addr(&s_lwip.netif, &zero, &zero, &zero);
}

void lwip_hosted_get_default_config(lwip_hosted_config_t *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->if_type = ESP_HOSTED_IF_STA;
    cfg->if_num = 0U;
    cfg->hostname = "esp32-p4";
    cfg->dhcp = true;
    cfg->mtu = 1500U;
}

bool lwip_hosted_init(const lwip_hosted_config_t *cfg)
{
    ip4_addr_t zero;

    if (cfg == NULL)
    {
        return false;
    }

    memset(&s_lwip, 0, sizeof(s_lwip));
    ip4_addr_set_zero(&zero);
    s_lwip.if_type = cfg->if_type;
    s_lwip.if_num = cfg->if_num;
    s_lwip.dhcp_enabled = cfg->dhcp;
    s_lwip.mtu = (cfg->mtu != 0U) ? cfg->mtu : 1500U;

    if (cfg->hostname != NULL)
    {
        size_t n = strlen(cfg->hostname);
        if (n >= sizeof(s_lwip.hostname))
        {
            n = sizeof(s_lwip.hostname) - 1U;
        }
        memcpy(s_lwip.hostname, cfg->hostname, n);
        s_lwip.hostname[n] = '\0';
    }

    lwip_init();

    if (netif_add(&s_lwip.netif,
                  &zero,
                  &zero,
                  &zero,
                  NULL,
                  lwip_hosted_netif_init_cb,
                  ethernet_input) == NULL)
    {
        memset(&s_lwip, 0, sizeof(s_lwip));
        return false;
    }

    if (cfg->hwaddr_valid)
    {
        memcpy(s_lwip.netif.hwaddr, cfg->hwaddr, sizeof(cfg->hwaddr));
        s_lwip.netif.hwaddr_len = 6U;
    }

    netif_set_default(&s_lwip.netif);
    netif_set_down(&s_lwip.netif);
    netif_set_link_down(&s_lwip.netif);
    esp_hosted_lwip_set_input_cb(lwip_hosted_rx_input, NULL);
    s_lwip.initialized = true;
    return true;
}

void lwip_hosted_deinit(void)
{
    if (!s_lwip.initialized)
    {
        return;
    }

    esp_hosted_lwip_set_input_cb(NULL, NULL);
    if (s_lwip.dhcp_running)
    {
        dhcp_release_and_stop(&s_lwip.netif);
    }
    netif_remove(&s_lwip.netif);
    memset(&s_lwip, 0, sizeof(s_lwip));
}

void lwip_hosted_poll(void)
{
    if (!s_lwip.initialized)
    {
        return;
    }

    esp_hosted_poll();
    esp_hosted_dispatch_rx();
    sys_check_timeouts();
}

void lwip_hosted_set_mac(const uint8_t mac[6])
{
    if (!s_lwip.initialized || mac == NULL)
    {
        return;
    }

    memcpy(s_lwip.netif.hwaddr, mac, 6U);
    s_lwip.netif.hwaddr_len = 6U;
}

void lwip_hosted_set_link_up(void)
{
    if (!s_lwip.initialized)
    {
        return;
    }

    if (!s_lwip.link_up)
    {
        netif_set_link_up(&s_lwip.netif);
        netif_set_up(&s_lwip.netif);
        if (s_lwip.dhcp_enabled && !s_lwip.dhcp_running)
        {
            dhcp_start(&s_lwip.netif);
            s_lwip.dhcp_running = true;
        }
        s_lwip.link_up = true;
    }
}

void lwip_hosted_set_link_down(void)
{
    if (!s_lwip.initialized)
    {
        return;
    }

    if (s_lwip.dhcp_running)
    {
        dhcp_release_and_stop(&s_lwip.netif);
        s_lwip.dhcp_running = false;
    }
    lwip_hosted_clear_addrs();
    netif_set_link_down(&s_lwip.netif);
    netif_set_down(&s_lwip.netif);
    s_lwip.link_up = false;
}

bool lwip_hosted_is_link_up(void)
{
    return s_lwip.link_up;
}

bool lwip_hosted_has_ip(void)
{
    return s_lwip.initialized && !ip4_addr_isany_val(*netif_ip4_addr(&s_lwip.netif));
}

struct netif *lwip_hosted_get_netif(void)
{
    return s_lwip.initialized ? &s_lwip.netif : NULL;
}

u32_t sys_now(void)
{
    return (u32_t)uptime_ms();
}

u32_t lwip_hosted_rand(void)
{
    s_lwip_rand_state = (u32_t)(s_lwip_rand_state * 1103515245U + 12345U + (u32_t)uptime_ms());
    return s_lwip_rand_state;
}
