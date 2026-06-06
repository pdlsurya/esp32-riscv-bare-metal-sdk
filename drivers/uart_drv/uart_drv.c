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

#include <stddef.h>
#include <stdint.h>
#include "delay.h"
#include "hal/gpio_ll.h"
#include "soc/soc.h"
#include "soc/soc_caps.h"
#include "soc/uart_periph.h"
#include "uart_drv.h"

#if defined(TARGET_SOC_ESP32P4)
/* Use non-atomic raw LL entry points in this bare-metal SDK. */
#undef uart_ll_reset_register
#undef uart_ll_set_sclk
#undef uart_ll_set_baudrate
#endif

static void uart_ll_enable_bus_clock_raw(uart_port_t port_num, bool enable)
{
#if defined(TARGET_SOC_ESP32P4)
    _uart_ll_enable_bus_clock(port_num, enable);
#else
    uart_ll_enable_bus_clock(port_num, enable);
#endif
}

static void uart_ll_reset_register_raw(uart_port_t port_num)
{
    uart_ll_reset_register(port_num);
}

static void uart_ll_sclk_enable_raw(uart_dev_t *port)
{
#if defined(TARGET_SOC_ESP32P4)
    _uart_ll_sclk_enable(port);
#else
    uart_ll_sclk_enable(port);
#endif
}

static void uart_ll_set_sclk_raw(uart_dev_t *port, uart_sclk_t sclk)
{
    uart_ll_set_sclk(port, sclk);
}

static bool uart_ll_set_baudrate_raw(uart_dev_t *port, uint32_t baud_rate, uint32_t clock_hz)
{
    return uart_ll_set_baudrate(port, baud_rate, clock_hz);
}

static int uart_get_hp_port_num(uart_dev_t *port)
{
    if (port == &UART0)
    {
        return UART_NUM_0;
    }

#if SOC_UART_HP_NUM > 1
    if (port == &UART1)
    {
        return UART_NUM_1;
    }
#endif

#if SOC_UART_HP_NUM > 2
    if (port == &UART2)
    {
        return UART_NUM_2;
    }
#endif

#if SOC_UART_HP_NUM > 3
    if (port == &UART3)
    {
        return UART_NUM_3;
    }
#endif

#if SOC_UART_HP_NUM > 4
    if (port == &UART4)
    {
        return UART_NUM_4;
    }
#endif

    return -1;
}

static bool uart_resolve_pin(int port_num, int32_t requested_pin, uint8_t pin_idx, int32_t *resolved_pin)
{
    if ((resolved_pin == NULL) || (port_num < 0) || (port_num >= SOC_UART_HP_NUM))
    {
        return false;
    }

    if (requested_pin == UART_PIN_UNUSED)
    {
        *resolved_pin = UART_PIN_UNUSED;
        return true;
    }

    if (requested_pin == UART_PIN_DEFAULT)
    {
        requested_pin = uart_periph_signal[port_num].pins[pin_idx].default_gpio;
    }

    if ((requested_pin < 0) || (requested_pin >= SOC_GPIO_PIN_COUNT))
    {
        return false;
    }

    *resolved_pin = requested_pin;
    return true;
}

static void uart_gpio_output_config(int32_t gpio_num, uint32_t signal_idx)
{
    if (gpio_num == UART_PIN_UNUSED)
    {
        return;
    }

    gpio_ll_func_sel(&GPIO, (uint8_t)gpio_num, PIN_FUNC_GPIO);
    GPIO.func_out_sel_cfg[gpio_num].out_sel = signal_idx;
    gpio_ll_output_enable(&GPIO, (uint32_t)gpio_num);
    gpio_ll_input_disable(&GPIO, (uint32_t)gpio_num);
}

static void uart_gpio_input_config(int32_t gpio_num, uint32_t signal_idx)
{
    if (gpio_num == UART_PIN_UNUSED)
    {
        return;
    }

    gpio_ll_func_sel(&GPIO, (uint8_t)gpio_num, PIN_FUNC_GPIO);
    gpio_ll_input_enable(&GPIO, (uint32_t)gpio_num);
    GPIO.func_in_sel_cfg[signal_idx].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[signal_idx].in_sel = gpio_num;
}

static uint32_t uart_get_clock_hz(void)
{
    return XTAL_CLK_FREQ;
}

bool uart_init(uart_dev_handle_t *dev, const uart_config_t *config)
{
    if ((dev == NULL) || (config == NULL) || (dev->port == NULL) || (config->baud_rate == 0U))
    {
        return false;
    }

    int port_num = uart_get_hp_port_num(dev->port);
    if (port_num < 0)
    {
        return false;
    }

    int32_t tx_pin = UART_PIN_UNUSED;
    int32_t rx_pin = UART_PIN_UNUSED;
    int32_t rts_pin = UART_PIN_UNUSED;
    int32_t cts_pin = UART_PIN_UNUSED;

    if (!uart_resolve_pin(port_num, config->tx_pin, SOC_UART_TX_PIN_IDX, &tx_pin) ||
        !uart_resolve_pin(port_num, config->rx_pin, SOC_UART_RX_PIN_IDX, &rx_pin) ||
        !uart_resolve_pin(port_num, config->rts_pin, SOC_UART_RTS_PIN_IDX, &rts_pin) ||
        !uart_resolve_pin(port_num, config->cts_pin, SOC_UART_CTS_PIN_IDX, &cts_pin))
    {
        return false;
    }

    if (((config->flow_control & UART_HW_FLOWCTRL_RTS) != 0U) && (rts_pin == UART_PIN_UNUSED))
    {
        return false;
    }

    if (((config->flow_control & UART_HW_FLOWCTRL_CTS) != 0U) && (cts_pin == UART_PIN_UNUSED))
    {
        return false;
    }

    uart_ll_enable_bus_clock_raw((uart_port_t)port_num, true);
    uart_ll_reset_register_raw((uart_port_t)port_num);
    uart_ll_sclk_enable_raw(dev->port);

    uart_ll_disable_intr_mask(dev->port, UART_LL_INTR_MASK);
    uart_ll_clr_intsts_mask(dev->port, UART_LL_INTR_MASK);
    uart_ll_txfifo_rst(dev->port);
    uart_ll_rxfifo_rst(dev->port);

    uart_ll_set_mode(dev->port, UART_MODE_UART);
    uart_ll_set_sclk_raw(dev->port, UART_SCLK_XTAL);
    if (!uart_ll_set_baudrate_raw(dev->port, config->baud_rate, uart_get_clock_hz()))
    {
        return false;
    }
    uart_ll_set_data_bit_num(dev->port, config->data_bits);
    uart_ll_set_stop_bits(dev->port, config->stop_bits);
    uart_ll_set_parity(dev->port, config->parity);

    uint8_t rx_flow_ctrl_thresh = config->rx_flow_ctrl_thresh;
    if ((((unsigned int)config->flow_control & (unsigned int)UART_HW_FLOWCTRL_RTS) != 0U) &&
        (rx_flow_ctrl_thresh == 0U))
    {
        rx_flow_ctrl_thresh = UART_LL_FIFO_DEF_LEN / 2U;
    }
    uart_ll_set_hw_flow_ctrl(dev->port, config->flow_control, rx_flow_ctrl_thresh);

    uart_gpio_output_config(tx_pin, UART_PERIPH_SIGNAL(port_num, SOC_UART_TX_PIN_IDX));
    uart_gpio_input_config(rx_pin, UART_PERIPH_SIGNAL(port_num, SOC_UART_RX_PIN_IDX));
    uart_gpio_output_config(rts_pin, UART_PERIPH_SIGNAL(port_num, SOC_UART_RTS_PIN_IDX));
    uart_gpio_input_config(cts_pin, UART_PERIPH_SIGNAL(port_num, SOC_UART_CTS_PIN_IDX));

    dev->port_num = (uint8_t)port_num;
    dev->initialized = true;

    return true;
}

size_t uart_write(uart_dev_handle_t *dev, const void *data, size_t len)
{
    if ((dev == NULL) || !dev->initialized || (dev->port == NULL) || (data == NULL) || (len == 0U))
    {
        return 0U;
    }

    const uint8_t *buf = (const uint8_t *)data;
    size_t written = 0U;

    while (written < len)
    {
        uint32_t fifo_space = uart_ll_get_txfifo_len(dev->port);
        if (fifo_space == 0U)
        {
            continue;
        }

        size_t chunk = len - written;
        if (chunk > fifo_space)
        {
            chunk = fifo_space;
        }

        uart_ll_write_txfifo(dev->port, &buf[written], (uint32_t)chunk);
        written += chunk;
    }

    while (!uart_ll_is_tx_idle(dev->port))
    {
    }

    return written;
}

size_t uart_read(uart_dev_handle_t *dev, void *data, size_t len)
{
    if ((dev == NULL) || !dev->initialized || (dev->port == NULL) || (data == NULL) || (len == 0U))
    {
        return 0U;
    }

    uint32_t available = uart_ll_get_rxfifo_len(dev->port);
    if (available == 0U)
    {
        return 0U;
    }

    if (available > len)
    {
        available = (uint32_t)len;
    }

    uart_ll_read_rxfifo(dev->port, (uint8_t *)data, available);
    return available;
}

bool uart_read_byte(uart_dev_handle_t *dev, uint8_t *byte, uint32_t timeout_us)
{
    if ((dev == NULL) || !dev->initialized || (dev->port == NULL) || (byte == NULL))
    {
        return false;
    }

    while (uart_ll_get_rxfifo_len(dev->port) == 0U)
    {
        if (timeout_us == 0U)
        {
            return false;
        }

        delay_us(1);
        timeout_us--;
    }

    uart_ll_read_rxfifo(dev->port, byte, 1U);
    return true;
}

void uart_flush_rx(uart_dev_handle_t *dev)
{
    if ((dev == NULL) || !dev->initialized || (dev->port == NULL))
    {
        return;
    }

    uart_ll_rxfifo_rst(dev->port);
}
