#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "riscv/rv_utils.h"
#include "usb_serial.h"
#include "interrupts.h"
#include "esp_rom_sys.h"

/**
 * @brief Interrupt handler table
 *
 */
static interrupt_handler_t interrupt_handlers[32];

/**
 * @brief Register interrupt handler
 *
 * @param interrupt_number Interrupt number
 * @param handler Interrupt handler
 */
void register_interrupt_handler(uint8_t interrupt_number, interrupt_handler_t handler)
{
    interrupt_handlers[interrupt_number] = handler;
}

/**
 * @brief Generic interrupt handler
 *
 * @param interrupt_number Interrupt number
 */
static inline void generic_interrupt_handler(uint8_t interrupt_number)
{
    assert_valid_rv_int_num(interrupt_number);

    if (interrupt_handlers[interrupt_number] != NULL)
    {
        interrupt_handlers[interrupt_number]();
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
 *
 */
__attribute__((interrupt, weak)) IRAM_ATTR void m_timer_interrupt_handler()
{
    while (1)
        ;
}

/**
 * @brief Machine software interrupt handler
 *
 */
__attribute__((interrupt, weak)) IRAM_ATTR void m_software_interrupt_handler()
{
    msi_clear();
    while (1)
        ;
}

/**
 * @brief User Timer interrupt handler
 *
 */
__attribute__((interrupt("user"), weak)) IRAM_ATTR void u_timer_interrupt_handler()
{
    uint32_t arg;
    asm volatile("mv %0,a0" : "=r"(arg));

    while (1)
        ;
}

/**
 * @brief  interrupt handler
 *
 */
__attribute__((interrupt, weak)) IRAM_ATTR void ext_interrupt_handler()
{
#if USE_ISR_STACK
    /*Load sp with isr/handler mode stack pointer stored in mscratch and store current thread mode sp to mscratch*/
    __asm__ volatile("csrrw sp,mscratch,sp");
#endif
    /*Store mcause, mepc and mstatus in local variables before enabling interrupt nesting*/
    uint32_t mcause = RV_READ_CSR(mcause);
    uint32_t mepc = RV_READ_CSR(mepc);
    uint32_t mstatus = RV_READ_CSR(mstatus);
    uint32_t interrupt_number = mcause & ~(0x80000000);

    /*Enable nested interrupts*/
    RV_SET_CSR(mstatus, MSTATUS_MIE);

    // Call the generic interrupt handler with the interrupt number
    generic_interrupt_handler(interrupt_number);

    /* Restore mstatus, mepc and mcause*/
    RV_WRITE_CSR(mstatus, mstatus);
    RV_WRITE_CSR(mcause, mcause);
    RV_WRITE_CSR(mepc, mepc);

#if USE_ISR_STACK
    /*Load sp with thread  mode stack pointer stored in mscratch and store current isr/handler mode sp to mscratch*/
    __asm__ volatile("csrrw sp,mscratch,sp");
#endif
}

/**
 * @brief Exception handler
 *
 */

__attribute__((interrupt)) IRAM_ATTR void exception_handler()
{

    uint32_t mcause = RV_READ_CSR(mcause);
    uint32_t mtval = RV_READ_CSR(mtval);
    uint32_t mepc = RV_READ_CSR(mepc);

    // Increment mepc to skip the instruction that caused the exception
    mepc = RV_READ_CSR(mepc);
    mepc += 4;
    RV_WRITE_CSR(mepc, mepc);

    if (mcause == CAUSE_MACHINE_ECALL || mcause == CAUSE_USER_ECALL)
    {

        ecall_handler();
    }
    else
    {

        serial_printf("Core panicked!\nException Id =%ld\nmtval=%lx\nmepc=%lx\n", mcause, mtval, mepc);

        esp_rom_software_reset_cpu(0);
    }
}
/**
 * @brief Vector table for Interrupts/Exceptions
 *
 */
__attribute__((section(".vector_table"), naked)) void vector_table()
{
    __asm__ volatile("j exception_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j m_software_interrupt_handler\n"
                     "j u_timer_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j m_timer_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n"
                     "j ext_interrupt_handler\n");
}
