#include <stdint.h>
#include <stdbool.h>
#include "hal/i2c_ll.h"
#include "hal/gpio_ll.h"
#include "i2c_drv.h"
#include "soc/i2c_periph.h"
#include "usb_serial.h"
#include "delay.h"

#define I2C_WRITE_BIT (0x0)
#define I2C_READ_BIT (0x1)
#define I2C_TRANSACTION_TIMEOUT_US (1000000U)

static int i2c_get_port_id(i2c_dev_t *port)
{
    if (port == &I2C0)
    {
        return 0;
    }
#if SOC_HP_I2C_NUM > 1
    if (port == &I2C1)
    {
        return 1;
    }
#endif
    return -1;
}

static const i2c_signal_conn_t *i2c_get_signal_conn(i2c_dev_t *port)
{
    int port_id = i2c_get_port_id(port);
    if (port_id < 0 || port_id >= SOC_HP_I2C_NUM)
    {
        return NULL;
    }

    return &i2c_periph_signal[port_id];
}

static void i2c_gpio_config(i2c_dev_t *port, uint8_t sda_pin, uint8_t scl_pin)
{
    const i2c_signal_conn_t *signals = i2c_get_signal_conn(port);
    if (signals == NULL)
    {
        return;
    }

    // Configure SDA pin
    gpio_ll_input_enable(&GPIO, sda_pin);                      // Enable input on sda_pin
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[sda_pin], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.pin[sda_pin].pad_driver = 1;                          // 0: normal output, 1: open drain
    GPIO.func_in_sel_cfg[signals->sda_in_sig].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[signals->sda_in_sig].in_sel = sda_pin;
    GPIO.func_out_sel_cfg[sda_pin].out_sel = signals->sda_out_sig;

    // Configure SCL pin
    gpio_ll_input_enable(&GPIO, scl_pin);                      // Enable input on scl_pin
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[scl_pin], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.pin[scl_pin].pad_driver = 1;
    GPIO.func_in_sel_cfg[signals->scl_in_sig].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[signals->scl_in_sig].in_sel = scl_pin;
    GPIO.func_out_sel_cfg[scl_pin].out_sel = signals->scl_out_sig;
}

static void i2c_set_speed(i2c_dev_t *port, i2c_speed_t speed)
{
    i2c_hal_clk_config_t bus_config = {0};

    i2c_ll_master_cal_bus_clk(XTAL_CLK_FREQ, speed, &bus_config);

    i2c_ll_master_set_bus_timing(port, &bus_config);

    i2c_ll_update(port);
}

/**
 * @brief Initialize the I2C hardware
 * @param config The configuration for the I2C hardware
 */
void i2c_init(i2c_config_t *config)
{
    int port_id;

    if ((config == NULL) || (config->port == NULL))
    {
        return;
    }

    port_id = i2c_get_port_id(config->port);
    if (port_id < 0)
    {
        return;
    }

    i2c_gpio_config(config->port, config->sda_pin, config->scl_pin); // Configure the GPIO pins for the I2C bus

    i2c_ll_enable_controller_clock(config->port, true);

    i2c_ll_enable_bus_clock(port_id, true);

    i2c_ll_enable_arbitration(config->port, false);

    // Set the I2C bus speed
    i2c_set_speed(config->port, config->speed);

    // Initialize the I2C hardware
    i2c_ll_set_mode(config->port, I2C_BUS_MODE_MASTER);

    // Update the I2C hardware with the new configuration
    i2c_ll_update(config->port);
}

/**
 * @brief Perform an I2C write transaction
 * @param dev The I2C device handle
 * @param data The data to be written
 * @param len The number of bytes to be written
 * @return True if the transaction was successful, false otherwise
 */
bool i2c_write(i2c_dev_handle_t *dev, uint8_t *data, uint8_t len)
{
    if ((dev == NULL) || (dev->port == NULL) || ((data == NULL) && (len > 0U)))
    {
        return false;
    }

    if (i2c_get_port_id(dev->port) < 0)
    {
        return false;
    }

    if (len == 0U)
    {
        return true;
    }

    int bytes_remaining = len;
    uint8_t slave_address = (dev->slave_address << 1) | I2C_WRITE_BIT;
    i2c_ll_hw_cmd_t cmd_start = {0};
    i2c_ll_hw_cmd_t cmd_write = {0};
    i2c_ll_hw_cmd_t cmd_end_stop = {0};
    uint8_t txn_id = 0;
    uint8_t write_cmd_idx = 1;

    while (bytes_remaining > 0)
    {
        if (txn_id == 0)
        {
            // Send RESTART command
            cmd_start.done = 0;
            cmd_start.op_code = I2C_LL_CMD_RESTART;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_start, write_cmd_idx - 1);

            // Send slave address
            i2c_ll_write_txfifo(dev->port, &slave_address, 1);
        }

        // Send data
        uint8_t bytes_to_send = bytes_remaining >= 31 ? 31 : bytes_remaining;
        i2c_ll_write_txfifo(dev->port, &data[txn_id * 31], bytes_to_send);

        // Send WRITE command
        cmd_write.op_code = I2C_LL_CMD_WRITE;
        cmd_write.ack_exp = 0;
        cmd_write.ack_en = 1;
        cmd_write.ack_val = 0;
        cmd_write.done = 0;
        cmd_write.byte_num = dev->port->sr.txfifo_cnt;
        i2c_ll_master_write_cmd_reg(dev->port, cmd_write, write_cmd_idx);

        // Update remaining bytes
        bytes_remaining -= bytes_to_send;

        // Send END or STOP command
        cmd_end_stop.done = 0;
        cmd_end_stop.op_code = bytes_remaining > 0 ? I2C_LL_CMD_END : I2C_LL_CMD_STOP;
        i2c_ll_master_write_cmd_reg(dev->port, cmd_end_stop, write_cmd_idx + 1);

        // Update I2C controller
        i2c_ll_update(dev->port);

        // Start the transaction
        i2c_ll_start_trans(dev->port);

        uint32_t timeout = I2C_TRANSACTION_TIMEOUT_US;

        // Wait for the transaction to complete
        while (!i2c_ll_master_is_cmd_done(dev->port, write_cmd_idx + 1))
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
 * @param dev The I2C device handle
 * @param data Buffer to store the data read
 * @param len Number of bytes to read
 * @return True if the transaction was successful, false otherwise
 */
bool i2c_read(i2c_dev_handle_t *dev, uint8_t *data, uint8_t len)
{
    if ((dev == NULL) || (dev->port == NULL) || ((data == NULL) && (len > 0U)))
    {
        return false;
    }

    if (i2c_get_port_id(dev->port) < 0)
    {
        return false;
    }

    if (len == 0U)
    {
        return true;
    }

    int bytes_remaining = len;
    uint8_t slave_address = (dev->slave_address << 1) | I2C_READ_BIT;
    i2c_ll_hw_cmd_t cmd_start = {0};
    i2c_ll_hw_cmd_t cmd_write = {0};
    i2c_ll_hw_cmd_t cmd_read = {0};
    i2c_ll_hw_cmd_t cmd_end_stop = {0};
    uint8_t txn_id = 0;
    uint8_t read_cmd_idx = 2;

    while (bytes_remaining > 0)
    {
        if (txn_id == 0)
        {
            // Send RESTART command
            cmd_start.done = 0;
            cmd_start.op_code = I2C_LL_CMD_RESTART;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_start, read_cmd_idx - 2);

            // Prepare WRITE command to send the slave address
            cmd_write.done = 0;
            cmd_write.op_code = I2C_LL_CMD_WRITE;
            cmd_write.ack_en = 1;
            cmd_write.ack_exp = 0;
            cmd_write.ack_val = 0;
            cmd_write.byte_num = 1;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_write, read_cmd_idx - 1);

            // Send slave address
            i2c_ll_write_txfifo(dev->port, &slave_address, 1);
        }

        // Determine the number of bytes to receive in this transaction
        uint8_t bytes_to_receive = bytes_remaining >= 32 ? 32 : bytes_remaining;
        bool final_chunk = (bytes_remaining <= 32);
        uint8_t wait_cmd_idx;

        if (final_chunk && (bytes_to_receive > 1U))
        {
            // Multi-byte I2C reads must ACK intermediate bytes and NACK only the last one.
            cmd_read.op_code = I2C_LL_CMD_READ;
            cmd_read.ack_exp = 0;
            cmd_read.ack_en = 0;
            cmd_read.ack_val = 0;
            cmd_read.done = 0;
            cmd_read.byte_num = bytes_to_receive - 1U;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_read, read_cmd_idx);

            cmd_read.ack_val = 1;
            cmd_read.byte_num = 1;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_read, read_cmd_idx + 1);

            cmd_end_stop.done = 0;
            cmd_end_stop.op_code = I2C_LL_CMD_STOP;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_end_stop, read_cmd_idx + 2);
            wait_cmd_idx = read_cmd_idx + 2;
        }
        else
        {
            cmd_read.op_code = I2C_LL_CMD_READ;
            cmd_read.ack_exp = 0;
            cmd_read.ack_en = 0;
            cmd_read.ack_val = final_chunk ? 1U : 0U;
            cmd_read.done = 0;
            cmd_read.byte_num = bytes_to_receive;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_read, read_cmd_idx);

            cmd_end_stop.done = 0;
            cmd_end_stop.op_code = final_chunk ? I2C_LL_CMD_STOP : I2C_LL_CMD_END;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_end_stop, read_cmd_idx + 1);
            wait_cmd_idx = read_cmd_idx + 1;
        }

        // Update remaining bytes
        bytes_remaining -= bytes_to_receive;

        // Update I2C controller
        i2c_ll_update(dev->port);

        // Start the transaction
        i2c_ll_start_trans(dev->port);

        uint32_t timeout = I2C_TRANSACTION_TIMEOUT_US;
        // Wait for the transaction to complete
        while (!i2c_ll_master_is_cmd_done(dev->port, wait_cmd_idx))
        {
            timeout--;
            if (timeout == 0)
            {
                return false;
            }
            delay_us(1);
        }

        // Read received data from RX FIFO
        i2c_ll_read_rxfifo(dev->port, &data[txn_id * 32], bytes_to_receive);

        // Adjust command index for subsequent transactions
        read_cmd_idx -= (txn_id == 0) ? 1 : 0;

        // Increment transaction ID
        txn_id++;
    }

    return true;
}

/**
 * @brief Perform an I2C read transaction for a specific register
 * @param dev The I2C device handle
 * @param reg The register address to read from
 * @param data Pointer to the buffer where the read data will be stored
 * @param len The number of bytes to be read
 * @return True if the transaction was successful, false otherwise
 */
bool i2c_read_register(i2c_dev_handle_t *dev, uint8_t reg, uint8_t *data, uint8_t len)
{
    if ((dev == NULL) || (dev->port == NULL) || ((data == NULL) && (len > 0U)))
    {
        return false;
    }

    if (i2c_get_port_id(dev->port) < 0)
    {
        return false;
    }

    if (len == 0U)
    {
        return true;
    }

    int bytes_remaining = len;
    uint8_t slave_address_wr = (dev->slave_address << 1) | I2C_WRITE_BIT;
    uint8_t slave_address_rd = (dev->slave_address << 1) | I2C_READ_BIT;
    i2c_ll_hw_cmd_t cmd_start = {0};
    i2c_ll_hw_cmd_t cmd_write0 = {0};
    i2c_ll_hw_cmd_t cmd_rstart = {0};
    i2c_ll_hw_cmd_t cmd_write1 = {0};
    i2c_ll_hw_cmd_t cmd_read = {0};
    i2c_ll_hw_cmd_t cmd_end_stop = {0};
    uint8_t txn_id = 0;
    uint8_t read_cmd_idx = 4;

    while (bytes_remaining > 0)
    {
        if (txn_id == 0)
        {
            // Send RESTART command
            cmd_start.done = 0;
            cmd_start.op_code = I2C_LL_CMD_RESTART;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_start, read_cmd_idx - 4);

            // Prepare and send WRITE command for slave and register address
            cmd_write0.done = 0;
            cmd_write0.op_code = I2C_LL_CMD_WRITE;
            cmd_write0.ack_en = 1;
            cmd_write0.ack_exp = 0;
            cmd_write0.ack_val = 0;
            cmd_write0.byte_num = 2;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_write0, read_cmd_idx - 3);

            // Send RESTART command before reading
            cmd_rstart.done = 0;
            cmd_rstart.op_code = I2C_LL_CMD_RESTART;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_rstart, read_cmd_idx - 2);

            // Send WRITE command for slave address in read mode
            cmd_write1.done = 0;
            cmd_write1.op_code = I2C_LL_CMD_WRITE;
            cmd_write1.ack_en = 1;
            cmd_write1.ack_exp = 0;
            cmd_write1.ack_val = 0;
            cmd_write1.byte_num = 1;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_write1, read_cmd_idx - 1);

            uint8_t buf[3] = {slave_address_wr, reg, slave_address_rd};
            // Write slave and register address to TX FIFO
            i2c_ll_write_txfifo(dev->port, buf, 3);
        }

        uint8_t bytes_to_receive = bytes_remaining >= 32 ? 32 : bytes_remaining;
        bool final_chunk = (bytes_remaining <= 32);
        uint8_t wait_cmd_idx;

        if (final_chunk && (bytes_to_receive > 1U))
        {
            // Multi-byte I2C reads must ACK intermediate bytes and NACK only the last one.
            cmd_read.op_code = I2C_LL_CMD_READ;
            cmd_read.ack_exp = 0;
            cmd_read.ack_en = 0;
            cmd_read.ack_val = 0;
            cmd_read.done = 0;
            cmd_read.byte_num = bytes_to_receive - 1U;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_read, read_cmd_idx);

            cmd_read.ack_val = 1;
            cmd_read.byte_num = 1;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_read, read_cmd_idx + 1);

            cmd_end_stop.done = 0;
            cmd_end_stop.op_code = I2C_LL_CMD_STOP;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_end_stop, read_cmd_idx + 2);
            wait_cmd_idx = read_cmd_idx + 2;
        }
        else
        {
            // Preserve the working single-byte behavior while ACKing intermediate full chunks.
            cmd_read.op_code = I2C_LL_CMD_READ;
            cmd_read.ack_exp = 0;
            cmd_read.ack_en = 0;
            cmd_read.ack_val = final_chunk ? 1U : 0U;
            cmd_read.done = 0;
            cmd_read.byte_num = bytes_to_receive;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_read, read_cmd_idx);

            cmd_end_stop.done = 0;
            cmd_end_stop.op_code = final_chunk ? I2C_LL_CMD_STOP : I2C_LL_CMD_END;
            i2c_ll_master_write_cmd_reg(dev->port, cmd_end_stop, read_cmd_idx + 1);
            wait_cmd_idx = read_cmd_idx + 1;
        }

        // Update remaining bytes
        bytes_remaining -= bytes_to_receive;

        // Update I2C controller
        i2c_ll_update(dev->port);

        // Start the transaction
        i2c_ll_start_trans(dev->port);

        uint32_t timeout = I2C_TRANSACTION_TIMEOUT_US;
        // Wait for the transaction to complete
        while (!i2c_ll_master_is_cmd_done(dev->port, wait_cmd_idx))
        {
            timeout--;
            if (timeout == 0)
            {
                return false;
            }
            delay_us(1);
        }

        // Read received data from RX FIFO
        i2c_ll_read_rxfifo(dev->port, &data[txn_id * 32], bytes_to_receive);

        // Adjust command index for subsequent transactions
        read_cmd_idx -= txn_id == 0 ? 1 : 0;

        // Increment transaction ID
        txn_id++;
    }
    return true;
}
