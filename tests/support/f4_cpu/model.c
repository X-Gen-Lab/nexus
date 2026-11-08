/**
 * \file            model.c
 * \brief           Host CPU and time boundaries for real STM32F4 providers
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/core/time.h"
#include "stm32f407xx.h"

NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint64_t g_f4_model_now;
bool g_f4_model_isr;
uint32_t g_f4_model_mask;
static uint32_t s_host_ordering;

/** \brief Advance the caller-controlled monotonic test clock. */
uint64_t nx_time_now_us(void) {
    return ++g_f4_model_now;
}
/** \brief Save the incoming mask before entering the modeled critical region.
 */
nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t saved = {g_f4_model_mask};
    g_f4_model_mask = 1U;
    return saved;
}
/** \brief Restore exactly the mask saved by the caller. */
void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    g_f4_model_mask = saved.value;
}
/** \brief Observe the injected execution context. */
bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
/** \brief Observe the modeled incoming interrupt mask. */
bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
/** \brief MMIO storage completes synchronously in this host fixture. */
void nx_arch_dsb(void) {
}
/** \brief Expose the fixture's selected interrupt-mask domain. */
nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    nx_arch_irq_masks_t masks = {g_f4_model_mask, 0U, 0U};
    return masks;
}
/** \brief Inject one external exception without claiming a physical vector. */
uint32_t nx_arch_exception_number(void) {
    return g_f4_model_isr ? 16U : 0U;
}
/** \brief These register fixtures execute as privileged callers. */
bool nx_arch_is_privileged(void) {
    return true;
}
/** \brief Preserve host ordering without claiming physical bus completion. */
void nx_arch_dmb(void) {
    (void)__atomic_fetch_add(&s_host_ordering, 0U, __ATOMIC_SEQ_CST);
}
/** \brief Preserve host ordering without a physical instruction pipeline. */
void nx_arch_isb(void) {
    (void)__atomic_fetch_add(&s_host_ordering, 0U, __ATOMIC_SEQ_CST);
}
/** \brief This host fixture has no physical DWT cycle counter. */
bool nx_arch_cycle_snapshot(uint32_t* cycles) {
    (void)cycles;
    return false;
}
