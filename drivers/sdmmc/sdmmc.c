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
#error "drivers/sdmmc/sdmmc.c currently supports ESP32-P4 only"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sdmmc.h"
#include "delay.h"
#include "usb_serial.h"
#include "hal/sd_types.h"
#include "hal/sdmmc_ll.h"
#include "hal/cache_ll.h"
#include "hal/gpio_ll.h"
#include "hal/ldo_ll.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"
#include "soc/sdmmc_pins.h"

#define SDMMC_BUS_WIDTH 4
#define SDMMC_SLOT0 0U
#define SDMMC_CMD_TIMEOUT_LOOPS 200000U
#define SDMMC_DATA_TIMEOUT_LOOPS 1200000U
#define SDMMC_BUSY_TIMEOUT_LOOPS 600000U
#define SDMMC_ACMD41_MAX_TRIES 1000U
#define SDMMC_HOST_DIV_PROBING 10U
#define SDMMC_CARD_DIV_PROBING 20U
#define SDMMC_HOST_DIV_TRANSFER 8U
#define SDMMC_CARD_DIV_TRANSFER 0U
#define SDMMC_CMD_GO_IDLE_STATE 0U
#define SDMMC_CMD_ALL_SEND_CID 2U
#define SDMMC_CMD_SEND_RELATIVE_ADDR 3U
#define SDMMC_CMD_SELECT_CARD 7U
#define SDMMC_CMD_SEND_IF_COND 8U
#define SDMMC_CMD_READ_CSD_REGISTER 9U
#define SDMMC_CMD_APP_CMD_PREFIX 55U
#define SDMMC_CMD_READ_SINGLE_BLOCK 17U
#define SDMMC_CMD_WRITE_SINGLE_BLOCK 24U
#define SDMMC_ACMD_SET_BUS_WIDTH 6U
#define SDMMC_ACMD_SD_SEND_OP_COND 41U

#ifndef SDMMC_USE_INTERNAL_LDO
#define SDMMC_USE_INTERNAL_LDO 1
#endif

#ifndef SDMMC_LDO_UNIT
/* Board SD1_VDD source is ESP_LDO_VO4 (VO4 -> unit index 3). */
#define SDMMC_LDO_UNIT 3
#endif

#ifndef SDMMC_LDO_VOLTAGE_MV
#define SDMMC_LDO_VOLTAGE_MV 3300
#endif

#ifndef SDMMC_LDO_STABLE_DELAY_MS
#define SDMMC_LDO_STABLE_DELAY_MS 2U
#endif

#ifndef SDMMC_USE_BOARD_POWER_GPIO
/* SD1_VDD is switched by AO3401 gate driven from GPIO45 on this board. */
#define SDMMC_USE_BOARD_POWER_GPIO 1
#endif

#ifndef SDMMC_POWER_GPIO
#define SDMMC_POWER_GPIO 45U
#endif

#ifndef SDMMC_POWER_ACTIVE_HIGH
/* AO3401 is PMOS high-side: gate low = ON, gate high = OFF. */
#define SDMMC_POWER_ACTIVE_HIGH 0
#endif

#ifndef SDMMC_POWER_STABLE_DELAY_MS
#define SDMMC_POWER_STABLE_DELAY_MS 5U
#endif

#define SDMMC_CMD_ERR_MASK (SDMMC_LL_EVENT_RESP_ERR | SDMMC_LL_EVENT_RTO | SDMMC_LL_EVENT_RCRC | SDMMC_LL_EVENT_HLE | SDMMC_LL_EVENT_SBE | SDMMC_LL_EVENT_EBE)
#define SDMMC_DATA_ERR_MASK (SDMMC_LL_EVENT_DCRC | SDMMC_LL_EVENT_FRUN | SDMMC_LL_EVENT_HTO | SDMMC_LL_EVENT_EBE | SDMMC_LL_EVENT_SBE)
#define SDMMC_IDSTS_DMA_FSM_SHIFT 13U
#define SDMMC_IDSTS_DMA_FSM_MASK 0xFU
/* IDMAC FSM state values from IDSTS[16:13] */
#define SDMMC_DMA_FSM_SUSPEND 1U
#define SDMMC_DMA_FSM_READ_REQ_WAIT 4U
#define SDMMC_DMA_FSM_WRITE_REQ_WAIT 5U

#ifndef SDMMC_DEBUG
#define SDMMC_DEBUG 0
#endif

#if SDMMC_DEBUG
#define SDMMC_LOG(...) printf(__VA_ARGS__)
#else
#define SDMMC_LOG(...) ((void)0)
#endif

typedef enum
{
    SDMMC_RESP_NONE = 0,
    SDMMC_RESP_SHORT = 1,
    SDMMC_RESP_LONG = 2,
} sdmmc_resp_t;

static sdmmc_dev_t *const s_sdmmc = &SDMMC;
static bool s_initialized = false;
static bool s_high_capacity = false;
static uint16_t s_card_rca = 0;
static sdmmc_desc_t s_sdmmc_dma_desc __attribute__((aligned(64)));
static uint8_t s_sdmmc_dma_bounce[SDMMC_SECTOR_SIZE] __attribute__((aligned(64)));

static inline uint32_t sdmmc_get_events(void)
{
    /* Some revisions expose completion in one of these paths only. */
    return sdmmc_ll_get_interrupt_raw(s_sdmmc); // | sdmmc_ll_get_intr_status(s_sdmmc);
}

static sdmmc_err_t sdmmc_wait_cmd_taken(uint32_t timeout_loops)
{
    while (timeout_loops-- > 0U)
    {
        if (s_sdmmc->cmd.start_command == 0U)
        {
            return SDMMC_OK;
        }
    }
    SDMMC_LOG("[sdmmc] cmd start timeout\n");
    return SDMMC_ERR_TIMEOUT;
}

static void sdmmc_internal_ldo_power_on(void)
{
#if SDMMC_USE_INTERNAL_LDO
    uint8_t dref = 0;
    uint8_t mul = 0;
    bool use_rail = false;

    ldo_ll_set_owner(SDMMC_LDO_UNIT, LDO_LL_UNIT_OWNER_SW);
    ldo_ll_voltage_to_dref_mul(SDMMC_LDO_UNIT, SDMMC_LDO_VOLTAGE_MV, &dref, &mul, &use_rail);
    ldo_ll_adjust_voltage(SDMMC_LDO_UNIT, dref, mul, use_rail);
    ldo_ll_enable_ripple_suppression(SDMMC_LDO_UNIT, true);
    ldo_ll_enable(SDMMC_LDO_UNIT, true);
    delay_ms(SDMMC_LDO_STABLE_DELAY_MS);

    SDMMC_LOG("[sdmmc] LDO unit=%d voltage=%dmV enabled\n", SDMMC_LDO_UNIT, SDMMC_LDO_VOLTAGE_MV);
#endif
}

static void sdmmc_board_power_on(void)
{
    sdmmc_internal_ldo_power_on();

#if SDMMC_USE_BOARD_POWER_GPIO
    gpio_ll_func_sel(&GPIO, SDMMC_POWER_GPIO, PIN_FUNC_GPIO);
    gpio_ll_input_disable(&GPIO, SDMMC_POWER_GPIO);
    gpio_ll_output_enable(&GPIO, SDMMC_POWER_GPIO);
    gpio_ll_set_level(&GPIO, SDMMC_POWER_GPIO, SDMMC_POWER_ACTIVE_HIGH ? 1 : 0);
    delay_ms(SDMMC_POWER_STABLE_DELAY_MS);
    SDMMC_LOG("[sdmmc] board power enabled on GPIO%u\n", (unsigned)SDMMC_POWER_GPIO);
#endif
}

static inline void sdmmc_setup_iomux_pin(uint32_t gpio_num, bool use_pullup)
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

bool sdmmc_gpio_setup_iomux_slot0(uint8_t bus_width)
{
    if ((bus_width != 1U) && (bus_width != 4U) && (bus_width != 8U))
    {
        return false;
    }

    // SD clock line should not be pulled up.
    sdmmc_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_CLK, false);

    // Board has external pull-ups to SD1_VDD; internal pull-ups remain configurable.
    const bool use_internal_pullups = (SDMMC_ENABLE_INTERNAL_PULLUPS != 0);
    sdmmc_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_CMD, use_internal_pullups);
    sdmmc_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_D0, use_internal_pullups);

    if (bus_width >= 4U)
    {
        sdmmc_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_D1, use_internal_pullups);
        sdmmc_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_D2, use_internal_pullups);
        sdmmc_setup_iomux_pin(SDMMC_SLOT0_IOMUX_PIN_NUM_D3, use_internal_pullups);
    }

    return true;
}

void sdmmc_gpio_config(void)
{
    (void)sdmmc_gpio_setup_iomux_slot0(SDMMC_BUS_WIDTH);
}

static sdmmc_err_t sdmmc_send_cmd(uint8_t cmd_idx, uint32_t arg, sdmmc_resp_t resp_type, bool check_crc, bool send_init, uint32_t *resp0)
{
    sdmmc_hw_cmd_t cmd = {0};
    uint32_t raw = 0;

    /* Match IDF behavior: wait until command FSM is ready before touching cmd regs. */
    sdmmc_err_t rc = sdmmc_wait_cmd_taken(SDMMC_CMD_TIMEOUT_LOOPS);
    if (rc != SDMMC_OK)
    {
        SDMMC_LOG("[sdmmc] CMD%u pre-wait timeout\n", cmd_idx);
        return rc;
    }

    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_set_command_arg(s_sdmmc, arg);

    cmd.cmd_index = cmd_idx;
    cmd.card_num = SDMMC_SLOT0;
    cmd.wait_complete = 1;
    cmd.send_init = send_init ? 1U : 0U;
    cmd.response_expect = (resp_type == SDMMC_RESP_NONE) ? 0U : 1U;
    cmd.response_long = (resp_type == SDMMC_RESP_LONG) ? 1U : 0U;
    cmd.check_response_crc = (check_crc && (resp_type != SDMMC_RESP_NONE)) ? 1U : 0U;
    cmd.use_hold_reg = 1;
    cmd.start_command = 1;
    sdmmc_ll_set_command(s_sdmmc, cmd);

    rc = sdmmc_wait_cmd_taken(SDMMC_CMD_TIMEOUT_LOOPS);
    if (rc != SDMMC_OK)
    {
        SDMMC_LOG("[sdmmc] CMD%u failed arg=0x%08x rc=%d\n", cmd_idx, (unsigned)arg, (int)rc);
        return rc;
    }

    {
        uint32_t timeout_loops = SDMMC_CMD_TIMEOUT_LOOPS;
        while (timeout_loops-- > 0U)
        {
            raw = sdmmc_get_events();
            if ((raw & SDMMC_CMD_ERR_MASK) != 0U)
            {
                sdmmc_ll_clear_interrupt(s_sdmmc, raw);
                SDMMC_LOG("[sdmmc] CMD%u err raw=0x%08x\n", cmd_idx, (unsigned)raw);
                return SDMMC_ERR_PROTOCOL;
            }
            if ((raw & SDMMC_LL_EVENT_CMD_DONE) != 0U)
            {
                sdmmc_ll_clear_interrupt(s_sdmmc, raw);
                break;
            }
        }
        if ((raw & SDMMC_LL_EVENT_CMD_DONE) == 0U)
        {
            SDMMC_LOG("[sdmmc] CMD%u done timeout status=0x%08x rint=0x%08x mint=0x%08x\n",
                      (unsigned)cmd_idx,
                      (unsigned)s_sdmmc->status.val,
                      (unsigned)sdmmc_ll_get_interrupt_raw(s_sdmmc),
                      (unsigned)sdmmc_ll_get_intr_status(s_sdmmc));
            return SDMMC_ERR_TIMEOUT;
        }
    }

    if (resp0 != NULL && resp_type != SDMMC_RESP_NONE)
    {
        *resp0 = s_sdmmc->resp[0];
    }

    return SDMMC_OK;
}

static sdmmc_err_t sdmmc_send_acmd(uint8_t acmd_idx, uint32_t acmd_arg, uint32_t *resp0)
{
    uint32_t rca_arg = (s_card_rca != 0U) ? (((uint32_t)s_card_rca) << 16) : 0U;
    sdmmc_err_t rc = sdmmc_send_cmd(SDMMC_CMD_APP_CMD_PREFIX, rca_arg, SDMMC_RESP_SHORT, true, false, NULL);
    if (rc != SDMMC_OK)
    {
        return rc;
    }
    /*
     * ACMD41 returns R3 (OCR), which has no valid CRC field.
     * Other ACMDs used here (e.g. ACMD6) use normal short responses with CRC.
     */
    const bool check_crc = (acmd_idx != SDMMC_ACMD_SD_SEND_OP_COND);
    return sdmmc_send_cmd(acmd_idx, acmd_arg, SDMMC_RESP_SHORT, check_crc, false, resp0);
}

static sdmmc_err_t sdmmc_update_clock_registers(void)
{
    sdmmc_hw_cmd_t cmd = {0};

    sdmmc_err_t rc = sdmmc_wait_cmd_taken(SDMMC_CMD_TIMEOUT_LOOPS);
    if (rc != SDMMC_OK)
    {
        SDMMC_LOG("[sdmmc] update clock pre-wait timeout\n");
        return rc;
    }

    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_set_command_arg(s_sdmmc, 0);
    cmd.card_num = SDMMC_SLOT0;
    cmd.update_clk_reg = 1;
    cmd.wait_complete = 1;
    cmd.use_hold_reg = 1;
    cmd.start_command = 1;
    sdmmc_ll_set_command(s_sdmmc, cmd);

    rc = sdmmc_wait_cmd_taken(SDMMC_CMD_TIMEOUT_LOOPS);
    if (rc != SDMMC_OK)
    {
        SDMMC_LOG("[sdmmc] update clock failed rc=%d\n", (int)rc);
        return rc;
    }
    uint32_t raw = sdmmc_get_events();
    if ((raw & SDMMC_CMD_ERR_MASK) != 0U)
    {
        sdmmc_ll_clear_interrupt(s_sdmmc, raw);
        SDMMC_LOG("[sdmmc] update clock err raw=0x%08x\n", (unsigned)raw);
        return SDMMC_ERR_PROTOCOL;
    }
    sdmmc_ll_clear_interrupt(s_sdmmc, raw);
    return rc;
}

static sdmmc_err_t sdmmc_set_slot0_clock(uint32_t host_div, uint8_t card_div)
{
    sdmmc_ll_enable_card_clock(s_sdmmc, SDMMC_SLOT0, false);
    sdmmc_err_t rc = sdmmc_update_clock_registers();
    if (rc != SDMMC_OK)
    {
        return rc;
    }

    sdmmc_ll_set_card_clock_div(s_sdmmc, SDMMC_SLOT0, card_div);
    sdmmc_ll_set_clock_div(s_sdmmc, host_div);
    sdmmc_ll_init_phase_delay(s_sdmmc);
    rc = sdmmc_update_clock_registers();
    if (rc != SDMMC_OK)
    {
        return rc;
    }

    sdmmc_ll_enable_card_clock(s_sdmmc, SDMMC_SLOT0, true);
    rc = sdmmc_update_clock_registers();
    if (rc != SDMMC_OK)
    {
        return rc;
    }

    sdmmc_ll_enable_card_clock_low_power(s_sdmmc, SDMMC_SLOT0, true);
    SDMMC_LOG("[sdmmc] clk host_div=%u card_div=%u\n", (unsigned)host_div, (unsigned)card_div);
    return SDMMC_OK;
}

static sdmmc_err_t sdmmc_wait_data_idle(void)
{
    uint32_t timeout_loops = SDMMC_BUSY_TIMEOUT_LOOPS;
    while (timeout_loops-- > 0U)
    {
        if (!sdmmc_ll_is_card_data_busy(s_sdmmc))
        {
            return SDMMC_OK;
        }
        delay_us(1);
    }
    return SDMMC_ERR_TIMEOUT;
}

static sdmmc_err_t sdmmc_transfer_sector(uint8_t cmd_idx, uint32_t lba, uint8_t *buffer, bool is_write)
{
    sdmmc_hw_cmd_t cmd = {0};
    uint32_t raw = 0;
    uint32_t dma_raw = 0;
    uint32_t arg = s_high_capacity ? lba : lba * SDMMC_SECTOR_SIZE;
    uint8_t *dma_buf = s_sdmmc_dma_bounce;                                       /* Address programmed to IDMAC */
    uint8_t *dma_buf_nc = (uint8_t *)CACHE_LL_L2MEM_NON_CACHE_ADDR(dma_buf);     /* CPU non-cache alias */
    sdmmc_desc_t *desc = &s_sdmmc_dma_desc;                                      /* Address programmed to IDMAC */
    sdmmc_desc_t *desc_nc = (sdmmc_desc_t *)CACHE_LL_L2MEM_NON_CACHE_ADDR(desc); /* CPU non-cache alias */

    if (is_write)
    {
        memcpy(dma_buf_nc, buffer, SDMMC_SECTOR_SIZE);
    }

    sdmmc_err_t rc = sdmmc_wait_data_idle();
    if (rc != SDMMC_OK)
    {
        return rc;
    }

    rc = sdmmc_wait_cmd_taken(SDMMC_CMD_TIMEOUT_LOOPS);
    if (rc != SDMMC_OK)
    {
        SDMMC_LOG("[sdmmc] CMD%u pre-wait timeout\n", cmd_idx);
        return rc;
    }

    sdmmc_ll_reset_fifo(s_sdmmc);
    sdmmc_ll_reset_dma(s_sdmmc);
    {
        uint32_t wait_loops = SDMMC_CMD_TIMEOUT_LOOPS;
        while (wait_loops-- > 0U)
        {
            if (sdmmc_ll_is_fifo_reset_done(s_sdmmc) && sdmmc_ll_is_dma_reset_done(s_sdmmc))
            {
                break;
            }
        }
        if (wait_loops == 0U)
        {
            SDMMC_LOG("[sdmmc] fifo/dma reset timeout\n");
            return SDMMC_ERR_TIMEOUT;
        }
    }

    sdmmc_ll_init_dma(s_sdmmc);
    sdmmc_ll_enable_dma(s_sdmmc, true);
    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_clear_idsts_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_set_block_size(s_sdmmc, SDMMC_SECTOR_SIZE);
    sdmmc_ll_set_data_transfer_len(s_sdmmc, SDMMC_SECTOR_SIZE);
    sdmmc_ll_set_command_arg(s_sdmmc, arg);

    memset(desc_nc, 0, sizeof(*desc_nc));
    desc_nc->first_descriptor = 1;
    desc_nc->last_descriptor = 1;
    desc_nc->second_address_chained = 1;
    desc_nc->end_of_ring = 1;
    desc_nc->owned_by_idmac = 1;
    desc_nc->buffer1_size = SDMMC_SECTOR_SIZE;
    desc_nc->buffer1_ptr = dma_buf;
    desc_nc->next_desc_ptr = NULL;
    sdmmc_ll_set_desc_addr(s_sdmmc, (uint32_t)(uintptr_t)desc);
    sdmmc_ll_poll_demand(s_sdmmc);

    cmd.cmd_index = cmd_idx;
    cmd.card_num = SDMMC_SLOT0;
    cmd.response_expect = 1;
    cmd.check_response_crc = 1;
    cmd.data_expected = 1;
    cmd.rw = is_write ? 1 : 0;
    cmd.wait_complete = 1;
    cmd.use_hold_reg = 1;
    cmd.transfer_mode = 0;
    cmd.start_command = 1;
    sdmmc_ll_set_command(s_sdmmc, cmd);

    if (sdmmc_wait_cmd_taken(SDMMC_CMD_TIMEOUT_LOOPS) != SDMMC_OK)
    {
        SDMMC_LOG("[sdmmc] CMD%u not taken\n", cmd_idx);
        return SDMMC_ERR_TIMEOUT;
    }

    bool data_done = false;
    bool dma_done = false;
    const uint32_t dma_done_mask = SDMMC_LL_EVENT_DMA_TI | SDMMC_LL_EVENT_DMA_RI | SDMMC_LL_EVENT_DMA_NI;
    const uint32_t dma_err_mask = BIT(2) | BIT(4) | BIT(5) | BIT(9);
    uint32_t timeout_loops = SDMMC_DATA_TIMEOUT_LOOPS;
    while (timeout_loops-- > 0U)
    {
        raw = sdmmc_get_events();
        dma_raw = sdmmc_ll_get_idsts_interrupt_raw(s_sdmmc);

        if ((raw & SDMMC_CMD_ERR_MASK) != 0U)
        {
            sdmmc_ll_clear_interrupt(s_sdmmc, raw);
            SDMMC_LOG("[sdmmc] cmd err CMD%u raw=0x%08x\n", cmd_idx, (unsigned)raw);
            return SDMMC_ERR_PROTOCOL;
        }
        if ((raw & SDMMC_DATA_ERR_MASK) != 0U)
        {
            sdmmc_ll_clear_interrupt(s_sdmmc, raw);
            SDMMC_LOG("[sdmmc] data err CMD%u raw=0x%08x\n", cmd_idx, (unsigned)raw);
            return SDMMC_ERR_IO;
        }
        if ((dma_raw & dma_err_mask) != 0U)
        {
            sdmmc_ll_clear_idsts_interrupt(s_sdmmc, dma_raw);
            SDMMC_LOG("[sdmmc] dma err CMD%u idsts=0x%08x dsc=0x%08x buf=0x%08x\n",
                      cmd_idx,
                      (unsigned)dma_raw,
                      (unsigned)s_sdmmc->dscaddr.dscaddr_reg,
                      (unsigned)s_sdmmc->bufaddr.bufaddr_reg);
            return SDMMC_ERR_IO;
        }
        if ((dma_raw & dma_done_mask) != 0U)
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

        /* IDSTS[16:13] = IDMAC FSM; these states may need a software resume kick in polling mode. */
        const uint32_t dma_fsm = (dma_raw >> SDMMC_IDSTS_DMA_FSM_SHIFT) & SDMMC_IDSTS_DMA_FSM_MASK;
        if (dma_fsm == SDMMC_DMA_FSM_SUSPEND ||
            dma_fsm == SDMMC_DMA_FSM_READ_REQ_WAIT ||
            dma_fsm == SDMMC_DMA_FSM_WRITE_REQ_WAIT)
        {
            /* Poll demand asks IDMAC to continue descriptor processing without interrupts. */
            sdmmc_ll_poll_demand(s_sdmmc);
        }

        if (data_done && dma_done)
        {
            if (!is_write)
            {
                memcpy(buffer, dma_buf_nc, SDMMC_SECTOR_SIZE);
            }
            SDMMC_LOG("[sdmmc] dma done CMD%u\n", cmd_idx);
            return sdmmc_wait_data_idle();
        }
    }
    SDMMC_LOG("[sdmmc] dma timeout CMD%u raw=0x%08x idsts=0x%08x status=0x%08x dsc=0x%08x buf=0x%08x tcb=%u tbb=%u\n",
              cmd_idx,
              (unsigned)raw,
              (unsigned)dma_raw,
              (unsigned)s_sdmmc->status.val,
              (unsigned)s_sdmmc->dscaddr.dscaddr_reg,
              (unsigned)s_sdmmc->bufaddr.bufaddr_reg,
              (unsigned)s_sdmmc->tcbcnt.tcbcnt_reg,
              (unsigned)s_sdmmc->tbbcnt.tbbcnt_reg);
    return SDMMC_ERR_TIMEOUT;
}

sdmmc_err_t sdmmc_read_sector(uint32_t lba, uint8_t *out_data)
{
    if (!s_initialized)
    {
        return SDMMC_ERR_NOT_READY;
    }
    if (out_data == NULL)
    {
        return SDMMC_ERR_INVALID_ARG;
    }

    return sdmmc_transfer_sector(SDMMC_CMD_READ_SINGLE_BLOCK, lba, out_data, false);
}

sdmmc_err_t sdmmc_write_sector(uint32_t lba, const uint8_t *data)
{
    if (!s_initialized)
    {
        return SDMMC_ERR_NOT_READY;
    }
    if (data == NULL)
    {
        return SDMMC_ERR_INVALID_ARG;
    }

    return sdmmc_transfer_sector(SDMMC_CMD_WRITE_SINGLE_BLOCK, lba, (uint8_t *)data, true);
}

sdmmc_err_t sdmmc_read_card_capacity(uint64_t *out_capacity_bytes)
{
    if (out_capacity_bytes == NULL)
    {
        return SDMMC_ERR_INVALID_ARG;
    }

    // CMD9: SEND_CSD, argument is RCA<<16
    sdmmc_err_t rc = sdmmc_send_cmd(
        SDMMC_CMD_READ_CSD_REGISTER,
        ((uint32_t)s_card_rca) << 16,
        SDMMC_RESP_LONG,
        true,
        false,
        NULL);
    if (rc != SDMMC_OK)
    {
        return rc;
    }

    uint32_t csd[4] = {
        s_sdmmc->resp[0],
        s_sdmmc->resp[1],
        s_sdmmc->resp[2],
        s_sdmmc->resp[3],
    };
    uint32_t csd_structure = (csd[3] >> 30) & 0x3U;
    if (csd_structure == 1U)
    {
        uint32_t c_size = ((csd[2] & 0x3FU) << 16) | ((csd[1] >> 16) & 0xFFFFU); // bits 69:48
        *out_capacity_bytes = ((uint64_t)c_size + 1ULL) * 512ULL * 1024ULL;
        return SDMMC_OK;
    }
    if (csd_structure == 0U)
    {
        uint32_t read_bl_len = (csd[2] >> 16) & 0xFU;                         // bits 83:80
        uint32_t c_size = ((csd[2] & 0x3FFU) << 2) | ((csd[1] >> 30) & 0x3U); // bits 73:62
        uint32_t c_size_mult = (csd[1] >> 15) & 0x7U;                         // bits 49:47
        *out_capacity_bytes = ((uint64_t)c_size + 1ULL) * (1ULL << (c_size_mult + 2U)) * (1ULL << read_bl_len);
        return SDMMC_OK;
    }
    else
    {
        return SDMMC_ERR_PROTOCOL;
    }
}

sdmmc_err_t sdmmc_init(void)
{
    s_initialized = false;
    s_high_capacity = false;
    s_card_rca = 0;

    sdmmc_board_power_on();

    /*
     * Board mapping (slot0 IOMUX):
     * CLK=GPIO43, CMD=GPIO44, D0=GPIO39, D1=GPIO40, D2=GPIO41, D3=GPIO42.
     * GPIO45 drives AO3401 gate for SD1_VDD, so 8-bit mode (D4 on GPIO45) is not usable.
     */
    if (sdmmc_gpio_setup_iomux_slot0(SDMMC_BUS_WIDTH) == false)
    {
        SDMMC_LOG("[sdmmc] iomux setup failed\n");
        return SDMMC_ERR_INVALID_ARG;
    }

    sdmmc_ll_enable_bus_clock(s_sdmmc, true);
    sdmmc_ll_enable_sdio_pll(s_sdmmc, true);
    sdmmc_ll_select_clk_source(s_sdmmc, SDMMC_CLK_SRC_PLL160M);
    sdmmc_ll_set_clock_div(s_sdmmc, SDMMC_HOST_DIV_PROBING);
    sdmmc_ll_init_phase_delay(s_sdmmc);

    sdmmc_ll_reset_controller(s_sdmmc);
    sdmmc_ll_reset_fifo(s_sdmmc);
    sdmmc_ll_reset_dma(s_sdmmc);

    uint32_t wait_loops = SDMMC_CMD_TIMEOUT_LOOPS;
    while (wait_loops-- > 0U)
    {
        if (sdmmc_ll_is_controller_reset_done(s_sdmmc) &&
            sdmmc_ll_is_fifo_reset_done(s_sdmmc) &&
            sdmmc_ll_is_dma_reset_done(s_sdmmc))
        {
            break;
        }
    }
    if (wait_loops == 0U)
    {
        SDMMC_LOG("[sdmmc] reset timeout\n");
        return SDMMC_ERR_TIMEOUT;
    }

    sdmmc_ll_clear_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_clear_idsts_interrupt(s_sdmmc, UINT32_MAX);
    sdmmc_ll_enable_dma(s_sdmmc, false);

    sdmmc_ll_set_response_timeout(s_sdmmc, 0xFFU);
    sdmmc_ll_set_data_timeout(s_sdmmc, 0x00FFFFFFU);
    sdmmc_err_t rc = sdmmc_set_slot0_clock(SDMMC_HOST_DIV_PROBING, SDMMC_CARD_DIV_PROBING);
    if (rc != SDMMC_OK)
    {
        return rc;
    }
    rc = sdmmc_send_cmd(SDMMC_CMD_GO_IDLE_STATE, 0U, SDMMC_RESP_NONE, false, true, NULL);
    if (rc != SDMMC_OK)
    {
        return rc;
    }

    uint32_t resp = 0;
    rc = sdmmc_send_cmd(SDMMC_CMD_SEND_IF_COND, 0x1AAU, SDMMC_RESP_SHORT, true, false, &resp);
    if (rc != SDMMC_OK || ((resp & 0xFFFU) != 0x1AAU))
    {
        SDMMC_LOG("[sdmmc] CMD8 failed rc=%d resp=0x%08x\n", (int)rc, (unsigned)resp);
        return SDMMC_ERR_PROTOCOL;
    }

    for (uint32_t i = 0; i < SDMMC_ACMD41_MAX_TRIES; i++)
    {
        rc = sdmmc_send_acmd(SDMMC_ACMD_SD_SEND_OP_COND, 0x40FF8000U, &resp);
        if (rc == SDMMC_OK && ((resp & BIT(31)) != 0U))
        {
            s_high_capacity = ((resp & BIT(30)) != 0U);
            break;
        }
        if (i == (SDMMC_ACMD41_MAX_TRIES - 1U))
        {
            SDMMC_LOG("[sdmmc] ACMD41 timeout\n");
            return SDMMC_ERR_TIMEOUT;
        }
        delay_ms(1U);
    }

    rc = sdmmc_send_cmd(SDMMC_CMD_ALL_SEND_CID, 0U, SDMMC_RESP_LONG, true, false, NULL);
    if (rc != SDMMC_OK)
    {
        return rc;
    }

    rc = sdmmc_send_cmd(SDMMC_CMD_SEND_RELATIVE_ADDR, 0U, SDMMC_RESP_SHORT, true, false, &resp);
    if (rc != SDMMC_OK)
    {
        return rc;
    }
    s_card_rca = (uint16_t)(resp >> 16);

    uint64_t cap = 0;
    if (sdmmc_read_card_capacity(&cap) == SDMMC_OK)
    {
        float size = (float)cap / (1024.0 * 1024.0 * 1024.0);
        uint16_t sizeInt = size;
        float tmpFrac = size - sizeInt;
        uint16_t tmpInt = tmpFrac * 100;
        SDMMC_LOG("[sdmmc] Card Size=%d.%d GB\n", sizeInt, tmpInt);
    }

    rc = sdmmc_send_cmd(SDMMC_CMD_SELECT_CARD, ((uint32_t)s_card_rca) << 16, SDMMC_RESP_SHORT, true, false, NULL);
    if (rc != SDMMC_OK)
    {
        return rc;
    }

    rc = sdmmc_send_acmd(SDMMC_ACMD_SET_BUS_WIDTH, 2U, NULL);
    if (rc != SDMMC_OK)
    {
        return rc;
    }
    sdmmc_ll_set_card_width(s_sdmmc, SDMMC_SLOT0, SD_BUS_WIDTH_4_BIT);

    rc = sdmmc_set_slot0_clock(SDMMC_HOST_DIV_TRANSFER, SDMMC_CARD_DIV_TRANSFER);
    if (rc != SDMMC_OK)
    {
        return rc;
    }

    s_initialized = true;
    SDMMC_LOG("[sdmmc] init ok, card=%s, rca=0x%04x\n", s_high_capacity ? "SDHC/SDXC" : "SDSC", s_card_rca);
    return SDMMC_OK;
}
