#include <stdint.h>
#include <stdbool.h>
#include "hal/i2c_ll.h"
#include "hal/gpio_ll.h"
#include "hal/pcnt_ll.h"
#include "i2c_drv.h"
#include "usb_serial.h"
#include "delay.h"

#define I2C_WRITE_BIT (0x0)
#define I2C_READ_BIT (0x1)

#define SDA_GPIO_SIG 46
#define SCL_GPIO_SIG 45

#define US_TO_I2C_CLK_CYCLES(us, i2c_clk) ((us) * (i2c_clk) / 1000000)

static void i2c_gpio_config(uint8_t sda_pin, uint8_t scl_pin)
{
    // Configure SDA pin

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[sda_pin], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.pin[sda_pin].pad_driver = 1;                          // 0: normal output, 1: open drain
    GPIO.func_in_sel_cfg[SDA_GPIO_SIG].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[SDA_GPIO_SIG].in_sel = sda_pin;
    GPIO.func_out_sel_cfg[sda_pin].out_sel = SDA_GPIO_SIG;

    // Configure SCL pin

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[scl_pin], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.pin[scl_pin].pad_driver = 1;
    GPIO.func_in_sel_cfg[SCL_GPIO_SIG].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[SCL_GPIO_SIG].in_sel = scl_pin;
    GPIO.func_out_sel_cfg[scl_pin].out_sel = SCL_GPIO_SIG;
}

static void i2c_set_speed(i2c_speed_t speed)
{
    // 1. Compute the required I2C function clock (40x SCL frequency)
    uint32_t i2c_clk = speed * 25;

    // 2. Calculate clock divider
    uint32_t div_num = (XTAL_CLK_FREQ / i2c_clk);

    // 3. Calculate SCL high and low periods
    uint32_t scl_period = i2c_clk / speed;
    uint32_t scl_low = scl_period / 2;
    uint32_t scl_high = scl_period / 2;

    i2c_hal_clk_config_t bus_cfg;
    bus_cfg.clkm_div = div_num;
    bus_cfg.scl_low = scl_low;
    bus_cfg.scl_high = scl_high;
    if (speed == I2C_SPEED_100K)
    {
        bus_cfg.scl_wait_high = US_TO_I2C_CLK_CYCLES(2, i2c_clk);
        bus_cfg.sda_hold = US_TO_I2C_CLK_CYCLES(0.5, i2c_clk);
        bus_cfg.hold = US_TO_I2C_CLK_CYCLES(4, i2c_clk);
        bus_cfg.sda_sample = US_TO_I2C_CLK_CYCLES(1, i2c_clk);
        bus_cfg.setup = US_TO_I2C_CLK_CYCLES(4, i2c_clk);
    }
    else if (speed == I2C_SPEED_400K)
    {
        bus_cfg.scl_wait_high = US_TO_I2C_CLK_CYCLES(0.1, i2c_clk);
        bus_cfg.sda_hold = US_TO_I2C_CLK_CYCLES(0.2, i2c_clk);
        bus_cfg.hold = US_TO_I2C_CLK_CYCLES(0.6, i2c_clk);
        bus_cfg.sda_sample = US_TO_I2C_CLK_CYCLES(0.3, i2c_clk);
        bus_cfg.setup = US_TO_I2C_CLK_CYCLES(0.6, i2c_clk);
    }

    bus_cfg.tout = 22;
    i2c_ll_master_set_bus_timing((&I2C0), &bus_cfg);
    i2c_ll_update(&I2C0);
}

/**
 * @brief Initialize the I2C hardware
 * @param config The configuration for the I2C hardware
 */
void i2c_init(i2c_config_t *config)
{
    // Configure the GPIO pins for the I2C bus
    i2c_gpio_config(config->sda_pin, config->scl_pin);

    // Set the I2C bus speed
    i2c_set_speed(config->speed);

    // Initialize the I2C hardware
    i2c_ll_master_init(&I2C0);

    // Update the I2C hardware with the new configuration
    i2c_ll_update(&I2C0);
}

/**
 * @brief Perform an I2C write transaction
 * @param addr The 7-bit slave address
 * @param data The data to be written
 * @param len The number of bytes to be written
 * @return True if the transaction was successful, false otherwise
 */
bool i2c_write(uint8_t addr, uint8_t *data, uint8_t len)
{
    int bytes_remaining = len;
    uint8_t slave_address = (addr << 1) | I2C_WRITE_BIT;
    i2c_ll_hw_cmd_t cmd_start;
    i2c_ll_hw_cmd_t cmd_write;
    i2c_ll_hw_cmd_t cmd_end_stop;
    uint8_t txn_id = 0;
    uint8_t write_cmd_idx = 1;

    while (bytes_remaining > 0)
    {
        if (txn_id == 0)
        {
            // Send RESTART command
            cmd_start.done = 0;
            cmd_start.op_code = I2C_LL_CMD_RESTART;
            i2c_ll_master_write_cmd_reg((&I2C0), cmd_start, write_cmd_idx - 1);

            // Send slave address
            i2c_ll_write_txfifo((&I2C0), &slave_address, 1);
        }

        // Send data
        uint8_t bytes_to_send = bytes_remaining >= 31 ? 31 : bytes_remaining;
        i2c_ll_write_txfifo((&I2C0), &data[txn_id * 31], bytes_to_send);

        // Send WRITE command
        cmd_write.op_code = I2C_LL_CMD_WRITE;
        cmd_write.ack_exp = 0;
        cmd_write.ack_en = 0;
        cmd_write.done = 0;
        cmd_write.byte_num = I2C0.sr.txfifo_cnt;
        i2c_ll_master_write_cmd_reg((&I2C0), cmd_write, write_cmd_idx);

        // Update remaining bytes
        bytes_remaining -= bytes_to_send;

        // Send END or STOP command
        cmd_end_stop.done = 0;
        cmd_end_stop.op_code = bytes_remaining > 0 ? I2C_LL_CMD_END : I2C_LL_CMD_STOP;
        i2c_ll_master_write_cmd_reg((&I2C0), cmd_end_stop, write_cmd_idx + 1);

        // Update I2C controller
        i2c_ll_update(&I2C0);

        // Start the transaction
        i2c_ll_master_trans_start((&I2C0));

        uint32_t timeout = 1000000;

        // Wait for the transaction to complete
        while (!i2c_ll_master_is_cmd_done((&I2C0), write_cmd_idx + 1))
        {
            timeout--;
            if (timeout == 0)
            {
                return false;
            }
            delay_us(1);
        }

        write_cmd_idx -= txn_id == 0 ? 1 : 0;

        // Increment transaction ID
        txn_id++;
    }
    return true;
}

/**
 * @brief Perform an I2C read transaction
 * @param addr The 7-bit slave address
 * @param data Buffer to store the data read
 * @param len Number of bytes to read
 * @return True if the transaction was successful, false otherwise
 */
bool i2c_read(uint8_t addr, uint8_t *data, uint8_t len)
{
    int bytes_remaining = len;
    uint8_t slave_address = (addr << 1) | I2C_READ_BIT;
    i2c_ll_hw_cmd_t cmd_start;
    i2c_ll_hw_cmd_t cmd_write;
    i2c_ll_hw_cmd_t cmd_read;
    i2c_ll_hw_cmd_t cmd_end_stop;
    uint8_t txn_id = 0;
    uint8_t read_cmd_idx = 2;

    while (bytes_remaining > 0)
    {
        if (txn_id == 0)
        {
            // Send RESTART command
            cmd_start.done = 0;
            cmd_start.op_code = I2C_LL_CMD_RESTART;
            i2c_ll_master_write_cmd_reg((&I2C0), cmd_start, read_cmd_idx - 2);

            // Prepare WRITE command to send the slave address
            cmd_write.done = 0;
            cmd_write.op_code = I2C_LL_CMD_WRITE;
            cmd_write.byte_num = 1;
            i2c_ll_master_write_cmd_reg((&I2C0), cmd_write, read_cmd_idx - 1);

            // Send slave address
            i2c_ll_write_txfifo((&I2C0), &slave_address, 1);
        }

        // Determine the number of bytes to receive in this transaction
        uint8_t bytes_to_receive = bytes_remaining >= 32 ? 32 : bytes_remaining;

        // Send READ command
        cmd_read.op_code = I2C_LL_CMD_READ;
        cmd_read.ack_exp = 0;
        cmd_read.ack_en = 0;
        cmd_read.done = 0;
        cmd_read.byte_num = bytes_to_receive;
        i2c_ll_master_write_cmd_reg((&I2C0), cmd_read, read_cmd_idx);

        // Update remaining bytes
        bytes_remaining -= bytes_to_receive;

        // Determine whether to send END or STOP command
        cmd_end_stop.done = 0;
        cmd_end_stop.op_code = bytes_remaining > 0 ? I2C_LL_CMD_END : I2C_LL_CMD_STOP;
        i2c_ll_master_write_cmd_reg((&I2C0), cmd_end_stop, read_cmd_idx + 1);

        // Update I2C controller
        i2c_ll_update(&I2C0);

        // Start the transaction
        i2c_ll_master_trans_start((&I2C0));

        uint32_t timeout = 1000000;
        // Wait for the transaction to complete
        while (!i2c_ll_master_is_cmd_done((&I2C0), read_cmd_idx + 1))
        {
            timeout--;
            if (timeout == 0)
            {
                return false;
            }
            delay_us(1);
        }

        // Read received data from RX FIFO
        i2c_ll_read_rxfifo((&I2C0), &data[txn_id * 32], bytes_to_receive);

        // Adjust command index for subsequent transactions
        read_cmd_idx -= (txn_id == 0) ? 1 : 0;

        // Increment transaction ID
        txn_id++;
    }

    return true;
}

/**
 * @brief Perform an I2C read transaction for a specific register
 * @param addr The 7-bit slave address
 * @param reg The register address to read from
 * @param data Pointer to the buffer where the read data will be stored
 * @param len The number of bytes to be read
 * @return True if the transaction was successful, false otherwise
 */
bool i2c_read_register(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len)
{
    int bytes_remaining = len;
    uint8_t slave_address_wr = (addr << 1) | I2C_WRITE_BIT;
    uint8_t slave_address_rd = (addr << 1) | I2C_READ_BIT;
    i2c_ll_hw_cmd_t cmd_start={0};
    i2c_ll_hw_cmd_t cmd_write0={0};
    i2c_ll_hw_cmd_t cmd_rstart={0};
    i2c_ll_hw_cmd_t cmd_write1={0};
    i2c_ll_hw_cmd_t cmd_read={0};
    i2c_ll_hw_cmd_t cmd_end_stop={0};
    uint8_t txn_id = 0;
    uint8_t read_cmd_idx = 4;

    while (bytes_remaining > 0)
    {
        if (txn_id == 0)
        {
            // Send RESTART command
            cmd_start.done = 0;
            cmd_start.op_code = I2C_LL_CMD_RESTART;
            i2c_ll_master_write_cmd_reg((&I2C0), cmd_start, read_cmd_idx - 4);

            // Prepare and send WRITE command for slave and register address
            cmd_write0.done = 0;
            cmd_write0.op_code = I2C_LL_CMD_WRITE;
            cmd_write0.byte_num = 2;
            i2c_ll_master_write_cmd_reg((&I2C0), cmd_write0, read_cmd_idx - 3);

            // Send RESTART command before reading
            cmd_rstart.done = 0;
            cmd_rstart.op_code = I2C_LL_CMD_RESTART;
            i2c_ll_master_write_cmd_reg((&I2C0), cmd_rstart, read_cmd_idx - 2);

            // Send WRITE command for slave address in read mode
            cmd_write1.done = 0;
            cmd_write1.op_code = I2C_LL_CMD_WRITE;
            cmd_write1.byte_num = 1;
            i2c_ll_master_write_cmd_reg((&I2C0), cmd_write1, read_cmd_idx - 1);

            uint8_t buf[3] = {slave_address_wr, reg, slave_address_rd};
            // Write slave and register address to TX FIFO
            i2c_ll_write_txfifo((&I2C0), buf, 3);
        }

        uint8_t bytes_to_receive = bytes_remaining >= 32 ? 32 : bytes_remaining;

        // Prepare READ command
        cmd_read.op_code = I2C_LL_CMD_READ;
        cmd_read.ack_exp = 0;
        cmd_read.ack_en = 0;
        cmd_read.done = 0;
        cmd_read.byte_num = bytes_to_receive;
        i2c_ll_master_write_cmd_reg((&I2C0), cmd_read, read_cmd_idx);

        // Update remaining bytes
        bytes_remaining -= bytes_to_receive;

        // Determine whether to send END or STOP command
        cmd_end_stop.done = 0;
        cmd_end_stop.op_code = bytes_remaining > 0 ? I2C_LL_CMD_END : I2C_LL_CMD_STOP;
        i2c_ll_master_write_cmd_reg((&I2C0), cmd_end_stop, read_cmd_idx + 1);

        // Update I2C controller
        i2c_ll_update(&I2C0);

        // Start the transaction
        i2c_ll_master_trans_start((&I2C0));

        uint32_t timeout = 1000000;
        // Wait for the transaction to complete
        while (!i2c_ll_master_is_cmd_done((&I2C0), read_cmd_idx+1))
        {
            timeout--;
            if (timeout == 0)
            {
                return false;
            }
            delay_us(1);
        }

        // Read received data from RX FIFO
        i2c_ll_read_rxfifo((&I2C0), &data[txn_id * 32], bytes_to_receive);

        // Adjust command index for subsequent transactions
        read_cmd_idx -= txn_id == 0 ? 1 : 0;

        // Increment transaction ID
        txn_id++;
    }

    return true;
}
