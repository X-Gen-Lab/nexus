/** Production ISR manager with real STM32F407 IRQ enums and NVIC call probes.
 * This is a host regression; ARM preemption and electrical HIL are separate. */
#include "hal/resource/nx_isr_manager.h"
#include "interrupt/stm32_interrupt.h"
#include <assert.h>
#include <stdio.h>

static unsigned prepared, enabled, disabled, calls;
static IRQn_Type last_irq;
static uint8_t last_priority;
void stm32_irq_prepare(IRQn_Type irq, uint8_t priority) {
    assert(prepared == enabled);
    last_irq = irq;
    last_priority = priority;
    ++prepared;
}
void stm32_irq_enable(IRQn_Type irq) {
    assert(prepared == enabled + 1 && irq == last_irq);
    ++enabled;
}
void stm32_irq_disable(IRQn_Type irq) { last_irq = irq; ++disabled; }
static void callback(void* data) { assert(data == &calls); ++calls; }

int main(void) {
    nx_isr_manager_t* manager = nx_isr_manager_get();
    assert(manager);
    /* Highest external IRQ in the actual CMSIS device declaration. */
    assert(manager->connect(manager, FPU_IRQn, callback, &calls, 15) == NX_OK);
    assert(last_irq == FPU_IRQn && last_priority == 15 && enabled == 1);
    stm32_isr_dispatch(FPU_IRQn);
    assert(calls == 1);
    assert(manager->connect(manager, FPU_IRQn, callback, &calls, 0) == NX_ERR_BUSY);
    assert(manager->disconnect(manager, FPU_IRQn) == NX_OK && last_irq == FPU_IRQn);
    stm32_isr_dispatch(FPU_IRQn);
    assert(calls == 1 && disabled == 1);
    assert(manager->disconnect(manager, FPU_IRQn) == NX_ERR_NOT_FOUND);
    assert(manager->connect(manager, FPU_IRQn, callback, &calls, 0) == NX_OK);
    assert(last_priority == 0);
    stm32_isr_dispatch(FPU_IRQn);
    assert(calls == 2);
    assert(manager->disconnect(manager, FPU_IRQn) == NX_OK);
    /* Every external IRQ, including USART6 and the high IRQ range, is distinct. */
    for (uint32_t irq = WWDG_IRQn; irq <= FPU_IRQn; ++irq)
        assert(manager->connect(manager, irq, callback, &calls, 7) == NX_OK);
    for (uint32_t irq = WWDG_IRQn; irq <= FPU_IRQn; ++irq)
        stm32_isr_dispatch((IRQn_Type)irq);
    assert(calls == 2 + (unsigned)FPU_IRQn + 1);
    for (uint32_t irq = WWDG_IRQn; irq <= FPU_IRQn; ++irq)
        assert(manager->disconnect(manager, irq) == NX_OK);
    unsigned hardware_calls = prepared + enabled + disabled;
    uint32_t invalid[] = { (uint32_t)FPU_IRQn + 1, UINT32_MAX,
                           (uint32_t)SysTick_IRQn, (uint32_t)NonMaskableInt_IRQn };
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        assert(manager->connect(manager, invalid[i], callback, &calls, 0) == NX_ERR_INVALID_PARAM);
        assert(manager->disconnect(manager, invalid[i]) == NX_ERR_INVALID_PARAM);
        stm32_isr_dispatch((IRQn_Type)invalid[i]);
    }
    assert(manager->connect(manager, WWDG_IRQn, callback, &calls, 16) == NX_ERR_INVALID_PARAM);
    assert(manager->connect(manager, WWDG_IRQn, NULL, &calls, 0) == NX_ERR_NULL_PTR);
    assert(manager->connect(NULL, WWDG_IRQn, callback, &calls, 0) == NX_ERR_NULL_PTR);
    assert(manager->disconnect(NULL, WWDG_IRQn) == NX_ERR_NULL_PTR);
    assert(prepared + enabled + disabled == hardware_calls);
    puts("STM32F407 actual IRQ numbering, all external IRQs, boundary and lifecycle passed");
    return 0;
}
