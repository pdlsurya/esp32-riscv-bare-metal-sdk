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
#include "sd_spi.h"

#define SD_SPI_XFER_CS_HOLD(tx_byte) sd_spi_transfer(tx_byte, true)

#define SD_SPI_XFER_CS_RELEASE(tx_byte) sd_spi_transfer(tx_byte, false)

static void sd_powerup_sequence()
{
    // give SD card time to power up
    sd_delay_ms(10);

    // send 80 clock cycles to synchronize
    sd_spi_dummy_clocks(80);

    // deselect SD card
    sd_spi_dummy_clocks(8);
}

static void sd_send_command(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    // send command to sd card
    SD_SPI_XFER_CS_HOLD(cmd | 0x40);

    // send argument
    SD_SPI_XFER_CS_HOLD((uint8_t)(arg >> 24));
    SD_SPI_XFER_CS_HOLD((uint8_t)(arg >> 16));
    SD_SPI_XFER_CS_HOLD((uint8_t)(arg >> 8));
    SD_SPI_XFER_CS_HOLD((uint8_t)(arg));

    // send crc
    SD_SPI_XFER_CS_HOLD(crc | 0x01);
}

static uint8_t sd_read_res1()
{
    uint8_t i = 0, res1;

    // keep polling until actual data received
    while ((res1 = SD_SPI_XFER_CS_HOLD(0xFF)) == 0xFF)
    {
        i++;

        // if no data received for 8 bytes, break
        if (i > 8)
        {
            break;
        }
    }

    return res1;
}

static uint8_t sd_goto_idle_state()
{
    // assert chip select
    sd_spi_dummy_clocks(8);
    SD_SPI_XFER_CS_HOLD(0xFF);

    // send CMD0
    sd_send_command(CMD0, CMD0_ARG, CMD0_CRC);

    // read response
    uint8_t res1 = sd_read_res1();

    // deassert chip select
    SD_SPI_XFER_CS_RELEASE(0xFF);
    sd_spi_dummy_clocks(8);

    return res1;
}

static void sd_read_res3_7(uint8_t *res)
{
    // read response 1 in R7
    res[0] = sd_read_res1();

    // if error reading R1, return
    if (res[0] > 1)
    {
        return;
    }

    // read remaining bytes
    res[1] = SD_SPI_XFER_CS_HOLD(0xFF);
    res[2] = SD_SPI_XFER_CS_HOLD(0xFF);
    res[3] = SD_SPI_XFER_CS_HOLD(0xFF);
    res[4] = SD_SPI_XFER_CS_HOLD(0xFF);
}

static void sd_send_if_cond_cmd(uint8_t *res)
{
    // assert chip select
    sd_spi_dummy_clocks(8);
    SD_SPI_XFER_CS_HOLD(0xFF);

    // send CMD8
    sd_send_command(CMD8, CMD8_ARG, CMD8_CRC);

    // read response
    sd_read_res3_7(res);

    // deassert chip select
    SD_SPI_XFER_CS_RELEASE(0xFF);
    sd_spi_dummy_clocks(8);
}

static void sd_read_ocr(uint8_t *res)
{
    // assert chip select
    sd_spi_dummy_clocks(8);
    SD_SPI_XFER_CS_HOLD(0xFF);

    // send CMD58
    sd_send_command(CMD58, CMD58_ARG, CMD58_CRC);

    // read response
    sd_read_res3_7(res);

    // deassert chip select
    SD_SPI_XFER_CS_RELEASE(0xFF);
    sd_spi_dummy_clocks(8);
}

static uint8_t sd_send_app_cmd()
{
    // assert chip select
    sd_spi_dummy_clocks(8);
    SD_SPI_XFER_CS_HOLD(0xFF);

    // send CMD0
    sd_send_command(CMD55, CMD55_ARG, CMD55_CRC);

    // read response
    uint8_t res1 = sd_read_res1();

    // deassert chip select
    SD_SPI_XFER_CS_RELEASE(0xFF);
    sd_spi_dummy_clocks(8);

    return res1;
}

static uint8_t sd_send_op_cond_cmd()
{
    // assert chip select
    sd_spi_dummy_clocks(8);
    SD_SPI_XFER_CS_HOLD(0xFF);

    // send CMD0
    sd_send_command(ACMD41, ACMD41_ARG, ACMD41_CRC);

    // read response
    uint8_t res1 = sd_read_res1();

    // deassert chip select
    SD_SPI_XFER_CS_RELEASE(0xFF);
    sd_spi_dummy_clocks(8);

    return res1;
}

static void sd_print_res1(uint8_t res)
{
    if (res & 0b10000000)
    {
        PRINTF("\tError: MSB = 1\r\n");
        return;
    }
    if (res == 0)
    {
        PRINTF("\t Card Ready \r\n");
        return;
    }
    if (PARAM_ERROR(res))
    {
        PRINTF("\tParameter Error\r\n");
    }
    if (ADDR_ERROR(res))
    {
        PRINTF("\tAddress Error\r\n");
    }
    if (ERASE_SEQ_ERROR(res))
    {
        PRINTF("\tErase Seq Error\r\n");
    }
    if (CRC_ERROR(res))
    {
        PRINTF("\tCRC Error\r\n");
    }
    if (ILLEGAL_CMD(res))
    {
        PRINTF("\tIllegal Cmd\r\n");
    }
    if (ERASE_RESET(res))
    {
        PRINTF("\tErase Rst Error\r\n");
    }
    if (IN_IDLE(res))
    {
        PRINTF("Idle State\r\n");
    }
}

static void sd_print_res7(uint8_t *res)
{
    sd_print_res1(res[0]);

    if (res[0] > 1)
    {
        return;
    }

    PRINTF("Command Version: %X", CMD_VER(res[1]));

    PRINTF("\tVoltage Accepted: ");
    if (VOL_ACC(res[3]) == VOLTAGE_ACC_27_33)
    {
        PRINTF("2.7-3.6V\r\n");
    }
    else if (VOL_ACC(res[3]) == VOLTAGE_ACC_LOW)
    {
        PRINTF("LOW VOLTAGE\r\n");
    }
    else if (VOL_ACC(res[3]) == VOLTAGE_ACC_RES1)
    {
        PRINTF("RESERVED\r\n");
    }
    else if (VOL_ACC(res[3]) == VOLTAGE_ACC_RES2)
    {
        PRINTF("RESERVED\r\n");
    }
    else
    {
        PRINTF("NOT DEFINED\n");
    }

    PRINTF("\tEcho: %x", res[4]);
}

static void sd_print_res3(uint8_t *res)
{
    sd_print_res1(res[0]);

    if (res[0] > 1)
    {
        return;
    }

    PRINTF("\tCard Power Up Status: ");
    if (POWER_UP_STATUS(res[1]))
    {
        PRINTF("READY\r\n");
        PRINTF("\tCCS Status: ");
        if (CCS_VAL(res[1]))
        {
            PRINTF("1\r\n");
        }
        else
        {
            PRINTF("0\r\n");
        }
    }
    else
    {
        PRINTF("BUSY\r\n");
    }

    PRINTF("\tVDD Window: ");
    if (VDD_2728(res[3]))
    {
        PRINTF("2.7-2.8, ");
    }
    if (VDD_2829(res[2]))
    {
        PRINTF("2.8-2.9, ");
    }
    if (VDD_2930(res[2]))
    {
        PRINTF("2.9-3.0, ");
    }
    if (VDD_3031(res[2]))
    {
        PRINTF("3.0-3.1, ");
    }
    if (VDD_3132(res[2]))
    {
        PRINTF("3.1-3.2, ");
    }
    if (VDD_3233(res[2]))
    {
        PRINTF("3.2-3.3, ");
    }
    if (VDD_3334(res[2]))
    {
        PRINTF("3.3-3.4, ");
    }
    if (VDD_3435(res[2]))
    {
        PRINTF("3.4-3.5, ");
    }
    if (VDD_3536(res[2]))
    {
        PRINTF("3.5-3.6");
    }
    PRINTF("\r\n");
}

static void sd_print_data_err_token(uint8_t token)
{
    if (SD_TOKEN_OOR(token))
    {
        PRINTF("\tData out of range\r\n");
    }
    if (SD_TOKEN_CECC(token))
    {
        PRINTF("\tCard ECC failed\r\n");
    }
    if (SD_TOKEN_CC(token))
    {
        PRINTF("\tCC Error\r\n");
    }
    if (SD_TOKEN_ERROR(token))
    {
        PRINTF("\tError\r\n");
    }
}

static sd_ret_t sd_read_start(uint8_t *buf, uint16_t read_len, uint8_t *token)
{
    uint8_t res1, read;
    uint16_t timeout;

    // read R1
    res1 = sd_read_res1();

    // if response received from card
    if (res1 == SD_READY)
    {
        // wait for a response token (timeout = 100ms)
        timeout = SD_READ_TIMEOUT;

        while ((read = SD_SPI_XFER_CS_HOLD(0xFF)) != 0xFE)
        {

            timeout--;
            if (timeout == 0)
            {
                break;
            }
            sd_delay_ms(1);
        }

        // if response token is 0xFE
        if (read == 0xFE)
        {
            // read 512 byte block
            for (uint16_t i = 0; i < read_len; i++)
            {
                *buf++ = SD_SPI_XFER_CS_HOLD(0xFF);
            }

            // read 16-bit CRC
            SD_SPI_XFER_CS_HOLD(0xFF);
            SD_SPI_XFER_CS_HOLD(0xFF);
        }

        // set token to card response
        *token = read;
    }

    return res1;
}

sd_ret_t sd_read_sector(uint32_t addr, uint8_t *buf)
{
    uint8_t res1, token;

    token = 0xFF;

    // assert chip select
    sd_spi_dummy_clocks(8);
    SD_SPI_XFER_CS_HOLD(0xFF);

    // send CMD17
    sd_send_command(CMD17, addr, CMD17_CRC);

    res1 = sd_read_start(buf, SD_BLOCK_LEN, &token);

    // deassert chip select
    SD_SPI_XFER_CS_RELEASE(0xFF);
    sd_spi_dummy_clocks(8);

    if (res1 == SD_READY)
    {
        // if error token received
        if (!(token & 0xF0))
        {
            sd_print_data_err_token(token);
            return SD_READ_ERROR;
        }
        else if (token == 0xFF)
        {
            PRINTF("Read Timeout\r\n");
            return SD_READ_ERROR;
        }
        return SD_READ_SUCCESS;
    }
    else
    {
        sd_print_res1(res1);
        return SD_READ_ERROR;
    }
}

sd_ret_t sd_write_sector(uint32_t addr, uint8_t *buf)
{
    // set token to none
    uint8_t token = 0xFF;
    uint8_t res1, timeout, read;

    // assert chip select
    sd_spi_dummy_clocks(8);
    SD_SPI_XFER_CS_HOLD(0xFF);

    // send CMD24
    sd_send_command(CMD24, addr, CMD24_CRC);

    // read response
    res1 = sd_read_res1();

    // if no error
    if (res1 == SD_READY)
    {
        // send start token
        SD_SPI_XFER_CS_HOLD(SD_START_TOKEN);

        // write buffer to card
        for (uint16_t i = 0; i < SD_BLOCK_LEN; i++)
        {
            SD_SPI_XFER_CS_HOLD(buf[i]);
        }
        // wait for a response (timeout = 250ms)
        timeout = SD_WRITE_TIMEOUT;

        while (timeout--)
        {
            if ((read = SD_SPI_XFER_CS_HOLD(0xFF)) != 0xFF)
            {
                break;
            }
            sd_delay_ms(1);
        }
        // if data accepted
        if ((read & 0x1F) == 0x05)
        {
            // set token to data accepted
            token = 0x05;

            // wait for write to finish (timeout = 250ms)
            timeout = SD_WRITE_TIMEOUT;
            while (SD_SPI_XFER_CS_HOLD(0xFF) == 0x00)
            {
                timeout--;
                if (timeout == 0)
                {
                    token = 0x00;
                    break;
                }
                sd_delay_ms(1);
            }
        }
    }
    // deassert chip select
    SD_SPI_XFER_CS_RELEASE(0xFF);
    sd_spi_dummy_clocks(8);

    if (res1 == SD_READY)
    {
        if (token == 0x05)
        {
            return SD_WRITE_SUCCESS;
        }

        else if (token == 0xFF || token == 0x00)
        {
            return SD_WRITE_ERROR;
        }
    }

    sd_print_res1(res1);

    return SD_WRITE_ERROR;
}

sd_ret_t sd_init()
{

    sd_spi_init();

    uint8_t res[5], cmd_attempts = 0;

    sd_powerup_sequence();

    // command card to idle
    while ((res[0] = sd_goto_idle_state()) != 0x01)
    {
        cmd_attempts++;
        if (cmd_attempts > 100)
        {
            if (res[0] == 0)
            {
                PRINTF("Card Not Found!\r\n");
            }

            sd_print_res1(res[0]);
            return SD_INIT_ERROR;
        }
    }

    // send interface conditions
    sd_send_if_cond_cmd(res);
    if (res[0] != 0x01)
    {
        sd_print_res1(res[0]);
        return SD_INIT_ERROR;
    }

    // check echo pattern
    if (res[4] != 0xAA)
    {
        sd_print_res7(res);
        return SD_INIT_ERROR;
    }

    // attempt to initialize card
    cmd_attempts = 0;
    do
    {
        if (cmd_attempts > 100)
        {
            return SD_INIT_ERROR;
        }

        // send app cmd
        res[0] = sd_send_app_cmd();

        // if no error in response
        if (res[0] < 2)
        {
            res[0] = sd_send_op_cond_cmd();
        }

        // wait
        sd_delay_ms(10);

        cmd_attempts++;
    } while (res[0] != SD_READY);

    // read OCR
    sd_read_ocr(res);
    // check card is ready
    if (!(res[1] & 0x80))
    {
        sd_print_res3(res);
        return SD_INIT_ERROR;
    }

    return SD_INIT_SUCCESS;
}
