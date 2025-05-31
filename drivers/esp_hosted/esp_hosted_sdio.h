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

#ifndef __ESP_HOSTED_SDIO_H
#define __ESP_HOSTED_SDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    uint8_t slot;
    uint8_t bus_width;
    uint32_t clock_khz;
    int pin_clk;
    int pin_cmd;
    int pin_d0;
    int pin_d1;
    int pin_d2;
    int pin_d3;
    int pin_reset;
    bool reset_active_low;
    uint16_t reset_pulse_ms;
    uint16_t post_reset_delay_ms;
    bool use_internal_pullups;
} esp_hosted_sdio_config_t;

void esp_hosted_sdio_get_default_config(esp_hosted_sdio_config_t *config);
bool esp_hosted_sdio_attach(const esp_hosted_sdio_config_t *config);
void esp_hosted_sdio_detach(void);
bool esp_hosted_sdio_is_attached(void);

bool esp_hosted_sdio_read_reg(uint32_t reg, void *data, size_t len);
bool esp_hosted_sdio_write_reg(uint32_t reg, const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
