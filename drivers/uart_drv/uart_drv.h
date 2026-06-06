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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hal/uart_ll.h"
#include "hal/uart_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UART_GET_HW(num) UART_LL_GET_HW(num)
#define UART_GET_DEV_HANDLE(num) \
    {                            \
        .port = UART_GET_HW(num),\
        .port_num = (uint8_t)(num)\
    }

#define UART_PIN_UNUSED (-1)
#define UART_PIN_DEFAULT (-2)

#define UART_DEFAULT_CONFIG()                 \
    {                                         \
        .tx_pin = UART_PIN_DEFAULT,           \
        .rx_pin = UART_PIN_DEFAULT,           \
        .rts_pin = UART_PIN_UNUSED,           \
        .cts_pin = UART_PIN_UNUSED,           \
        .baud_rate = 115200U,                 \
        .data_bits = UART_DATA_8_BITS,        \
        .stop_bits = UART_STOP_BITS_1,        \
        .parity = UART_PARITY_DISABLE,        \
        .flow_control = UART_HW_FLOWCTRL_DISABLE, \
        .rx_flow_ctrl_thresh = 0U,            \
    }

typedef struct
{
    int32_t tx_pin;
    int32_t rx_pin;
    int32_t rts_pin;
    int32_t cts_pin;
    uint32_t baud_rate;
    uart_word_length_t data_bits;
    uart_stop_bits_t stop_bits;
    uart_parity_t parity;
    uart_hw_flowcontrol_t flow_control;
    uint8_t rx_flow_ctrl_thresh;
} uart_config_t;

typedef struct
{
    uart_dev_t *port;
    uint8_t port_num;
    bool initialized;
} uart_dev_handle_t;

/**
 * @brief Initialize a high-power UART peripheral in polling mode.
 *
 * Pins can be set to `UART_PIN_DEFAULT` to use the SoC default routing, or to
 * `UART_PIN_UNUSED` to skip that signal.
 */
bool uart_init(uart_dev_handle_t *dev, const uart_config_t *config);

/**
 * @brief Blocking write into the UART TX FIFO and wait for the transmitter to go idle.
 *
 * @return Number of bytes written.
 */
size_t uart_write(uart_dev_handle_t *dev, const void *data, size_t len);

/**
 * @brief Read up to `len` bytes already present in the RX FIFO.
 *
 * @return Number of bytes read.
 */
size_t uart_read(uart_dev_handle_t *dev, void *data, size_t len);

/**
 * @brief Poll until one byte is available or the timeout expires.
 */
bool uart_read_byte(uart_dev_handle_t *dev, uint8_t *byte, uint32_t timeout_us);

/**
 * @brief Discard all pending RX FIFO data.
 */
void uart_flush_rx(uart_dev_handle_t *dev);

#ifdef __cplusplus
}
#endif
