#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "riscv/rv_utils.h"
#include "usb_serial.h"
#include "interrupts.h"
#include "esp_rom_sys.h"
#include "mtimer.h"

#define INTERRUPT_HANDLER_COUNT 32U
#define MCAUSE_INTERRUPT_BIT 0x80000000UL
#define EXCEPTION_CODE_MASK 0x3FU
#define EXCEPTION_PC_ADVANCE 4U

/**
 * @brief Interrupt handler table
 */
static interrupt_handler_t interrupt_handlers[INTERRUPT_HANDLER_COUNT];

/**
 * @brief Register interrupt handler
 *
 * @param interrupt_number Interrupt number
 * @param handler Interrupt handler
 */
void register_interrupt_handler(uint8_t interrupt_number, interrupt_handler_t handler)
{
    if (interrupt_number >= INTERRUPT_HANDLER_COUNT)
    {
        return;
    }

    interrupt_handlers[interrupt_number] = handler;
}

/**
 * @brief Generic interrupt handler
 *
 * @param interrupt_number Interrupt number
 */
static inline void generic_interrupt_handler(uint8_t interrupt_number)
{
    if (interrupt_number < RV_EXTERNAL_INT_OFFSET)
    {
        return;
    }

    uint32_t handler_index = (uint32_t)(interrupt_number - RV_EXTERNAL_INT_OFFSET);
    if (handler_index >= RV_EXTERNAL_INT_COUNT)
    {
        return;
    }

    if (interrupt_handlers[handler_index] != NULL)
    {
        interrupt_handlers[handler_index]();
    }
}

__attribute__((weak)) void ecall_handler()
{
    while (1)
    {
    }
}

/**
 * @brief Machine Timer interrupt handler
 */
__attribute__((interrupt, weak)) TCM_IRAM_ATTR void m_timer_interrupt_handler()
{
    while (1)
    {
    }
}

/**
 * @brief Machine software interrupt handler
 */
__attribute__((interrupt, weak)) TCM_IRAM_ATTR void m_software_interrupt_handler()
{
    msi_clear();
    printf("Machine software interrupt from core->%d\n", rv_utils_get_core_id());
}

/**
 * @brief Dummy interrupt handler
 */
__attribute__((interrupt, weak)) IRAM_ATTR void dummy_interrupt_handler()
{
    uint32_t arg;
    asm volatile("mv %0,a0" : "=r"(arg));

    while (1)
    {
    }
}

/**
 * @brief External interrupt handler
 */
__attribute__((interrupt, weak)) IRAM_ATTR void ext_interrupt_handler()
{
#if USE_ISR_STACK
    /* Load sp with ISR stack pointer from mscratch and save current thread sp to mscratch. */
    __asm__ volatile("csrrw sp,mscratch,sp");
#endif

    /* Store mcause, mepc and mstatus before enabling interrupt nesting. */
    uint32_t mcause = RV_READ_CSR(mcause);
    uint32_t mepc = RV_READ_CSR(mepc);
    uint32_t mstatus = RV_READ_CSR(mstatus);
    /*
     * On ESP32-P4 CLIC, mcause may contain non-id bits above the low byte.
     * Use the low 8 bits to preserve the previously working dispatch behavior.
     */
    uint8_t interrupt_number = (uint8_t)(mcause & (~MCAUSE_INTERRUPT_BIT));

    /* Enable nested interrupts. */
    RV_SET_CSR(mstatus, MSTATUS_MIE);

    /* Call the generic interrupt handler with the interrupt number. */
    generic_interrupt_handler(interrupt_number);

    /* Restore mstatus, mepc and mcause. */
    RV_WRITE_CSR(mstatus, mstatus);
    RV_WRITE_CSR(mcause, mcause);
    RV_WRITE_CSR(mepc, mepc);

#if USE_ISR_STACK
    /* Load sp with thread mode stack pointer from mscratch and save current ISR sp to mscratch. */
    __asm__ volatile("csrrw sp,mscratch,sp");
#endif
}

/**
 * @brief Exception handler
 */
__attribute__((section(".exception_handler"), aligned(64), interrupt)) void exception_handler()
{
    uint32_t mcause = RV_READ_CSR(mcause);
    uint32_t mtval = RV_READ_CSR(mtval);
    uint32_t mepc = RV_READ_CSR(mepc);
    uint32_t code = mcause & EXCEPTION_CODE_MASK;

    /* Increment mepc to skip the instruction that caused the exception. */
    mepc = RV_READ_CSR(mepc);
    mepc += EXCEPTION_PC_ADVANCE;
    RV_WRITE_CSR(mepc, mepc);

    if (code == CAUSE_MACHINE_ECALL || code == CAUSE_USER_ECALL)
    {
        ecall_handler();
    }
    else
    {
        printf("Core %d panicked!\nException Id =%ld\nmtval=%lx\nmepc=%lx\n", rv_utils_get_core_id(), code, mtval, mepc);

        esp_rom_software_reset_system();
    }
}

/**
 * @brief Vector table for Interrupts/Exceptions
 */
__attribute__((section(".vector_table"))) interrupt_handler_t vector_table[48] = {
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    m_software_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    m_timer_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    dummy_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler,
    ext_interrupt_handler};
