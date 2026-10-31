/**
 * \file            cpu_model.cpp
 * \brief           Explicit task/IRQ interleavings, not physical IRQ timing
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include <atomic>
#include <cstdlib>

static unsigned s_depth;
static bool s_isr;
static void (*s_restore_hook)(void);
static void (*s_dsb_hook)(void);

extern "C" void nx_hal_test_cpu_reset(void) {
    if (s_depth != 0U)
        std::abort();
    s_isr = false;
    s_restore_hook = nullptr;
    s_dsb_hook = nullptr;
}
extern "C" bool nx_hal_test_isr(bool next) {
    bool previous = s_isr;
    s_isr = next;
    return previous;
}
extern "C" void nx_hal_test_restore_hook(void (*hook)(void)) {
    s_restore_hook = hook;
}
extern "C" void nx_hal_test_dsb_hook(void (*hook)(void)) {
    s_dsb_hook = hook;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = {s_depth++};
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    if (s_depth == 0U || previous.value != s_depth - 1U)
        std::abort();
    s_depth = previous.value;
    if (s_depth == 0U && s_restore_hook != nullptr)
        s_restore_hook();
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return s_depth != 0U;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return {s_depth != 0U ? 1U : 0U, 0U, 0U};
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return s_isr ? 16U : 0U;
}
extern "C" bool nx_arch_in_isr(void) {
    return s_isr;
}
extern "C" bool nx_arch_is_privileged(void) {
    return true;
}
/**
 * \brief           Order synchronous model observations and reentrant IRQ hooks
 * \details         IRQs are explicit calls on one host thread. Compiler
 * ordering is sufficient here; Native architecture tests retain their real
 * host-thread synchronization and barrier coverage.
 */
extern "C" void nx_arch_dmb(void) {
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
extern "C" void nx_arch_dsb(void) {
    nx_arch_dmb();
    if (s_dsb_hook != nullptr)
        s_dsb_hook();
}
extern "C" void nx_arch_isb(void) {
    nx_arch_dmb();
}
extern "C" bool nx_arch_cycle_snapshot(uint32_t*) {
    return false;
}
