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
    RV_SWAP_CSR(mscratch, sp); //__asm__ volatile("csrrw sp,mscratch,sp");
#endif

    SET_MTIMECMP((GET_MTIME() + cb_init.period_ticks));

    // Invoke callback function
    cb_init.cb_function();

#if USE_ISR_STACK
    /*Load sp with thread  mode stack pointer stored in mscratch and store current isr/handler mode sp to mscratch*/
    RV_SWAP_CSR(mscratch, sp); // __asm__ volatile("csrrw sp,mscratch,sp");

#endif
}
