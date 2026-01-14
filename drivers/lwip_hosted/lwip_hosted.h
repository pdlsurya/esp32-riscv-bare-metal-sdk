/*
 * MIT License
 *
 * Copyright (c) 2025 Surya Poudel
 */

#ifndef __LWIP_HOSTED_H
#define __LWIP_HOSTED_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_hosted.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    esp_hosted_if_t if_type;
    uint8_t if_num;
    const char *hostname;
    bool dhcp;
    uint16_t mtu;
    uint8_t hwaddr[6];
    bool hwaddr_valid;
} lwip_hosted_config_t;

void lwip_hosted_get_default_config(lwip_hosted_config_t *cfg);
bool lwip_hosted_init(const lwip_hosted_config_t *cfg);
void lwip_hosted_deinit(void);
void lwip_hosted_poll(void);
void lwip_hosted_set_mac(const uint8_t mac[6]);
void lwip_hosted_set_link_up(void);
void lwip_hosted_set_link_down(void);
bool lwip_hosted_is_link_up(void);
bool lwip_hosted_has_ip(void);
struct netif *lwip_hosted_get_netif(void);

#ifdef __cplusplus
}
#endif

#endif
