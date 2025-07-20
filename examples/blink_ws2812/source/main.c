#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "delay.h"
#include "gpio_drv.h"
#include "ws2812.h"

#define LED_GPIO_PIN 2U

int main(void)
{
    uint32_t step = 0U;

    delay_ms(200U);
    printf("ESP32 blink + WS2812 example\n");

    gpio_set_direction(LED_GPIO_PIN, GPIO_OUTPUT);
    gpio_write(LED_GPIO_PIN, 0U);

    ws2812_init();

    while (1)
    {
        gpio_toggle(LED_GPIO_PIN);

        switch (step & 0x3U)
        {
            case 0U:
                ws2812_write((ws2812_color_t)WS2812_COLOR_RED);
                printf("[blink_ws2812] led=RED\n");
                break;

            case 1U:
                ws2812_write((ws2812_color_t)WS2812_COLOR_GREEN);
                printf("[blink_ws2812] led=GREEN\n");
                break;

            case 2U:
                ws2812_write((ws2812_color_t)WS2812_COLOR_BLUE);
                printf("[blink_ws2812] led=BLUE\n");
                break;

            default:
                ws2812_write((ws2812_color_t)WS2812_COLOR_BLACK);
                printf("[blink_ws2812] led=OFF\n");
                break;
        }

        step++;
        delay_ms(500U);
    }

    return 0;
}
