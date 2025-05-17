#include <stdint.h>
#include "hal/i2c_ll.h"
#include "hal/gpio_ll.h"
#include "hal/pcnt_ll.h"
#include "i2c_driver.h"

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

void i2c_init(i2c_config_t *config)
{
    i2c_gpio_config(config->sda_pin, config->scl_pin);

    i2c_set_speed(config->speed);

    i2c_ll_master_init(&I2C0);
    i2c_ll_update(&I2C0);
}

bool i2c_write(uint8_t addr, uint8_t *data, uint8_t len)
{
    int bytes_remaining = len;
    uint8_t slave_address = addr << 1;
    i2c_ll_hw_cmd_t cmd_rstart;
    i2c_ll_hw_cmd_t cmd_write;
    i2c_ll_hw_cmd_t cmd_end_stop;
    uint8_t txn_id = 0;
    uint8_t write_cmd_idx = 1;

    while (bytes_remaining > 0)
    {
        if (txn_id == 0)
        {
            cmd_rstart.done = 0;
            cmd_rstart.op_code = I2C_LL_CMD_RESTART;
            i2c_ll_master_write_cmd_reg((&I2C0), cmd_rstart, write_cmd_idx - 1);
            i2c_ll_write_txfifo((&I2C0), &slave_address, 1);
        }

        i2c_ll_write_txfifo((&I2C0), &data[txn_id * 31], bytes_remaining >= 31 ? 31 : bytes_remaining);
        cmd_write.op_code = I2C_LL_CMD_WRITE;
        cmd_write.ack_exp = 0;
        cmd_write.ack_en = 1;
        cmd_write.done = 0;
        cmd_write.byte_num = I2C0.sr.txfifo_cnt;
        i2c_ll_master_write_cmd_reg((&I2C0), cmd_write, write_cmd_idx);

        bytes_remaining -= txn_id == 0 ? I2C0.sr.txfifo_cnt - 1 : I2C0.sr.txfifo_cnt;

        cmd_end_stop.done = 0;
        cmd_end_stop.op_code = bytes_remaining > 0 ? I2C_LL_CMD_END : I2C_LL_CMD_STOP;
        i2c_ll_master_write_cmd_reg((&I2C0), cmd_end_stop, write_cmd_idx + 1);

        i2c_ll_update(&I2C0);
        i2c_ll_master_trans_start((&I2C0));

        while (!i2c_ll_master_is_cmd_done((&I2C0), write_cmd_idx + 1))
            ;

        write_cmd_idx -= txn_id == 0 ? 1 : 0;

        txn_id++;
    }
    return true;
}