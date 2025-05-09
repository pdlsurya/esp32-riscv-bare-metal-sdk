#pragma once

#include "riscv/rv_utils.h"
#include "soc/soc.h"

#define USE_ISR_STACK 0

#define MSIP_REG 0x20001800
#define MSI_IRQn 3

// Exception cause codes
#define CAUSE_MISALIGNED_FETCH 0x0
#define CAUSE_FETCH_ACCESS 0x1
#define CAUSE_ILLEGAL_INSTRUCTION 0x2
#define CAUSE_BREAKPOINT 0x3
#define CAUSE_MISALIGNED_LOAD 0x4
#define CAUSE_LOAD_ACCESS 0x5
#define CAUSE_MISALIGNED_STORE 0x6
#define CAUSE_STORE_ACCESS 0x7
#define CAUSE_USER_ECALL 0x8
#define CAUSE_SUPERVISOR_ECALL 0x9
#define CAUSE_HYPERVISOR_ECALL 0xA
#define CAUSE_MACHINE_ECALL 0xB
#define CAUSE_FETCH_PAGE_FAULT 0xC
#define CAUSE_LOAD_PAGE_FAULT 0xD
#define CAUSE_STORE_PAGE_FAULT 0xF

typedef void (*interrupt_handler_t)();

/**
 * @brief Enable machine software interrupt
 *
 */
static inline void msi_enable()
{
    rv_utils_intr_enable(1 << MSI_IRQn);
}

/**
 * @brief Trigger machine software interrupt
 *
 */
static inline void msi_trigger()
{
    REG_WRITE(MSIP_REG, 1);
}

/**
 * @brief Clear pending machine software interrupt
 *
 */
static inline void msi_clear()
{
    REG_WRITE(MSIP_REG, 0);
}

/**
 * @brief Register interrupt handler for a specific interrupt number
 *
 * @param interrupt_number
 * @param handler
 */
void register_interrupt_handler(uint8_t interrupt_number, interrupt_handler_t handler);
