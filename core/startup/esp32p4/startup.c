#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "hal/mwdt_ll.h"
#include "hal/rwdt_ll.h"
#include "hal/clk_tree_ll.h"
#include "hal/cpu_utility_ll.h"
#include "esp_rom_sys.h"
#include "riscv/rv_utils.h"
#include "interrupts.h"
#include "mtimer.h"
#include "usb_serial.h"
#include "startup.h"

#define CORE1_STACK_SIZE 2048
/**
 * @brief Vector table for Interrupts and Exceptions
 *
 */
extern interrupt_handler_t vector_table[48];
extern void exception_handler(void);
void _start(void);

/* Extern declarations for linker symbols */
extern uint32_t __stack_top;  /* Top of the stack */
extern uint32_t __bss_start;  /* Start of the .bss section */
extern uint32_t __bss_end;    /* End of the .bss section */
extern uint32_t __data_start; /* Start of the .data section in RAM */
extern uint32_t __data_end;   /* End of the .data section in RAM */
extern uint32_t __data_load;  /* Start of the .text section in Flash */
extern uint32_t __tcm_start;
extern uint32_t __tcm_end;
extern uint32_t __tcm_load;

/*Flash image header for direct boot mode*/
__attribute__((section(".flash_header"), used)) const uint32_t flash_header[2] = {0xAEDB041D, 0xAEDB041D};

__attribute__((aligned(8))) uint32_t core1_stack[CORE1_STACK_SIZE];

extern int main(void); // User's main function

static entry_func_t core1_main_func;

static inline void cpu_init()
{
    rv_utils_set_mtvec((uint32_t)&exception_handler); // Set the mtvec register with the address of the exception handler

    rv_utils_set_mtvt((uint32_t)&vector_table); // Set the mtvt register with the address of the vector table

    /*Set CPU clock source to PLL*/
    clk_ll_cpu_set_src(SOC_CPU_CLK_SRC_PLL);

    // Start with enabling fpu
    rv_utils_enable_fpu();
}

/**
 * @brief Set core 1 stack pointer and jump to user's main function of core 1
 *
 */
void core1_reset_handler(void)
{

    asm volatile("la sp, core1_stack + %0" ::"i"(CORE1_STACK_SIZE)); // Load the stack pointer

    cpu_init();

    ets_set_appcpu_boot_addr(0);

    // Jump to user's main function
    core1_main_func();
}

/**
 * @brief Starts the application CPU (core 1) and executes the given entry function.
 *
 * This function sets the application CPU's boot address to the given entry function,
 * enables the application CPU's clock and resets the application CPU, and then
 * unstalls the application CPU.
 *
 * @param[in] entry_func The entry function to execute on the application CPU.
 */
void core1_start(entry_func_t entry_func)
{
    core1_main_func = entry_func;

    ets_set_appcpu_boot_addr((uint32_t)core1_reset_handler);

    cpu_utility_ll_unstall_cpu(1);

    cpu_utility_ll_enable_clock_and_reset_app_cpu();
}

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

    /*Disable Main System Watchdog Timer 1*/
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

    cpu_init();

    // Disable watchdog timers
    watchdog_disable();

    uint32_t *data_src = &__data_load;
    uint32_t *data_dest = &__data_start;

    // Copy the data segment
    while (data_dest < &__data_end)
    {
        *data_dest++ = *data_src++;
    }

    uint32_t *tcm_dst = &__tcm_start;
    uint32_t *tcm_src = &__tcm_load;
    // Copy TCM segment
    while (tcm_dst < &__tcm_end)
    {
        *tcm_dst++ = *tcm_src++;
    }

    // Zero initialize the BSS section
    uint32_t *bss = &__bss_start;
    while (bss < &__bss_end)
    {
        *bss++ = 0;
    }

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

    asm volatile("la sp, __stack_top\n"
                 "j reset_handler"); // Load the stack pointer
}