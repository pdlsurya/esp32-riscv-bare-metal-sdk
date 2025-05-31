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

#if !defined(TARGET_SOC_ESP32P4)
#error "drivers/esp_hosted/esp_hosted_sdio.c currently supports ESP32-P4 only"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "esp_bit_defs.h"
#include "esp_hosted.h"
#include "esp_hosted_sdio.h"
#include "delay.h"
#include "esp_rom_gpio.h"
#include "hal/cache_ll.h"
#include "hal/gpio_ll.h"
#include "hal/sd_types.h"
#include "hal/sdmmc_ll.h"
#include "soc/io_mux_reg.h"
#include "soc/sdmmc_periph.h"
#include "soc/sdmmc_pins.h"
#include "usb_serial.h"

#define ESP_HOSTED_SDIO_BLOCK_SIZE              512U
#define ESP_HOSTED_SDIO_PROBE_HOST_DIV          10U
#define ESP_HOSTED_SDIO_PROBE_CARD_DIV          20U
#define ESP_HOSTED_SDIO_CMD_TIMEOUT_LOOPS       200000U
#define ESP_HOSTED_SDIO_DATA_TIMEOUT_LOOPS      1200000U
#define ESP_HOSTED_SDIO_BUSY_TIMEOUT_LOOPS      600000U
#define ESP_HOSTED_SDIO_IO_READY_RETRIES        200U
#define ESP_HOSTED_SDIO_ATTACH_RETRIES          3U
#define ESP_HOSTED_SDIO_DATA_PATH_RETRIES       32U
#define ESP_HOSTED_SDIO_DMA_BUF_SIZE            4096U
#define ESP_HOSTED_SDIO_FRAME_BUF_SIZE          2048U
#define ESP_HOSTED_SDIO_TX_FRAME_BUF_SIZE       2048U
#define ESP_HOSTED_SDIO_TX_BUF_MAX              0x1000U
#define ESP_HOSTED_SDIO_TX_BUF_MASK             0x0FFFU
#define ESP_HOSTED_SDIO_RX_BUF_WINDOW           1536U
#define ESP_HOSTED_SDIO_RX_BYTE_MAX             0x100000U
#define ESP_HOSTED_SDIO_SLAVE_LEN_MASK          0xFFFFFU
#define ESP_HOSTED_SDIO_ADDR_MASK               0x3FFU
#define ESP_HOSTED_SDIO_FUNC_0                  0U
#define ESP_HOSTED_SDIO_FUNC_1                  1U
#define ESP_HOSTED_SDIO_SLOT0                   0U
#define ESP_HOSTED_SDIO_SLOT1                   1U
#define ESP_HOSTED_SDIO_CMD0                    0U
#define ESP_HOSTED_SDIO_CMD3                    3U
#define ESP_HOSTED_SDIO_CMD5                    5U
#define ESP_HOSTED_SDIO_CMD7                    7U
#define ESP_HOSTED_SDIO_CMD52                   52U
#define ESP_HOSTED_SDIO_CMD53                   53U
#define ESP_HOSTED_SDIO_CMD5_OCR_ARG            0x00FF8000U
#define ESP_HOSTED_SDIO_R4_READY                BIT(31)
#define ESP_HOSTED_SDIO_RCA_SHIFT               16U
#define ESP_HOSTED_SDIO_INT_NEW_PACKET          BIT(23)
#define ESP_HOSTED_SDIO_INT_START_THROTTLE      BIT(7)
#define ESP_HOSTED_SDIO_INT_STOP_THROTTLE       BIT(6)
#define ESP_HOSTED_SDIO_CCCR_FN_ENABLE          0x02U
#define ESP_HOSTED_SDIO_CCCR_FN_READY           0x03U
#define ESP_HOSTED_SDIO_CCCR_INT_ENABLE         0x04U
#define ESP_HOSTED_SDIO_CCCR_BUS_WIDTH          0x07U
#define ESP_HOSTED_SDIO_CCCR_BLKSIZEL           0x10U
#define ESP_HOSTED_SDIO_CCCR_BLKSIZEH           0x11U
#define ESP_HOSTED_SDIO_CCCR_BUS_WIDTH_4        0x02U
#define ESP_HOSTED_SDIO_CCCR_FN1_EN             BIT(1)
#define ESP_HOSTED_SDIO_CCCR_INT_MASTER_EN      BIT(0)
#define ESP_HOSTED_SDIO_SLAVE_BASE              0x3FF55000U
#define ESP_HOSTED_SDIO_HOST_TO_SLAVE_INTR      (ESP_HOSTED_SDIO_SLAVE_BASE + 0x8CU)
#define ESP_HOSTED_SDIO_SLAVE_INT_RAW_REG       (ESP_HOSTED_SDIO_SLAVE_BASE + 0x50U)
#define ESP_HOSTED_SDIO_SLAVE_INT_CLR_REG       (ESP_HOSTED_SDIO_SLAVE_BASE + 0xD4U)
#define ESP_HOSTED_SDIO_SLAVE_PACKET_LEN_REG    (ESP_HOSTED_SDIO_SLAVE_BASE + 0x60U)
#define ESP_HOSTED_SDIO_SLAVE_TOKEN_RDATA       (ESP_HOSTED_SDIO_SLAVE_BASE + 0x44U)
#define ESP_HOSTED_SDIO_SLAVE_CMD53_END_ADDR    0x1F800U
#define ESP_HOSTED_SDIO_HOST_INTR_OPEN_DATA     0U
#define ESP_HOSTED_SDIO_MORE_FRAGMENT           BIT(0)
#define ESP_HOSTED_SDIO_CMD_ERR_MASK            (SDMMC_LL_EVENT_RESP_ERR | SDMMC_LL_EVENT_RTO | SDMMC_LL_EVENT_RCRC | SDMMC_LL_EVENT_HLE | SDMMC_LL_EVENT_SBE | SDMMC_LL_EVENT_EBE)
#define ESP_HOSTED_SDIO_DATA_ERR_MASK           (SDMMC_LL_EVENT_DCRC | SDMMC_LL_EVENT_FRUN | SDMMC_LL_EVENT_HTO | SDMMC_LL_EVENT_EBE | SDMMC_LL_EVENT_SBE)
#define ESP_HOSTED_SDIO_DMA_DONE_MASK           (SDMMC_LL_EVENT_DMA_TI | SDMMC_LL_EVENT_DMA_RI | SDMMC_LL_EVENT_DMA_NI)
#define ESP_HOSTED_SDIO_DMA_ERR_MASK            (BIT(2) | BIT(4) | BIT(5) | BIT(9))
#define ESP_HOSTED_SDIO_IDSTS_DMA_FSM_SHIFT     13U
#define ESP_HOSTED_SDIO_IDSTS_DMA_FSM_MASK      0xFU
#define ESP_HOSTED_SDIO_DMA_FSM_SUSPEND         1U
#define ESP_HOSTED_SDIO_DMA_FSM_READ_REQ_WAIT   4U
#define ESP_HOSTED_SDIO_DMA_FSM_WRITE_REQ_WAIT  5U

#ifndef ESP_HOSTED_SDIO_DEBUG
#define ESP_HOSTED_SDIO_DEBUG 0
#endif

#if ESP_HOSTED_SDIO_DEBUG
#define ESP_HOSTED_SDIO_LOG(...) serial_printf(__VA_ARGS__)
#else
#define ESP_HOSTED_SDIO_LOG(...) ((void)0)
#endif

#define ESP_HOSTED_SDIO_ERR(...) serial_printf("[esp_hosted_sdio] " __VA_ARGS__)

typedef enum
{
    ESP_HOSTED_SDIO_RESP_NONE = 0,
    ESP_HOSTED_SDIO_RESP_SHORT = 1,
    ESP_HOSTED_SDIO_RESP_LONG = 2,
} esp_hosted_sdio_resp_t;

typedef struct __attribute__((packed))
{
    uint8_t if_type : 4;
    uint8_t if_num : 4;
    uint8_t flags;
    uint16_t len;
    uint16_t offset;
    uint16_t checksum;
    uint16_t seq_num;
    uint8_t throttle_cmd : 2;
    uint8_t reserved2 : 6;
    union
    {
        uint8_t reserved3;
        uint8_t hci_pkt_type;
        uint8_t priv_pkt_type;
    };
} esp_hosted_sdio_payload_header_t;

typedef struct
{
    esp_hosted_sdio_config_t config;
    bool attached;
    bool tx_throttled;
    uint16_t rca;
    uint32_t rx_byte_count;
    uint32_t tx_buf_count;
    sdmmc_desc_t dma_desc __attribute__((aligned(64)));
    uint8_t dma_bounce[ESP_HOSTED_SDIO_DMA_BUF_SIZE] __attribute__((aligned(64)));
    uint8_t frame_buf[ESP_HOSTED_SDIO_FRAME_BUF_SIZE] __attribute__((aligned(64)));
    uint8_t tx_frame[ESP_HOSTED_SDIO_TX_FRAME_BUF_SIZE] __attribute__((aligned(64)));
} esp_hosted_sdio_ctx_t;

static sdmmc_dev_t *const s_sdmmc = &SDMMC;
static esp_hosted_sdio_ctx_t s_ctx;
static uint32_t s_cmd52_int_raw_fail_count;

static size_t esp_hosted_sdio_transport_tx(const esp_hosted_frame_info_t *info,
                                           const void *payload,
                                           size_t len,
                                           void *ctx);
static void esp_hosted_sdio_transport_poll(void *ctx);

static const esp_hosted_transport_t s_transport = {
    .tx = esp_hosted_sdio_transport_tx,
    .poll = esp_hosted_sdio_transport_poll,
    .ctx = &s_ctx,
};

static inline uint32_t esp_hosted_sdio_get_events(void)
{
    return sdmmc_ll_get_interrupt_raw(s_sdmmc);
}

static inline uint32_t esp_hosted_sdio_round_up_blocks(uint32_t len)
{
    return (len + ESP_HOSTED_SDIO_BLOCK_SIZE - 1U) / ESP_HOSTED_SDIO_BLOCK_SIZE;
}

static uint16_t esp_hosted_sdio_compute_checksum(const uint8_t *buf, uint32_t len)
{
    uint32_t i;
    uint32_t checksum = 0U;

    if (buf == NULL)
    {
        return 0U;
    }

    for (i = 0; i < len; i++)
    {
        checksum += buf[i];
    }

    return (uint16_t)checksum;
}

static bool esp_hosted_sdio_wait_cmd_taken(uint32_t timeout_loops)
{
    while (timeout_loops-- > 0U)
    {
        if (s_sdmmc->cmd.start_command == 0U)
        {
            return true;
        }
    }
    return false;
}

static bool esp_hosted_sdio_wait_reset_done(void)
{
    uint32_t wait_loops = ESP_HOSTED_SDIO_CMD_TIMEOUT_LOOPS;

    while (wait_loops-- > 0U)
    {
        if (sdmmc_ll_is_controller_reset_done(s_sdmmc) &&
            sdmmc_ll_is_fifo_reset_done(s_sdmmc) &&
            sdmmc_ll_is_dma_reset_done(s_sdmmc))
        {
            return true;
        }
    }
    return false;
}

static void esp_hosted_sdio_log_reset_state(void)
{
    ESP_HOSTED_SDIO_ERR("reset state ctrl=0x%08lx clkdiv=0x%08lx clksrc=0x%08lx clkena=0x%08lx raw=0x%08lx\n",
                        (unsigned long)s_sdmmc->ctrl.val,
                        (unsigned long)s_sdmmc->clkdiv.val,
                        (unsigned long)s_sdmmc->clksrc.val,
                        (unsigned long)s_sdmmc->clkena.val,
                        (unsigned long)esp_hosted_sdio_get_events());
}

static bool esp_hosted_sdio_wait_data_idle(void)
{
    uint32_t timeout_loops = ESP_HOSTED_SDIO_BUSY_TIMEOUT_LOOPS;

    while (timeout_loops-- > 0U)
    {
        if (!sdmmc_ll_is_card_data_busy(s_sdmmc))
        {
            return true;
        }
        delay_us(1U);
    }
    return false;
}

static bool esp_hosted_sdio_update_clock_registers(uint8_t slot)
{
    sdmmc_hw_cmd_t cmd = {0};

    if (!esp_hosted_sdio_wait_cmd_taken(ESP_HOSTED_SDIO_CMD_TIMEOUT_LOOPS))
    {
        return false;
    }

    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_set_command_arg(s_sdmmc, 0U);
    cmd.card_num = slot;
    cmd.update_clk_reg = 1;
    cmd.wait_complete = 1;
    cmd.use_hold_reg = 1;
    cmd.start_command = 1;
    sdmmc_ll_set_command(s_sdmmc, cmd);

    if (!esp_hosted_sdio_wait_cmd_taken(ESP_HOSTED_SDIO_CMD_TIMEOUT_LOOPS))
    {
        return false;
    }

    if ((esp_hosted_sdio_get_events() & ESP_HOSTED_SDIO_CMD_ERR_MASK) != 0U)
    {
        sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
        return false;
    }

    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    return true;
}

static bool esp_hosted_sdio_set_clock(uint8_t slot, uint32_t host_div, uint8_t card_div)
{
    sdmmc_ll_enable_card_clock(s_sdmmc, slot, false);
    if (!esp_hosted_sdio_update_clock_registers(slot))
    {
        return false;
    }

    sdmmc_ll_set_card_clock_div(s_sdmmc, slot, card_div);
    sdmmc_ll_set_clock_div(s_sdmmc, host_div);
    sdmmc_ll_init_phase_delay(s_sdmmc);
    if (!esp_hosted_sdio_update_clock_registers(slot))
    {
        return false;
    }

    sdmmc_ll_enable_card_clock(s_sdmmc, slot, true);
    if (!esp_hosted_sdio_update_clock_registers(slot))
    {
        return false;
    }

    sdmmc_ll_enable_card_clock_low_power(s_sdmmc, slot, true);
    return true;
}

static bool esp_hosted_sdio_send_cmd(const esp_hosted_sdio_ctx_t *ctx,
                                     uint8_t cmd_idx,
                                     uint32_t arg,
                                     esp_hosted_sdio_resp_t resp_type,
                                     bool check_crc,
                                     bool send_init,
                                     uint32_t *resp0)
{
    sdmmc_hw_cmd_t cmd = {0};
    uint32_t raw = 0;
    uint32_t timeout_loops = ESP_HOSTED_SDIO_CMD_TIMEOUT_LOOPS;

    if (!esp_hosted_sdio_wait_cmd_taken(ESP_HOSTED_SDIO_CMD_TIMEOUT_LOOPS))
    {
        return false;
    }

    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_set_command_arg(s_sdmmc, arg);

    cmd.cmd_index = cmd_idx;
    cmd.card_num = ctx->config.slot;
    cmd.wait_complete = 1;
    cmd.send_init = send_init ? 1U : 0U;
    cmd.response_expect = (resp_type == ESP_HOSTED_SDIO_RESP_NONE) ? 0U : 1U;
    cmd.response_long = (resp_type == ESP_HOSTED_SDIO_RESP_LONG) ? 1U : 0U;
    cmd.check_response_crc = (check_crc && (resp_type != ESP_HOSTED_SDIO_RESP_NONE)) ? 1U : 0U;
    cmd.use_hold_reg = 1;
    cmd.start_command = 1;
    sdmmc_ll_set_command(s_sdmmc, cmd);

    if (!esp_hosted_sdio_wait_cmd_taken(ESP_HOSTED_SDIO_CMD_TIMEOUT_LOOPS))
    {
        return false;
    }

    while (timeout_loops-- > 0U)
    {
        raw = esp_hosted_sdio_get_events();
        if ((raw & ESP_HOSTED_SDIO_CMD_ERR_MASK) != 0U)
        {
            sdmmc_ll_clear_interrupt(s_sdmmc, raw);
            return false;
        }
        if ((raw & SDMMC_LL_EVENT_CMD_DONE) != 0U)
        {
            sdmmc_ll_clear_interrupt(s_sdmmc, raw);
            if (resp0 != NULL && resp_type != ESP_HOSTED_SDIO_RESP_NONE)
            {
                *resp0 = s_sdmmc->resp[0];
            }
            return true;
        }
    }

    return false;
}

static bool esp_hosted_sdio_transfer_data(const esp_hosted_sdio_ctx_t *ctx,
                                          uint8_t cmd_idx,
                                          uint32_t arg,
                                          uint8_t *buffer,
                                          uint32_t len,
                                          uint32_t block_size,
                                          bool is_write)
{
    sdmmc_hw_cmd_t cmd = {0};
    uint32_t raw = 0;
    uint32_t dma_raw = 0;
    uint8_t *dma_buf = s_ctx.dma_bounce;
    uint8_t *dma_buf_nc = (uint8_t *)CACHE_LL_L2MEM_NON_CACHE_ADDR(dma_buf);
    sdmmc_desc_t *desc = &s_ctx.dma_desc;
    sdmmc_desc_t *desc_nc = (sdmmc_desc_t *)CACHE_LL_L2MEM_NON_CACHE_ADDR(desc);
    bool data_done = false;
    bool dma_done = false;
    uint32_t timeout_loops = ESP_HOSTED_SDIO_DATA_TIMEOUT_LOOPS;

    if (len == 0U || len > ESP_HOSTED_SDIO_DMA_BUF_SIZE || block_size == 0U)
    {
        return false;
    }

    if (is_write)
    {
        memcpy(dma_buf_nc, buffer, len);
    }

    if (!esp_hosted_sdio_wait_data_idle())
    {
        return false;
    }

    if (!esp_hosted_sdio_wait_cmd_taken(ESP_HOSTED_SDIO_CMD_TIMEOUT_LOOPS))
    {
        return false;
    }

    sdmmc_ll_reset_fifo(s_sdmmc);
    sdmmc_ll_reset_dma(s_sdmmc);
    if (!esp_hosted_sdio_wait_reset_done())
    {
        return false;
    }

    sdmmc_ll_init_dma(s_sdmmc);
    sdmmc_ll_enable_dma(s_sdmmc, true);
    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_clear_idsts_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_set_block_size(s_sdmmc, block_size);
    sdmmc_ll_set_data_transfer_len(s_sdmmc, len);
    sdmmc_ll_set_command_arg(s_sdmmc, arg);

    memset(desc_nc, 0, sizeof(*desc_nc));
    desc_nc->first_descriptor = 1;
    desc_nc->last_descriptor = 1;
    desc_nc->second_address_chained = 1;
    desc_nc->end_of_ring = 1;
    desc_nc->owned_by_idmac = 1;
    desc_nc->buffer1_size = len;
    desc_nc->buffer1_ptr = dma_buf;
    desc_nc->next_desc_ptr = NULL;
    sdmmc_ll_set_desc_addr(s_sdmmc, (uint32_t)(uintptr_t)desc);
    sdmmc_ll_poll_demand(s_sdmmc);

    cmd.cmd_index = cmd_idx;
    cmd.card_num = ctx->config.slot;
    cmd.response_expect = 1;
    cmd.check_response_crc = 1;
    cmd.data_expected = 1;
    cmd.rw = is_write ? 1U : 0U;
    cmd.wait_complete = 1;
    cmd.use_hold_reg = 1;
    cmd.transfer_mode = 0;
    cmd.start_command = 1;
    sdmmc_ll_set_command(s_sdmmc, cmd);

    if (!esp_hosted_sdio_wait_cmd_taken(ESP_HOSTED_SDIO_CMD_TIMEOUT_LOOPS))
    {
        return false;
    }

    while (timeout_loops-- > 0U)
    {
        raw = esp_hosted_sdio_get_events();
        dma_raw = sdmmc_ll_get_idsts_interrupt_raw(s_sdmmc);

        if ((raw & ESP_HOSTED_SDIO_CMD_ERR_MASK) != 0U)
        {
            sdmmc_ll_clear_interrupt(s_sdmmc, raw);
            return false;
        }
        if ((raw & ESP_HOSTED_SDIO_DATA_ERR_MASK) != 0U)
        {
            sdmmc_ll_clear_interrupt(s_sdmmc, raw);
            return false;
        }
        if ((dma_raw & ESP_HOSTED_SDIO_DMA_ERR_MASK) != 0U)
        {
            sdmmc_ll_clear_idsts_interrupt(s_sdmmc, dma_raw);
            return false;
        }
        if ((dma_raw & ESP_HOSTED_SDIO_DMA_DONE_MASK) != 0U)
        {
            dma_done = true;
            sdmmc_ll_clear_idsts_interrupt(s_sdmmc, dma_raw);
        }
        if ((raw & SDMMC_LL_EVENT_CMD_DONE) != 0U)
        {
            sdmmc_ll_clear_interrupt(s_sdmmc, SDMMC_LL_EVENT_CMD_DONE);
        }
        if ((raw & SDMMC_LL_EVENT_DATA_OVER) != 0U)
        {
            data_done = true;
            sdmmc_ll_clear_interrupt(s_sdmmc, raw);
        }

        raw = (dma_raw >> ESP_HOSTED_SDIO_IDSTS_DMA_FSM_SHIFT) & ESP_HOSTED_SDIO_IDSTS_DMA_FSM_MASK;
        if (raw == ESP_HOSTED_SDIO_DMA_FSM_SUSPEND ||
            raw == ESP_HOSTED_SDIO_DMA_FSM_READ_REQ_WAIT ||
            raw == ESP_HOSTED_SDIO_DMA_FSM_WRITE_REQ_WAIT)
        {
            sdmmc_ll_poll_demand(s_sdmmc);
        }

        if (data_done && dma_done)
        {
            if (!is_write)
            {
                memcpy(buffer, dma_buf_nc, len);
            }
            return esp_hosted_sdio_wait_data_idle();
        }
    }

    return false;
}

static uint32_t esp_hosted_sdio_calc_host_div(uint32_t clock_khz)
{
    uint32_t host_div;

    if (clock_khz == 0U)
    {
        return 8U;
    }

    host_div = (160000U + clock_khz - 1U) / clock_khz;
    if (host_div < 1U)
    {
        host_div = 1U;
    }
    if (host_div > 255U)
    {
        host_div = 255U;
    }

    return host_div;
}

static void esp_hosted_sdio_setup_reset_pin(const esp_hosted_sdio_config_t *config)
{
    if (config->pin_reset < 0)
    {
        return;
    }

    gpio_ll_func_sel(&GPIO, (uint32_t)config->pin_reset, PIN_FUNC_GPIO);
    gpio_ll_input_disable(&GPIO, (uint32_t)config->pin_reset);
    gpio_ll_output_enable(&GPIO, (uint32_t)config->pin_reset);
    gpio_ll_set_level(&GPIO, (uint32_t)config->pin_reset, config->reset_active_low ? 1 : 0);
    delay_ms(1U);
    gpio_ll_set_level(&GPIO, (uint32_t)config->pin_reset, config->reset_active_low ? 0 : 1);
    delay_ms(config->reset_pulse_ms);
    gpio_ll_set_level(&GPIO, (uint32_t)config->pin_reset, config->reset_active_low ? 1 : 0);
    delay_ms(config->post_reset_delay_ms);
}

static void esp_hosted_sdio_setup_iomux_pin(uint32_t gpio_num, bool use_pullup)
{
    gpio_ll_func_sel(&GPIO, gpio_num, SDMMC_SLOT0_FUNC);
    gpio_ll_pulldown_dis(&GPIO, gpio_num);
    gpio_ll_input_enable(&GPIO, gpio_num);
    gpio_ll_set_drive_capability(&GPIO, gpio_num, GPIO_DRIVE_CAP_3);
    if (use_pullup)
    {
        gpio_ll_pullup_en(&GPIO, gpio_num);
    }
    else
    {
        gpio_ll_pullup_dis(&GPIO, gpio_num);
    }
}

static bool esp_hosted_sdio_setup_slot0_pins(const esp_hosted_sdio_config_t *config)
{
    if ((config->pin_clk >= 0 && config->pin_clk != SDMMC_SLOT0_IOMUX_PIN_NUM_CLK) ||
        (config->pin_cmd >= 0 && config->pin_cmd != SDMMC_SLOT0_IOMUX_PIN_NUM_CMD) ||
        (config->pin_d0 >= 0 && config->pin_d0 != SDMMC_SLOT0_IOMUX_PIN_NUM_D0) ||
        (config->bus_width >= 4U && config->pin_d1 >= 0 && config->pin_d1 != SDMMC_SLOT0_IOMUX_PIN_NUM_D1) ||
        (config->bus_width >= 4U && config->pin_d2 >= 0 && config->pin_d2 != SDMMC_SLOT0_IOMUX_PIN_NUM_D2) ||
        (config->bus_width >= 4U && config->pin_d3 >= 0 && config->pin_d3 != SDMMC_SLOT0_IOMUX_PIN_NUM_D3))
    {
        ESP_HOSTED_SDIO_ERR("slot0 only supports native IOMUX pins\n");
        return false;
    }

    esp_hosted_sdio_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_CLK, false);
    esp_hosted_sdio_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_CMD, config->use_internal_pullups);
    esp_hosted_sdio_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_D0, config->use_internal_pullups);
    if (config->bus_width >= 4U)
    {
        esp_hosted_sdio_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_D1, config->use_internal_pullups);
        esp_hosted_sdio_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_D2, config->use_internal_pullups);
        esp_hosted_sdio_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_D3, config->use_internal_pullups);
    }

    return true;
}

static void esp_hosted_sdio_setup_matrix_pin(uint32_t gpio_num, bool output_en, bool input_en, bool pullup)
{
    gpio_ll_func_sel(&GPIO, gpio_num, PIN_FUNC_GPIO);
    gpio_ll_pulldown_dis(&GPIO, gpio_num);
    if (input_en)
    {
        gpio_ll_input_enable(&GPIO, gpio_num);
    }
    else
    {
        gpio_ll_input_disable(&GPIO, gpio_num);
    }
    if (output_en)
    {
        gpio_ll_output_enable(&GPIO, gpio_num);
    }
    else
    {
        gpio_ll_output_disable(&GPIO, gpio_num);
    }
    gpio_ll_set_drive_capability(&GPIO, gpio_num, GPIO_DRIVE_CAP_3);
    if (pullup)
    {
        gpio_ll_pullup_en(&GPIO, gpio_num);
    }
    else
    {
        gpio_ll_pullup_dis(&GPIO, gpio_num);
    }
}

static bool esp_hosted_sdio_setup_slot1_pins(const esp_hosted_sdio_config_t *config)
{
    const sdmmc_slot_io_info_t *sig = &sdmmc_slot_gpio_sig[ESP_HOSTED_SDIO_SLOT1];

    if (config->pin_clk < 0 || config->pin_cmd < 0 || config->pin_d0 < 0)
    {
        return false;
    }
    if (config->bus_width >= 4U && (config->pin_d1 < 0 || config->pin_d2 < 0 || config->pin_d3 < 0))
    {
        return false;
    }

    esp_hosted_sdio_setup_matrix_pin((uint32_t)config->pin_clk, true, false, false);
    esp_rom_gpio_connect_out_signal((uint32_t)config->pin_clk, sig->clk, false, false);

    esp_hosted_sdio_setup_matrix_pin((uint32_t)config->pin_cmd, true, true, config->use_internal_pullups);
    esp_rom_gpio_connect_out_signal((uint32_t)config->pin_cmd, sig->cmd, false, false);
    esp_rom_gpio_connect_in_signal((uint32_t)config->pin_cmd, sig->cmd, false);

    esp_hosted_sdio_setup_matrix_pin((uint32_t)config->pin_d0, true, true, config->use_internal_pullups);
    esp_rom_gpio_connect_out_signal((uint32_t)config->pin_d0, sig->d0, false, false);
    esp_rom_gpio_connect_in_signal((uint32_t)config->pin_d0, sig->d0, false);

    if (config->bus_width >= 4U)
    {
        esp_hosted_sdio_setup_matrix_pin((uint32_t)config->pin_d1, true, true, config->use_internal_pullups);
        esp_rom_gpio_connect_out_signal((uint32_t)config->pin_d1, sig->d1, false, false);
        esp_rom_gpio_connect_in_signal((uint32_t)config->pin_d1, sig->d1, false);

        esp_hosted_sdio_setup_matrix_pin((uint32_t)config->pin_d2, true, true, config->use_internal_pullups);
        esp_rom_gpio_connect_out_signal((uint32_t)config->pin_d2, sig->d2, false, false);
        esp_rom_gpio_connect_in_signal((uint32_t)config->pin_d2, sig->d2, false);

        esp_hosted_sdio_setup_matrix_pin((uint32_t)config->pin_d3, true, true, config->use_internal_pullups);
        esp_rom_gpio_connect_out_signal((uint32_t)config->pin_d3, sig->d3, false, false);
        esp_rom_gpio_connect_in_signal((uint32_t)config->pin_d3, sig->d3, false);
    }

    return true;
}

static bool esp_hosted_sdio_setup_pins(const esp_hosted_sdio_config_t *config)
{
    if (config->slot == ESP_HOSTED_SDIO_SLOT0)
    {
        return esp_hosted_sdio_setup_slot0_pins(config);
    }
    if (config->slot == ESP_HOSTED_SDIO_SLOT1)
    {
        return esp_hosted_sdio_setup_slot1_pins(config);
    }
    return false;
}

static bool esp_hosted_sdio_hw_init(esp_hosted_sdio_ctx_t *ctx)
{
    sd_bus_width_t width = (ctx->config.bus_width >= 4U) ? SD_BUS_WIDTH_4_BIT : SD_BUS_WIDTH_1_BIT;

    esp_hosted_sdio_setup_reset_pin(&ctx->config);

    if (!esp_hosted_sdio_setup_pins(&ctx->config))
    {
        ESP_HOSTED_SDIO_ERR("pin setup failed\n");
        return false;
    }

    sdmmc_ll_enable_bus_clock(s_sdmmc, true);
    sdmmc_ll_enable_sdio_pll(s_sdmmc, true);
    sdmmc_ll_select_clk_source(s_sdmmc, SDMMC_CLK_SRC_PLL160M);
    sdmmc_ll_set_clock_div(s_sdmmc, ESP_HOSTED_SDIO_PROBE_HOST_DIV);
    sdmmc_ll_init_phase_delay(s_sdmmc);
    sdmmc_ll_reset_controller(s_sdmmc);
    sdmmc_ll_reset_fifo(s_sdmmc);
    sdmmc_ll_reset_dma(s_sdmmc);
    if (!esp_hosted_sdio_wait_reset_done())
    {
        ESP_HOSTED_SDIO_ERR("controller reset timeout\n");
        esp_hosted_sdio_log_reset_state();
        return false;
    }

    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_clear_idsts_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_enable_dma(s_sdmmc, false);
    sdmmc_ll_enable_global_interrupt(s_sdmmc, false);
    sdmmc_ll_set_response_timeout(s_sdmmc, 0xFFU);
    sdmmc_ll_set_data_timeout(s_sdmmc, 0x00FFFFFFU);
    sdmmc_ll_set_card_width(s_sdmmc, ctx->config.slot, width);

    if (!esp_hosted_sdio_set_clock(ctx->config.slot,
                                   ESP_HOSTED_SDIO_PROBE_HOST_DIV,
                                   ESP_HOSTED_SDIO_PROBE_CARD_DIV))
    {
        ESP_HOSTED_SDIO_ERR("probe clock setup failed\n");
        return false;
    }

    if (!esp_hosted_sdio_send_cmd(ctx,
                                  ESP_HOSTED_SDIO_CMD0,
                                  0U,
                                  ESP_HOSTED_SDIO_RESP_NONE,
                                  false,
                                  true,
                                  NULL))
    {
        ESP_HOSTED_SDIO_ERR("CMD0 failed\n");
        return false;
    }

    return true;
}

static void esp_hosted_sdio_hw_deinit(const esp_hosted_sdio_ctx_t *ctx)
{
    sdmmc_ll_enable_dma(s_sdmmc, false);
    sdmmc_ll_enable_card_clock(s_sdmmc, ctx->config.slot, false);
    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_clear_idsts_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_enable_bus_clock(s_sdmmc, false);
}

static bool esp_hosted_sdio_cmd52_rw(uint8_t function,
                                     uint32_t reg,
                                     bool is_write,
                                     bool raw,
                                     uint8_t *data)
{
    uint32_t resp = 0;
    uint32_t arg;

    if (data == NULL)
    {
        return false;
    }

    arg = ((uint32_t)is_write << 31) |
          ((uint32_t)function << 28) |
          ((uint32_t)raw << 27) |
          ((reg & 0x1FFFFU) << 9) |
          (uint32_t)(*data);

    if (!esp_hosted_sdio_send_cmd(&s_ctx,
                                  ESP_HOSTED_SDIO_CMD52,
                                  arg,
                                  ESP_HOSTED_SDIO_RESP_SHORT,
                                  true,
                                  false,
                                  &resp))
    {
        if (function == ESP_HOSTED_SDIO_FUNC_1 &&
            reg == (ESP_HOSTED_SDIO_SLAVE_INT_RAW_REG & ESP_HOSTED_SDIO_ADDR_MASK) &&
            !is_write &&
            !raw)
        {
            s_cmd52_int_raw_fail_count++;
            if (s_cmd52_int_raw_fail_count == 1U ||
                (s_cmd52_int_raw_fail_count % 4096U) == 0U)
            {
                ESP_HOSTED_SDIO_ERR("CMD52 failed fn=%u reg=0x%05lx (%lu times)\n",
                                    (unsigned)function,
                                    (unsigned long)reg,
                                    (unsigned long)s_cmd52_int_raw_fail_count);
            }
        }
        else
        {
            ESP_HOSTED_SDIO_ERR("CMD52 failed fn=%u reg=0x%05lx write=%u raw=%u val=0x%02x\n",
                                (unsigned)function,
                                (unsigned long)reg,
                                (unsigned)is_write,
                                (unsigned)raw,
                                (unsigned)*data);
        }
        return false;
    }

    *data = (uint8_t)(resp & 0xFFU);
    return true;
}

static bool esp_hosted_sdio_cmd53_rw(uint8_t function,
                                     uint32_t addr,
                                     bool is_write,
                                     bool block_mode,
                                     bool op_code,
                                     uint8_t *data,
                                     uint32_t len)
{
    uint32_t count;
    uint32_t arg;
    uint32_t block_size;

    if (data == NULL || len == 0U || len > ESP_HOSTED_SDIO_DMA_BUF_SIZE)
    {
        return false;
    }

    count = block_mode ? (len / ESP_HOSTED_SDIO_BLOCK_SIZE) : (len & 0x1FFU);
    if (!block_mode && len == ESP_HOSTED_SDIO_BLOCK_SIZE)
    {
        count = 0U;
    }

    arg = ((uint32_t)is_write << 31) |
          ((uint32_t)function << 28) |
          ((uint32_t)block_mode << 27) |
          ((uint32_t)op_code << 26) |
          ((addr & 0x1FFFFU) << 9) |
          (count & 0x1FFU);

    block_size = block_mode ? ESP_HOSTED_SDIO_BLOCK_SIZE : len;
    return esp_hosted_sdio_transfer_data(&s_ctx,
                                         ESP_HOSTED_SDIO_CMD53,
                                         arg,
                                         data,
                                         len,
                                         block_size,
                                         is_write);
}

static bool esp_hosted_sdio_read_bytes(uint8_t function, uint32_t addr, uint8_t *data, uint32_t len)
{
    while (len >= ESP_HOSTED_SDIO_BLOCK_SIZE)
    {
        uint32_t chunk_blocks = esp_hosted_sdio_round_up_blocks(len);
        uint32_t chunk;

        if (chunk_blocks > (ESP_HOSTED_SDIO_DMA_BUF_SIZE / ESP_HOSTED_SDIO_BLOCK_SIZE))
        {
            chunk_blocks = ESP_HOSTED_SDIO_DMA_BUF_SIZE / ESP_HOSTED_SDIO_BLOCK_SIZE;
        }
        chunk = chunk_blocks * ESP_HOSTED_SDIO_BLOCK_SIZE;
        if (chunk > len)
        {
            chunk = (len / ESP_HOSTED_SDIO_BLOCK_SIZE) * ESP_HOSTED_SDIO_BLOCK_SIZE;
        }
        if (chunk == 0U)
        {
            break;
        }
        if (!esp_hosted_sdio_cmd53_rw(function, addr, false, true, true, data, chunk))
        {
            return false;
        }
        addr += chunk;
        data += chunk;
        len -= chunk;
    }

    while (len > 0U)
    {
        uint32_t chunk = (len > ESP_HOSTED_SDIO_BLOCK_SIZE) ? ESP_HOSTED_SDIO_BLOCK_SIZE : len;
        if (!esp_hosted_sdio_cmd53_rw(function, addr, false, false, true, data, chunk))
        {
            return false;
        }
        addr += chunk;
        data += chunk;
        len -= chunk;
    }

    return true;
}

static bool esp_hosted_sdio_write_bytes(uint8_t function, uint32_t addr, const uint8_t *data, uint32_t len)
{
    while (len >= ESP_HOSTED_SDIO_BLOCK_SIZE)
    {
        uint32_t chunk_blocks = esp_hosted_sdio_round_up_blocks(len);
        uint32_t chunk;

        if (chunk_blocks > (ESP_HOSTED_SDIO_DMA_BUF_SIZE / ESP_HOSTED_SDIO_BLOCK_SIZE))
        {
            chunk_blocks = ESP_HOSTED_SDIO_DMA_BUF_SIZE / ESP_HOSTED_SDIO_BLOCK_SIZE;
        }
        chunk = chunk_blocks * ESP_HOSTED_SDIO_BLOCK_SIZE;
        if (chunk > len)
        {
            chunk = (len / ESP_HOSTED_SDIO_BLOCK_SIZE) * ESP_HOSTED_SDIO_BLOCK_SIZE;
        }
        if (chunk == 0U)
        {
            break;
        }
        if (!esp_hosted_sdio_cmd53_rw(function, addr, true, true, true, (uint8_t *)data, chunk))
        {
            return false;
        }
        addr += chunk;
        data += chunk;
        len -= chunk;
    }

    while (len > 0U)
    {
        uint32_t chunk = (len > ESP_HOSTED_SDIO_BLOCK_SIZE) ? ESP_HOSTED_SDIO_BLOCK_SIZE : len;
        if (!esp_hosted_sdio_cmd53_rw(function, addr, true, false, true, (uint8_t *)data, chunk))
        {
            return false;
        }
        addr += chunk;
        data += chunk;
        len -= chunk;
    }

    return true;
}

static bool esp_hosted_sdio_write_padded_blocks(uint8_t function,
                                                uint32_t addr,
                                                const uint8_t *data,
                                                uint32_t actual_len)
{
    uint32_t padded_len;

    if (data == NULL || actual_len == 0U)
    {
        return false;
    }

    padded_len = esp_hosted_sdio_round_up_blocks(actual_len) * ESP_HOSTED_SDIO_BLOCK_SIZE;
    if (padded_len > sizeof(s_ctx.tx_frame))
    {
        return false;
    }

    if (padded_len != actual_len)
    {
        memset(s_ctx.tx_frame + actual_len, 0, padded_len - actual_len);
    }

    return esp_hosted_sdio_cmd53_rw(function,
                                    addr,
                                    true,
                                    true,
                                    true,
                                    (uint8_t *)data,
                                    padded_len);
}

static bool esp_hosted_sdio_read_reg_bytes(uint32_t reg, uint8_t *data, uint32_t len)
{
    uint32_t i;

    if (data == NULL || len == 0U)
    {
        return false;
    }

    reg &= ESP_HOSTED_SDIO_ADDR_MASK;
    if (len > 1U)
    {
        /*
         * Match esp-hosted reference behavior: use SDIO byte-stream register
         * access for multi-byte reads, then fallback to CMD52 loop if needed.
         */
        if (esp_hosted_sdio_cmd53_rw(ESP_HOSTED_SDIO_FUNC_1, reg, false, false, true, data, len))
        {
            return true;
        }
    }

    for (i = 0; i < len; i++)
    {
        uint8_t value = 0U;
        if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_1, reg + i, false, false, &value))
        {
            return false;
        }
        data[i] = value;
    }

    return true;
}

static bool esp_hosted_sdio_write_reg_bytes(uint32_t reg, const uint8_t *data, uint32_t len)
{
    uint32_t i;

    if (data == NULL || len == 0U)
    {
        return false;
    }

    reg &= ESP_HOSTED_SDIO_ADDR_MASK;
    if (len > 1U)
    {
        /*
         * Prefer SDIO byte-stream register writes for multi-byte accesses.
         */
        if (esp_hosted_sdio_cmd53_rw(ESP_HOSTED_SDIO_FUNC_1, reg, true, false, true, (uint8_t *)data, len))
        {
            return true;
        }
    }

    for (i = 0; i < len; i++)
    {
        uint8_t value = data[i];
        if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_1, reg + i, true, false, &value))
        {
            return false;
        }
    }

    return true;
}

static bool esp_hosted_sdio_set_block_size(uint8_t function, uint16_t value)
{
    uint32_t offset = (uint32_t)function * 0x100U;
    uint8_t low = (uint8_t)(value & 0xFFU);
    uint8_t high = (uint8_t)(value >> 8);

    if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                  offset + ESP_HOSTED_SDIO_CCCR_BLKSIZEL,
                                  true,
                                  false,
                                  &low))
    {
        return false;
    }
    if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                  offset + ESP_HOSTED_SDIO_CCCR_BLKSIZEH,
                                  true,
                                  false,
                                  &high))
    {
        return false;
    }
    return true;
}

static bool esp_hosted_sdio_get_tx_buffer_num(uint32_t *tx_num)
{
    uint32_t token = 0;

    if (tx_num == NULL)
    {
        return false;
    }
    if (!esp_hosted_sdio_read_reg_bytes(ESP_HOSTED_SDIO_SLAVE_TOKEN_RDATA,
                                        (uint8_t *)&token,
                                        sizeof(token)))
    {
        return false;
    }

    token = (token >> 16) & ESP_HOSTED_SDIO_TX_BUF_MASK;
    token = (token + ESP_HOSTED_SDIO_TX_BUF_MAX - s_ctx.tx_buf_count) % ESP_HOSTED_SDIO_TX_BUF_MAX;
    *tx_num = token;
    return true;
}

static bool esp_hosted_sdio_get_len_from_slave(uint32_t *rx_size)
{
    uint32_t len = 0;
    uint32_t temp;

    if (rx_size == NULL)
    {
        return false;
    }
    if (!esp_hosted_sdio_read_reg_bytes(ESP_HOSTED_SDIO_SLAVE_PACKET_LEN_REG,
                                        (uint8_t *)&len,
                                        sizeof(len)))
    {
        return false;
    }

    len &= ESP_HOSTED_SDIO_SLAVE_LEN_MASK;
    if (len >= s_ctx.rx_byte_count)
    {
        len = (len + ESP_HOSTED_SDIO_RX_BYTE_MAX - s_ctx.rx_byte_count) % ESP_HOSTED_SDIO_RX_BYTE_MAX;
    }
    else
    {
        temp = ESP_HOSTED_SDIO_RX_BYTE_MAX - s_ctx.rx_byte_count;
        len = temp + len;
    }

    *rx_size = len;
    return true;
}

static bool esp_hosted_sdio_generate_slave_intr(uint8_t intr_no)
{
    uint8_t intr_mask = BIT(intr_no);
    return esp_hosted_sdio_write_reg_bytes(ESP_HOSTED_SDIO_HOST_TO_SLAVE_INTR, &intr_mask, sizeof(intr_mask));
}

static bool esp_hosted_sdio_card_init(esp_hosted_sdio_ctx_t *ctx)
{
    uint32_t resp = 0;
    uint8_t ioe = 0;
    uint8_t ior = 0;
    uint8_t ie = 0;
    uint8_t bus_width = 0;
    uint16_t i;

    if (!esp_hosted_sdio_send_cmd(ctx,
                                  ESP_HOSTED_SDIO_CMD5,
                                  0U,
                                  ESP_HOSTED_SDIO_RESP_SHORT,
                                  false,
                                  false,
                                  &resp))
    {
        ESP_HOSTED_SDIO_ERR("CMD5 probe failed\n");
        return false;
    }

    for (i = 0; i < ESP_HOSTED_SDIO_IO_READY_RETRIES; i++)
    {
        if (!esp_hosted_sdio_send_cmd(ctx,
                                      ESP_HOSTED_SDIO_CMD5,
                                      ESP_HOSTED_SDIO_CMD5_OCR_ARG,
                                      ESP_HOSTED_SDIO_RESP_SHORT,
                                      false,
                                      false,
                                      &resp))
        {
            return false;
        }
        if ((resp & ESP_HOSTED_SDIO_R4_READY) != 0U)
        {
            break;
        }
        delay_ms(1U);
    }
    if (i >= ESP_HOSTED_SDIO_IO_READY_RETRIES)
    {
        ESP_HOSTED_SDIO_ERR("CMD5 ready timeout\n");
        return false;
    }

    if (!esp_hosted_sdio_send_cmd(ctx,
                                  ESP_HOSTED_SDIO_CMD3,
                                  0U,
                                  ESP_HOSTED_SDIO_RESP_SHORT,
                                  true,
                                  false,
                                  &resp))
    {
        ESP_HOSTED_SDIO_ERR("CMD3 failed\n");
        return false;
    }
    ctx->rca = (uint16_t)(resp >> ESP_HOSTED_SDIO_RCA_SHIFT);
    if (ctx->rca == 0U)
    {
        ESP_HOSTED_SDIO_ERR("invalid RCA\n");
        return false;
    }

    if (!esp_hosted_sdio_send_cmd(ctx,
                                  ESP_HOSTED_SDIO_CMD7,
                                  ((uint32_t)ctx->rca) << ESP_HOSTED_SDIO_RCA_SHIFT,
                                  ESP_HOSTED_SDIO_RESP_SHORT,
                                  true,
                                  false,
                                  NULL))
    {
        ESP_HOSTED_SDIO_ERR("CMD7 failed\n");
        return false;
    }

    if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                  ESP_HOSTED_SDIO_CCCR_FN_ENABLE,
                                  false,
                                  false,
                                  &ioe))
    {
        return false;
    }
    ioe |= ESP_HOSTED_SDIO_CCCR_FN1_EN;
    if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                  ESP_HOSTED_SDIO_CCCR_FN_ENABLE,
                                  true,
                                  false,
                                  &ioe))
    {
        return false;
    }
    if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                  ESP_HOSTED_SDIO_CCCR_FN_ENABLE,
                                  false,
                                  false,
                                  &ioe))
    {
        return false;
    }
    ESP_HOSTED_SDIO_LOG("[esp_hosted_sdio] fn_enable=0x%02x\n", (unsigned)ioe);

    for (i = 0; i < ESP_HOSTED_SDIO_IO_READY_RETRIES; i++)
    {
        if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                      ESP_HOSTED_SDIO_CCCR_FN_READY,
                                      false,
                                      false,
                                      &ior))
        {
            delay_ms(10U);
            continue;
        }
        if ((ior & ESP_HOSTED_SDIO_CCCR_FN1_EN) != 0U)
        {
            ESP_HOSTED_SDIO_LOG("[esp_hosted_sdio] fn_ready=0x%02x\n", (unsigned)ior);
            break;
        }
        delay_ms(10U);
    }
    if (i >= ESP_HOSTED_SDIO_IO_READY_RETRIES)
    {
        ESP_HOSTED_SDIO_ERR("function 1 ready timeout\n");
        return false;
    }

    if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                  ESP_HOSTED_SDIO_CCCR_INT_ENABLE,
                                  false,
                                  false,
                                  &ie))
    {
        return false;
    }
    ie |= ESP_HOSTED_SDIO_CCCR_INT_MASTER_EN | ESP_HOSTED_SDIO_CCCR_FN1_EN;
    if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                  ESP_HOSTED_SDIO_CCCR_INT_ENABLE,
                                  true,
                                  false,
                                  &ie))
    {
        return false;
    }

    if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                  ESP_HOSTED_SDIO_CCCR_BUS_WIDTH,
                                  false,
                                  false,
                                  &bus_width))
    {
        return false;
    }
    if (ctx->config.bus_width >= 4U)
    {
        bus_width = (uint8_t)((bus_width & ~0x3U) | ESP_HOSTED_SDIO_CCCR_BUS_WIDTH_4);
        if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                      ESP_HOSTED_SDIO_CCCR_BUS_WIDTH,
                                      true,
                                      false,
                                      &bus_width))
        {
            return false;
        }
        sdmmc_ll_set_card_width(s_sdmmc, ctx->config.slot, SD_BUS_WIDTH_4_BIT);
    }
    else
    {
        bus_width = (uint8_t)(bus_width & ~0x3U);
        if (!esp_hosted_sdio_cmd52_rw(ESP_HOSTED_SDIO_FUNC_0,
                                      ESP_HOSTED_SDIO_CCCR_BUS_WIDTH,
                                      true,
                                      false,
                                      &bus_width))
        {
            return false;
        }
        sdmmc_ll_set_card_width(s_sdmmc, ctx->config.slot, SD_BUS_WIDTH_1_BIT);
    }

    if (!esp_hosted_sdio_set_block_size(ESP_HOSTED_SDIO_FUNC_0, ESP_HOSTED_SDIO_BLOCK_SIZE) ||
        !esp_hosted_sdio_set_block_size(ESP_HOSTED_SDIO_FUNC_1, ESP_HOSTED_SDIO_BLOCK_SIZE))
    {
        ESP_HOSTED_SDIO_ERR("block size config failed\n");
        return false;
    }

    if (!esp_hosted_sdio_set_clock(ctx->config.slot,
                                   esp_hosted_sdio_calc_host_div(ctx->config.clock_khz),
                                   0U))
    {
        ESP_HOSTED_SDIO_ERR("transfer clock setup failed\n");
        return false;
    }

    return true;
}

static bool esp_hosted_sdio_is_zero_padding(const uint8_t *data, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++)
    {
        if (data[i] != 0U)
        {
            return false;
        }
    }
    return true;
}

static void esp_hosted_sdio_submit_rx_frame(const esp_hosted_sdio_payload_header_t *header,
                                            const uint8_t *payload,
                                            uint16_t len)
{
    esp_hosted_frame_info_t info = {0};

    info.if_type = (esp_hosted_if_t)header->if_type;
    info.if_num = header->if_num;
    info.flags = header->flags;
    info.payload_len = len;
    info.payload_offset = header->offset;
    info.checksum = header->checksum;
    info.seq_num = header->seq_num;
    info.throttle_cmd = header->throttle_cmd;
    info.hci_pkt_type = header->hci_pkt_type;
    info.priv_pkt_type = header->priv_pkt_type;

    if (info.if_type == ESP_HOSTED_IF_SERIAL ||
        info.if_type == ESP_HOSTED_IF_HCI ||
        info.if_type == ESP_HOSTED_IF_PRIV)
    {
        ESP_HOSTED_SDIO_LOG("[esp_hosted_sdio] rx if=%u ifnum=%u len=%u flags=0x%02x seq=%u aux=0x%02x\n",
                            (unsigned)info.if_type,
                            (unsigned)info.if_num,
                            (unsigned)len,
                            (unsigned)info.flags,
                            (unsigned)info.seq_num,
                            (unsigned)((info.if_type == ESP_HOSTED_IF_HCI) ? info.hci_pkt_type : info.priv_pkt_type));
    }

    (void)esp_hosted_rx_submit(&info, payload, len);
}

static bool esp_hosted_sdio_drain_rx_bytes(uint32_t total_len)
{
    uint32_t addr = ESP_HOSTED_SDIO_SLAVE_CMD53_END_ADDR - total_len;

    while (total_len > 0U)
    {
        uint32_t chunk = total_len;
        if (chunk > sizeof(s_ctx.frame_buf))
        {
            chunk = sizeof(s_ctx.frame_buf);
        }
        if (!esp_hosted_sdio_read_bytes(ESP_HOSTED_SDIO_FUNC_1, addr, s_ctx.frame_buf, chunk))
        {
            return false;
        }
        addr += chunk;
        total_len -= chunk;
    }

    return true;
}

static void esp_hosted_sdio_parse_rx_frames(uint8_t *data, uint32_t len)
{
    uint32_t cursor = 0U;

    while (cursor + sizeof(esp_hosted_sdio_payload_header_t) <= len)
    {
        esp_hosted_sdio_payload_header_t *header = (esp_hosted_sdio_payload_header_t *)(data + cursor);
        uint32_t payload_offset = header->offset;
        uint32_t payload_len = header->len;
        uint32_t frame_len = payload_offset + payload_len;

        if (payload_len == 0U && esp_hosted_sdio_is_zero_padding(data + cursor, len - cursor))
        {
            return;
        }

        if (frame_len == 0U ||
            payload_offset != sizeof(esp_hosted_sdio_payload_header_t) ||
            cursor + frame_len > len ||
            payload_len > ESP_HOSTED_RX_PAYLOAD_MAX)
        {
            if ((header->flags & ESP_HOSTED_SDIO_MORE_FRAGMENT) != 0U)
            {
                ESP_HOSTED_SDIO_ERR("fragmented rx frame is not reassembled yet\n");
            }
            else
            {
                ESP_HOSTED_SDIO_ERR("invalid rx frame len=%u offset=%u total=%u\n",
                                    (unsigned)payload_len,
                                    (unsigned)payload_offset,
                                    (unsigned)len);
            }
            return;
        }

        esp_hosted_sdio_submit_rx_frame(header, data + cursor + payload_offset, (uint16_t)payload_len);
        cursor += frame_len;
    }
}

static size_t esp_hosted_sdio_transport_tx(const esp_hosted_frame_info_t *info,
                                           const void *payload,
                                           size_t len,
                                           void *ctx)
{
    esp_hosted_sdio_ctx_t *sdio = (esp_hosted_sdio_ctx_t *)ctx;
    esp_hosted_sdio_payload_header_t *header;
    uint32_t tx_buf_num = 0;
    uint32_t frame_len;
    uint32_t buf_needed;

    if (sdio == NULL || info == NULL || (!sdio->attached) || (payload == NULL && len != 0U))
    {
        return 0U;
    }
    if (len > ESP_HOSTED_RX_PAYLOAD_MAX)
    {
        return 0U;
    }
    if (sdio->tx_throttled &&
        (info->if_type == ESP_HOSTED_IF_STA || info->if_type == ESP_HOSTED_IF_AP || info->if_type == ESP_HOSTED_IF_ETH))
    {
        return 0U;
    }

    header = (esp_hosted_sdio_payload_header_t *)sdio->tx_frame;
    memset(header, 0, sizeof(*header));
    header->if_type = (uint8_t)info->if_type;
    header->if_num = info->if_num;
    header->flags = info->flags;
    header->len = (uint16_t)len;
    header->offset = (uint16_t)sizeof(*header);
    header->checksum = 0U;
    header->seq_num = info->seq_num;
    header->throttle_cmd = info->throttle_cmd;
    if (info->if_type == ESP_HOSTED_IF_HCI)
    {
        header->hci_pkt_type = info->hci_pkt_type;
    }

    if (len != 0U)
    {
        memcpy(sdio->tx_frame + sizeof(*header), payload, len);
    }
    frame_len = (uint32_t)(sizeof(*header) + len);
    if (frame_len > sizeof(sdio->tx_frame))
    {
        ESP_HOSTED_SDIO_ERR("tx frame too large len=%u max=%u\n",
                            (unsigned)frame_len,
                            (unsigned)sizeof(sdio->tx_frame));
        return 0U;
    }

    header->checksum = esp_hosted_sdio_compute_checksum(sdio->tx_frame, frame_len);

    if (info->if_type == ESP_HOSTED_IF_SERIAL ||
        info->if_type == ESP_HOSTED_IF_HCI ||
        info->if_type == ESP_HOSTED_IF_PRIV)
    {
        ESP_HOSTED_SDIO_LOG("[esp_hosted_sdio] tx if=%u ifnum=%u len=%u flags=0x%02x seq=%u aux=0x%02x\n",
                            (unsigned)info->if_type,
                            (unsigned)info->if_num,
                            (unsigned)len,
                            (unsigned)info->flags,
                            (unsigned)info->seq_num,
                            (unsigned)((info->if_type == ESP_HOSTED_IF_HCI) ? header->hci_pkt_type : header->priv_pkt_type));
    }

    buf_needed = (frame_len + ESP_HOSTED_SDIO_RX_BUF_WINDOW - 1U) / ESP_HOSTED_SDIO_RX_BUF_WINDOW;
    if (!esp_hosted_sdio_get_tx_buffer_num(&tx_buf_num) || tx_buf_num < buf_needed)
    {
        return 0U;
    }

    if (!esp_hosted_sdio_write_padded_blocks(ESP_HOSTED_SDIO_FUNC_1,
                                             ESP_HOSTED_SDIO_SLAVE_CMD53_END_ADDR - frame_len,
                                             sdio->tx_frame,
                                             frame_len))
    {
        ESP_HOSTED_SDIO_ERR("tx write failed len=%u\n", (unsigned)frame_len);
        return 0U;
    }

    sdio->tx_buf_count = (sdio->tx_buf_count + buf_needed) % ESP_HOSTED_SDIO_TX_BUF_MAX;
    return len;
}

static void esp_hosted_sdio_transport_poll(void *ctx)
{
    esp_hosted_sdio_ctx_t *sdio = (esp_hosted_sdio_ctx_t *)ctx;
    uint32_t interrupts = 0;
    uint32_t len_from_slave = 0;
    uint32_t poll_budget = 4U;

    if (sdio == NULL || !sdio->attached)
    {
        return;
    }

    while (poll_budget-- > 0U)
    {
        if (!esp_hosted_sdio_read_reg_bytes(ESP_HOSTED_SDIO_SLAVE_INT_RAW_REG,
                                            (uint8_t *)&interrupts,
                                            sizeof(interrupts)))
        {
            return;
        }
        if (interrupts == 0U)
        {
            return;
        }

        (void)esp_hosted_sdio_write_reg_bytes(ESP_HOSTED_SDIO_SLAVE_INT_CLR_REG,
                                              (const uint8_t *)&interrupts,
                                              sizeof(interrupts));

        if ((interrupts & ESP_HOSTED_SDIO_INT_START_THROTTLE) != 0U)
        {
            sdio->tx_throttled = true;
        }
        if ((interrupts & ESP_HOSTED_SDIO_INT_STOP_THROTTLE) != 0U)
        {
            sdio->tx_throttled = false;
        }
        if ((interrupts & ESP_HOSTED_SDIO_INT_NEW_PACKET) == 0U)
        {
            continue;
        }

        if (!esp_hosted_sdio_get_len_from_slave(&len_from_slave) || len_from_slave == 0U)
        {
            return;
        }

        if (len_from_slave > sizeof(sdio->frame_buf))
        {
            ESP_HOSTED_SDIO_ERR("rx frame too large len=%u, dropping\n", (unsigned)len_from_slave);
            if (!esp_hosted_sdio_drain_rx_bytes(len_from_slave))
            {
                return;
            }
            sdio->rx_byte_count = (sdio->rx_byte_count + len_from_slave) % ESP_HOSTED_SDIO_RX_BYTE_MAX;
            continue;
        }

        if (!esp_hosted_sdio_read_bytes(ESP_HOSTED_SDIO_FUNC_1,
                                        ESP_HOSTED_SDIO_SLAVE_CMD53_END_ADDR - len_from_slave,
                                        sdio->frame_buf,
                                        len_from_slave))
        {
            ESP_HOSTED_SDIO_ERR("rx read failed len=%u\n", (unsigned)len_from_slave);
            return;
        }

        sdio->rx_byte_count = (sdio->rx_byte_count + len_from_slave) % ESP_HOSTED_SDIO_RX_BYTE_MAX;
        esp_hosted_sdio_parse_rx_frames(sdio->frame_buf, len_from_slave);
    }
}

void esp_hosted_sdio_get_default_config(esp_hosted_sdio_config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->slot = ESP_HOSTED_SDIO_SLOT1;
    config->bus_width = 4U;
    config->clock_khz = 25000U;
    config->pin_clk = 18;
    config->pin_cmd = 19;
    config->pin_d0 = 14;
    config->pin_d1 = 15;
    config->pin_d2 = 16;
    config->pin_d3 = 17;
    config->pin_reset = 54;
    config->reset_active_low = true;
    config->reset_pulse_ms = 10U;
    config->post_reset_delay_ms = 1500U;
    config->use_internal_pullups = false;
}

bool esp_hosted_sdio_attach(const esp_hosted_sdio_config_t *config)
{
    esp_hosted_sdio_config_t cfg;
    uint32_t attempt;

    if (config == NULL)
    {
        esp_hosted_sdio_get_default_config(&cfg);
        config = &cfg;
    }

    if (s_ctx.attached)
    {
        esp_hosted_sdio_detach();
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_cmd52_int_raw_fail_count = 0U;
    s_ctx.config = *config;

    if (s_ctx.config.bus_width != 1U && s_ctx.config.bus_width != 4U)
    {
        ESP_HOSTED_SDIO_ERR("only 1-bit and 4-bit SDIO are supported\n");
        return false;
    }

    for (attempt = 0; attempt < ESP_HOSTED_SDIO_ATTACH_RETRIES; attempt++)
    {
        ESP_HOSTED_SDIO_LOG("[esp_hosted_sdio] attach attempt=%lu slot=%u width=%u clk=%lu kHz pullups=%u\n",
                            (unsigned long)(attempt + 1U),
                            (unsigned)s_ctx.config.slot,
                            (unsigned)s_ctx.config.bus_width,
                            (unsigned long)s_ctx.config.clock_khz,
                            (unsigned)s_ctx.config.use_internal_pullups);

        if (!esp_hosted_sdio_hw_init(&s_ctx))
        {
            ESP_HOSTED_SDIO_ERR("host init failed attempt=%lu\n", (unsigned long)(attempt + 1U));
            esp_hosted_sdio_hw_deinit(&s_ctx);
            delay_ms(20U);
            continue;
        }
        if (!esp_hosted_sdio_card_init(&s_ctx))
        {
            ESP_HOSTED_SDIO_ERR("card init failed attempt=%lu\n", (unsigned long)(attempt + 1U));
            esp_hosted_sdio_hw_deinit(&s_ctx);
            delay_ms(20U);
            continue;
        }
        break;
    }
    if (attempt >= ESP_HOSTED_SDIO_ATTACH_RETRIES)
    {
        return false;
    }

    if (esp_hosted_is_initialized())
    {
        if (!esp_hosted_set_transport(&s_transport))
        {
            ESP_HOSTED_SDIO_ERR("failed to set transport\n");
            esp_hosted_sdio_hw_deinit(&s_ctx);
            return false;
        }
    }
    else if (!esp_hosted_init(&s_transport))
    {
        ESP_HOSTED_SDIO_ERR("failed to initialize hosted transport\n");
        esp_hosted_sdio_hw_deinit(&s_ctx);
        return false;
    }

    s_ctx.attached = true;

    if (!esp_hosted_sdio_generate_slave_intr(ESP_HOSTED_SDIO_HOST_INTR_OPEN_DATA))
    {
        ESP_HOSTED_SDIO_ERR("failed to open slave data path\n");
        esp_hosted_sdio_detach();
        return false;
    }

    ESP_HOSTED_SDIO_LOG("[esp_hosted_sdio] attached slot=%u width=%u clk=%u kHz\n",
                        (unsigned)s_ctx.config.slot,
                        (unsigned)s_ctx.config.bus_width,
                        (unsigned)s_ctx.config.clock_khz);
    return true;
}

void esp_hosted_sdio_detach(void)
{
    if (esp_hosted_is_initialized())
    {
        (void)esp_hosted_set_transport(NULL);
    }

    if (s_ctx.attached)
    {
        esp_hosted_sdio_hw_deinit(&s_ctx);
    }
    memset(&s_ctx, 0, sizeof(s_ctx));
}

bool esp_hosted_sdio_is_attached(void)
{
    return s_ctx.attached;
}

bool esp_hosted_sdio_read_reg(uint32_t reg, void *data, size_t len)
{
    if (!s_ctx.attached || (data == NULL && len != 0U) || len > UINT32_MAX)
    {
        return false;
    }
    return esp_hosted_sdio_read_reg_bytes(reg, (uint8_t *)data, (uint32_t)len);
}

bool esp_hosted_sdio_write_reg(uint32_t reg, const void *data, size_t len)
{
    if (!s_ctx.attached || (data == NULL && len != 0U) || len > UINT32_MAX)
    {
        return false;
    }
    return esp_hosted_sdio_write_reg_bytes(reg, (const uint8_t *)data, (uint32_t)len);
}
