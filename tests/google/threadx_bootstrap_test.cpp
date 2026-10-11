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
#include "nexus/os/threadx.h"
#include <cstdlib>
#include <gtest/gtest.h>
namespace {
int mode;
bool initialized;
void product(void*) {
    std::_Exit(65);
}
void enter(int selected) {
    mode = selected;
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
    std::_Exit(reason == NX_ERROR_CONTEXT ? 42 : 43);
}
