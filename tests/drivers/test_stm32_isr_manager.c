/** Production ISR manager with real STM32F407 IRQ enums and NVIC call probes.
 * This is a host regression; ARM preemption and electrical HIL are separate. */
#include "arch/nx_arch.h"
#include "hal/provider/nx_device_provider.h"
#include "hal/resource/nx_isr_manager.h"
#include "interrupt/stm32_interrupt.h"
#include <assert.h>
#include <stdio.h>

static unsigned prepared, enabled, disabled, calls;
static IRQn_Type last_irq;
static uint8_t last_priority;
nexus_test_nvic_t nexus_test_nvic;
static uint32_t primask, basepri, faultmask;
static unsigned irq_depth, saves, restores, pending_clears;
static bool in_isr, shutdown_fence, retain_enable;

nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = {primask};
    primask = 1;
    ++irq_depth;
    ++saves;
    return previous;
}
void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    assert(irq_depth > 0 && primask == 1);
    --irq_depth;
    primask = previous.value;
    ++restores;
}
bool nx_arch_irq_is_masked(void) {
    return primask != 0 || basepri != 0 || faultmask != 0;
}
bool nx_arch_in_isr(void) {
    return in_isr;
}
void nx_arch_dmb(void) {
    assert(primask == 1 && irq_depth > 0);
}
bool nx_device_shutdown_is_active(void) {
    assert(primask == 1 && irq_depth > 0);
    return shutdown_fence;
}
static unsigned bank(IRQn_Type irq) {
    assert(irq >= WWDG_IRQn && irq <= FPU_IRQn);
    return (unsigned)irq / 32U;
}
static uint32_t bit(IRQn_Type irq) {
    return UINT32_C(1) << ((unsigned)irq % 32U);
}
void stm32_irq_prepare(IRQn_Type irq, uint8_t priority) {
    assert(primask == 1 && irq_depth > 0);
    assert(prepared == enabled);
    last_irq = irq;
    last_priority = priority;
    ++prepared;
    NVIC->ISPR[bank(irq)] &= ~bit(irq);
}
void stm32_irq_enable(IRQn_Type irq) {
    assert(primask == 1 && irq_depth > 0);
    assert(prepared == enabled + 1 && irq == last_irq);
    NVIC->ISER[bank(irq)] |= bit(irq);
    ++enabled;
}
void stm32_irq_disable(IRQn_Type irq) {
    assert(primask == 1 && irq_depth > 0);
    last_irq = irq;
    if (!retain_enable) {
        NVIC->ISER[bank(irq)] &= ~bit(irq);
    }
    ++disabled;
}
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) {
    assert(primask == 1 && irq_depth > 0);
    NVIC->ISPR[bank(irq)] &= ~bit(irq);
    ++pending_clears;
}
static void callback(void* data) {
    assert(primask == 0 && irq_depth == 0 && data == &calls);
    ++calls;
}

static void high_irq_context_and_retry(nx_isr_manager_t* manager) {
    assert(stm32_isr_manager_is_idle());
    primask = 1;
    assert(stm32_isr_manager_is_idle() && primask == 1);
    primask = 0;
    unsigned hardware_calls = prepared + enabled + disabled + pending_clears;
    uint32_t* masks[] = {&primask, &basepri, &faultmask};
    for (unsigned i = 0; i < sizeof(masks) / sizeof(masks[0]); ++i) {
        *masks[i] = 1;
        assert(manager->connect(manager, FPU_IRQn, callback, &calls, 7) ==
               NX_ERR_CONTEXT);
        assert(*masks[i] == 1 && stm32_isr_manager_is_idle());
        *masks[i] = 0;
    }
    in_isr = true;
    assert(manager->connect(manager, FPU_IRQn, callback, &calls, 7) ==
           NX_ERR_CONTEXT);
    in_isr = false;
    shutdown_fence = true;
    assert(manager->connect(manager, FPU_IRQn, callback, &calls, 7) ==
           NX_ERR_BUSY);
    shutdown_fence = false;
    assert(hardware_calls == prepared + enabled + disabled + pending_clears);

    assert(manager->connect(manager, FPU_IRQn, callback, &calls, 7) == NX_OK);
    hardware_calls = prepared + enabled + disabled + pending_clears;
    for (unsigned i = 0; i < sizeof(masks) / sizeof(masks[0]); ++i) {
        *masks[i] = 1;
        assert(manager->disconnect(manager, FPU_IRQn) == NX_ERR_CONTEXT);
        assert(*masks[i] == 1 && !stm32_isr_manager_is_idle());
        *masks[i] = 0;
    }
    in_isr = true;
    assert(manager->disconnect(manager, FPU_IRQn) == NX_ERR_CONTEXT);
    in_isr = false;
    NVIC->IABR[bank(FPU_IRQn)] = bit(FPU_IRQn);
    assert(manager->disconnect(manager, FPU_IRQn) == NX_ERR_BUSY);
    assert(hardware_calls == prepared + enabled + disabled + pending_clears);
    NVIC->IABR[bank(FPU_IRQn)] = 0;

    retain_enable = true;
    assert(manager->disconnect(manager, FPU_IRQn) == NX_ERR_BUSY);
    assert(!stm32_isr_manager_is_idle());
    unsigned before_callback = calls;
    in_isr = true;
    stm32_isr_dispatch(FPU_IRQn);
    in_isr = false;
    assert(calls == before_callback + 1);
    retain_enable = false;
    /* Shutdown blocks new owners but must still allow existing owners to drain.
     */
    shutdown_fence = true;
    assert(manager->disconnect(manager, FPU_IRQn) == NX_OK);
    shutdown_fence = false;
    assert(stm32_isr_manager_is_idle() && primask == 0 && irq_depth == 0);
}

int main(void) {
    nx_isr_manager_t* manager = nx_isr_manager_get();
    assert(manager);
    /* Highest external IRQ in the actual CMSIS device declaration. */
    assert(manager->connect(manager, FPU_IRQn, callback, &calls, 15) == NX_OK);
    assert(last_irq == FPU_IRQn && last_priority == 15 && enabled == 1);
    stm32_isr_dispatch(FPU_IRQn);
    assert(calls == 1);
    assert(manager->connect(manager, FPU_IRQn, callback, &calls, 0) ==
           NX_ERR_BUSY);
    assert(manager->disconnect(manager, FPU_IRQn) == NX_OK &&
           last_irq == FPU_IRQn);
    stm32_isr_dispatch(FPU_IRQn);
    assert(calls == 1 && disabled == 1);
    assert(manager->disconnect(manager, FPU_IRQn) == NX_ERR_NOT_FOUND);
    assert(manager->connect(manager, FPU_IRQn, callback, &calls, 0) == NX_OK);
    assert(last_priority == 0);
    stm32_isr_dispatch(FPU_IRQn);
    assert(calls == 2);
    assert(manager->disconnect(manager, FPU_IRQn) == NX_OK);
    /* Every external IRQ, including USART6 and the high IRQ range, is distinct.
     */
    for (uint32_t irq = WWDG_IRQn; irq <= FPU_IRQn; ++irq)
        assert(manager->connect(manager, irq, callback, &calls, 7) == NX_OK);
    for (uint32_t irq = WWDG_IRQn; irq <= FPU_IRQn; ++irq)
        stm32_isr_dispatch((IRQn_Type)irq);
    assert(calls == 2 + (unsigned)FPU_IRQn + 1);
    for (uint32_t irq = WWDG_IRQn; irq <= FPU_IRQn; ++irq)
        assert(manager->disconnect(manager, irq) == NX_OK);
    unsigned hardware_calls = prepared + enabled + disabled;
    uint32_t invalid[] = {(uint32_t)FPU_IRQn + 1, UINT32_MAX,
                          (uint32_t)SysTick_IRQn,
                          (uint32_t)NonMaskableInt_IRQn};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(manager->connect(manager, invalid[i], callback, &calls, 0) ==
               NX_ERR_INVALID_PARAM);
        assert(manager->disconnect(manager, invalid[i]) ==
               NX_ERR_INVALID_PARAM);
        stm32_isr_dispatch((IRQn_Type)invalid[i]);
    }
    assert(manager->connect(manager, WWDG_IRQn, callback, &calls, 16) ==
           NX_ERR_INVALID_PARAM);
    assert(manager->connect(manager, WWDG_IRQn, NULL, &calls, 0) ==
           NX_ERR_NULL_PTR);
    assert(manager->connect(NULL, WWDG_IRQn, callback, &calls, 0) ==
           NX_ERR_NULL_PTR);
    assert(manager->disconnect(NULL, WWDG_IRQn) == NX_ERR_NULL_PTR);
    assert(prepared + enabled + disabled == hardware_calls);
    high_irq_context_and_retry(manager);
    assert(saves == restores && irq_depth == 0 && !nx_arch_irq_is_masked());
    puts("STM32F407 actual IRQ numbering, all external IRQs, boundary and "
         "lifecycle passed");
    return 0;
}
