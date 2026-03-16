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

#ifndef __SDMMC_H
#define __SDMMC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SDMMC_SECTOR_SIZE 512
/*
 * Internal pull-ups for CMD/DAT lines.
 * Keep disabled when the board already provides external pull-ups (recommended).
 */
#ifndef SDMMC_ENABLE_INTERNAL_PULLUPS
#define SDMMC_ENABLE_INTERNAL_PULLUPS 0
#endif

typedef enum
{
    SDMMC_OK = 0,
    SDMMC_ERR_INVALID_ARG = -1,
    SDMMC_ERR_TIMEOUT = -2,
    SDMMC_ERR_PROTOCOL = -3,
    SDMMC_ERR_IO = -4,
    SDMMC_ERR_NOT_READY = -5,
} sdmmc_err_t;


/**
 * @brief Initialize SD card on ESP32-P4 SDMMC slot0 (polling mode, DMA, no interrupts).
 *
 * @return SDMMC_OK on success, negative error code on failure.
 */
sdmmc_err_t sdmmc_init(void);

/**
 * @brief Read one 512-byte sector using DMA polling mode.
 *
 * @param lba Sector LBA.
 * @param out_data Destination buffer of at least 512 bytes.
 * @return SDMMC_OK on success, negative error code on failure.
 */
sdmmc_err_t sdmmc_read_sector(uint32_t lba, uint8_t *out_data);

/**
 * @brief Write one 512-byte sector using DMA polling mode.
 *
 * @param lba Sector LBA.
 * @param data Source buffer of 512 bytes.
 * @return SDMMC_OK on success, negative error code on failure.
 */
sdmmc_err_t sdmmc_write_sector(uint32_t lba, const uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif
