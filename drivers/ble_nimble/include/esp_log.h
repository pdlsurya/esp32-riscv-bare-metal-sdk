#ifndef __ESP_LOG_H
#define __ESP_LOG_H

#include "usb_serial.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_LOGE(tag, fmt, ...) serial_printf("[E][%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) serial_printf("[W][%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) serial_printf("[I][%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) serial_printf("[D][%s] " fmt, tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) serial_printf("[V][%s] " fmt, tag, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
