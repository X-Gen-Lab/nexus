/**
 * \file            threadx_bootstrap_test.cpp
 * \brief           Actual kernel callback rejects corrupted Arch context
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"
#include "nexus/os/threadx.h"
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
namespace {
int mode;
bool initialized;
bool verify_observer;
unsigned observations;
void product(void*) {
    std::_Exit(65);
}
void enter(int selected, bool observation_required = false) {
    mode = selected;
    verify_observer = observation_required;
    nx_threadx_kernel_enter(product, nullptr);
    std::_Exit(66);
}
TEST(ThreadXBootstrap, RejectsISRCallbackBeforeProduct) {
    EXPECT_EXIT(enter(1), ::testing::ExitedWithCode(42), "");
}
TEST(ThreadXBootstrap, RejectsUnprivilegedCallbackBeforeProduct) {
    EXPECT_EXIT(enter(2), ::testing::ExitedWithCode(42), "");
}
TEST(ThreadXBootstrap, RejectsFaultMaskBeforeProduct) {
    EXPECT_EXIT(enter(3), ::testing::ExitedWithCode(42), "");
}
TEST(ThreadXBootstrap, RejectsUnselectedBasepriBeforeProduct) {
    EXPECT_EXIT(enter(4), ::testing::ExitedWithCode(42), "");
}
TEST(ThreadXBootstrap, CanonicalUnmaskedCallbackInvokesProduct) {
    EXPECT_EXIT(enter(0), ::testing::ExitedWithCode(65), "");
}
TEST(ThreadXBootstrap, CanonicalPrimaskCallbackInvokesProduct) {
    EXPECT_EXIT(enter(5), ::testing::ExitedWithCode(65), "");
}
TEST(ThreadXBootstrap, ObserverPrecedesOverriddenProductHookForContextFault) {
    EXPECT_EXIT(enter(1, true), ::testing::ExitedWithCode(42), "");
}
TEST(ThreadXBootstrap, ObserverPrecedesProductHookForMaskedBootstrapFault) {
    EXPECT_EXIT(enter(3, true), ::testing::ExitedWithCode(42), "");
}
} /* namespace */
extern "C" void __real__tx_initialize_low_level();
extern "C" void __wrap__tx_initialize_low_level() {
    __real__tx_initialize_low_level();
    initialized = true;
}
extern "C" bool __wrap_nx_arch_in_isr() {
    return initialized && mode == 1;
}
extern "C" bool __wrap_nx_arch_is_privileged() {
    return !(initialized && mode == 2);
}
extern "C" nx_arch_irq_masks_t __wrap_nx_arch_irq_masks() {
    nx_arch_irq_masks_t masks{};
    if (initialized) {
        masks.faultmask = mode == 3 ? 1U : 0U;
        masks.basepri = mode == 4 ? 80U : 0U;
        masks.primask = mode == 5 ? 1U : 0U;
    }
    return masks;
}
extern "C" void nx_threadx_fail_stop(nx_result_t reason) {
    if (verify_observer && observations != 1) {
        std::_Exit(68);
    }
    std::_Exit(reason == NX_ERROR_CONTEXT ? 42 : 43);
}
extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    if (domain != NX_FAULT_THREADX_ADAPTER || reason != NX_ERROR_CONTEXT ||
        file == nullptr || std::strstr(file, "threadx.c") == nullptr ||
        line == 0) {
        std::_Exit(69);
    }
    ++observations;
}
