#include <stdint.h>
#include <string.h>
#include "hal/gpio_ll.h"
#include "hal/rmt_ll.h"
#include "spi_drv.h"
#include "ws2812.h"

#define WS2812_PIN 8
#define RMT_SIG 71

#define WS2812_BRIGHTNESS 0.5

#define RMT_CLK_FREQ (XTAL_CLK_FREQ / 2)
#define RMT_US_TO_CLK_CYCLES(us) ((uint32_t)(us * RMT_CLK_FREQ / 1000000))

#define WS2812_T1H RMT_US_TO_CLK_CYCLES(0.8)
#define WS2812_T1L RMT_US_TO_CLK_CYCLES(0.4)
#define WS2812_T0H RMT_US_TO_CLK_CYCLES(0.4)
#define WS2812_T0L RMT_US_TO_CLK_CYCLES(0.8)
#define WS2812_RESET RMT_US_TO_CLK_CYCLES(50)

static spi_device_handle_t spi_dev;

static void rmt_gpio_config()
{

    GPIO.enable_w1ts.enable_w1ts = 1 << WS2812_PIN;
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[WS2812_PIN], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.func_out_sel_cfg[WS2812_PIN].out_sel = RMT_SIG;
    GPIO.func_out_sel_cfg[WS2812_PIN].oen_sel = 1;
}

/**
 * @brief Send color data to WS2812 LED strip
 *
 * This function writes the specified color to the WS2812 LED strip. The color is adjusted
 * by the defined brightness level before being sent.
 *
 * @param color The color to be sent to the LED strip
 */
void ws2812_write(ws2812_color_t color)
{
    // Adjust color brightness
    color.blue *= WS2812_BRIGHTNESS;
    color.red *= WS2812_BRIGHTNESS;
    color.green *= WS2812_BRIGHTNESS;

#if WS2812_USE_SPI
    uint8_t buffer[24] = {0}; // Buffer to store SPI data

    uint8_t index = 0;
    for (int i = 23; i >= 0; i--)
    {
        // Populate buffer based on color bit value
        if (color.val & (1U << i))
        {
            buffer[index++] = 0b110; // Logic '1' for SPI
        }
        else
        {
            buffer[index++] = 0b100; // Logic '0' for SPI
        }
    }
    spi_transceive(&spi_dev, buffer, NULL, 24); // Send data via SPI

#elif WS2812_USE_RMT
    rmt_ll_tx_reset_pointer(&RMT, 0); // Reset RMT pointer
    ws2812_pulse_t pulse = {0};

    for (int i = 23; i >= 0; i--)
    {
        // Set pulse duration and level based on color bit value
        if (color.val & (1U << i))
        {
            pulse.duration0 = WS2812_T1L;
            pulse.level0 = 0;
            pulse.duration1 = WS2812_T1H;
            pulse.level1 = 1;
        }
        else
        {
            pulse.duration0 = WS2812_T0L;
            pulse.level0 = 0;
            pulse.duration1 = WS2812_T0H;
            pulse.level1 = 1;
        }
        RMT.chndata[0].chndata = pulse.val; // Write pulse to RMT channel
    }

    // Send reset pulse
    pulse.duration0 = WS2812_RESET;
    pulse.level0 = 0;
    pulse.duration1 = WS2812_T0L;
    pulse.level1 = 0;
    RMT.chndata[0].chndata = WS2812_RESET;

    rmt_ll_tx_start(&RMT, 0); // Start RMT transmission

#endif
}

/**
 * @brief Initialize WS2812 LED strip
 *
 * This function initializes the WS2812 LED strip using either SPI or RMT based on
 * the configuration. It sets up the necessary hardware and configurations for
 * communication with the LED strip.
 */
void ws2812_init()
{
#if WS2812_USE_SPI
    // Configure SPI pins
    spi_pins_t spi_pins = {.mosi = WS2812_PIN, .miso = 9, .sck = 14};
    spi_init(spi_pins);

    // Set SPI device parameters
    spi_dev.speed_hz = 2000000;    // Set SPI clock speed
    spi_dev.cs_pin = SPI_DEVICE_0; // Chip select pin
    spi_dev.id = 0;                // Device ID
    spi_dev.mode = 0;              // SPI mode
    spi_device_config(&spi_dev);   // Configure SPI device

#elif WS2812_USE_RMT

    rmt_gpio_config();                                              // Configure RMT GPIO
    rmt_ll_set_group_clock_src(&RMT, 0, RMT_CLK_SRC_XTAL, 2, 0, 0); // Set RMT clock source
    rmt_ll_tx_enable_carrier_modulation(&RMT, 0, false);            // Disable carrier modulation
    rmt_ll_tx_set_channel_clock_div(&RMT, 0, 1);                    // Set RMT channel clock divider

#endif
    ws2812_color_t color = {0}; // Initialize color to zero

    ws2812_write(color); // Send initial color to LED strip
}
