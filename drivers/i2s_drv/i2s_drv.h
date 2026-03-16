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
#include "hal/i2s_ll.h"
#include "hal/i2s_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define I2S_PIN_UNUSED (0xFFU)

#define I2S_GET_HW(num) I2S_LL_GET_HW(num)
#define I2S_GET_DEV_HANDLE(num) \
    {                           \
        .port = I2S_GET_HW(num) \
    }

typedef struct
{
    uint8_t mclk;
    uint8_t bclk;
    uint8_t ws;
    uint8_t dout;
    uint8_t din;
} i2s_pins_t;

typedef struct
{
    i2s_dev_t *port;
    i2s_pins_t pins;

    i2s_role_t role;
    i2s_slot_mode_t slot_mode;

    uint32_t sample_rate_hz;
    uint32_t mclk_multiple;
    uint8_t data_bit_width;
    uint8_t slot_bit_width;

    i2s_clock_src_t clk_src;
    uint8_t dma_channel;
} i2s_config_t;

typedef struct
{
    i2s_dev_t *port;
    uint8_t port_id;
    uint8_t dma_channel;

    i2s_role_t role;
    i2s_slot_mode_t slot_mode;

    uint32_t sample_rate_hz;
    uint32_t mclk_hz;
    uint32_t bclk_hz;
    uint8_t data_bit_width;
    uint8_t slot_bit_width;

    bool stream_mode;
    bool stream_started;
    bool stream_error;
    uint8_t stream_buf_count;
    uint16_t stream_buf_size;
    uint8_t stream_head;
    uint8_t stream_tail;
    uint8_t stream_queued;

    bool initialized;
} i2s_dev_handle_t;

/**
 * @brief Initialize I2S in standard mode (TX path).
 *
 * @note This driver currently supports TX transfers via DMA.
 */
bool i2s_init(i2s_dev_handle_t *dev, const i2s_config_t *config);

/**
 * @brief Deinitialize I2S TX path and disconnect DMA channel.
 */
void i2s_deinit(i2s_dev_handle_t *dev);

/**
 * @brief Blocking DMA write.
 *
 * @param dev Initialized I2S handle.
 * @param data Source buffer.
 * @param len_bytes Number of bytes to transmit.
 * @param timeout_us Timeout per DMA chunk.
 * @return true on success, false on timeout or invalid args.
 */
bool i2s_write(i2s_dev_handle_t *dev, const void *data, size_t len_bytes, uint32_t timeout_us);

/**
 * @brief Enable continuous TX streaming with an internal DMA buffer queue.
 *
 * @param dev Initialized I2S handle.
 * @param buffer_count Number of DMA buffers to use.
 * @param buffer_size_bytes Size of each DMA buffer.
 * @return true on success, false on invalid args.
 */
bool i2s_stream_begin(i2s_dev_handle_t *dev, uint8_t buffer_count, size_t buffer_size_bytes);

/**
 * @brief Poll the DMA queue and reclaim completed TX buffers.
 */
void i2s_stream_poll(i2s_dev_handle_t *dev);

/**
 * @brief Queue PCM data into the TX stream.
 *
 * @return Number of bytes queued.
 */
size_t i2s_stream_write(i2s_dev_handle_t *dev, const void *data, size_t len_bytes, uint32_t timeout_us);

/**
 * @brief Number of free bytes currently available in the TX stream queue.
 */
size_t i2s_stream_get_free_bytes(const i2s_dev_handle_t *dev);

/**
 * @brief Wait for all queued TX buffers to finish.
 */
bool i2s_stream_drain(i2s_dev_handle_t *dev, uint32_t timeout_us);

/**
 * @brief Stop streaming and release all queued TX buffers.
 */
void i2s_stream_stop(i2s_dev_handle_t *dev);

#ifdef __cplusplus
}
#endif
