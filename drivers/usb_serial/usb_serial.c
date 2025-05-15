#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "delay.h"
#include "hal/usb_serial_jtag_ll.h"

int serial_printf(const char *format, ...)
{
    char buf[192];
    va_list args;
    va_start(args, format);
    int size = vsprintf(buf, format, args);
    int bytes_remaining = size;
    va_end(args);
    uint8_t fragments = size % 64 == 0 ? size % 64 : size % 64 + 1;
    uint16_t timeout = 5600; // in microseconds

    for (int fragment = 0; fragment < fragments; fragment++)
    {
        for (int i = 0; i < (bytes_remaining >= 64 ? 64 : bytes_remaining); i++)
        {
            while ((!USB_SERIAL_JTAG.ep1_conf.serial_in_ep_data_free))
            {
                delay_us(1);
                timeout--;
                if (timeout == 0)
                    return -1;
            }
            USB_SERIAL_JTAG.ep1.rdwr_byte = buf[fragment * 64 + i];
        }
        if (bytes_remaining < 64)
        {
            usb_serial_jtag_ll_txfifo_flush();
        }

        bytes_remaining -= 64;
    }
    return size;
}

/**
 * @brief read a string from usb serial
 *
 * @return rx string
 */
char *serial_read_string()
{
    int idx = 0;
    static char string_buf[192];
    memset(string_buf, 0, sizeof(string_buf));

    if (USB_SERIAL_JTAG.ep1_conf.serial_out_ep_data_avail)
    {
        while (USB_SERIAL_JTAG.ep1_conf.serial_out_ep_data_avail)
        {
            char rx_byte = USB_SERIAL_JTAG.ep1.rdwr_byte;
            if (rx_byte == '\r' || rx_byte == '\n' || idx == sizeof(string_buf) - 1)
                break;
            string_buf[idx++] = rx_byte;
        }
    }

    return string_buf;
}