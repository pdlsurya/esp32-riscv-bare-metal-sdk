#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hal/i2c_ll.h"

#define I2C_CONCAT(a, b) a##b

#define I2C_GET_HW(num) I2C_CONCAT(&I2C, num)

#define I2C_GET_DEV_HANDLE(num, address) {.port = I2C_GET_HW(num), .slave_address = address}

typedef enum
{
    I2C_SPEED_100K = 100000,
    I2C_SPEED_400K = 400000
} i2c_speed_t;

typedef struct
{
    i2c_dev_t *port;
    uint8_t slave_address;

} i2c_dev_handle_t;

typedef struct
{
    i2c_dev_t *port;
    i2c_speed_t speed;
    uint8_t sda_pin;
    uint8_t scl_pin;
} i2c_config_t;

void i2c_init(i2c_config_t *config);

bool i2c_write(i2c_dev_handle_t *dev, uint8_t *data, uint8_t len);

bool i2c_read(i2c_dev_handle_t *dev, uint8_t *data, uint8_t len);

bool i2c_read_register(i2c_dev_handle_t *dev, uint8_t reg, uint8_t *data, uint8_t len);
