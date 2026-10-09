#include "gd32f470_platform.h"
#include "arch/nx_arch.h"
#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
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
static bool gd32_timebase_owned;
int nx_gd32f470_timebase_init(void) {
    if (gd32_timebase_owned) { return -1; }
    /* Acquire before the first write. A failed initialization still owns
     * every partial timer/IRQ/clock effect until successful cleanup. */
    gd32_timebase_owned = true;
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
    uint32_t bank = (uint32_t)TIMER1_IRQn / 32u;
    uint32_t bit = UINT32_C(1) << ((uint32_t)TIMER1_IRQn % 32u);
    return (RCU_APB1EN & RCU_APB1EN_TIMER1EN) != 0u &&
           (RCU_APB1SPEN & RCU_APB1SPEN_TIMER1SPEN) != 0u &&
           (TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) != 0u &&
           (TIMER_DMAINTEN(TIMER1) & TIMER_DMAINTEN_UPIE) != 0u &&
           TIMER_PSC(TIMER1) == 99u && TIMER_CAR(TIMER1) == UINT32_MAX &&
           (NVIC->ISER[bank] & bit) != 0u ? 0 : -1;
}

nx_status_t nx_gd32f470_resources_idle(void) {
    /* F470's last external vector is IPA (90), beyond the FPU vector. The
     * dedicated TIMER1 belongs to the platform; active IRQs are never waived. */
    for (uint32_t irq = 0; irq <= (uint32_t)IPA_IRQn; ++irq) {
        uint32_t bank = irq / 32u;
        uint32_t bit = UINT32_C(1) << (irq % 32u);
        if ((NVIC->IABR[bank] & bit) != 0u ||
            ((irq != (uint32_t)TIMER1_IRQn || !gd32_timebase_owned) &&
             ((NVIC->ISER[bank] | NVIC->ISPR[bank]) & bit) != 0u)) {
            return NX_ERR_BUSY;
        }
    }
    for (uint32_t channel = 0; channel < 8u; ++channel) {
        if ((DMA_CHCTL(DMA0, channel) & DMA_CHXCTL_CHEN) != 0u ||
            (DMA_CHCTL(DMA1, channel) & DMA_CHXCTL_CHEN) != 0u) {
            return NX_ERR_BUSY;
        }
    }
    return NX_OK;
}

int nx_gd32f470_timebase_deinit(void) {
    if (!gd32_timebase_owned) { return 0; }
    uint32_t bank = (uint32_t)TIMER1_IRQn / 32u;
    uint32_t bit = UINT32_C(1) << ((uint32_t)TIMER1_IRQn % 32u);
    if ((NVIC->IABR[bank] & bit) != 0u) {
        return -1;
    }
    NVIC_DisableIRQ(TIMER1_IRQn);
    timer_disable(TIMER1);
    TIMER_DMAINTEN(TIMER1) = 0u;
    timer_interrupt_flag_clear(TIMER1, TIMER_INT_FLAG_UP);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    if ((TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) != 0u ||
        TIMER_DMAINTEN(TIMER1) != 0u ||
        (NVIC->ISER[bank] & bit) != 0u || (NVIC->ISPR[bank] & bit) != 0u) {
        return -1;
    }
    rcu_periph_clock_sleep_disable(RCU_TIMER1_SLP);
    rcu_periph_clock_disable(RCU_TIMER1);
    if ((RCU_APB1SPEN & RCU_APB1SPEN_TIMER1SPEN) != 0u ||
        (RCU_APB1EN & RCU_APB1EN_TIMER1EN) != 0u) {
        return -1;
    }
    timer_epoch = 0u;
    gd32_timebase_owned = false;
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
