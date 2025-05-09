#include "stdint.h"
#include "riscv/rv_utils.h"
#include "hal/usb_serial_jtag_ll.h"
#include "hal/systimer_ll.h"
#include "hal/gpio_ll.h"
#include "mtimer.h"
#include "usb_serial.h"

static mtimer_cb_init_t cb_init;

void mtimer_callback_init(mtimer_cb_init_t *p_mtimer_cb_init)
{
    cb_init.period_ticks = p_mtimer_cb_init->period_ticks;
    cb_init.cb_function = p_mtimer_cb_init->cb_function;
    SET_MTIMECMP((GET_MTIME() + p_mtimer_cb_init->period_ticks));
    MTIMER->mtime_ctl.m_int_enable = 1;

    rv_utils_intr_enable(1 << MTIMER_IRQn);
}

__attribute__((interrupt)) IRAM_ATTR void m_timer_interrupt_handler()
{
#if USE_ISR_STACK
    /*Load sp with isr/handler mode stack pointer stored in mscratch and store current thread mode sp to mscratch*/
    __asm__ volatile("csrrw sp,mscratch,sp");
#endif

    SET_MTIMECMP((GET_MTIME() + cb_init.period_ticks));

    /*Store mcause, mepc and mstatus in local variables before enabling interrupt nesting*/
    uint32_t mcause_val = RV_READ_CSR(mcause);
    uint32_t mepc_val = RV_READ_CSR(mepc);
    uint32_t mstatus_val = RV_READ_CSR(mstatus);

    // Invoke callback function
    cb_init.cb_function();

    /* Restore mstatus, mepc and mcause*/
    RV_WRITE_CSR(mstatus, mstatus_val);
    RV_WRITE_CSR(mcause, mcause_val);
    RV_WRITE_CSR(mepc, mepc_val);

#if USE_ISR_STACK
    /*Load sp with thread  mode stack pointer stored in mscratch and store current isr/handler mode sp to mscratch*/
    __asm__ volatile("csrrw sp,mscratch,sp");
#endif
}
