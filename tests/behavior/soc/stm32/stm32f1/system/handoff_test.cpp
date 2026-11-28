/**
 * \file            handoff_test.cpp
 * \brief           Model-verified MPU-less F103RB guarded boot handoff.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "handoff_model.h"
#include "nexus/arch/arch.h"
#include "stm32f1xx.h"
#include <gtest/gtest.h>
#include <unordered_map>

namespace {
constexpr uint32_t vector_address = 0x08004200U;
constexpr uint32_t allowed_stack = 0x20005000U;
constexpr nx_stm32f103rb_executable_t executable = {0x08004310U, 0x08005000U};
struct {
    std::unordered_map<uintptr_t, uint32_t> registers;
    nx_result_t stop_result;
    nx_arch_irq_masks_t masks;
    uint32_t control;
    uint32_t exception;
    uint32_t stack;
    uint32_t pc;
    uint16_t flash_kib;
    unsigned stop_calls;
    unsigned vector_reads;
    unsigned tails;
    bool privileged;
} model;

#define SET(reg, value)                                                        \
    model.registers[reinterpret_cast<uintptr_t>(&(reg))] = (value)

class F103Handoff : public testing::Test {
  protected:
    nx_stm32f103rb_handoff_t owner{};
    nx_clock_barrier_t barrier{};

    void SetUp() override {
        model = {};
        model.privileged = true;
        model.stop_result = NX_SUCCESS;
        model.stack = allowed_stack;
        model.pc = executable.begin | 1U;
        model.flash_kib = 128U;
        SET(SCB->CPUID, 0x411FC231U);
        SET(SCB->VTOR, FLASH_BASE);
        SET(SCB->CCR, SCB_CCR_STKALIGN_Msk);
        SET(RCC->CR,
            RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos));
        SET(RCC->AHBENR, RCC_AHBENR_SRAMEN | RCC_AHBENR_FLITFEN);
        SET(FLASH->CR, FLASH_CR_LOCK);
        SET(FLASH->OBR, FLASH_OBR_IWDG_SW);
        ASSERT_EQ(nx_clock_domain_construct(&owner.domain, 2U), NX_SUCCESS);
    }

    void Prepare() {
        ASSERT_EQ(nx_clock_domain_close(&owner.domain, &barrier), NX_SUCCESS);
        ASSERT_EQ(nx_stm32f103rb_handoff_prepare(&owner, barrier), NX_SUCCESS);
        model.masks.primask = 1U;
    }
};

TEST_F(F103Handoff, RequiresActualOwnerDrainAndPlatformStop) {
    nx_clock_lease_t pending{};
    ASSERT_EQ(nx_clock_domain_acquire(&owner.domain, &pending), NX_SUCCESS);
    ASSERT_EQ(nx_clock_domain_close(&owner.domain, &barrier), NX_SUCCESS);
    EXPECT_EQ(nx_stm32f103rb_handoff_prepare(&owner, barrier), NX_ERROR_BUSY);
    EXPECT_EQ(model.stop_calls, 0U);
    ASSERT_EQ(nx_clock_domain_release(&owner.domain, &pending), NX_SUCCESS);
    model.stop_result = NX_ERROR_IO;
    EXPECT_EQ(nx_stm32f103rb_handoff_prepare(&owner, barrier), NX_ERROR_IO);
    model.masks.primask = 1U;
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_STATE);
    EXPECT_EQ(model.vector_reads, 0U);
    EXPECT_EQ(model.tails, 0U);
}

TEST_F(F103Handoff, MasksPspAndStaleEpochPreventTransfer) {
    Prepare();
    model.masks.basepri = 32U;
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_CONTEXT);
    model.masks.basepri = 0U;
    model.control = 2U;
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_CONTEXT);
    model.control = 0U;
    ASSERT_EQ(nx_clock_domain_reopen(barrier), NX_SUCCESS);
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_STATE);
    EXPECT_EQ(model.vector_reads, 0U);
    EXPECT_EQ(model.masks.primask, 1U);
}

TEST_F(F103Handoff, RejectsRetainedHardwareAndUnknownFlashBeforeVectorReads) {
    Prepare();
    SET(NVIC->ISPR[1], 1U);
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_BUSY);
    SET(NVIC->ISPR[1], 0U);
    SET(SysTick->CTRL, SysTick_CTRL_ENABLE_Msk);
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_BUSY);
    SET(SysTick->CTRL, 0U);
    SET(RCC->BDCR, RCC_BDCR_RTCEN);
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_BUSY);
    SET(RCC->BDCR, 0U);
    SET(RCC->CSR, RCC_CSR_LSIRDY);
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_BUSY);
    SET(RCC->CSR, 0U);
    SET(RCC->AHBENR,
        RCC_AHBENR_DMA1EN | RCC_AHBENR_SRAMEN | RCC_AHBENR_FLITFEN);
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_BUSY);
    SET(RCC->AHBENR, RCC_AHBENR_SRAMEN | RCC_AHBENR_FLITFEN);
    SET(FLASH->SR, FLASH_SR_BSY);
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_BUSY);
    SET(FLASH->SR, 0U);
    model.flash_kib = 64U;
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(model.vector_reads, 0U);
    EXPECT_EQ(model.tails, 0U);
}

TEST_F(F103Handoff, ChecksExactIntervalAndMspBeforeBranch) {
    Prepare();
    EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier,
                                           {0x08004202U, 0x08004400U}),
              NX_ERROR_INVALID);
    EXPECT_EQ(model.vector_reads, 0U);
    for (uint32_t stack :
         {0x20000000U, 0x200007F8U, 0x20005008U, 0x20004FFCU}) {
        model.stack = stack;
        EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
                  NX_ERROR_INVALID);
    }
    model.stack = allowed_stack;
    for (uint32_t pc :
         {executable.begin, executable.begin - 1U, executable.end | 1U}) {
        model.pc = pc;
        EXPECT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
                  NX_ERROR_INVALID);
    }
    EXPECT_EQ(model.tails, 0U);
}

TEST_F(F103Handoff, ValidVectorUsesExactTailAndKeepsPrimask) {
    Prepare();
    ASSERT_EQ(nx_stm32f103rb_handoff_enter(&owner, barrier, executable),
              NX_SUCCESS);
    EXPECT_EQ(model.vector_reads, 2U);
    EXPECT_EQ(model.stop_calls, 1U);
    EXPECT_EQ(model.tails, 1U);
    EXPECT_EQ(model.masks.primask, 1U);
}
} /* namespace */

extern "C" uint32_t f103_read(uintptr_t address) {
    const auto value = model.registers.find(address);
    return value == model.registers.end() ? 0U : value->second;
}
extern "C" uint16_t f103_flash_kib(void) {
    return model.flash_kib;
}
extern "C" uint32_t f103_control(void) {
    return model.control;
}
extern "C" uint32_t f103_vector_word(uint32_t address) {
    ++model.vector_reads;
    EXPECT_TRUE(address == vector_address || address == vector_address + 4U);
    return address == vector_address ? model.stack : model.pc;
}
extern "C" nx_result_t f103_tail(uint32_t vector, uint32_t stack, uint32_t pc) {
    ++model.tails;
    EXPECT_EQ(vector, vector_address);
    EXPECT_EQ(stack, allowed_stack);
    EXPECT_EQ(pc, model.pc);
    EXPECT_EQ(model.masks.primask, 1U);
    return NX_SUCCESS;
}
extern "C" nx_result_t nx_platform_stop(void) {
    ++model.stop_calls;
    EXPECT_EQ(model.masks.primask, 0U);
    return model.stop_result;
}
extern "C" bool nx_stm32f1_system_live(void) {
    return false;
}
extern "C" uint32_t nx_arch_exception_number(void) {
    return model.exception;
}
extern "C" bool nx_arch_is_privileged(void) {
    return model.privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return model.exception != 0U;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return model.masks.primask != 0U || model.masks.basepri != 0U ||
           model.masks.faultmask != 0U;
}
extern "C" nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    return model.masks;
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t previous{model.masks.primask};
    model.masks.primask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    model.masks.primask = previous.value;
}
