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

#ifndef __SD_H
#define __SD_H

#include <stdint.h>
#include <stdbool.h>
#include "sd_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CMD0 0
#define CMD0_ARG 0x00000000
#define CMD0_CRC 0x94

// SEND IF_COND
#define CMD8 8
#define CMD8_ARG 0x000001AA
#define CMD8_CRC 0x86 //(1000011 << 1)

// READ CSD
#define CMD9 9
#define CMD9_ARG 0x00000000
#define CMD9_CRC 0x00

// Read OCR
#define CMD58 58
#define CMD58_ARG 0x00000000
#define CMD58_CRC 0x00

#define CMD55 55
#define CMD55_ARG 0x00000000
#define CMD55_CRC 0x00

#define ACMD41 41
#define ACMD41_ARG 0x40000000
#define ACMD41_CRC 0x00

// Read Single Block
#define CMD17 17
#define CMD17_CRC 0x95
#define SD_READ_TIMEOUT 250

// Write Single Block
#define CMD24 24
#define CMD24_CRC 0x00
#define SD_WRITE_TIMEOUT 100

// Read Multiple Block
#define CMD18 18
#define CMD18_CRC 0x00

// STOP_MULTIPLE_READ
#define CMD12 12
#define CMD12_ARG 0x00000000
#define CMD12_CRC 0x00

// Write Multiple Block
#define CMD25 25
#define CMD25_CRC 0x00

#define PARAM_ERROR(X) X & 0b01000000
#define ADDR_ERROR(X) X & 0b00100000
#define ERASE_SEQ_ERROR(X) X & 0b00010000
#define CRC_ERROR(X) X & 0b00001000
#define ILLEGAL_CMD(X) X & 0b00000100
#define ERASE_RESET(X) X & 0b00000010
#define IN_IDLE(X) X & 0b00000001

#define CMD_VER(X) ((X >> 4) & 0x0F)
#define VOL_ACC(X) (X & 0x1F)

#define VOLTAGE_ACC_27_33 0b00000001
#define VOLTAGE_ACC_LOW 0b00000010
#define VOLTAGE_ACC_RES1 0b00000100
#define VOLTAGE_ACC_RES2 0b00001000

#define POWER_UP_STATUS(X) X & 0x80
#define CCS_VAL(X) X & 0x40
#define VDD_2728(X) X & 0b10000000
#define VDD_2829(X) X & 0b00000001
#define VDD_2930(X) X & 0b00000010
#define VDD_3031(X) X & 0b00000100
#define VDD_3132(X) X & 0b00001000
#define VDD_3233(X) X & 0b00010000
#define VDD_3334(X) X & 0b00100000
#define VDD_3435(X) X & 0b01000000
#define VDD_3536(X) X & 0b10000000

#define SD_TOKEN_OOR(X) X & 0b00001000
#define SD_TOKEN_CECC(X) X & 0b00000100
#define SD_TOKEN_CC(X) X & 0b00000010
#define SD_TOKEN_ERROR(X) X & 0b00000001

#define SD_START_TOKEN 0xFE
#define SD_BLOCK_LEN 512

    typedef enum
    {
        SD_READY,
        SD_INIT_SUCCESS,
        SD_INIT_ERROR,
        SD_READ_SUCCESS,
        SD_READ_ERROR,
        SD_WRITE_SUCCESS,
        SD_WRITE_ERROR
    } sd_ret_t;

    sd_ret_t sd_init();

    sd_ret_t sd_write_sector(uint32_t addr, uint8_t *buf);

    sd_ret_t sd_read_sector(uint32_t addr, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif
