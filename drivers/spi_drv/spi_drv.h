#pragma once

#include <stdbool.h>
#include "hal/spi_ll.h"

#define SPI_CONCAT(a, b) a##b

#define SPI_GET_HW(num) SPI_CONCAT(&GPSPI, num)

#define SPI_GET_DEV_HANDLE(num) {.port = SPI_GET_HW(num)}
#define SPI_CS_UNUSED 0xFFU

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
    int8_t id;
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
 * @param hold_cs_low Keep CS asserted after transfer completion when true
 */
void spi_transceive(spi_dev_handle_t *dev, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len, bool hold_cs_low);

uint8_t spi_transfer_byte(spi_dev_handle_t *dev, uint8_t tx_byte, bool hold_cs_low);

/**
 * @brief Generate SPI clock cycles in dummy phase.
 *
 * @param dev SPI device
 * @param cycles Number of SCK cycles to generate
 * @param cs_low Generate clocks with device CS asserted (true) or deasserted (false)
 * @param hold_cs_low Keep CS asserted after dummy clocks when cs_low is true
 */
void spi_send_dummy_clocks(spi_dev_handle_t *dev, uint32_t cycles, bool cs_low, bool hold_cs_low);
