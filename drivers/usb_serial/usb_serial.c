#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "delay.h"
#include "hal/usb_serial_jtag_ll.h"
#include "usb_serial.h"

static int serial_write_bytes(const char *buf, size_t len)
{
    size_t offset = 0;

    if (buf == NULL)
    {
        return -1;
    }

    while (offset < len)
    {
        size_t chunk = len - offset;
        uint16_t timeout = 5600; // in microseconds

        if (chunk > 64U)
        {
            chunk = 64U;
        }

        for (size_t i = 0; i < chunk; i++)
        {
            while ((!USB_SERIAL_JTAG.ep1_conf.serial_in_ep_data_free))
            {
                delay_us(1);
                timeout--;
                if (timeout == 0)
                {
                    return -1;
                }
            }
            USB_SERIAL_JTAG.ep1.rdwr_byte = buf[offset + i];
        }
        if (chunk < 64U)
        {
            usb_serial_jtag_ll_txfifo_flush();
        }

        offset += chunk;
    }

    return (int)len;
}

int serial_write(const char *buf, size_t len)
{
    return serial_write_bytes(buf, len);
}

/**
 * @brief read a string from usb serial
 *
 * @return rx string
 */
char *serial_read_string()
{
    int idx = 0;
    static char string_buf[256];
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
