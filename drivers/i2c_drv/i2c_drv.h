#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    I2C_SPEED_100K = 100000,
    I2C_SPEED_400K = 400000
} i2c_speed_t;

typedef struct
{
    uint8_t sda_pin;
    uint8_t scl_pin;
    i2c_speed_t speed;
} i2c_config_t;

void i2c_init(i2c_config_t *config);

bool i2c_write(uint8_t addr, uint8_t *data, uint8_t len);

bool i2c_read(uint8_t addr, uint8_t *data, uint8_t len);

bool i2c_read_register(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len);
