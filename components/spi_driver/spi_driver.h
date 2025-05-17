#pragma once

#include "hal/spi_ll.h"

typedef struct
{
    int8_t sck;
    int8_t miso;
    int8_t mosi;
} spi_pins_t;

typedef enum
{
    SPI_DEVICE_0 = 16,
    SPI_DEVICE_1,
    SPI_DEVICE_2,
    SPI_DEVICE_3,
    SPI_DEVICE_4,
    SPI_DEVICE_5,
    SPI_DEVICE_6
} spi_device_id_t;

typedef struct
{
    uint32_t speed_hz;
    spi_ll_clock_val_t clk_reg_val;
    int8_t cs_pin;
    uint8_t id;
    uint8_t mode;
} spi_device_handle_t;

#define SPI_DEFAULT_PINS \
    {                    \
        .sck = -1,       \
        .miso = -1,      \
        .mosi = -1,      \
    }

void spi_device_config(spi_device_handle_t *dev);

void spi_driver_init(spi_pins_t pins);

/**
 * @brief Transmit and receive spi data
 *
 * @param tx_buf
 * @param rx_buf
 * @param len
 */
void spi_transceive(spi_device_handle_t *dev, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len);

uint8_t spi_transfer_byte(spi_device_handle_t *dev, uint8_t tx_byte);
