#include <stdint.h>
#include "hal/spi_ll.h"
#include "hal/gpio_ll.h"
#include "hal/pcnt_ll.h"
#include "spi_drv.h"

#define SPI_CLK_DUTY_50 128

#if defined(TARGET_SOC_ESP32P4)

#define SPI3_MOSI_GPIO_SIG 49 ///< GPIO matrix signal number of MOSI pin
#define SPI3_MISO_GPIO_SIG 48 ///< GPIO matrix signal number of MISO pin
#define SPI3_SCK_GPIO_SIG 47  ///< GPIO matrix signal number of SCK pin
#define SPI3_CS0_SIG 52       ///< GPIO matrix signal number of CS0 pin
#define SPI3_CS1_SIG 46       ///< GPIO matrix signal number of CS1 pin
#define SPI3_CS2_SIG 45       ///< GPIO matrix signal number of CS2 pin

#define SPI2_MOSI_GPIO_SIG 55 ///< GPIO matrix signal number of MOSI pin
#define SPI2_MISO_GPIO_SIG 54 ///< GPIO matrix signal number of MISO pin
#define SPI2_SCK_GPIO_SIG 53  ///< GPIO matrix signal number of SCK pin
#define SPI2_CS0_SIG 62       ///< GPIO matrix signal number of CS0 pin
#define SPI2_CS1_SIG 63       ///< GPIO matrix signal number of CS1 pin
#define SPI2_CS2_SIG 64       ///< GPIO matrix signal number of CS2 pin
#define SPI2_CS3_SIG 65       ///< GPIO matrix signal number of CS3 pin
#define SPI2_CS4_SIG 66       ///< GPIO matrix signal number of CS4 pin
#define SPI2_CS5_SIG 67       ///< GPIO matrix signal number of CS5 pin

#elif defined(TARGET_SOC_ESP32C6)

#define SPI2_MOSI_GPIO_SIG 65 ///< GPIO matrix signal number of MOSI pin
#define SPI2_MISO_GPIO_SIG 64 ///< GPIO matrix signal number of MISO pin
#define SPI2_SCK_GPIO_SIG 63  ///< GPIO matrix signal number of SCK pin
#define SPI2_CS0_SIG 68       ///< GPIO matrix signal number of CS0 pin
#define SPI2_CS1_SIG 101      ///< GPIO matrix signal number of CS1 pin
#define SPI2_CS2_SIG 102      ///< GPIO matrix signal number of CS2 pin
#define SPI2_CS3_SIG 103      ///< GPIO matrix signal number of CS3 pin
#define SPI2_CS4_SIG 104      ///< GPIO matrix signal number of CS4 pin
#define SPI2_CS5_SIG 105      ///< GPIO matrix signal number of CS5 pin

#define SPI3_MOSI_GPIO_SIG 22 ///< GPIO matrix signal number of MOSI pin
#define SPI3_MISO_GPIO_SIG 22 ///< GPIO matrix signal number of MISO pin
#define SPI3_SCK_GPIO_SIG 22  ///< GPIO matrix signal number of SCK pin
#define SPI3_CS0_SIG 22       ///< GPIO matrix signal number of CS0 pin
#define SPI3_CS1_SIG 22       ///< GPIO matrix signal number of CS1 pin
#define SPI3_CS2_SIG 22       ///< GPIO matrix signal number of CS2 pin

#endif

static spi_pins_t spi_pins;

static bool spi_use_iomux = false;

static bool driver_configured[2] = {false, false};

static uint8_t cs_signals[2][6] = {
    {SPI2_CS0_SIG, SPI2_CS1_SIG, SPI2_CS2_SIG, SPI2_CS3_SIG, SPI2_CS4_SIG, SPI2_CS5_SIG},
    {SPI3_CS0_SIG, SPI3_CS1_SIG, SPI3_CS2_SIG},

};

static void spi_gpio_config(spi_config_t *config)
{

    // Configure mosi pin
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[config->pins.mosi], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.func_out_sel_cfg[config->pins.mosi].out_sel = config->port == &GPSPI2 ? SPI2_MOSI_GPIO_SIG : SPI3_MOSI_GPIO_SIG;

    // Configure miso pin
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[config->pins.miso], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.func_in_sel_cfg[config->port == &GPSPI2 ? SPI2_MISO_GPIO_SIG : SPI3_MISO_GPIO_SIG].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[config->port == &GPSPI2 ? SPI2_MISO_GPIO_SIG : SPI3_MISO_GPIO_SIG].in_sel = config->pins.miso;

    // Configure sck pin
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[config->pins.sck], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.func_out_sel_cfg[config->pins.sck].out_sel = config->port == &GPSPI2 ? SPI2_SCK_GPIO_SIG : SPI3_SCK_GPIO_SIG;
}

void spi_device_config(spi_dev_handle_t *dev)
{
    uint8_t spi_port_num = dev->port == &GPSPI2 ? 2 : 3;

    spi_ll_master_cal_clock(APB_CLK_FREQ, dev->speed_hz, SPI_CLK_DUTY_50, &dev->clk_reg_val);

    gpio_ll_func_sel(&GPIO, dev->cs_pin, PIN_FUNC_GPIO);

    GPIO.func_out_sel_cfg[dev->cs_pin].out_sel = cs_signals[spi_port_num - 2][dev->id];
}

void spi_init(spi_config_t *config)
{
    uint8_t spi_port_num = config->port == &GPSPI2 ? 2 : 3;
    if (driver_configured[spi_port_num - 2])
    {
        return;
    }

    spi_ll_set_clk_source(config->port, SPI_CLK_SRC_XTAL);

    spi_ll_master_init(config->port);

    spi_gpio_config(config);

    // enable full duplex mode
    spi_ll_set_half_duplex(config->port, false);

    spi_ll_apply_config(config->port);

    driver_configured[spi_port_num - 2] = true;
}

void spi_transceive(spi_dev_handle_t *dev, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len)
{

    uint32_t txn_count = (len + 63) / 64;

    for (int txn_idx = 0; txn_idx < txn_count; txn_idx++)
    {
        uint8_t tx_len = len >= 64 ? 64 : len;
        uint8_t words = (tx_len + 3) / 4;

        spi_ll_set_mosi_bitlen(dev->port, tx_len * 8);
        spi_ll_set_miso_bitlen(dev->port, tx_len * 8);
        spi_ll_master_set_mode(dev->port, dev->mode);
        spi_ll_master_set_clock_by_reg(dev->port, &dev->clk_reg_val);
        spi_ll_master_select_cs(dev->port, dev->id);
        spi_ll_apply_config(dev->port);

        for (int i = 0; i < words; i++)
        {

            dev->port->data_buf[i].buf = ((uint32_t *)tx_buf)[i + (txn_idx * 16)];
        }
        // Start SPI transfer
        spi_ll_user_start(dev->port);

        // Wait until transfer is complete
        while (spi_ll_get_running_cmd(dev->port))
            ;
        if (rx_buf != NULL)
        {
            for (int i = 0; i < words; i++)
            {

                ((uint32_t *)rx_buf)[i + (txn_idx * 16)] = dev->port->data_buf[i].buf;
            }
        }

        len -= 64;
    }
}

/**
 * @brief Transmit byte and return received byte
 *
 * @param dev SPI device
 * @param byte TX byte
 * @return RX byte
 */
uint8_t spi_transfer_byte(spi_dev_handle_t *dev, uint8_t tx_byte)
{

    spi_ll_set_mosi_bitlen(dev->port, 8);
    spi_ll_set_miso_bitlen(dev->port, 8);
    spi_ll_master_set_mode(dev->port, dev->mode);
    spi_ll_master_set_clock_by_reg(dev->port, &dev->clk_reg_val);
    spi_ll_master_select_cs(dev->port, dev->id);
    spi_ll_apply_config(dev->port);

    dev->port->data_buf[0].buf = tx_byte;

    // Start SPI transfer
    spi_ll_user_start(dev->port);

    // Wait until transfer is complete
    while (spi_ll_get_running_cmd(dev->port))
        ;

    return dev->port->data_buf[0].buf;
}
