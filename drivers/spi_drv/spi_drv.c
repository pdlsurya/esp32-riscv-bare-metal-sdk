#include <stdint.h>
#include "hal/spi_ll.h"
#include "hal/gpio_ll.h"
#include "hal/pcnt_ll.h"
#include "spi_drv.h"

#define SPI_DEFAULT_MISO 2
#define SPI_DEFAULT_MOSI 7
#define SPI_DEFAULT_SCK 6
#define SPI_DEFAULT_CS0 16
#define SPI_DEFAULT_CS1 17
#define SPI_DEFAULT_CS2 18
#define SPI_DEFAULT_CS3 19
#define SPI_DEFAULT_CS4 20
#define SPI_DEFAULT_CS5 21

#define SPI_CLK_DUTY_50 128

#define MOSI_GPIO_SIG 65 ///< GPIO matrix signal number of MOSI pin
#define MISO_GPIO_SIG 64 ///< GPIO matrix signal number of MISO pin
#define SCK_GPIO_SIG 63  ///< GPIO matrix signal number of SCK pin
#define CS0_SIG 68       ///< GPIO matrix signal number of CS0 pin
#define CS1_SIG 101      ///< GPIO matrix signal number of CS1 pin
#define CS2_SIG 102      ///< GPIO matrix signal number of CS2 pin
#define CS3_SIG 103      ///< GPIO matrix signal number of CS3 pin
#define CS4_SIG 104      ///< GPIO matrix signal number of CS4 pin
#define CS5_SIG 105      ///< GPIO matrix signal number of CS5 pin

static spi_pins_t spi_pins;

static bool driver_configured = false;

static bool spi_use_iomux = false;

static uint8_t cs_signals[6] = {CS0_SIG, CS1_SIG, CS2_SIG, CS3_SIG, CS4_SIG, CS5_SIG};

static void spi_gpio_config(spi_pins_t pins)
{
    if (pins.mosi == -1 && pins.miso == -1 && pins.sck == -1)
    {
        spi_use_iomux = true;

        // MISO function set
        gpio_ll_func_sel(&GPIO, SPI_DEFAULT_MISO, FUNC_GPIO2_FSPIQ);

        // MOSI function set
        gpio_ll_func_sel(&GPIO, SPI_DEFAULT_MOSI, FUNC_MTDO_FSPID);

        // SCK function set
        gpio_ll_func_sel(&GPIO, SPI_DEFAULT_SCK, FUNC_MTCK_FSPICLK);
    }
    else
    {
        // Use GPIO matrix

        // Configure mosi pin
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pins.mosi], PIN_FUNC_GPIO); // Set as GPIO
        GPIO.func_out_sel_cfg[pins.mosi].out_sel = MOSI_GPIO_SIG;

        // Configure miso pin
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pins.miso], PIN_FUNC_GPIO); // Set as GPIO
        GPIO.func_in_sel_cfg[MISO_GPIO_SIG].sig_in_sel = 1;
        GPIO.func_in_sel_cfg[MISO_GPIO_SIG].in_sel = pins.miso;

        // Configure sck pin
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pins.sck], PIN_FUNC_GPIO); // Set as GPIO
        GPIO.func_out_sel_cfg[pins.sck].out_sel = SCK_GPIO_SIG;
    }
}

void spi_device_config(spi_device_handle_t *dev)
{
    spi_ll_master_cal_clock(APB_CLK_FREQ, dev->speed_hz, SPI_CLK_DUTY_50, &dev->clk_reg_val);

    if (spi_use_iomux)
    {
        switch (dev->id)
        {
        case 0:
            gpio_ll_func_sel(&GPIO, SPI_DEFAULT_CS0, FUNC_U0TXD_FSPICS0);
            break;
        case 1:
            gpio_ll_func_sel(&GPIO, SPI_DEFAULT_CS1, FUNC_U0RXD_FSPICS1);
            break;
        case 2:
            gpio_ll_func_sel(&GPIO, SPI_DEFAULT_CS2, FUNC_SDIO_CMD_FSPICS2);
            break;
        case 3:
            gpio_ll_func_sel(&GPIO, SPI_DEFAULT_CS3, FUNC_SDIO_CLK_FSPICS3);
            break;
        case 4:
            gpio_ll_func_sel(&GPIO, SPI_DEFAULT_CS4, FUNC_SDIO_DATA0_FSPICS4);
            break;
        case 5:
            gpio_ll_func_sel(&GPIO, SPI_DEFAULT_CS5, FUNC_SDIO_DATA1_FSPICS5);
            break;

        default:
            break;
        }
    }
    else // use gpio matrix
    {
        gpio_ll_func_sel(&GPIO, dev->cs_pin, PIN_FUNC_GPIO);

        GPIO.func_out_sel_cfg[dev->cs_pin].out_sel = cs_signals[dev->id];
    }
}

void spi_init(spi_pins_t pins)
{
    if (driver_configured)
        return;

    spi_ll_set_clk_source(&GPSPI2, SPI_CLK_SRC_XTAL);

    spi_ll_master_init(&GPSPI2);

    spi_gpio_config(pins);

    // enable full duplex mode
    spi_ll_set_half_duplex(&GPSPI2, false);

    spi_ll_apply_config(&GPSPI2);

    driver_configured = true;
}

void spi_transceive(spi_device_handle_t *dev, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len)
{

    uint32_t txn_count = (len + 63) / 64;

    for (int txn_idx = 0; txn_idx < txn_count; txn_idx++)
    {
        uint8_t tx_len = len >= 64 ? 64 : len;
        uint8_t words = (tx_len + 3) / 4;

        spi_ll_set_mosi_bitlen(&GPSPI2, tx_len * 8);
        spi_ll_set_miso_bitlen(&GPSPI2, tx_len * 8);
        spi_ll_master_set_mode(&GPSPI2, dev->mode);
        spi_ll_master_set_clock_by_reg(&GPSPI2, &dev->clk_reg_val);
        spi_ll_master_select_cs(&GPSPI2, dev->id);
        spi_ll_apply_config(&GPSPI2);

        for (int i = 0; i < words; i++)
        {

            GPSPI2.data_buf[i].buf = ((uint32_t *)tx_buf)[i + (txn_idx * 16)];
        }
        // Start SPI transfer
        spi_ll_user_start(&GPSPI2);

        // Wait until transfer is complete
        while (spi_ll_get_running_cmd(&GPSPI2))
            ;
        if (rx_buf != NULL)
        {
            for (int i = 0; i < words; i++)
            {

                ((uint32_t *)rx_buf)[i + (txn_idx * 16)] = GPSPI2.data_buf[i].buf;
            }
        }

        len -= 64;
    }
}

/**
 * @brief Transmit byte and return received byte
 *
 * @param byte TX byte
 * @return RX byte
 */
uint8_t spi_transfer_byte(spi_device_handle_t *dev, uint8_t tx_byte)
{

    spi_ll_set_mosi_bitlen(&GPSPI2, 8);
    spi_ll_set_miso_bitlen(&GPSPI2, 8);
    spi_ll_master_set_mode(&GPSPI2, dev->mode);
    spi_ll_master_set_clock_by_reg(&GPSPI2, &dev->clk_reg_val);
    spi_ll_master_select_cs(&GPSPI2, dev->id);
    spi_ll_apply_config(&GPSPI2);

    GPSPI2.data_buf[0].buf = tx_byte;

    // Start SPI transfer
    spi_ll_user_start(&GPSPI2);

    // Wait until transfer is complete
    while (spi_ll_get_running_cmd(&GPSPI2))
        ;

    return GPSPI2.data_buf[0].buf;
}
