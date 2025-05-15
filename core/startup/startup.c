#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "hal/mwdt_ll.h"
#include "hal/rwdt_ll.h"
#include "hal/clk_tree_ll.h"
#include "riscv/rv_utils.h"
#include "mtimer.h"

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
extern uint32_t __text_end;   /* End of the .text section in Flash */

/*Flash image header for direct boot mode*/
__attribute__((section(".flash_header"), used)) const uint32_t flash_header[2] = {0xAEDB041D, 0xAEDB041D};

extern int main(void); // User's main function

/**
 * @brief Disable Watchdog Timers
 *
 */
static void watchdog_disable()
{
    /*Disable RTC Watchdog Timer*/
    rwdt_ll_write_protect_disable(&LP_WDT);
    rwdt_ll_disable(&LP_WDT);
    rwdt_ll_set_flashboot_en(&LP_WDT, false);
    rwdt_ll_write_protect_enable(&LP_WDT);

    /*Disable Main System Watchdog Timer 0*/
    mwdt_ll_write_protect_disable(&TIMERG0);
    mwdt_ll_disable(&TIMERG0);
    mwdt_ll_set_flashboot_en(&TIMERG0, false);
    mwdt_ll_write_protect_enable(&TIMERG0);

    /*Disable Main System Watchdog Timer 0*/
    mwdt_ll_write_protect_disable(&TIMERG1);
    mwdt_ll_disable(&TIMERG1);
    mwdt_ll_set_flashboot_en(&TIMERG1, false);
    mwdt_ll_write_protect_enable(&TIMERG1);

    /*Disable Super Watchdog Timer*/
    (&LP_WDT)->swd_wprotect.swd_wkey = TIMG_WDT_WKEY_VALUE; // Disable write protect
    (&LP_WDT)->swd_config.swd_auto_feed_en = 1;
    (&LP_WDT)->swd_wprotect.swd_wkey = 0;
}

__attribute__((noreturn, used)) static void reset_handler(void)
{

    watchdog_disable();

    /*Set CPU clock source to PLL*/
    clk_ll_cpu_set_src(SOC_CPU_CLK_SRC_PLL);

    uint32_t *src = &__text_end;
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

    // Enable machine timer
    mtimer_enable();

    /* Jump to the main function */
    main();

    /* Infinite loop in case main() returns */
    while (1)
    {
    }
}

/**
 * @brief Entry Point: Setup vector table, stack pointer and call reset handler
 *
 */
__attribute__((section(".entry"), naked, noreturn, used)) void _start(void)
{
    rv_utils_set_mtvec((uint32_t)&vector_table); // Set the mtvec register with the address of the vector table

    __asm__ volatile(
        "la sp, __stack_top\n" // Load the stack pointer
        "j reset_handler\n");  // Jump to the reset handler
}