#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "hal/mwdt_ll.h"
#include "hal/rwdt_ll.h"
#include "hal/clk_tree_ll.h"
#include "riscv/rv_utils.h"
#include "mtimer.h"
#include "esp_rom_sys.h"

/**
 * @brief Vector table for Interrupts and Exceptions
 *
 */
extern void vector_table(void);

/* Extern declarations for linker symbols */
extern uint32_t __stack_top;  /* Top of the stack */
extern uint32_t __bss_start;  /* Start of the .bss section */
extern uint32_t __bss_end;    /* End of the .bss section */
extern uint32_t __data_start; /* Start of the .data section in RAM */
extern uint32_t __data_end;   /* End of the .data section in RAM */
extern uint32_t __data_load;  /* Data load address in the flash data mapping */
extern uint32_t __iram_load;
extern uint32_t __iram_start;
extern uint32_t __iram_end;

/*Flash image header for direct boot mode*/
__attribute__((section(".flash_header"), used)) const uint32_t flash_header[2] = {0xAEDB041D, 0xAEDB041D};

extern int main(void); // User's main function

static inline void cpu_init(void)
{
    rv_utils_intr_global_disable();
    REG_WRITE(INTERRUPT_CORE0_CPU_INT_ENABLE_REG, 0);
    REG_WRITE(INTERRUPT_CORE0_CPU_INT_THRESH_REG, RVHAL_INTR_ENABLE_THRESH);

    /*Set CPU clock source to PLL*/
    clk_ll_cpu_set_divider(1);
    clk_ll_cpu_set_freq_mhz_from_pll(160);
    clk_ll_cpu_set_src(SOC_CPU_CLK_SRC_PLL);
    esp_rom_set_cpu_ticks_per_us(160);
}

/**
 * @brief Disable Watchdog Timers
 *
 */
static void watchdog_disable()
{
    /*Disable RTC Watchdog Timer*/
    rwdt_ll_write_protect_disable(&RTCCNTL);
    rwdt_ll_disable(&RTCCNTL);
    rwdt_ll_set_flashboot_en(&RTCCNTL, false);
    rwdt_ll_write_protect_enable(&RTCCNTL);

    /*Disable Main System Watchdog Timer 0*/
    mwdt_ll_write_protect_disable(&TIMERG0);
    mwdt_ll_disable(&TIMERG0);
    mwdt_ll_set_flashboot_en(&TIMERG0, false);
    mwdt_ll_write_protect_enable(&TIMERG0);

    /*Disable Main System Watchdog Timer 1*/
    mwdt_ll_write_protect_disable(&TIMERG1);
    mwdt_ll_disable(&TIMERG1);
    mwdt_ll_set_flashboot_en(&TIMERG1, false);
    mwdt_ll_write_protect_enable(&TIMERG1);

    /*Disable Super Watchdog Timer*/
    RTCCNTL.swd_wprotect = 0x8F1D312A; // Disable write protect
    RTCCNTL.swd_conf.swd_auto_feed_en = 1;
    RTCCNTL.swd_wprotect = 0;
}

__attribute__((noreturn, used)) static void reset_handler(void)
{

    cpu_init();

    watchdog_disable();

    /* Instruction RAM has a separate data-bus alias on ESP32-C3. */
    uint32_t *src = &__iram_load;
    uint32_t *iram = (uint32_t *)MAP_IRAM_TO_DRAM((uintptr_t)&__iram_start);
    uint32_t *iram_end = (uint32_t *)MAP_IRAM_TO_DRAM((uintptr_t)&__iram_end);
    while (iram < iram_end)
    {
        *iram++ = *src++;
    }
    __asm__ volatile("fence.i" ::: "memory");

    src = &__data_load;
    uint32_t *dest = &__data_start;

    // Copy the data segment
    while (dest < &__data_end)
    {
        *dest++ = *src++;
    }

    // Zero initialize the BSS section
    uint32_t *bss = &__bss_start;
    while (bss < &__bss_end)
    {
        *bss++ = 0;
    }

    rv_utils_set_mtvec((uint32_t)&vector_table);

    // Enable the timer backing the mtimer API
    mtimer_enable();

    /* Jump to the main function */
    main();

    /* Infinite loop in case main() returns */
    while (1)
    {
    }
}

/**
 * @brief Entry Point: Setup stack pointer and call reset handler
 *
 */
__attribute__((section(".entry"), naked, noreturn, used)) void _start(void)
{
    __asm__ volatile(
        ".option push\n"
        ".option norelax\n"
        "la gp, __global_pointer$\n"
        ".option pop\n"
        "la sp, __stack_top\n" // Load the stack pointer
        "j reset_handler\n");  // Jump to the reset handler
}
