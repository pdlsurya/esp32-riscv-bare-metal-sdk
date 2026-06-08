#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "delay.h"
#include "uart_drv.h"

#if defined(TARGET_SOC_ESP32C6)
#define DEMO_UART_PORT_NUM 0U
#define DEMO_UART_TX_PIN 20
#define DEMO_UART_RX_PIN 19
#elif defined(TARGET_SOC_ESP32P4)
#define DEMO_UART_PORT_NUM 1U
#define DEMO_UART_TX_PIN 10
#define DEMO_UART_RX_PIN 11
#else
#error "uart_echo example does not define default pins for this target"
#endif

#define DEMO_UART_BAUD_RATE 115200U
#define DEMO_POLL_PERIOD_MS 10U
#define DEMO_LOG_PERIOD_MS 1000U
#define DEMO_LOG_TICKS (DEMO_LOG_PERIOD_MS / DEMO_POLL_PERIOD_MS)

static uart_dev_handle_t s_uart = UART_GET_DEV_HANDLE(DEMO_UART_PORT_NUM);

static void uart_write_string(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    uart_write(&s_uart, text, strlen(text));
}

int main(void)
{
    uart_config_t uart_config = UART_DEFAULT_CONFIG();
    uint32_t log_count = 0U;
    uint32_t poll_ticks = 0U;
    uint8_t rx_byte = 0U;
    char message[80];

    uart_config.tx_pin = DEMO_UART_TX_PIN;
    uart_config.rx_pin = DEMO_UART_RX_PIN;
    uart_config.baud_rate = DEMO_UART_BAUD_RATE;

    if (!uart_init(&s_uart, &uart_config))
    {
        while (1)
        {
            delay_ms(DEMO_POLL_PERIOD_MS);
        }
    }

    uart_write_string("\r\nUART echo example started\r\n");
    (void)snprintf(message, sizeof(message),
                   "UART%u TX=GPIO%u RX=GPIO%u baud=%lu\r\n",
                   (unsigned int)DEMO_UART_PORT_NUM,
                   (unsigned int)DEMO_UART_TX_PIN,
                   (unsigned int)DEMO_UART_RX_PIN,
                   (unsigned long)DEMO_UART_BAUD_RATE);
    uart_write_string(message);
    uart_write_string("Type characters to echo them back.\r\n");

    while (1)
    {
        while (uart_read_byte(&s_uart, &rx_byte, 0U))
        {
            uart_write(&s_uart, &rx_byte, 1U);
            if (rx_byte == '\r')
            {
                uart_write_string("\n");
            }
        }

        poll_ticks++;
        if (poll_ticks >= DEMO_LOG_TICKS)
        {
            int len = snprintf(message, sizeof(message),
                               "[uart_echo] log %lu\r\n",
                               (unsigned long)log_count);
            if (len > 0)
            {
                uart_write(&s_uart, message, (size_t)len);
            }

            log_count++;
            poll_ticks = 0U;
        }

        delay_ms(DEMO_POLL_PERIOD_MS);
    }

    return 0;
}
