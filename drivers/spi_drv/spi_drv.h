#pragma once

#include "hal/spi_ll.h"

#define SPI_CONCAT(a, b) a##b

#define SPI_GET_HW(num) SPI_CONCAT(&GPSPI, num)

#define SPI_GET_DEV_HANDLE(num) {.port = SPI_GET_HW(num)}

typedef struct
{
    uint8_t sck;
    uint8_t miso;
    uint8_t mosi;
} spi_pins_t;

typedef struct
{
    spi_dev_t *port;
    spi_pins_t pins;
} spi_config_t;

typedef struct
{
    spi_dev_t *port;
    uint32_t speed_hz;
    spi_ll_clock_val_t clk_reg_val;
    uint8_t cs_pin;
    uint8_t id;
    uint8_t mode;
} spi_dev_handle_t;

void spi_device_config(spi_dev_handle_t *dev);

void spi_init(spi_config_t *config);

/**
 * @brief Transmit and receive spi data
 *
 * @param tx_buf
 * @param rx_buf
 * @param len
 */
void spi_transceive(spi_dev_handle_t *dev, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len);

uint8_t spi_transfer_byte(spi_dev_handle_t *dev, uint8_t tx_byte);
