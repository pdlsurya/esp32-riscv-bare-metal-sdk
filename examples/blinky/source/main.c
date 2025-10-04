#include "gpio_drv.h"
#include "delay.h"
#include "usb_serial.h"
#include "ws2812.h"

#define LED_PIN 15

int main()
{
    gpio_set_direction(LED_PIN, GPIO_OUTPUT);

    ws2812_init();

    while (1)
    {
        gpio_write(LED_PIN, 1);
        ws2812_write((ws2812_color_t)WS2812_COLOR_YELLOW);
        delay_ms(300);
        gpio_write(LED_PIN, 0);
        ws2812_write((ws2812_color_t)WS2812_COLOR_BLACK);
        delay_ms(300);
    }

    return 0;
}
