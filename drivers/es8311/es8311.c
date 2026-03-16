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

/*
 * This implementation is derived from the public ES8311 component logic in
 * Espressif's esp-bsp project and adapted to this SDK's bare-metal I2C API.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "es8311.h"
#include "delay.h"
#include "usb_serial.h"

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

/* Register map */
#define ES8311_RESET_REG00       0x00
#define ES8311_CLK_MANAGER_REG01 0x01
#define ES8311_CLK_MANAGER_REG02 0x02
#define ES8311_CLK_MANAGER_REG03 0x03
#define ES8311_CLK_MANAGER_REG04 0x04
#define ES8311_CLK_MANAGER_REG05 0x05
#define ES8311_CLK_MANAGER_REG06 0x06
#define ES8311_CLK_MANAGER_REG07 0x07
#define ES8311_CLK_MANAGER_REG08 0x08
#define ES8311_SDPIN_REG09       0x09
#define ES8311_SDPOUT_REG0A      0x0A
#define ES8311_SYSTEM_REG0D      0x0D
#define ES8311_SYSTEM_REG0E      0x0E
#define ES8311_SYSTEM_REG12      0x12
#define ES8311_SYSTEM_REG13      0x13
#define ES8311_SYSTEM_REG14      0x14
#define ES8311_ADC_REG15         0x15
#define ES8311_ADC_REG16         0x16
#define ES8311_ADC_REG17         0x17
#define ES8311_ADC_REG1C         0x1C
#define ES8311_DAC_REG31         0x31
#define ES8311_DAC_REG32         0x32
#define ES8311_DAC_REG37         0x37

typedef struct
{
    uint32_t mclk_hz;
    uint32_t sample_rate_hz;
    uint8_t pre_div;
    uint8_t pre_multi;
    uint8_t adc_div;
    uint8_t dac_div;
    uint8_t fs_mode;
    uint8_t lrck_h;
    uint8_t lrck_l;
    uint8_t bclk_div;
    uint8_t adc_osr;
    uint8_t dac_osr;
} es8311_coeff_div_t;

/* Coefficients from Espressif reference ES8311 driver */
static const es8311_coeff_div_t s_coeff_div[] = {
    /* 8k */
    {12288000, 8000, 0x06, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {18432000, 8000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x05, 0xFF, 0x18, 0x10, 0x10},
    {16384000, 8000, 0x08, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {8192000, 8000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {6144000, 8000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {4096000, 8000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {3072000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {2048000, 8000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1536000, 8000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1024000, 8000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},

    /* 11.025k */
    {11289600, 11025, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {5644800, 11025, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {2822400, 11025, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1411200, 11025, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},

    /* 12k */
    {12288000, 12000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {6144000, 12000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {3072000, 12000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1536000, 12000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},

    /* 16k */
    {12288000, 16000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {18432000, 16000, 0x03, 0x01, 0x03, 0x03, 0x00, 0x02, 0xFF, 0x0C, 0x10, 0x10},
    {16384000, 16000, 0x04, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {8192000, 16000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {6144000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {4096000, 16000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {3072000, 16000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {2048000, 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1536000, 16000, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1024000, 16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},

    /* 22.05k */
    {11289600, 22050, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {5644800, 22050, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {2822400, 22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1411200, 22050, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {705600, 22050, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},

    /* 24k */
    {12288000, 24000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {18432000, 24000, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {6144000, 24000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {3072000, 24000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1536000, 24000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},

    /* 32k */
    {12288000, 32000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {18432000, 32000, 0x03, 0x02, 0x03, 0x03, 0x00, 0x02, 0xFF, 0x0C, 0x10, 0x10},
    {16384000, 32000, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {8192000, 32000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {6144000, 32000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {4096000, 32000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {3072000, 32000, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {2048000, 32000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1536000, 32000, 0x03, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7F, 0x02, 0x10, 0x10},
    {1024000, 32000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},

    /* 44.1k */
    {11289600, 44100, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {5644800, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {2822400, 44100, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1411200, 44100, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},

    /* 48k */
    {12288000, 48000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {18432000, 48000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {6144000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {3072000, 48000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1536000, 48000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},

    /* 64k */
    {12288000, 64000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {18432000, 64000, 0x03, 0x02, 0x03, 0x03, 0x01, 0x01, 0x7F, 0x06, 0x10, 0x10},
    {16384000, 64000, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {8192000, 64000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {6144000, 64000, 0x01, 0x02, 0x03, 0x03, 0x01, 0x01, 0x7F, 0x06, 0x10, 0x10},
    {4096000, 64000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {3072000, 64000, 0x01, 0x03, 0x03, 0x03, 0x01, 0x01, 0x7F, 0x06, 0x10, 0x10},
    {2048000, 64000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1536000, 64000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0xBF, 0x03, 0x18, 0x18},
    {1024000, 64000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7F, 0x02, 0x10, 0x10},

    /* 88.2k */
    {11289600, 88200, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {5644800, 88200, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {2822400, 88200, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1411200, 88200, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7F, 0x02, 0x10, 0x10},

    /* 96k */
    {12288000, 96000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {18432000, 96000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {6144000, 96000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {3072000, 96000, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0xFF, 0x04, 0x10, 0x10},
    {1536000, 96000, 0x01, 0x03, 0x01, 0x01, 0x01, 0x00, 0x7F, 0x02, 0x10, 0x10},
};

static int es8311_get_coeff_index(uint32_t mclk_hz, uint32_t sample_rate_hz)
{
    uint32_t count = sizeof(s_coeff_div) / sizeof(s_coeff_div[0]);
    for (uint32_t i = 0; i < count; i++)
    {
        if ((s_coeff_div[i].mclk_hz == mclk_hz) && (s_coeff_div[i].sample_rate_hz == sample_rate_hz))
        {
            return (int)i;
        }
    }

    return -1;
}

static es8311_err_t es8311_apply_resolution(es8311_resolution_t resolution, uint8_t *reg_value)
{
    if (reg_value == NULL)
    {
        return ES8311_ERR_INVALID_ARG;
    }

    switch (resolution)
    {
    case ES8311_RESOLUTION_16:
        *reg_value |= (3U << 2);
        break;
    case ES8311_RESOLUTION_18:
        *reg_value |= (2U << 2);
        break;
    case ES8311_RESOLUTION_20:
        *reg_value |= (1U << 2);
        break;
    case ES8311_RESOLUTION_24:
        *reg_value |= (0U << 2);
        break;
    case ES8311_RESOLUTION_32:
        *reg_value |= (4U << 2);
        break;
    default:
        return ES8311_ERR_INVALID_ARG;
    }

    return ES8311_OK;
}

es8311_err_t es8311_write_register(es8311_dev_t *dev, uint8_t reg_addr, uint8_t value)
{
    if (dev == NULL)
    {
        return ES8311_ERR_INVALID_ARG;
    }

    uint8_t write_buf[2] = {reg_addr, value};
    return i2c_write(&dev->i2c, write_buf, sizeof(write_buf)) ? ES8311_OK : ES8311_ERR_I2C;
}

es8311_err_t es8311_read_register(es8311_dev_t *dev, uint8_t reg_addr, uint8_t *value)
{
    if ((dev == NULL) || (value == NULL))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    return i2c_read_register(&dev->i2c, reg_addr, value, 1) ? ES8311_OK : ES8311_ERR_I2C;
}

es8311_err_t es8311_set_sample_frequency(es8311_dev_t *dev, uint32_t mclk_hz, uint32_t sample_rate_hz)
{
    if ((dev == NULL) || (sample_rate_hz < 8000U) || (sample_rate_hz > 96000U))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    int coeff_idx = es8311_get_coeff_index(mclk_hz, sample_rate_hz);
    if (coeff_idx < 0)
    {
        return ES8311_ERR_UNSUPPORTED_RATE;
    }

    const es8311_coeff_div_t *coeff = &s_coeff_div[coeff_idx];
    uint8_t regv = 0;

    if (es8311_read_register(dev, ES8311_CLK_MANAGER_REG02, &regv) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    regv &= 0x07U;
    regv |= (uint8_t)((coeff->pre_div - 1U) << 5);
    regv |= (uint8_t)(coeff->pre_multi << 3);
    if (es8311_write_register(dev, ES8311_CLK_MANAGER_REG02, regv) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }

    if (es8311_write_register(dev, ES8311_CLK_MANAGER_REG03, (uint8_t)((coeff->fs_mode << 6) | coeff->adc_osr)) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    if (es8311_write_register(dev, ES8311_CLK_MANAGER_REG04, coeff->dac_osr) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    if (es8311_write_register(dev, ES8311_CLK_MANAGER_REG05, (uint8_t)(((coeff->adc_div - 1U) << 4) | (coeff->dac_div - 1U))) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }

    if (es8311_read_register(dev, ES8311_CLK_MANAGER_REG06, &regv) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    regv &= 0xE0U;
    if (coeff->bclk_div < 19U)
    {
        regv |= (uint8_t)(coeff->bclk_div - 1U);
    }
    else
    {
        regv |= coeff->bclk_div;
    }
    if (es8311_write_register(dev, ES8311_CLK_MANAGER_REG06, regv) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }

    if (es8311_read_register(dev, ES8311_CLK_MANAGER_REG07, &regv) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    regv &= 0xC0U;
    regv |= coeff->lrck_h;
    if (es8311_write_register(dev, ES8311_CLK_MANAGER_REG07, regv) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    if (es8311_write_register(dev, ES8311_CLK_MANAGER_REG08, coeff->lrck_l) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }

    dev->sample_rate_hz = sample_rate_hz;
    dev->mclk_hz = mclk_hz;
    return ES8311_OK;
}

static es8311_err_t es8311_config_format(es8311_dev_t *dev, es8311_resolution_t res_in, es8311_resolution_t res_out)
{
    uint8_t reg00 = 0;
    uint8_t reg09 = 0;
    uint8_t reg0A = 0;

    if (es8311_read_register(dev, ES8311_RESET_REG00, &reg00) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    reg00 &= 0xBFU; /* Slave serial mode */
    if (es8311_write_register(dev, ES8311_RESET_REG00, reg00) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }

    if ((es8311_apply_resolution(res_in, &reg09) != ES8311_OK) ||
        (es8311_apply_resolution(res_out, &reg0A) != ES8311_OK))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    if ((es8311_write_register(dev, ES8311_SDPIN_REG09, reg09) != ES8311_OK) ||
        (es8311_write_register(dev, ES8311_SDPOUT_REG0A, reg0A) != ES8311_OK))
    {
        return ES8311_ERR_I2C;
    }

    return ES8311_OK;
}

es8311_err_t es8311_configure_microphone(es8311_dev_t *dev, bool digital_mic)
{
    if (dev == NULL)
    {
        return ES8311_ERR_INVALID_ARG;
    }

    uint8_t reg14 = 0x1AU; /* enable analog MIC and max PGA gain */
    if (digital_mic)
    {
        reg14 |= BIT(6);
    }

    if (es8311_write_register(dev, ES8311_ADC_REG17, 0xC8U) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    return es8311_write_register(dev, ES8311_SYSTEM_REG14, reg14);
}

es8311_err_t es8311_init(es8311_dev_t *dev, const es8311_config_t *config)
{
    if ((dev == NULL) || (config == NULL) || (config->i2c_port == NULL))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    if ((config->sample_rate_hz < 8000U) || (config->sample_rate_hz > 96000U))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    if ((!config->mclk_from_mclk_pin) && (config->resolution_in != config->resolution_out))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    dev->i2c.port = config->i2c_port;
    dev->i2c.slave_address = config->i2c_addr;
    dev->resolution_in = config->resolution_in;
    dev->resolution_out = config->resolution_out;
    dev->mclk_from_mclk_pin = config->mclk_from_mclk_pin;
    dev->initialized = false;

    /* Codec reset sequence */
    if ((es8311_write_register(dev, ES8311_RESET_REG00, 0x1FU) != ES8311_OK))
    {
        return ES8311_ERR_I2C;
    }
    delay_ms(20);
    if ((es8311_write_register(dev, ES8311_RESET_REG00, 0x00U) != ES8311_OK) ||
        (es8311_write_register(dev, ES8311_RESET_REG00, 0x80U) != ES8311_OK))
    {
        return ES8311_ERR_I2C;
    }

    uint8_t reg01 = 0x3FU;
    uint8_t reg06 = 0;
    uint32_t mclk_hz = config->mclk_hz;

    if (!config->mclk_from_mclk_pin)
    {
        mclk_hz = config->sample_rate_hz * (uint32_t)config->resolution_out * 2U;
        reg01 |= BIT(7); /* Select BCLK as MCLK source */
    }
    if (config->mclk_inverted)
    {
        reg01 |= BIT(6);
    }

    if (es8311_write_register(dev, ES8311_CLK_MANAGER_REG01, reg01) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }

    if (es8311_read_register(dev, ES8311_CLK_MANAGER_REG06, &reg06) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    if (config->sclk_inverted)
    {
        reg06 |= BIT(5);
    }
    else
    {
        reg06 &= (uint8_t)~BIT(5);
    }
    if (es8311_write_register(dev, ES8311_CLK_MANAGER_REG06, reg06) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }

    es8311_err_t rc = es8311_set_sample_frequency(dev, mclk_hz, config->sample_rate_hz);
    if (rc != ES8311_OK)
    {
        return rc;
    }

    rc = es8311_config_format(dev, config->resolution_in, config->resolution_out);
    if (rc != ES8311_OK)
    {
        return rc;
    }

    if ((es8311_write_register(dev, ES8311_SYSTEM_REG0D, 0x01U) != ES8311_OK) ||
        (es8311_write_register(dev, ES8311_SYSTEM_REG0E, 0x02U) != ES8311_OK) ||
        (es8311_write_register(dev, ES8311_SYSTEM_REG12, 0x00U) != ES8311_OK) ||
        (es8311_write_register(dev, ES8311_SYSTEM_REG13, 0x10U) != ES8311_OK) ||
        (es8311_write_register(dev, ES8311_ADC_REG1C, 0x6AU) != ES8311_OK) ||
        (es8311_write_register(dev, ES8311_DAC_REG37, 0x08U) != ES8311_OK))
    {
        return ES8311_ERR_I2C;
    }

    dev->initialized = true;
    return ES8311_OK;
}

es8311_err_t es8311_set_voice_volume(es8311_dev_t *dev, uint8_t volume_percent)
{
    if (dev == NULL)
    {
        return ES8311_ERR_INVALID_ARG;
    }

    if (volume_percent > 100U)
    {
        volume_percent = 100U;
    }

    uint8_t reg32 = 0;
    if (volume_percent > 0U)
    {
        reg32 = (uint8_t)(((uint16_t)volume_percent * 256U) / 100U - 1U);
    }

    return es8311_write_register(dev, ES8311_DAC_REG32, reg32);
}

es8311_err_t es8311_get_voice_volume(es8311_dev_t *dev, uint8_t *volume_percent)
{
    if ((dev == NULL) || (volume_percent == NULL))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    uint8_t reg32 = 0;
    if (es8311_read_register(dev, ES8311_DAC_REG32, &reg32) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }

    if (reg32 == 0U)
    {
        *volume_percent = 0U;
    }
    else
    {
        uint16_t vol = ((uint16_t)reg32 * 100U) / 256U + 1U;
        *volume_percent = (vol > 100U) ? 100U : (uint8_t)vol;
    }

    return ES8311_OK;
}

es8311_err_t es8311_set_voice_mute(es8311_dev_t *dev, bool mute)
{
    if (dev == NULL)
    {
        return ES8311_ERR_INVALID_ARG;
    }

    uint8_t reg31 = 0;
    if (es8311_read_register(dev, ES8311_DAC_REG31, &reg31) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }

    if (mute)
    {
        reg31 |= (uint8_t)(BIT(6) | BIT(5));
    }
    else
    {
        reg31 &= (uint8_t)~(BIT(6) | BIT(5));
    }

    return es8311_write_register(dev, ES8311_DAC_REG31, reg31);
}

es8311_err_t es8311_set_mic_gain(es8311_dev_t *dev, es8311_mic_gain_t gain)
{
    if ((dev == NULL) || (gain > ES8311_MIC_GAIN_42DB))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    return es8311_write_register(dev, ES8311_ADC_REG16, (uint8_t)gain);
}

es8311_err_t es8311_set_voice_fade(es8311_dev_t *dev, es8311_fade_t fade)
{
    if ((dev == NULL) || (fade > ES8311_FADE_65536LRCK))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    uint8_t reg37 = 0;
    if (es8311_read_register(dev, ES8311_DAC_REG37, &reg37) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    reg37 &= 0x0FU;
    reg37 |= ((uint8_t)fade << 4);

    return es8311_write_register(dev, ES8311_DAC_REG37, reg37);
}

es8311_err_t es8311_set_mic_fade(es8311_dev_t *dev, es8311_fade_t fade)
{
    if ((dev == NULL) || (fade > ES8311_FADE_65536LRCK))
    {
        return ES8311_ERR_INVALID_ARG;
    }

    uint8_t reg15 = 0;
    if (es8311_read_register(dev, ES8311_ADC_REG15, &reg15) != ES8311_OK)
    {
        return ES8311_ERR_I2C;
    }
    reg15 &= 0x0FU;
    reg15 |= ((uint8_t)fade << 4);

    return es8311_write_register(dev, ES8311_ADC_REG15, reg15);
}

void es8311_register_dump(es8311_dev_t *dev)
{
    if (dev == NULL)
    {
        return;
    }

    for (uint8_t reg = 0; reg < 0x4A; reg++)
    {
        uint8_t value = 0;
        if (es8311_read_register(dev, reg, &value) == ES8311_OK)
        {
            serial_printf("ES8311 REG[0x%02X] = 0x%02X\r\n", reg, value);
        }
        else
        {
            serial_printf("ES8311 REG[0x%02X] read failed\r\n", reg);
            break;
        }
    }
}
