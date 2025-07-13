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

#ifndef __SD_PLATFORM_H
#define __SD_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include "usb_serial.h"
#include "hal/gpio_ll.h"
#include "spi_drv.h"
#include "delay.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef SD_SPI_HOST
#define SD_SPI_HOST 2
#endif

#ifndef SD_SPI_PIN_MISO
#define SD_SPI_PIN_MISO 39
#endif

#ifndef SD_SPI_PIN_MOSI
#define SD_SPI_PIN_MOSI 44
#endif

#ifndef SD_SPI_PIN_SCK
#define SD_SPI_PIN_SCK 43
#endif

#ifndef SD_SPI_PIN_CS
#define SD_SPI_PIN_CS 42
#endif

#ifndef SD_SPI_FREQ_HZ
/* Keep SPI clock low to satisfy card initialization sequence in all states. */
#define SD_SPI_FREQ_HZ 20000000
#endif

#if SD_SPI_HOST == 2
#define SD_SPI_HOST_DEV SPI_GET_HW(2)
#elif SD_SPI_HOST == 3
#define SD_SPI_HOST_DEV SPI_GET_HW(3)
#else
#error "SD_SPI_HOST must be 2 or 3"
#endif

#ifndef PRINTF
#define PRINTF(...) printf(__VA_ARGS__)
#endif

#define sd_cs_select() gpio_ll_set_level(&GPIO, SD_SPI_PIN_CS, 0)
#define sd_cs_deselect() gpio_ll_set_level(&GPIO, SD_SPI_PIN_CS, 1)

    void sd_spi_init(void);

    void sd_spi_dummy_clocks(uint32_t cycles);

    uint8_t sd_spi_transfer(uint8_t tx_byte, bool hold_cs_low);

    static inline void sd_delay_ms(uint32_t ms)
    {
        delay_ms(ms);
    }

#ifdef __cplusplus
}
#endif

#endif
