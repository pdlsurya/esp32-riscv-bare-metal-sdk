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

#include <stdint.h>
#include <stdbool.h>
#include "sd_platform.h"
#include "spi_drv.h"
#include "hal/gpio_ll.h"

#if defined(TARGET_SOC_ESP32P4)
#include "hal/ldo_ll.h"
#endif

#if defined(TARGET_SOC_ESP32P4)

#ifndef SD_SPI_USE_INTERNAL_LDO
#define SD_SPI_USE_INTERNAL_LDO 1
#endif

#ifndef SD_SPI_LDO_UNIT
/* Board SD1_VDD source is ESP_LDO_VO4 (VO4 -> unit index 3). */
#define SD_SPI_LDO_UNIT 3
#endif

#ifndef SD_SPI_LDO_VOLTAGE_MV
#define SD_SPI_LDO_VOLTAGE_MV 3300
#endif

#ifndef SD_SPI_LDO_STABLE_DELAY_MS
#define SD_SPI_LDO_STABLE_DELAY_MS 2U
#endif

#ifndef SD_SPI_USE_BOARD_POWER_GPIO
/* SD1_VDD is switched by AO3401 gate driven from GPIO45 on this board. */
#define SD_SPI_USE_BOARD_POWER_GPIO 1
#endif

#ifndef SD_SPI_POWER_GPIO
#define SD_SPI_POWER_GPIO 45U
#endif

#ifndef SD_SPI_POWER_ACTIVE_HIGH
/* AO3401 is PMOS high-side: gate low = ON, gate high = OFF. */
#define SD_SPI_POWER_ACTIVE_HIGH 0
#endif

#ifndef SD_SPI_POWER_STABLE_DELAY_MS
#define SD_SPI_POWER_STABLE_DELAY_MS 5U
#endif

static void sd_spi_internal_ldo_power_on(void)
{
#if SD_SPI_USE_INTERNAL_LDO
    uint8_t dref = 0;
    uint8_t mul = 0;
    bool use_rail = false;

    ldo_ll_set_owner(SD_SPI_LDO_UNIT, LDO_LL_UNIT_OWNER_SW);
    ldo_ll_voltage_to_dref_mul(SD_SPI_LDO_UNIT, SD_SPI_LDO_VOLTAGE_MV, &dref, &mul, &use_rail);
    ldo_ll_adjust_voltage(SD_SPI_LDO_UNIT, dref, mul, use_rail);
    ldo_ll_enable_ripple_suppression(SD_SPI_LDO_UNIT, true);
    ldo_ll_enable(SD_SPI_LDO_UNIT, true);
    delay_ms(SD_SPI_LDO_STABLE_DELAY_MS);
#endif
}

static void sd_spi_board_power_on(void)
{
    sd_spi_internal_ldo_power_on();

#if SD_SPI_USE_BOARD_POWER_GPIO
    gpio_ll_func_sel(&GPIO, SD_SPI_POWER_GPIO, PIN_FUNC_GPIO);
    gpio_ll_input_disable(&GPIO, SD_SPI_POWER_GPIO);
    gpio_ll_output_enable(&GPIO, SD_SPI_POWER_GPIO);
    gpio_ll_set_level(&GPIO, SD_SPI_POWER_GPIO, SD_SPI_POWER_ACTIVE_HIGH ? 1 : 0);
    delay_ms(SD_SPI_POWER_STABLE_DELAY_MS);
#endif
}

#else

static void sd_spi_board_power_on(void)
{
}

#endif

static spi_dev_handle_t s_sd_spi_dev;
static bool s_sd_spi_initialized = false;

void sd_spi_init(void)
{
    if (s_sd_spi_initialized)
    {
        return;
    }

    sd_spi_board_power_on();

    spi_pins_t spi_pins = {
        .sck = SD_SPI_PIN_SCK,
        .miso = SD_SPI_PIN_MISO,
        .mosi = SD_SPI_PIN_MOSI,
    };
    spi_config_t spi_config = {
        .port = SD_SPI_HOST_DEV,
        .pins = spi_pins,
    };
    spi_init(&spi_config);

    s_sd_spi_dev.port = spi_config.port;
    s_sd_spi_dev.speed_hz = SD_SPI_FREQ_HZ;
    s_sd_spi_dev.cs_pin = SD_SPI_PIN_CS;
    s_sd_spi_dev.id = 0;
    s_sd_spi_dev.mode = 0;
    spi_device_config(&s_sd_spi_dev);

    s_sd_spi_initialized = true;
}

void sd_spi_set_frequency(uint32_t hz)
{
    if (!s_sd_spi_initialized || hz == 0U)
    {
        return;
    }

    s_sd_spi_dev.speed_hz = hz;
    spi_device_config(&s_sd_spi_dev);
}

uint8_t sd_spi_transfer(uint8_t tx_byte, bool hold_cs_low)
{
    if (!s_sd_spi_initialized)
    {
        sd_spi_init();
    }
    return spi_transfer_byte(&s_sd_spi_dev, tx_byte, hold_cs_low);
}

void sd_spi_dummy_clocks(uint32_t cycles)
{
    if (!s_sd_spi_initialized)
    {
        sd_spi_init();
    }
    spi_send_dummy_clocks(&s_sd_spi_dev, cycles, false, false);
}
