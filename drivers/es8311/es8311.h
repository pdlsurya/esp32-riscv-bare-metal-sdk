/*
 * MIT License
 *
 * Copyright (c) 2026 Surya Poudel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "i2c_drv.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ES8311_I2C_ADDR_0 0x18U
#define ES8311_I2C_ADDR_1 0x19U

typedef enum
{
    ES8311_OK = 0,
    ES8311_ERR_INVALID_ARG = -1,
    ES8311_ERR_I2C = -2,
    ES8311_ERR_UNSUPPORTED_RATE = -3
} es8311_err_t;

typedef enum
{
    ES8311_RESOLUTION_16 = 16,
    ES8311_RESOLUTION_18 = 18,
    ES8311_RESOLUTION_20 = 20,
    ES8311_RESOLUTION_24 = 24,
    ES8311_RESOLUTION_32 = 32
} es8311_resolution_t;

typedef enum
{
    ES8311_MIC_GAIN_0DB = 0,
    ES8311_MIC_GAIN_6DB,
    ES8311_MIC_GAIN_12DB,
    ES8311_MIC_GAIN_18DB,
    ES8311_MIC_GAIN_24DB,
    ES8311_MIC_GAIN_30DB,
    ES8311_MIC_GAIN_36DB,
    ES8311_MIC_GAIN_42DB,
} es8311_mic_gain_t;

typedef enum
{
    ES8311_FADE_OFF = 0,
    ES8311_FADE_4LRCK,
    ES8311_FADE_8LRCK,
    ES8311_FADE_16LRCK,
    ES8311_FADE_32LRCK,
    ES8311_FADE_64LRCK,
    ES8311_FADE_128LRCK,
    ES8311_FADE_256LRCK,
    ES8311_FADE_512LRCK,
    ES8311_FADE_1024LRCK,
    ES8311_FADE_2048LRCK,
    ES8311_FADE_4096LRCK,
    ES8311_FADE_8192LRCK,
    ES8311_FADE_16384LRCK,
    ES8311_FADE_32768LRCK,
    ES8311_FADE_65536LRCK
} es8311_fade_t;

typedef struct
{
    i2c_dev_t *i2c_port;
    uint8_t i2c_addr;
    bool mclk_inverted;
    bool sclk_inverted;
    bool mclk_from_mclk_pin;
    uint32_t mclk_hz;
    uint32_t sample_rate_hz;
    es8311_resolution_t resolution_in;
    es8311_resolution_t resolution_out;
} es8311_config_t;

typedef struct
{
    i2c_dev_handle_t i2c;
    uint32_t sample_rate_hz;
    uint32_t mclk_hz;
    es8311_resolution_t resolution_in;
    es8311_resolution_t resolution_out;
    bool mclk_from_mclk_pin;
    bool initialized;
} es8311_dev_t;

es8311_err_t es8311_init(es8311_dev_t *dev, const es8311_config_t *config);

es8311_err_t es8311_write_register(es8311_dev_t *dev, uint8_t reg_addr, uint8_t value);
es8311_err_t es8311_read_register(es8311_dev_t *dev, uint8_t reg_addr, uint8_t *value);

es8311_err_t es8311_set_sample_frequency(es8311_dev_t *dev, uint32_t mclk_hz, uint32_t sample_rate_hz);
es8311_err_t es8311_set_voice_volume(es8311_dev_t *dev, uint8_t volume_percent);
es8311_err_t es8311_get_voice_volume(es8311_dev_t *dev, uint8_t *volume_percent);
es8311_err_t es8311_set_voice_mute(es8311_dev_t *dev, bool mute);
es8311_err_t es8311_set_mic_gain(es8311_dev_t *dev, es8311_mic_gain_t gain);
es8311_err_t es8311_set_voice_fade(es8311_dev_t *dev, es8311_fade_t fade);
es8311_err_t es8311_set_mic_fade(es8311_dev_t *dev, es8311_fade_t fade);
es8311_err_t es8311_configure_microphone(es8311_dev_t *dev, bool digital_mic);

void es8311_register_dump(es8311_dev_t *dev);

#ifdef __cplusplus
}
#endif
