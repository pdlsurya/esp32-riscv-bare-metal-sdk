#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hal/spi_ll.h"
#include "hal/spi_hal.h"
#include "hal/gpio_ll.h"
#include "delay.h"
#include "spi_drv.h"
#include "soc/gdma_channel.h"
#include "soc/soc_caps.h"

#if defined(TARGET_SOC_ESP32P4)
#include "hal/gdma_ll.h"
#include "hal/axi_dma_ll.h"
#include "hal/cache_ll.h"
#elif defined(TARGET_SOC_ESP32C6)
#include "hal/gdma_ll.h"
#endif

#define SPI_CLK_DUTY_50 128
#define SPI_DMA_TIMEOUT_US 200000U

#if defined(__riscv)
#define SPI_DMA_MEM_BARRIER() asm volatile("fence rw, rw" ::: "memory")
#else
#define SPI_DMA_MEM_BARRIER() \
    do                        \
    {                         \
    } while (0)
#endif

#if defined(TARGET_SOC_ESP32P4)
#define SPI_DMA_DESC_ALIGN 8U
#define SPI_DMA_BUF_ALIGN 16U
#define SPI_DMA_MAX_CHUNK DMA_DESCRIPTOR_BUFFER_MAX_SIZE_16B_ALIGNED
typedef axi_dma_dev_t spi_gdma_dev_t;
#define SPI_DMA_HW AXI_DMA_LL_GET_HW(0)
#define SPI_DMA_FORCE_ENABLE_CLOCK(dma)                              \
    do                                                               \
    {                                                                \
        _gdma_ll_enable_bus_clock(GDMA_LL_AXI_GROUP_START_ID, true); \
        axi_dma_ll_force_enable_reg_clock((dma), true);              \
        axi_dma_ll_set_default_memory_range((dma));                  \
    } while (0)
#define SPI_DMA_TX_RESET_CHANNEL(dma, ch) axi_dma_ll_tx_reset_channel((dma), (ch))
#define SPI_DMA_TX_ENABLE_OWNER_CHECK(dma, ch, enable) axi_dma_ll_tx_enable_owner_check((dma), (ch), (enable))
#define SPI_DMA_TX_ENABLE_DATA_BURST(dma, ch, enable) axi_dma_ll_tx_enable_data_burst((dma), (ch), (enable))
#define SPI_DMA_TX_ENABLE_DESC_BURST(dma, ch, enable) axi_dma_ll_tx_enable_descriptor_burst((dma), (ch), (enable))
#define SPI_DMA_TX_SET_EOF_MODE(dma, ch, mode) axi_dma_ll_tx_set_eof_mode((dma), (ch), (mode))
#define SPI_DMA_TX_ENABLE_AUTO_WB(dma, ch, enable) axi_dma_ll_tx_enable_auto_write_back((dma), (ch), (enable))
#define SPI_DMA_TX_SET_PRIORITY(dma, ch, priority) axi_dma_ll_tx_set_priority((dma), (ch), (priority))
#define SPI_DMA_TX_CONNECT(dma, ch, periph_id) axi_dma_ll_tx_connect_to_periph((dma), (ch), GDMA_TRIG_PERIPH_SPI, (periph_id))
#define SPI_DMA_TX_CLEAR_INTERRUPT(dma, ch, mask) axi_dma_ll_tx_clear_interrupt_status((dma), (ch), (mask))
#define SPI_DMA_TX_GET_INTERRUPT(dma, ch) axi_dma_ll_tx_get_interrupt_status((dma), (ch), true)
#define SPI_DMA_TX_SET_DESC(dma, ch, addr) axi_dma_ll_tx_set_desc_addr((dma), (ch), (addr))
#define SPI_DMA_TX_START(dma, ch) axi_dma_ll_tx_start((dma), (ch))
#define SPI_DMA_TX_STOP(dma, ch) axi_dma_ll_tx_stop((dma), (ch))
#define SPI_DMA_RX_RESET_CHANNEL(dma, ch) axi_dma_ll_rx_reset_channel((dma), (ch))
#define SPI_DMA_RX_ENABLE_OWNER_CHECK(dma, ch, enable) axi_dma_ll_rx_enable_owner_check((dma), (ch), (enable))
#define SPI_DMA_RX_ENABLE_DATA_BURST(dma, ch, enable) axi_dma_ll_rx_enable_data_burst((dma), (ch), (enable))
#define SPI_DMA_RX_ENABLE_DESC_BURST(dma, ch, enable) axi_dma_ll_rx_enable_descriptor_burst((dma), (ch), (enable))
#define SPI_DMA_RX_SET_PRIORITY(dma, ch, priority) axi_dma_ll_rx_set_priority((dma), (ch), (priority))
#define SPI_DMA_RX_CONNECT(dma, ch, periph_id) axi_dma_ll_rx_connect_to_periph((dma), (ch), GDMA_TRIG_PERIPH_SPI, (periph_id))
#define SPI_DMA_RX_CLEAR_INTERRUPT(dma, ch, mask) axi_dma_ll_rx_clear_interrupt_status((dma), (ch), (mask))
#define SPI_DMA_RX_GET_INTERRUPT(dma, ch) axi_dma_ll_rx_get_interrupt_status((dma), (ch), true)
#define SPI_DMA_RX_SET_DESC(dma, ch, addr) axi_dma_ll_rx_set_desc_addr((dma), (ch), (addr))
#define SPI_DMA_RX_START(dma, ch) axi_dma_ll_rx_start((dma), (ch))
#define SPI_DMA_RX_STOP(dma, ch) axi_dma_ll_rx_stop((dma), (ch))
#elif defined(TARGET_SOC_ESP32C6)
#define SPI_DMA_DESC_ALIGN 4U
#define SPI_DMA_BUF_ALIGN 4U
#define SPI_DMA_MAX_CHUNK DMA_DESCRIPTOR_BUFFER_MAX_SIZE_4B_ALIGNED
typedef gdma_dev_t spi_gdma_dev_t;
#define SPI_DMA_HW GDMA_LL_GET_HW(0)
#define SPI_DMA_FORCE_ENABLE_CLOCK(dma)                              \
    do                                                               \
    {                                                                \
        _gdma_ll_enable_bus_clock(GDMA_LL_AHB_GROUP_START_ID, true); \
        gdma_ll_force_enable_reg_clock((dma), true);                 \
    } while (0)
#define SPI_DMA_TX_RESET_CHANNEL(dma, ch) gdma_ll_tx_reset_channel((dma), (ch))
#define SPI_DMA_TX_ENABLE_OWNER_CHECK(dma, ch, enable) gdma_ll_tx_enable_owner_check((dma), (ch), (enable))
#define SPI_DMA_TX_ENABLE_DATA_BURST(dma, ch, enable) gdma_ll_tx_enable_data_burst((dma), (ch), (enable))
#define SPI_DMA_TX_ENABLE_DESC_BURST(dma, ch, enable) gdma_ll_tx_enable_descriptor_burst((dma), (ch), (enable))
#define SPI_DMA_TX_SET_EOF_MODE(dma, ch, mode) gdma_ll_tx_set_eof_mode((dma), (ch), (mode))
#define SPI_DMA_TX_ENABLE_AUTO_WB(dma, ch, enable) gdma_ll_tx_enable_auto_write_back((dma), (ch), (enable))
#define SPI_DMA_TX_SET_PRIORITY(dma, ch, priority) gdma_ll_tx_set_priority((dma), (ch), (priority))
#define SPI_DMA_TX_CONNECT(dma, ch, periph_id) gdma_ll_tx_connect_to_periph((dma), (ch), GDMA_TRIG_PERIPH_SPI, (periph_id))
#define SPI_DMA_TX_CLEAR_INTERRUPT(dma, ch, mask) gdma_ll_tx_clear_interrupt_status((dma), (ch), (mask))
#define SPI_DMA_TX_GET_INTERRUPT(dma, ch) gdma_ll_tx_get_interrupt_status((dma), (ch), true)
#define SPI_DMA_TX_SET_DESC(dma, ch, addr) gdma_ll_tx_set_desc_addr((dma), (ch), (addr))
#define SPI_DMA_TX_START(dma, ch) gdma_ll_tx_start((dma), (ch))
#define SPI_DMA_TX_STOP(dma, ch) gdma_ll_tx_stop((dma), (ch))
#define SPI_DMA_RX_RESET_CHANNEL(dma, ch) gdma_ll_rx_reset_channel((dma), (ch))
#define SPI_DMA_RX_ENABLE_OWNER_CHECK(dma, ch, enable) gdma_ll_rx_enable_owner_check((dma), (ch), (enable))
#define SPI_DMA_RX_ENABLE_DATA_BURST(dma, ch, enable) gdma_ll_rx_enable_data_burst((dma), (ch), (enable))
#define SPI_DMA_RX_ENABLE_DESC_BURST(dma, ch, enable) gdma_ll_rx_enable_descriptor_burst((dma), (ch), (enable))
#define SPI_DMA_RX_SET_PRIORITY(dma, ch, priority) gdma_ll_rx_set_priority((dma), (ch), (priority))
#define SPI_DMA_RX_CONNECT(dma, ch, periph_id) gdma_ll_rx_connect_to_periph((dma), (ch), GDMA_TRIG_PERIPH_SPI, (periph_id))
#define SPI_DMA_RX_CLEAR_INTERRUPT(dma, ch, mask) gdma_ll_rx_clear_interrupt_status((dma), (ch), (mask))
#define SPI_DMA_RX_GET_INTERRUPT(dma, ch) gdma_ll_rx_get_interrupt_status((dma), (ch), true)
#define SPI_DMA_RX_SET_DESC(dma, ch, addr) gdma_ll_rx_set_desc_addr((dma), (ch), (addr))
#define SPI_DMA_RX_START(dma, ch) gdma_ll_rx_start((dma), (ch))
#define SPI_DMA_RX_STOP(dma, ch) gdma_ll_rx_stop((dma), (ch))
#endif

#if defined(TARGET_SOC_ESP32P4)

#define SPI3_MOSI_GPIO_SIG 49 ///< GPIO matrix signal number of MOSI pin
#define SPI3_MISO_GPIO_SIG 48 ///< GPIO matrix signal number of MISO pin
#define SPI3_SCK_GPIO_SIG 47  ///< GPIO matrix signal number of SCK pin
#define SPI3_CS0_SIG 52       ///< GPIO matrix signal number of CS0 pin
#define SPI3_CS1_SIG 46       ///< GPIO matrix signal number of CS1 pin
#define SPI3_CS2_SIG 45       ///< GPIO matrix signal number of CS2 pin

#define SPI2_MOSI_GPIO_SIG 55 ///< GPIO matrix signal number of MOSI pin
#define SPI2_MISO_GPIO_SIG 54 ///< GPIO matrix signal number of MISO pin
#define SPI2_SCK_GPIO_SIG 53  ///< GPIO matrix signal number of SCK pin
#define SPI2_CS0_SIG 62       ///< GPIO matrix signal number of CS0 pin
#define SPI2_CS1_SIG 63       ///< GPIO matrix signal number of CS1 pin
#define SPI2_CS2_SIG 64       ///< GPIO matrix signal number of CS2 pin
#define SPI2_CS3_SIG 65       ///< GPIO matrix signal number of CS3 pin
#define SPI2_CS4_SIG 66       ///< GPIO matrix signal number of CS4 pin
#define SPI2_CS5_SIG 67       ///< GPIO matrix signal number of CS5 pin

#elif defined(TARGET_SOC_ESP32C6)

#define SPI2_MOSI_GPIO_SIG 65 ///< GPIO matrix signal number of MOSI pin
#define SPI2_MISO_GPIO_SIG 64 ///< GPIO matrix signal number of MISO pin
#define SPI2_SCK_GPIO_SIG 63  ///< GPIO matrix signal number of SCK pin
#define SPI2_CS0_SIG 68       ///< GPIO matrix signal number of CS0 pin
#define SPI2_CS1_SIG 101      ///< GPIO matrix signal number of CS1 pin
#define SPI2_CS2_SIG 102      ///< GPIO matrix signal number of CS2 pin
#define SPI2_CS3_SIG 103      ///< GPIO matrix signal number of CS3 pin
#define SPI2_CS4_SIG 104      ///< GPIO matrix signal number of CS4 pin
#define SPI2_CS5_SIG 105      ///< GPIO matrix signal number of CS5 pin

#define SPI3_MOSI_GPIO_SIG 22 ///< GPIO matrix signal number of MOSI pin
#define SPI3_MISO_GPIO_SIG 22 ///< GPIO matrix signal number of MISO pin
#define SPI3_SCK_GPIO_SIG 22  ///< GPIO matrix signal number of SCK pin
#define SPI3_CS0_SIG 22       ///< GPIO matrix signal number of CS0 pin
#define SPI3_CS1_SIG 22       ///< GPIO matrix signal number of CS1 pin
#define SPI3_CS2_SIG 22       ///< GPIO matrix signal number of CS2 pin

#endif

static bool driver_configured[2] = {false, false};

static uint8_t cs_signals[2][6] = {
    {SPI2_CS0_SIG, SPI2_CS1_SIG, SPI2_CS2_SIG, SPI2_CS3_SIG, SPI2_CS4_SIG, SPI2_CS5_SIG},
    {SPI3_CS0_SIG, SPI3_CS1_SIG, SPI3_CS2_SIG},

};

#if defined(TARGET_SOC_ESP32P4) || defined(TARGET_SOC_ESP32C6)
static spi_dma_desc_t s_spi_dma_tx_desc[2][SOC_GDMA_PAIRS_PER_GROUP_MAX] __attribute__((aligned(SPI_DMA_DESC_ALIGN)));
static spi_dma_desc_t s_spi_dma_rx_desc[2][SOC_GDMA_PAIRS_PER_GROUP_MAX] __attribute__((aligned(SPI_DMA_DESC_ALIGN)));
static uint8_t s_spi_dma_tx_buf[2][SOC_GDMA_PAIRS_PER_GROUP_MAX][SPI_DMA_MAX_CHUNK] __attribute__((aligned(SPI_DMA_BUF_ALIGN)));
static uint8_t s_spi_dma_rx_buf[2][SOC_GDMA_PAIRS_PER_GROUP_MAX][SPI_DMA_MAX_CHUNK] __attribute__((aligned(SPI_DMA_BUF_ALIGN)));

static int spi_get_port_index(spi_dev_t *port)
{
    if (port == &GPSPI2)
    {
        return 0;
    }
#if defined(TARGET_SOC_ESP32P4)
    if (port == &GPSPI3)
    {
        return 1;
    }
#endif
    return -1;
}

static int spi_get_dma_periph_id(spi_dev_t *port)
{
    if (port == &GPSPI2)
    {
        return SOC_GDMA_TRIG_PERIPH_SPI2;
    }
#if defined(TARGET_SOC_ESP32P4)
    if (port == &GPSPI3)
    {
        return SOC_GDMA_TRIG_PERIPH_SPI3;
    }
#endif
    return -1;
}

static spi_dma_desc_t *spi_get_dma_desc_cpu(spi_dma_desc_t *desc)
{
#if defined(TARGET_SOC_ESP32P4)
    return (spi_dma_desc_t *)CACHE_LL_L2MEM_NON_CACHE_ADDR(desc);
#else
    return desc;
#endif
}

static uint8_t *spi_get_dma_buf_cpu(uint8_t *buf)
{
#if defined(TARGET_SOC_ESP32P4)
    return (uint8_t *)CACHE_LL_L2MEM_NON_CACHE_ADDR(buf);
#else
    return buf;
#endif
}

static bool spi_dma_is_usable(const spi_dev_handle_t *dev, uint32_t len)
{
    if (dev == NULL || !dev->dma_enabled || len == 0U || len > SPI_DMA_MAX_CHUNK)
    {
        return false;
    }
    if (dev->dma_channel >= SOC_GDMA_PAIRS_PER_GROUP_MAX)
    {
        return false;
    }
    if (spi_get_port_index(dev->port) < 0 || spi_get_dma_periph_id(dev->port) < 0)
    {
        return false;
    }
    return true;
}

static void spi_dma_setup_descriptor(spi_dma_desc_t *desc_cpu, void *buffer, uint32_t len)
{
    memset(desc_cpu, 0, sizeof(*desc_cpu));
    desc_cpu->dw0.size = len;
    desc_cpu->dw0.length = len;
    desc_cpu->dw0.suc_eof = 1;
    desc_cpu->buffer = buffer;
    desc_cpu->next = NULL;
}

static void spi_dma_reset_periph(spi_dev_t *port)
{
    spi_ll_dma_tx_enable(port, false);
    spi_ll_dma_rx_enable(port, false);
    spi_ll_cpu_tx_fifo_reset(port);
    spi_ll_dma_tx_fifo_reset(port);
    spi_ll_dma_rx_fifo_reset(port);
    spi_ll_outfifo_empty_clr(port);
    spi_ll_infifo_full_clr(port);
    port->dma_int_clr.val = UINT32_MAX;
}

static void spi_dma_transceive(spi_dev_handle_t *dev, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len, bool hold_cs_low)
{
    spi_gdma_dev_t *dma = SPI_DMA_HW;
    int port_idx = spi_get_port_index(dev->port);
    int periph_id = spi_get_dma_periph_id(dev->port);
    uint8_t ch = dev->dma_channel;
    bool need_rx = (rx_buf != NULL);
    bool dma_error = false;
    bool dma_done = false;
    uint32_t tx_status = 0U;
    uint32_t rx_status = 0U;

    if (dma == NULL || port_idx < 0 || periph_id < 0)
    {
        return;
    }

    SPI_DMA_FORCE_ENABLE_CLOCK(dma);

    spi_dma_desc_t *tx_desc = &s_spi_dma_tx_desc[port_idx][ch];
    spi_dma_desc_t *tx_desc_cpu = spi_get_dma_desc_cpu(tx_desc);
    uint8_t *tx_dma_buf = &s_spi_dma_tx_buf[port_idx][ch][0];
    uint8_t *tx_dma_buf_cpu = spi_get_dma_buf_cpu(tx_dma_buf);

    if (tx_buf != NULL)
    {
        memcpy(tx_dma_buf_cpu, tx_buf, len);
    }
    else
    {
        memset(tx_dma_buf_cpu, 0xFF, len);
    }
    spi_dma_setup_descriptor(tx_desc_cpu, tx_dma_buf, len);

    spi_dma_desc_t *rx_desc = NULL;
    spi_dma_desc_t *rx_desc_cpu = NULL;
    uint8_t *rx_dma_buf = NULL;
    uint8_t *rx_dma_buf_cpu = NULL;
    if (need_rx)
    {
        rx_desc = &s_spi_dma_rx_desc[port_idx][ch];
        rx_desc_cpu = spi_get_dma_desc_cpu(rx_desc);
        rx_dma_buf = &s_spi_dma_rx_buf[port_idx][ch][0];
        rx_dma_buf_cpu = spi_get_dma_buf_cpu(rx_dma_buf);
        spi_dma_setup_descriptor(rx_desc_cpu, rx_dma_buf, len);
    }
    SPI_DMA_MEM_BARRIER();
    tx_desc_cpu->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    if (need_rx)
    {
        rx_desc_cpu->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    }

    spi_dma_reset_periph(dev->port);

    SPI_DMA_TX_STOP(dma, ch);
    SPI_DMA_TX_RESET_CHANNEL(dma, ch);
    SPI_DMA_TX_ENABLE_OWNER_CHECK(dma, ch, true);
    SPI_DMA_TX_ENABLE_DATA_BURST(dma, ch, false);
    SPI_DMA_TX_ENABLE_DESC_BURST(dma, ch, false);
    SPI_DMA_TX_SET_EOF_MODE(dma, ch, 1U);
    SPI_DMA_TX_ENABLE_AUTO_WB(dma, ch, true);
    SPI_DMA_TX_SET_PRIORITY(dma, ch, 1U);
    SPI_DMA_TX_CLEAR_INTERRUPT(dma, ch, UINT32_MAX);
    SPI_DMA_TX_CONNECT(dma, ch, periph_id);
    SPI_DMA_TX_SET_DESC(dma, ch, (uint32_t)(uintptr_t)tx_desc);

    if (need_rx)
    {
        SPI_DMA_RX_STOP(dma, ch);
        SPI_DMA_RX_RESET_CHANNEL(dma, ch);
        SPI_DMA_RX_ENABLE_OWNER_CHECK(dma, ch, true);
        SPI_DMA_RX_ENABLE_DATA_BURST(dma, ch, false);
        SPI_DMA_RX_ENABLE_DESC_BURST(dma, ch, false);
        SPI_DMA_RX_SET_PRIORITY(dma, ch, 1U);
        SPI_DMA_RX_CLEAR_INTERRUPT(dma, ch, UINT32_MAX);
        SPI_DMA_RX_CONNECT(dma, ch, periph_id);
        SPI_DMA_RX_SET_DESC(dma, ch, (uint32_t)(uintptr_t)rx_desc);
    }

    spi_ll_set_mosi_bitlen(dev->port, len * 8U);
    spi_ll_set_miso_bitlen(dev->port, len * 8U);
    spi_ll_master_set_mode(dev->port, dev->mode);
    spi_ll_master_set_clock_by_reg(dev->port, &dev->clk_reg_val);
    spi_ll_master_keep_cs(dev->port, hold_cs_low);
    spi_ll_master_select_cs(dev->port, dev->id);
    spi_ll_dma_set_rx_eof_generation(dev->port, false);
    spi_ll_apply_config(dev->port);

    if (need_rx)
    {
        spi_ll_dma_rx_fifo_reset(dev->port);
        spi_ll_infifo_full_clr(dev->port);
        spi_ll_dma_rx_enable(dev->port, true);
        SPI_DMA_RX_START(dma, ch);
    }
    spi_ll_dma_tx_fifo_reset(dev->port);
    spi_ll_outfifo_empty_clr(dev->port);
    spi_ll_dma_tx_enable(dev->port, true);
    spi_ll_enable_mosi(dev->port, true);
    spi_ll_enable_miso(dev->port, need_rx);
    SPI_DMA_TX_START(dma, ch);
    bool tx_fifo_ready = false;
    for (uint32_t budget = SPI_DMA_TIMEOUT_US; budget > 0U; budget--)
    {
        if (dev->port->dma_conf.dma_outfifo_empty == 0U)
        {
            tx_fifo_ready = true;
            break;
        }
        delay_us(1);
    }
    if (!tx_fifo_ready)
    {
        dma_error = true;
        tx_status = SPI_DMA_TX_GET_INTERRUPT(dma, ch);
        if (need_rx)
        {
            rx_status = SPI_DMA_RX_GET_INTERRUPT(dma, ch);
        }
        spi_dma_reset_periph(dev->port);
        SPI_DMA_TX_STOP(dma, ch);
        if (need_rx)
        {
            SPI_DMA_RX_STOP(dma, ch);
        }
        SPI_DMA_TX_CLEAR_INTERRUPT(dma, ch, tx_status);
        if (need_rx)
        {
            SPI_DMA_RX_CLEAR_INTERRUPT(dma, ch, rx_status);
        }
        return;
    }
    spi_ll_user_start(dev->port);

    for (uint32_t budget = SPI_DMA_TIMEOUT_US; budget > 0U; budget--)
    {
        tx_status = SPI_DMA_TX_GET_INTERRUPT(dma, ch);
        if (need_rx)
        {
            rx_status = SPI_DMA_RX_GET_INTERRUPT(dma, ch);
        }
        if ((tx_status & GDMA_LL_EVENT_TX_DESC_ERROR) != 0U ||
            (need_rx && (rx_status & (GDMA_LL_EVENT_RX_DESC_ERROR | GDMA_LL_EVENT_RX_ERR_EOF | GDMA_LL_EVENT_RX_DESC_EMPTY)) != 0U))
        {
            dma_error = true;
            break;
        }
        if ((tx_status & (GDMA_LL_EVENT_TX_EOF | GDMA_LL_EVENT_TX_DONE)) != 0U &&
            (!need_rx || (rx_status & (GDMA_LL_EVENT_RX_SUC_EOF | GDMA_LL_EVENT_RX_DONE)) != 0U))
        {
            dma_done = true;
            break;
        }
        delay_us(1);
    }

    if (!dma_done && !dma_error)
    {
        dma_error = true;
    }

    tx_status |= SPI_DMA_TX_GET_INTERRUPT(dma, ch);
    if (need_rx)
    {
        rx_status |= SPI_DMA_RX_GET_INTERRUPT(dma, ch);
    }

    if (!dma_error)
    {
        while (!spi_ll_usr_is_done(dev->port))
            ;
    }

    spi_dma_reset_periph(dev->port);
    SPI_DMA_TX_STOP(dma, ch);
    if (need_rx)
    {
        SPI_DMA_RX_STOP(dma, ch);
    }
    SPI_DMA_TX_CLEAR_INTERRUPT(dma, ch, tx_status);
    if (need_rx)
    {
        SPI_DMA_RX_CLEAR_INTERRUPT(dma, ch, rx_status);
        if (!dma_error)
        {
            memcpy(rx_buf, rx_dma_buf_cpu, len);
        }
    }
}
#endif

static void spi_gpio_config(spi_config_t *config)
{

    // Configure mosi pin
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[config->pins.mosi], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.func_out_sel_cfg[config->pins.mosi].out_sel = config->port == &GPSPI2 ? SPI2_MOSI_GPIO_SIG : SPI3_MOSI_GPIO_SIG;
    gpio_ll_output_enable(&GPIO, config->pins.mosi);
    gpio_ll_input_disable(&GPIO, config->pins.mosi);

    // Configure miso pin
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[config->pins.miso], PIN_FUNC_GPIO); // Set as GPIO
    gpio_ll_input_enable(&GPIO, config->pins.miso);
    GPIO.func_in_sel_cfg[config->port == &GPSPI2 ? SPI2_MISO_GPIO_SIG : SPI3_MISO_GPIO_SIG].sig_in_sel = 1;
    GPIO.func_in_sel_cfg[config->port == &GPSPI2 ? SPI2_MISO_GPIO_SIG : SPI3_MISO_GPIO_SIG].in_sel = config->pins.miso;

    // Configure sck pin
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[config->pins.sck], PIN_FUNC_GPIO); // Set as GPIO
    GPIO.func_out_sel_cfg[config->pins.sck].out_sel = config->port == &GPSPI2 ? SPI2_SCK_GPIO_SIG : SPI3_SCK_GPIO_SIG;
    gpio_ll_output_enable(&GPIO, config->pins.sck);
    gpio_ll_input_disable(&GPIO, config->pins.sck);
}

void spi_device_config(spi_dev_handle_t *dev)
{
    spi_ll_master_cal_clock(APB_CLK_FREQ, dev->speed_hz, SPI_CLK_DUTY_50, &dev->clk_reg_val);

    if (dev->cs_pin == SPI_CS_UNUSED)
    {
        return;
    }

    uint8_t spi_port_num = dev->port == &GPSPI2 ? 2 : 3;

    gpio_ll_func_sel(&GPIO, dev->cs_pin, PIN_FUNC_GPIO);
    GPIO.func_out_sel_cfg[dev->cs_pin].out_sel = cs_signals[spi_port_num - 2][dev->id];
    gpio_ll_output_enable(&GPIO, dev->cs_pin);
    gpio_ll_input_disable(&GPIO, dev->cs_pin);
}

void spi_init(spi_config_t *config)
{
    uint8_t spi_port_num = config->port == &GPSPI2 ? 2 : 3;
    if (driver_configured[spi_port_num - 2])
    {
        return;
    }

    spi_ll_set_clk_source(config->port, SPI_CLK_SRC_XTAL);

    spi_ll_master_init(config->port);

    spi_gpio_config(config);

    // enable full duplex mode
    spi_ll_set_half_duplex(config->port, false);

    spi_ll_apply_config(config->port);

    driver_configured[spi_port_num - 2] = true;
}

void spi_transceive(spi_dev_handle_t *dev, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t len, bool hold_cs_low)
{
#if defined(TARGET_SOC_ESP32P4) || defined(TARGET_SOC_ESP32C6)
    if (spi_dma_is_usable(dev, len))
    {
        spi_dma_transceive(dev, tx_buf, rx_buf, len, hold_cs_low);
        return;
    }
#endif

    uint32_t txn_count = (len + 63) / 64;

    for (int txn_idx = 0; txn_idx < txn_count; txn_idx++)
    {
        uint8_t tx_len = len >= 64 ? 64 : len;
        uint8_t words = (tx_len + 3) / 4;

        spi_ll_set_mosi_bitlen(dev->port, tx_len * 8);
        spi_ll_set_miso_bitlen(dev->port, tx_len * 8);
        spi_ll_enable_mosi(dev->port, 1);
        spi_ll_enable_miso(dev->port, 1);
        spi_ll_master_set_mode(dev->port, dev->mode);
        spi_ll_master_set_clock_by_reg(dev->port, &dev->clk_reg_val);
        const bool keep_cs = hold_cs_low || (txn_idx + 1 < txn_count);
        spi_ll_master_keep_cs(dev->port, keep_cs);
        spi_ll_master_select_cs(dev->port, dev->id);
        spi_ll_apply_config(dev->port);

        for (int i = 0; i < words; i++)
        {

            dev->port->data_buf[i].buf = ((uint32_t *)tx_buf)[i + (txn_idx * 16)];
        }
        // Start SPI transfer
        spi_ll_user_start(dev->port);

        // Wait until transfer is complete
        while (spi_ll_get_running_cmd(dev->port))
            ;
        if (rx_buf != NULL)
        {
            for (int i = 0; i < words; i++)
            {

                ((uint32_t *)rx_buf)[i + (txn_idx * 16)] = dev->port->data_buf[i].buf;
            }
        }

        len -= 64;
    }
}

/**
 * @brief Transmit byte and return received byte
 *
 * @param dev SPI device
 * @param byte TX byte
 * @return RX byte
 */
uint8_t spi_transfer_byte(spi_dev_handle_t *dev, uint8_t tx_byte, bool hold_cs_low)
{

    spi_ll_set_mosi_bitlen(dev->port, 8);
    spi_ll_set_miso_bitlen(dev->port, 8);
    spi_ll_enable_mosi(dev->port, 1);
    spi_ll_enable_miso(dev->port, 1);
    spi_ll_master_set_mode(dev->port, dev->mode);
    spi_ll_master_set_clock_by_reg(dev->port, &dev->clk_reg_val);
    spi_ll_master_keep_cs(dev->port, hold_cs_low);
    spi_ll_master_select_cs(dev->port, dev->id);
    spi_ll_apply_config(dev->port);

    dev->port->data_buf[0].buf = tx_byte;

    // Start SPI transfer
    spi_ll_user_start(dev->port);

    // Wait until transfer is complete
    while (spi_ll_get_running_cmd(dev->port))
        ;

    return dev->port->data_buf[0].buf;
}

void spi_send_dummy_clocks(spi_dev_handle_t *dev, uint32_t cycles, bool cs_low, bool hold_cs_low)
{
    while (cycles > 0U)
    {
        uint32_t chunk_cycles = (cycles > 256U) ? 256U : cycles;
        const bool keep_cs = cs_low && (hold_cs_low || (cycles > chunk_cycles));

        spi_ll_enable_mosi(dev->port, 0);
        spi_ll_enable_miso(dev->port, 0);
        spi_ll_set_dummy(dev->port, chunk_cycles);
        spi_ll_master_set_mode(dev->port, dev->mode);
        spi_ll_master_set_clock_by_reg(dev->port, &dev->clk_reg_val);
        spi_ll_master_keep_cs(dev->port, keep_cs);
        spi_ll_master_select_cs(dev->port, cs_low ? dev->id : -1);
        spi_ll_apply_config(dev->port);

        spi_ll_user_start(dev->port);
        while (spi_ll_get_running_cmd(dev->port))
            ;

        cycles -= chunk_cycles;
    }

    spi_ll_set_dummy(dev->port, 0);
    spi_ll_enable_mosi(dev->port, 1);
    spi_ll_enable_miso(dev->port, 1);
}
