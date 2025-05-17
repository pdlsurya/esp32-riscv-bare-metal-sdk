#include "hal/gpio_ll.h"
#include "delay.h"
#include "usb_serial.h"
#include "ws2812.h"

#define LED_PIN 15

int main()
{
    gpio_ll_output_enable(&GPIO, LED_PIN);

    // initialize ws2812
    ws2812_init();

    int count = 0;

    while (1)
    {
        gpio_ll_set_level(&GPIO, LED_PIN, 1);
        ws2812_send((ws2812_color_t)WS2812_COLOR_PURPLE);
        delay_ms(200);

        gpio_ll_set_level(&GPIO, LED_PIN, 0);
        ws2812_send((ws2812_color_t)WS2812_COLOR_BLACK);
        delay_ms(200);
        serial_printf("Hello world!->%d\n", count++);
    }

    return 0;
}
