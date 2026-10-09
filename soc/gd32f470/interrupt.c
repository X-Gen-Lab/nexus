#include "gd32f470_platform.h"
#include "arch/nx_arch.h"
#include "gd32f4xx.h"
#include "nexus_config.h"
#ifdef NX_CONFIG_OSAL_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
extern void xPortSysTickHandler(void);
#endif

/* Dedicated 32-bit TIMER1 runs at 1MHz even while tasks sleep. It is a SoC
 * resource, never exported as an application timer. The overflow ISR extends
 * it to 64 bits; interrupt masking must remain below its 71-minute wrap. */
static volatile uint32_t timer_epoch;
int nx_gd32f470_timebase_init(void) {
    rcu_periph_clock_enable(RCU_TIMER1);
    rcu_periph_clock_sleep_enable(RCU_TIMER1_SLP);
    timer_deinit(TIMER1);
    timer_parameter_struct parameters;
    timer_struct_para_init(&parameters);
    parameters.prescaler = 99u; /* APB1=50MHz, timer multiplier=2 -> 100MHz. */
    parameters.alignedmode = TIMER_COUNTER_EDGE;
    parameters.counterdirection = TIMER_COUNTER_UP;
    parameters.period = UINT32_MAX;
    parameters.clockdivision = TIMER_CKDIV_DIV1;
    timer_init(TIMER1, &parameters);
    timer_counter_value_config(TIMER1, 0u);
    timer_epoch = 0u;
    timer_interrupt_flag_clear(TIMER1, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER1, TIMER_INT_UP);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_SetPriority(TIMER1_IRQn, 5u);
    NVIC_EnableIRQ(TIMER1_IRQn);
    timer_enable(TIMER1);
    return 0;
}
uint64_t nx_gd32f470_timestamp_us(void) {
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    uint32_t epoch = timer_epoch;
    uint32_t count = timer_counter_read(TIMER1);
    if (timer_interrupt_flag_get(TIMER1, TIMER_INT_FLAG_UP) != RESET) {
        /* Wrap can be pending behind this caller: sample the post-wrap count
         * without consuming the flag owned by the overflow ISR. */
        count = timer_counter_read(TIMER1);
        ++epoch;
    }
    uint64_t result = ((uint64_t)epoch << 32) | count;
    nx_arch_irq_restore(previous);
    return result;
}
uint32_t nx_gd32f470_millis(void) { return (uint32_t)(nx_gd32f470_timestamp_us() / 1000u); }
void TIMER1_IRQHandler(void) {
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (timer_interrupt_flag_get(TIMER1, TIMER_INT_FLAG_UP) != RESET) {
        ++timer_epoch;
        timer_interrupt_flag_clear(TIMER1, TIMER_INT_FLAG_UP);
    }
    nx_arch_irq_restore(previous);
}
void SysTick_Handler(void) {
#ifdef NX_CONFIG_OSAL_FREERTOS
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) { xPortSysTickHandler(); }
#endif
}
void NMI_Handler(void) { for (;;) { __NOP(); } }
void HardFault_Handler(void) { for (;;) { __NOP(); } }
void MemManage_Handler(void) { for (;;) { __NOP(); } }
void BusFault_Handler(void) { for (;;) { __NOP(); } }
void UsageFault_Handler(void) { for (;;) { __NOP(); } }
#ifndef NX_CONFIG_OSAL_FREERTOS
void SVC_Handler(void) { }
void PendSV_Handler(void) { }
#endif
