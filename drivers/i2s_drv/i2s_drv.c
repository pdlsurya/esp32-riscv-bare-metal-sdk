/*
 * MIT License
 *
 * Copyright (c) 2026 Surya Poudel
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

#include <string.h>
#include "i2s_drv.h"
#include "delay.h"
#include "hal/dma_types.h"
#include "hal/gdma_ll.h"
#include "hal/gpio_ll.h"
#include "hal/hal_utils.h"
#include "soc/io_mux_reg.h"
#include "soc/i2s_periph.h"
#include "soc/soc_caps.h"
#include "soc/gdma_channel.h"

#if defined(TARGET_SOC_ESP32P4)
#include "hal/ahb_dma_ll.h"
#include "hal/cache_ll.h"

/* Use non-atomic raw LL entry points in this bare-metal SDK. */
#undef i2s_ll_enable_bus_clock
#undef i2s_ll_reset_register
#undef i2s_ll_enable_core_clock
#undef i2s_ll_tx_disable_clock
#undef i2s_ll_rx_disable_clock

#endif

#ifndef I2S_DEFAULT_MCLK_MULTIPLE
#define I2S_DEFAULT_MCLK_MULTIPLE 256U
#endif

#ifndef I2S_DEFAULT_TIMEOUT_US
#define I2S_DEFAULT_TIMEOUT_US 200000U
#endif

#ifndef I2S_DMA_MAX_CHUNK
#define I2S_DMA_MAX_CHUNK DMA_DESCRIPTOR_BUFFER_MAX_SIZE_4B_ALIGNED
#endif

#ifndef I2S_STREAM_MAX_BUFFERS
#define I2S_STREAM_MAX_BUFFERS 4U
#endif

#ifndef I2S_STREAM_MIN_PREFILL
#define I2S_STREAM_MIN_PREFILL 2U
#endif

#ifndef I2S_DRV_DEBUG_LOG
#define I2S_DRV_DEBUG_LOG 0
#endif

#if I2S_DRV_DEBUG_LOG
#include "usb_serial.h"
#endif

#if I2S_DRV_DEBUG_LOG
#define I2S_LOG(fmt, ...) serial_printf("[i2s] " fmt "\r\n", ##__VA_ARGS__)
#else
#define I2S_LOG(...)
#endif

#if defined(TARGET_SOC_ESP32P4)
typedef ahb_dma_dev_t i2s_dma_dev_t;
#define I2S_DMA_HW AHB_DMA_LL_GET_HW(0)

#define I2S_DMA_FORCE_ENABLE_CLOCK(dma)        \
    do                                         \
    {                                          \
        _gdma_ll_enable_bus_clock(GDMA_LL_AHB_GROUP_START_ID, true); \
        ahb_dma_ll_force_enable_reg_clock((dma), true);               \
        ahb_dma_ll_set_default_memory_range((dma));                   \
    } while (0)
#define I2S_DMA_TX_RESET_CHANNEL(dma, ch) ahb_dma_ll_tx_reset_channel((dma), (ch))
#define I2S_DMA_TX_ENABLE_OWNER_CHECK(dma, ch, enable) ahb_dma_ll_tx_enable_owner_check((dma), (ch), (enable))
#define I2S_DMA_TX_ENABLE_DATA_BURST(dma, ch, enable) ahb_dma_ll_tx_enable_data_burst((dma), (ch), (enable))
#define I2S_DMA_TX_ENABLE_DESC_BURST(dma, ch, enable) ahb_dma_ll_tx_enable_descriptor_burst((dma), (ch), (enable))
#define I2S_DMA_TX_SET_EOF_MODE(dma, ch, mode) ahb_dma_ll_tx_set_eof_mode((dma), (ch), (mode))
#define I2S_DMA_TX_ENABLE_AUTO_WB(dma, ch, enable) ahb_dma_ll_tx_enable_auto_write_back((dma), (ch), (enable))
#define I2S_DMA_TX_SET_PRIORITY(dma, ch, priority) ahb_dma_ll_tx_set_priority((dma), (ch), (priority))
#define I2S_DMA_TX_CONNECT(dma, ch, periph_id) ahb_dma_ll_tx_connect_to_periph((dma), (ch), GDMA_TRIG_PERIPH_I2S, (periph_id))
#define I2S_DMA_TX_DISCONNECT(dma, ch) ahb_dma_ll_tx_disconnect_from_periph((dma), (ch))
#define I2S_DMA_TX_CLEAR_INTERRUPT(dma, ch, mask) ahb_dma_ll_tx_clear_interrupt_status((dma), (ch), (mask))
#define I2S_DMA_TX_GET_INTERRUPT(dma, ch) ahb_dma_ll_tx_get_interrupt_status((dma), (ch), true)
#define I2S_DMA_TX_SET_DESC(dma, ch, addr) ahb_dma_ll_tx_set_desc_addr((dma), (ch), (addr))
#define I2S_DMA_TX_START(dma, ch) ahb_dma_ll_tx_start((dma), (ch))
#define I2S_DMA_TX_STOP(dma, ch) ahb_dma_ll_tx_stop((dma), (ch))
#define I2S_DMA_TX_IS_IDLE(dma, ch) ahb_dma_ll_tx_is_desc_fsm_idle((dma), (ch))
#else
typedef gdma_dev_t i2s_dma_dev_t;
#define I2S_DMA_HW GDMA_LL_GET_HW(0)

#define I2S_DMA_FORCE_ENABLE_CLOCK(dma)        \
    do                                         \
    {                                          \
        _gdma_ll_enable_bus_clock(GDMA_LL_AHB_GROUP_START_ID, true); \
        gdma_ll_force_enable_reg_clock((dma), true);                  \
    } while (0)
#define I2S_DMA_TX_RESET_CHANNEL(dma, ch) gdma_ll_tx_reset_channel((dma), (ch))
#define I2S_DMA_TX_ENABLE_OWNER_CHECK(dma, ch, enable) gdma_ll_tx_enable_owner_check((dma), (ch), (enable))
#define I2S_DMA_TX_ENABLE_DATA_BURST(dma, ch, enable) gdma_ll_tx_enable_data_burst((dma), (ch), (enable))
#define I2S_DMA_TX_ENABLE_DESC_BURST(dma, ch, enable) gdma_ll_tx_enable_descriptor_burst((dma), (ch), (enable))
#define I2S_DMA_TX_SET_EOF_MODE(dma, ch, mode) gdma_ll_tx_set_eof_mode((dma), (ch), (mode))
#define I2S_DMA_TX_ENABLE_AUTO_WB(dma, ch, enable) gdma_ll_tx_enable_auto_write_back((dma), (ch), (enable))
#define I2S_DMA_TX_SET_PRIORITY(dma, ch, priority) gdma_ll_tx_set_priority((dma), (ch), (priority))
#define I2S_DMA_TX_CONNECT(dma, ch, periph_id) gdma_ll_tx_connect_to_periph((dma), (ch), GDMA_TRIG_PERIPH_I2S, (periph_id))
#define I2S_DMA_TX_DISCONNECT(dma, ch) gdma_ll_tx_disconnect_from_periph((dma), (ch))
#define I2S_DMA_TX_CLEAR_INTERRUPT(dma, ch, mask) gdma_ll_tx_clear_interrupt_status((dma), (ch), (mask))
#define I2S_DMA_TX_GET_INTERRUPT(dma, ch) gdma_ll_tx_get_interrupt_status((dma), (ch), true)
#define I2S_DMA_TX_SET_DESC(dma, ch, addr) gdma_ll_tx_set_desc_addr((dma), (ch), (addr))
#define I2S_DMA_TX_START(dma, ch) gdma_ll_tx_start((dma), (ch))
#define I2S_DMA_TX_STOP(dma, ch) gdma_ll_tx_stop((dma), (ch))
#define I2S_DMA_TX_IS_IDLE(dma, ch) gdma_ll_tx_is_desc_fsm_idle((dma), (ch))
#endif

/*
 * Keep DMA descriptors in static storage (not stack). On ESP32-P4, stack
 * addresses can fall outside the preconfigured DMA memory window.
 */
static dma_descriptor_t s_i2s_dma_desc[SOC_I2S_NUM][SOC_GDMA_PAIRS_PER_GROUP_MAX][I2S_STREAM_MAX_BUFFERS] __attribute__((aligned(4)));
static uint8_t s_i2s_dma_buf[SOC_I2S_NUM][SOC_GDMA_PAIRS_PER_GROUP_MAX][I2S_STREAM_MAX_BUFFERS][I2S_DMA_MAX_CHUNK] __attribute__((aligned(4)));

static int i2s_get_port_id(i2s_dev_t *port)
{
    if (port == &I2S0)
    {
        return 0;
    }
#if SOC_I2S_NUM > 1
    if (port == &I2S1)
    {
        return 1;
    }
#endif
#if SOC_I2S_NUM > 2
    if (port == &I2S2)
    {
        return 2;
    }
#endif
    return -1;
}

static int i2s_get_dma_periph_id(uint8_t port_id)
{
#if defined(TARGET_SOC_ESP32P4)
    switch (port_id)
    {
    case 0:
        return SOC_GDMA_TRIG_PERIPH_I2S0;
    case 1:
        return SOC_GDMA_TRIG_PERIPH_I2S1;
    case 2:
        return SOC_GDMA_TRIG_PERIPH_I2S2;
    default:
        return -1;
    }
#else
    (void)port_id;
    return SOC_GDMA_TRIG_PERIPH_I2S0;
#endif
}

static dma_descriptor_t *i2s_get_dma_desc(uint8_t port_id, uint8_t dma_channel, uint8_t slot)
{
    return &s_i2s_dma_desc[port_id][dma_channel][slot];
}

static dma_descriptor_t *i2s_get_dma_desc_cpu(uint8_t port_id, uint8_t dma_channel, uint8_t slot)
{
#if defined(TARGET_SOC_ESP32P4)
    return (dma_descriptor_t *)CACHE_LL_L2MEM_NON_CACHE_ADDR(i2s_get_dma_desc(port_id, dma_channel, slot));
#else
    return i2s_get_dma_desc(port_id, dma_channel, slot);
#endif
}

static uint8_t *i2s_get_dma_buf(uint8_t port_id, uint8_t dma_channel, uint8_t slot)
{
    return &s_i2s_dma_buf[port_id][dma_channel][slot][0];
}

static uint8_t *i2s_get_dma_buf_cpu(uint8_t port_id, uint8_t dma_channel, uint8_t slot)
{
#if defined(TARGET_SOC_ESP32P4)
    return (uint8_t *)CACHE_LL_L2MEM_NON_CACHE_ADDR(i2s_get_dma_buf(port_id, dma_channel, slot));
#else
    return i2s_get_dma_buf(port_id, dma_channel, slot);
#endif
}

static void i2s_gpio_output_config(uint8_t gpio_num, uint32_t signal_idx)
{
    if (gpio_num == I2S_PIN_UNUSED || signal_idx == (uint32_t)UINT8_MAX)
    {
        return;
    }

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio_num], PIN_FUNC_GPIO);
    GPIO.func_out_sel_cfg[gpio_num].out_sel = signal_idx;
    gpio_ll_output_enable(&GPIO, gpio_num);
    gpio_ll_input_disable(&GPIO, gpio_num);
}

static void i2s_gpio_input_config(uint8_t gpio_num, uint32_t signal_idx)
{
    if (gpio_num == I2S_PIN_UNUSED || signal_idx == (uint32_t)UINT8_MAX)
    {
        return;
    }

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio_num], PIN_FUNC_GPIO);
    gpio_ll_input_enable(&GPIO, gpio_num);
    GPIO.func_in_sel_cfg[signal_idx].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[signal_idx].in_sel = gpio_num;
}

static void i2s_configure_pins(const i2s_config_t *config, uint8_t port_id)
{
    const i2s_signal_conn_t *signals = &i2s_periph_signal[port_id];

    uint32_t tx_data_out_sig = signals->data_out_sig;
    uint32_t rx_data_in_sig = signals->data_in_sig;

#if SOC_I2S_PDM_MAX_TX_LINES
    tx_data_out_sig = signals->data_out_sigs[0];
#endif
#if SOC_I2S_PDM_MAX_RX_LINES
    rx_data_in_sig = signals->data_in_sigs[0];
#endif

    if (config->role == I2S_ROLE_MASTER)
    {
        i2s_gpio_output_config(config->pins.bclk, signals->m_tx_bck_sig);
        i2s_gpio_output_config(config->pins.ws, signals->m_tx_ws_sig);
    }
    else
    {
        i2s_gpio_input_config(config->pins.bclk, signals->s_tx_bck_sig);
        i2s_gpio_input_config(config->pins.ws, signals->s_tx_ws_sig);
    }

    i2s_gpio_output_config(config->pins.dout, tx_data_out_sig);
    i2s_gpio_input_config(config->pins.din, rx_data_in_sig);
    i2s_gpio_output_config(config->pins.mclk, signals->mck_out_sig);
}

static void i2s_ll_tx_enable_clock_raw(i2s_dev_t *port)
{
#if defined(TARGET_SOC_ESP32P4)
    _i2s_ll_tx_enable_clock(port);
#else
    i2s_ll_tx_enable_clock(port);
#endif
}

static void i2s_ll_tx_clk_set_src_raw(i2s_dev_t *port, i2s_clock_src_t src)
{
#if defined(TARGET_SOC_ESP32P4)
    _i2s_ll_tx_clk_set_src(port, src);
#else
    i2s_ll_tx_clk_set_src(port, src);
#endif
}

static void i2s_ll_tx_set_mclk_raw(i2s_dev_t *port, const hal_utils_clk_div_t *mclk_div)
{
#if defined(TARGET_SOC_ESP32P4)
    _i2s_ll_tx_set_mclk(port, mclk_div);
#else
    i2s_ll_tx_set_mclk(port, mclk_div);
#endif
}

static void i2s_ll_mclk_bind_to_tx_raw(i2s_dev_t *port)
{
#if defined(TARGET_SOC_ESP32P4)
    _i2s_ll_mclk_bind_to_tx_clk(port);
#else
    i2s_ll_mclk_bind_to_tx_clk(port);
#endif
}

static uint32_t i2s_get_source_clock_hz(i2s_clock_src_t src)
{
#if defined(TARGET_SOC_ESP32P4)
    switch (src)
    {
    case I2S_CLK_SRC_XTAL:
        return 40000000U;
    case I2S_CLK_SRC_PLL_160M:
        return 160000000U;
    default:
        return I2S_LL_DEFAULT_CLK_FREQ;
    }
#else
    switch (src)
    {
    case I2S_CLK_SRC_XTAL:
        return 40000000U;
    case I2S_CLK_SRC_PLL_240M:
        return 240000000U;
    case I2S_CLK_SRC_PLL_160M:
        return 160000000U;
    default:
        return I2S_LL_DEFAULT_CLK_FREQ;
    }
#endif
}

static bool i2s_validate_slot_bits(uint8_t bits)
{
    return bits == 8U || bits == 16U || bits == 24U || bits == 32U;
}

static bool i2s_configure_std_tx(i2s_dev_handle_t *dev, const i2s_config_t *config)
{
    const uint8_t data_bits = config->data_bit_width;
    const uint8_t slot_bits = config->slot_bit_width == 0U ? data_bits : config->slot_bit_width;

    if (!i2s_validate_slot_bits(data_bits) || !i2s_validate_slot_bits(slot_bits) || slot_bits < data_bits)
    {
        return false;
    }

    i2s_ll_tx_enable_std(dev->port);
    i2s_ll_tx_set_slave_mod(dev->port, config->role == I2S_ROLE_SLAVE);

    i2s_ll_tx_set_ws_width(dev->port, slot_bits);
    i2s_ll_tx_set_sample_bit(dev->port, slot_bits, data_bits);
    i2s_ll_tx_set_half_sample_bit(dev->port, slot_bits);

    i2s_ll_tx_select_std_slot(dev->port, I2S_STD_SLOT_BOTH);
    i2s_ll_tx_enable_msb_shift(dev->port, true);
    i2s_ll_tx_enable_left_align(dev->port, false);
    i2s_ll_tx_enable_big_endian(dev->port, false);
    i2s_ll_tx_set_bit_order(dev->port, false);
    i2s_ll_tx_set_ws_idle_pol(dev->port, false);
    i2s_ll_tx_enable_mono_mode(dev->port, config->slot_mode == I2S_SLOT_MODE_MONO);
    i2s_ll_set_single_data(dev->port, 0U);

    dev->port->tx_conf.tx_stop_en = 1;

    i2s_ll_tx_reset_fifo(dev->port);
    i2s_ll_tx_update(dev->port);

    dev->data_bit_width = data_bits;
    dev->slot_bit_width = slot_bits;

    return true;
}

static bool i2s_configure_clock(i2s_dev_handle_t *dev, const i2s_config_t *config)
{
    const uint32_t channels = 2U;
    const uint32_t slot_bits = dev->slot_bit_width;
    const uint32_t mclk_multiple = (config->mclk_multiple == 0U) ? I2S_DEFAULT_MCLK_MULTIPLE : config->mclk_multiple;

    uint64_t bclk_calc = (uint64_t)config->sample_rate_hz * (uint64_t)slot_bits * (uint64_t)channels;
    uint64_t mclk_calc = (uint64_t)config->sample_rate_hz * (uint64_t)mclk_multiple;

    if (config->sample_rate_hz == 0U || bclk_calc == 0U || bclk_calc > UINT32_MAX || mclk_calc > UINT32_MAX)
    {
        I2S_LOG("clock invalid input fs=%lu bclk_calc=%lu mclk_calc=%lu",
                (unsigned long)config->sample_rate_hz,
                (unsigned long)bclk_calc,
                (unsigned long)mclk_calc);
        return false;
    }

    uint32_t bclk_hz = (uint32_t)bclk_calc;
    uint32_t mclk_hz = (uint32_t)mclk_calc;
    if (mclk_hz < bclk_hz)
    {
        mclk_hz = bclk_hz;
    }

    uint32_t src_clk_hz = i2s_get_source_clock_hz(config->clk_src);
    if (src_clk_hz == 0U)
    {
        I2S_LOG("clock source invalid src=%d", (int)config->clk_src);
        return false;
    }

    uint32_t int_div = (src_clk_hz + (mclk_hz / 2U)) / mclk_hz;
    if (int_div < 2U)
    {
        int_div = 2U;
    }
    if (int_div >= I2S_LL_CLK_FRAC_DIV_N_MAX)
    {
        int_div = I2S_LL_CLK_FRAC_DIV_N_MAX - 1U;
    }

    hal_utils_clk_div_t mclk_div = {
        .integer = int_div,
        .denominator = 0U,
        .numerator = 0U,
    };

    uint32_t real_mclk = src_clk_hz / int_div;
    if (real_mclk == 0U)
    {
        I2S_LOG("clock real_mclk=0 src=%lu int_div=%lu",
                (unsigned long)src_clk_hz, (unsigned long)int_div);
        return false;
    }

    uint32_t bclk_div = (real_mclk + (bclk_hz / 2U)) / bclk_hz;
    if (bclk_div == 0U)
    {
        bclk_div = 1U;
    }
    if (bclk_div > 64U)
    {
        I2S_LOG("clock bclk_div too large=%lu real_mclk=%lu bclk=%lu",
                (unsigned long)bclk_div,
                (unsigned long)real_mclk,
                (unsigned long)bclk_hz);
        return false;
    }

    i2s_ll_tx_clk_set_src_raw(dev->port, config->clk_src);
    i2s_ll_tx_set_mclk_raw(dev->port, &mclk_div);
    i2s_ll_tx_set_bck_div_num(dev->port, bclk_div);
    i2s_ll_tx_update(dev->port);

    dev->mclk_hz = real_mclk;
    dev->bclk_hz = real_mclk / bclk_div;
    return true;
}

static bool i2s_dma_init_tx(i2s_dev_handle_t *dev, bool auto_write_back)
{
    i2s_dma_dev_t *dma = I2S_DMA_HW;
    if (dma == NULL)
    {
        I2S_LOG("dma hw is null");
        return false;
    }

    int periph_id = i2s_get_dma_periph_id(dev->port_id);
    if (periph_id < 0)
    {
        I2S_LOG("dma invalid periph id for port=%u", dev->port_id);
        return false;
    }

    I2S_DMA_FORCE_ENABLE_CLOCK(dma);

    I2S_DMA_TX_STOP(dma, dev->dma_channel);
    I2S_DMA_TX_RESET_CHANNEL(dma, dev->dma_channel);

    I2S_DMA_TX_ENABLE_OWNER_CHECK(dma, dev->dma_channel, true);
    /*
     * Keep burst disabled for I2S audio chunks to avoid strict alignment
     * requirements on descriptor/data fetch across SoC variants.
     */
    I2S_DMA_TX_ENABLE_DATA_BURST(dma, dev->dma_channel, false);
    I2S_DMA_TX_ENABLE_DESC_BURST(dma, dev->dma_channel, false);
    I2S_DMA_TX_SET_EOF_MODE(dma, dev->dma_channel, 1U);
    I2S_DMA_TX_ENABLE_AUTO_WB(dma, dev->dma_channel, auto_write_back);
    I2S_DMA_TX_SET_PRIORITY(dma, dev->dma_channel, 1U);

    I2S_DMA_TX_CLEAR_INTERRUPT(dma, dev->dma_channel, UINT32_MAX);
    I2S_DMA_TX_CONNECT(dma, dev->dma_channel, periph_id);

    return true;
}

static bool i2s_dma_write_chunk(i2s_dev_handle_t *dev, const uint8_t *data, size_t len_bytes, uint32_t timeout_us)
{
    i2s_dma_dev_t *dma = I2S_DMA_HW;
    if (dma == NULL || len_bytes == 0U)
    {
        return false;
    }

    dma_descriptor_t *dma_desc = i2s_get_dma_desc(dev->port_id, dev->dma_channel, 0U);
    dma_descriptor_t *dma_desc_cpu = i2s_get_dma_desc_cpu(dev->port_id, dev->dma_channel, 0U);
    uint8_t *dma_buf = i2s_get_dma_buf(dev->port_id, dev->dma_channel, 0U);
    uint8_t *dma_buf_cpu = i2s_get_dma_buf_cpu(dev->port_id, dev->dma_channel, 0U);

    memcpy(dma_buf_cpu, data, len_bytes);

    memset(dma_desc_cpu, 0, sizeof(*dma_desc_cpu));
    dma_desc_cpu->dw0.size = (uint32_t)len_bytes;
    dma_desc_cpu->dw0.length = (uint32_t)len_bytes;
    dma_desc_cpu->dw0.suc_eof = 1;
    dma_desc_cpu->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    dma_desc_cpu->buffer = (void *)dma_buf;
    dma_desc_cpu->next = NULL;

    I2S_DMA_TX_STOP(dma, dev->dma_channel);
    I2S_DMA_TX_RESET_CHANNEL(dma, dev->dma_channel);
    I2S_DMA_TX_CLEAR_INTERRUPT(dma, dev->dma_channel, UINT32_MAX);
    I2S_DMA_TX_CONNECT(dma, dev->dma_channel, i2s_get_dma_periph_id(dev->port_id));

    i2s_ll_tx_stop(dev->port);
    i2s_ll_tx_reset_fifo(dev->port);
    dev->port->int_clr.tx_done_int_clr = 1;

    I2S_DMA_TX_SET_DESC(dma, dev->dma_channel, (uint32_t)(uintptr_t)dma_desc);
    I2S_DMA_TX_START(dma, dev->dma_channel);
    i2s_ll_tx_start(dev->port);

    uint32_t budget = timeout_us == 0U ? I2S_DEFAULT_TIMEOUT_US : timeout_us;
    uint32_t dma_status = 0U;
    while (budget-- > 0U)
    {
        dma_status = I2S_DMA_TX_GET_INTERRUPT(dma, dev->dma_channel);
        if ((dma_status & GDMA_LL_EVENT_TX_DESC_ERROR) != 0U)
        {
            I2S_DMA_TX_CLEAR_INTERRUPT(dma, dev->dma_channel, dma_status);
            i2s_ll_tx_stop(dev->port);
            I2S_DMA_TX_STOP(dma, dev->dma_channel);
            I2S_LOG("tx desc error status=0x%08lx len=%lu ch=%u",
                    (unsigned long)dma_status,
                    (unsigned long)len_bytes,
                    dev->dma_channel);
            return false;
        }
        if ((dma_status & (GDMA_LL_EVENT_TX_EOF | GDMA_LL_EVENT_TX_DONE)) != 0U)
        {
            break;
        }
        delay_us(1);
    }

    if ((dma_status & (GDMA_LL_EVENT_TX_EOF | GDMA_LL_EVENT_TX_DONE)) == 0U)
    {
        i2s_ll_tx_stop(dev->port);
        I2S_DMA_TX_STOP(dma, dev->dma_channel);
        I2S_LOG("tx dma timeout status=0x%08lx len=%lu ch=%u",
                (unsigned long)dma_status,
                (unsigned long)len_bytes,
                dev->dma_channel);
        return false;
    }

    I2S_DMA_TX_CLEAR_INTERRUPT(dma, dev->dma_channel, dma_status);

    budget = timeout_us == 0U ? I2S_DEFAULT_TIMEOUT_US : timeout_us;
    while (budget-- > 0U)
    {
        if (dev->port->int_raw.tx_done_int_raw)
        {
            break;
        }
        delay_us(1);
    }

    dev->port->int_clr.tx_done_int_clr = 1;
    i2s_ll_tx_stop(dev->port);
    I2S_DMA_TX_STOP(dma, dev->dma_channel);

    /*
     * On some ESP32-P4 revisions the DMA EOF arrives reliably while TX_DONE
     * may not assert for each short chunk. Treat DMA completion as success.
     */
    if (budget == 0U)
    {
        I2S_LOG("tx_done timeout ignored status=0x%08lx len=%lu ch=%u",
                (unsigned long)dma_status,
                (unsigned long)len_bytes,
                dev->dma_channel);
    }
    return true;
}

static void i2s_stream_reset_queue(i2s_dev_handle_t *dev)
{
    uint8_t count = dev->stream_buf_count;
    uint16_t size = dev->stream_buf_size;

    for (uint8_t i = 0; i < count; i++)
    {
        dma_descriptor_t *desc_cpu = i2s_get_dma_desc_cpu(dev->port_id, dev->dma_channel, i);
        uint8_t *buf_cpu = i2s_get_dma_buf_cpu(dev->port_id, dev->dma_channel, i);

        memset(buf_cpu, 0, size);
        memset(desc_cpu, 0, sizeof(*desc_cpu));
        desc_cpu->dw0.size = size;
        desc_cpu->dw0.length = 0U;
        desc_cpu->dw0.suc_eof = 1U;
        desc_cpu->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_CPU;
        desc_cpu->buffer = (void *)i2s_get_dma_buf(dev->port_id, dev->dma_channel, i);
        desc_cpu->next = NULL;
    }

    dev->stream_head = 0U;
    dev->stream_tail = 0U;
    dev->stream_queued = 0U;
    dev->stream_started = false;
    dev->stream_error = false;
}

static bool i2s_stream_start_dma(i2s_dev_handle_t *dev)
{
    i2s_dma_dev_t *dma = I2S_DMA_HW;
    if (dma == NULL || dev->stream_queued == 0U)
    {
        return false;
    }

    if (!i2s_dma_init_tx(dev, true))
    {
        return false;
    }

    I2S_DMA_TX_SET_DESC(dma, dev->dma_channel,
                        (uint32_t)(uintptr_t)i2s_get_dma_desc(dev->port_id, dev->dma_channel, dev->stream_tail));

    i2s_ll_tx_stop(dev->port);
    i2s_ll_tx_reset_fifo(dev->port);
    dev->port->int_clr.tx_done_int_clr = 1;

    I2S_DMA_TX_START(dma, dev->dma_channel);
    i2s_ll_tx_start(dev->port);
    dev->stream_started = true;
    dev->stream_error = false;
    return true;
}

static void i2s_stream_try_start(i2s_dev_handle_t *dev)
{
    i2s_dma_dev_t *dma = I2S_DMA_HW;
    uint8_t prefill = (dev->stream_buf_count < I2S_STREAM_MIN_PREFILL) ? dev->stream_buf_count : I2S_STREAM_MIN_PREFILL;

    if (!dev->stream_mode || dev->stream_error || dev->stream_queued < prefill)
    {
        return;
    }

    if (dma != NULL && dev->stream_started && !I2S_DMA_TX_IS_IDLE(dma, dev->dma_channel))
    {
        return;
    }

    (void)i2s_stream_start_dma(dev);
}

static void i2s_stream_reclaim_completed(i2s_dev_handle_t *dev)
{
    while (dev->stream_queued > 0U)
    {
        dma_descriptor_t *desc_cpu = i2s_get_dma_desc_cpu(dev->port_id, dev->dma_channel, dev->stream_tail);
        if (desc_cpu->dw0.owner != DMA_DESCRIPTOR_BUFFER_OWNER_CPU)
        {
            break;
        }

        desc_cpu->dw0.length = 0U;
        desc_cpu->dw0.suc_eof = 1U;
        desc_cpu->next = NULL;

        dev->stream_tail++;
        if (dev->stream_tail >= dev->stream_buf_count)
        {
            dev->stream_tail = 0U;
        }
        dev->stream_queued--;
    }
}

bool i2s_init(i2s_dev_handle_t *dev, const i2s_config_t *config)
{
    if (dev == NULL || config == NULL || config->port == NULL)
    {
        I2S_LOG("init invalid args dev=%p cfg=%p port=%p", (void *)dev, (void *)config, config ? (void *)config->port : NULL);
        return false;
    }

    if ((config->role != I2S_ROLE_MASTER) && (config->role != I2S_ROLE_SLAVE))
    {
        I2S_LOG("init invalid role=%d", (int)config->role);
        return false;
    }

    if ((config->slot_mode != I2S_SLOT_MODE_MONO) && (config->slot_mode != I2S_SLOT_MODE_STEREO))
    {
        I2S_LOG("init invalid slot mode=%d", (int)config->slot_mode);
        return false;
    }

    if (config->dma_channel >= SOC_GDMA_PAIRS_PER_GROUP_MAX)
    {
        I2S_LOG("init invalid dma channel=%u max=%u", config->dma_channel, SOC_GDMA_PAIRS_PER_GROUP_MAX);
        return false;
    }

    int port_id = i2s_get_port_id(config->port);
    if (port_id < 0)
    {
        I2S_LOG("init invalid port=%p", (void *)config->port);
        return false;
    }

    memset(dev, 0, sizeof(*dev));

    dev->port = config->port;
    dev->port_id = (uint8_t)port_id;
    dev->dma_channel = config->dma_channel;
    dev->role = config->role;
    dev->slot_mode = config->slot_mode;
    dev->sample_rate_hz = config->sample_rate_hz;

    i2s_ll_enable_bus_clock(port_id, true);
    i2s_ll_reset_register(port_id);
    i2s_ll_enable_core_clock(dev->port, true);

    i2s_ll_tx_enable_clock_raw(dev->port);
    i2s_ll_mclk_bind_to_tx_raw(dev->port);
    i2s_ll_tx_reset(dev->port);
    i2s_ll_tx_reset_fifo(dev->port);

    i2s_configure_pins(config, dev->port_id);

    if (!i2s_configure_std_tx(dev, config))
    {
        I2S_LOG("init std tx failed data_bits=%u slot_bits=%u",
                config->data_bit_width, config->slot_bit_width);
        return false;
    }

    if (!i2s_configure_clock(dev, config))
    {
        I2S_LOG("init clock failed fs=%lu data_bits=%u slot_bits=%u src=%d mclk_mul=%lu",
                (unsigned long)config->sample_rate_hz,
                config->data_bit_width,
                config->slot_bit_width,
                (int)config->clk_src,
                (unsigned long)config->mclk_multiple);
        return false;
    }

    if (!i2s_dma_init_tx(dev, false))
    {
        I2S_LOG("init dma failed port=%u dma_ch=%u", dev->port_id, dev->dma_channel);
        return false;
    }

    dev->initialized = true;
    return true;
}

void i2s_deinit(i2s_dev_handle_t *dev)
{
    if (dev == NULL || !dev->initialized)
    {
        return;
    }

    i2s_stream_stop(dev);

    i2s_dma_dev_t *dma = I2S_DMA_HW;

    i2s_ll_tx_stop(dev->port);
    if (dma != NULL)
    {
        I2S_DMA_TX_STOP(dma, dev->dma_channel);
        I2S_DMA_TX_DISCONNECT(dma, dev->dma_channel);
        I2S_DMA_TX_CLEAR_INTERRUPT(dma, dev->dma_channel, UINT32_MAX);
    }

    i2s_ll_tx_disable_clock(dev->port);
    dev->initialized = false;
}

bool i2s_write(i2s_dev_handle_t *dev, const void *data, size_t len_bytes, uint32_t timeout_us)
{
    if (dev == NULL || !dev->initialized || data == NULL)
    {
        return false;
    }
    if (dev->stream_mode)
    {
        I2S_LOG("blocking write disabled while stream mode is active");
        return false;
    }

    const uint8_t *src = (const uint8_t *)data;
    size_t remaining = len_bytes;

    while (remaining > 0U)
    {
        size_t chunk = remaining > I2S_DMA_MAX_CHUNK ? I2S_DMA_MAX_CHUNK : remaining;
        if (!i2s_dma_write_chunk(dev, src, chunk, timeout_us))
        {
            return false;
        }
        src += chunk;
        remaining -= chunk;
    }

    return true;
}

bool i2s_stream_begin(i2s_dev_handle_t *dev, uint8_t buffer_count, size_t buffer_size_bytes)
{
    if (dev == NULL || !dev->initialized)
    {
        return false;
    }
    if (buffer_count == 0U || buffer_count > I2S_STREAM_MAX_BUFFERS)
    {
        return false;
    }
    if (buffer_size_bytes == 0U || buffer_size_bytes > I2S_DMA_MAX_CHUNK)
    {
        return false;
    }

    i2s_stream_stop(dev);

    dev->stream_mode = true;
    dev->stream_buf_count = buffer_count;
    dev->stream_buf_size = (uint16_t)buffer_size_bytes;
    i2s_stream_reset_queue(dev);
    return true;
}

void i2s_stream_poll(i2s_dev_handle_t *dev)
{
    if (dev == NULL || !dev->initialized || !dev->stream_mode)
    {
        return;
    }

    i2s_dma_dev_t *dma = I2S_DMA_HW;
    if (dma == NULL)
    {
        dev->stream_error = true;
        return;
    }

    uint32_t dma_status = I2S_DMA_TX_GET_INTERRUPT(dma, dev->dma_channel);
    if ((dma_status & GDMA_LL_EVENT_TX_DESC_ERROR) != 0U)
    {
        I2S_DMA_TX_CLEAR_INTERRUPT(dma, dev->dma_channel, dma_status);
        I2S_DMA_TX_STOP(dma, dev->dma_channel);
        i2s_ll_tx_stop(dev->port);
        dev->stream_started = false;
        dev->stream_error = true;
        I2S_LOG("stream desc error status=0x%08lx ch=%u",
                (unsigned long)dma_status,
                dev->dma_channel);
        return;
    }
    if (dma_status != 0U)
    {
        I2S_DMA_TX_CLEAR_INTERRUPT(dma, dev->dma_channel, dma_status);
    }

    i2s_stream_reclaim_completed(dev);

    if (dev->stream_started && dev->stream_queued == 0U && I2S_DMA_TX_IS_IDLE(dma, dev->dma_channel))
    {
        I2S_DMA_TX_STOP(dma, dev->dma_channel);
        i2s_ll_tx_stop(dev->port);
        dev->stream_started = false;
    }
}

size_t i2s_stream_get_free_bytes(const i2s_dev_handle_t *dev)
{
    if (dev == NULL || !dev->initialized || !dev->stream_mode)
    {
        return 0U;
    }
    return (size_t)(dev->stream_buf_count - dev->stream_queued) * (size_t)dev->stream_buf_size;
}

size_t i2s_stream_write(i2s_dev_handle_t *dev, const void *data, size_t len_bytes, uint32_t timeout_us)
{
    if (dev == NULL || !dev->initialized || !dev->stream_mode || data == NULL || len_bytes == 0U)
    {
        return 0U;
    }
    if (dev->stream_error)
    {
        return 0U;
    }

    const uint8_t *src = (const uint8_t *)data;
    size_t written = 0U;
    uint32_t budget = (timeout_us == 0U) ? I2S_DEFAULT_TIMEOUT_US : timeout_us;

    while (written < len_bytes)
    {
        i2s_stream_poll(dev);

        if (dev->stream_queued >= dev->stream_buf_count)
        {
            if (budget == 0U)
            {
                break;
            }
            delay_us(1U);
            budget--;
            continue;
        }

        uint8_t slot = dev->stream_head;
        size_t chunk = len_bytes - written;
        if (chunk > dev->stream_buf_size)
        {
            chunk = dev->stream_buf_size;
        }

        dma_descriptor_t *desc_cpu = i2s_get_dma_desc_cpu(dev->port_id, dev->dma_channel, slot);
        uint8_t *buf_cpu = i2s_get_dma_buf_cpu(dev->port_id, dev->dma_channel, slot);
        uint8_t *buf_dma = i2s_get_dma_buf(dev->port_id, dev->dma_channel, slot);

        memcpy(buf_cpu, src + written, chunk);

        memset(desc_cpu, 0, sizeof(*desc_cpu));
        desc_cpu->dw0.size = (uint32_t)chunk;
        desc_cpu->dw0.length = (uint32_t)chunk;
        desc_cpu->dw0.suc_eof = 1U;
        desc_cpu->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
        desc_cpu->buffer = (void *)buf_dma;
        desc_cpu->next = NULL;

        if (dev->stream_queued > 0U)
        {
            uint8_t prev = (slot == 0U) ? (dev->stream_buf_count - 1U) : (uint8_t)(slot - 1U);
            dma_descriptor_t *prev_desc_cpu = i2s_get_dma_desc_cpu(dev->port_id, dev->dma_channel, prev);
            prev_desc_cpu->dw0.suc_eof = 0U;
            prev_desc_cpu->next = i2s_get_dma_desc(dev->port_id, dev->dma_channel, slot);
        }

        dev->stream_head++;
        if (dev->stream_head >= dev->stream_buf_count)
        {
            dev->stream_head = 0U;
        }
        dev->stream_queued++;
        written += chunk;

        i2s_stream_try_start(dev);
    }

    return written;
}

bool i2s_stream_drain(i2s_dev_handle_t *dev, uint32_t timeout_us)
{
    if (dev == NULL || !dev->initialized || !dev->stream_mode)
    {
        return false;
    }

    uint32_t budget = (timeout_us == 0U) ? I2S_DEFAULT_TIMEOUT_US : timeout_us;
    while (budget-- > 0U)
    {
        i2s_stream_poll(dev);
        if (dev->stream_error)
        {
            return false;
        }
        if (dev->stream_queued == 0U && !dev->stream_started)
        {
            return true;
        }
        delay_us(1U);
    }
    return false;
}

void i2s_stream_stop(i2s_dev_handle_t *dev)
{
    if (dev == NULL || !dev->initialized)
    {
        return;
    }

    i2s_dma_dev_t *dma = I2S_DMA_HW;
    if (dma != NULL)
    {
        I2S_DMA_TX_STOP(dma, dev->dma_channel);
        I2S_DMA_TX_CLEAR_INTERRUPT(dma, dev->dma_channel, UINT32_MAX);
    }
    i2s_ll_tx_stop(dev->port);

    if (dev->stream_mode)
    {
        i2s_stream_reset_queue(dev);
    }
}
