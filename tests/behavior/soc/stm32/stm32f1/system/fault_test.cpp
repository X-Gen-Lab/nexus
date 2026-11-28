/**
 * \file            fault_test.cpp
 * \brief           Exact fault sample and retained publication contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "fault_model.h"
#include "nexus/soc/stm32f103rb_fault.h"
#include <gtest/gtest.h>

SCB_Type f103_fault_scb{};
nx_fault_record_t f103_fault_storage{};
static bool privileged;

extern "C" bool f103_fault_privileged(void) {
    return privileged;
}

class F103Fault : public testing::Test {
  protected:
    void SetUp() override {
        f103_fault_storage = {};
        privileged = true;
        f103_fault_scb.CFSR = 0U;
        f103_fault_scb.HFSR = SCB_HFSR_FORCED_Msk;
        f103_fault_scb.ICSR = 3U;
        f103_fault_scb.SHCSR = SCB_SHCSR_BUSFAULTENA_Msk;
        f103_fault_scb.MMFAR = 0x20000101U;
        f103_fault_scb.BFAR = 0x40000402U;
        f103_fault_scb.AFSR = UINT32_MAX;
        f103_fault_scb.DFSR = UINT32_MAX;
    }
};

TEST_F(F103Fault, SamplesOnlyDocumentedStatusAndRetainsFirstCommit) {
    f103_fault_scb.CFSR = SCB_CFSR_BFARVALID_Msk;
    ASSERT_EQ(nx_stm32f103rb_fault_capture(&f103_fault_storage, 17U, 31U,
                                           0xfffffff9U),
              NX_SUCCESS);
    f103_fault_scb.CFSR = 0U;
    EXPECT_EQ(nx_stm32f103rb_fault_capture(&f103_fault_storage, 18U, 99U, 0U),
              NX_ERROR_BUSY);

    nx_fault_capture_t facts{};
    ASSERT_EQ(nx_fault_record_read(&f103_fault_storage, &facts), NX_SUCCESS);
    EXPECT_EQ(facts.sequence, 17U);
    EXPECT_EQ(facts.domain, NX_FAULT_PLATFORM);
    EXPECT_EQ(facts.reason, 31U);
    EXPECT_EQ(facts.exception, 3U);
    EXPECT_EQ(facts.exception_return, 0xfffffff9U);
    EXPECT_EQ(facts.flags, NX_FAULT_STATUS_VALID);
    EXPECT_EQ(facts.status[0], SCB_CFSR_BFARVALID_Msk);
    EXPECT_EQ(facts.status[1], SCB_HFSR_FORCED_Msk);
    EXPECT_EQ(facts.status[2], 0U);
    EXPECT_EQ(facts.status[3], 0U);
    EXPECT_EQ(facts.status[4], 0U);
    EXPECT_EQ(facts.status[5], f103_fault_scb.BFAR);
    EXPECT_EQ(facts.status[6], SCB_SHCSR_BUSFAULTENA_Msk);
    EXPECT_EQ(facts.status[7], 3U);
    EXPECT_EQ(facts.msp, 0U);
    EXPECT_EQ(facts.psp, 0U);
    EXPECT_EQ(facts.core[6], 0U);
}

TEST_F(F103Fault, RefusesUnavailablePrivilegeAndInvalidStorage) {
    privileged = false;
    EXPECT_EQ(nx_stm32f103rb_fault_capture(&f103_fault_storage, 1U, 2U, 0U),
              NX_ERROR_CONTEXT);
    privileged = true;
    EXPECT_EQ(nx_stm32f103rb_fault_capture(nullptr, 1U, 2U, 0U),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_stm32f103rb_fault_capture(
            reinterpret_cast<nx_fault_record_t*>(0x40000000U), 1U, 2U, 0U),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_stm32f103rb_fault_capture(
                  reinterpret_cast<nx_fault_record_t*>(
                      reinterpret_cast<uintptr_t>(&f103_fault_storage) + 1U),
                  1U, 2U, 0U),
              NX_ERROR_INVALID);
    nx_fault_capture_t facts{};
    EXPECT_EQ(nx_fault_record_read(&f103_fault_storage, &facts),
              NX_ERROR_EMPTY);
}

TEST_F(F103Fault, DecodesIndependentBootResetAndRejectsCorruption) {
    ASSERT_EQ(nx_stm32f103rb_fault_capture(&f103_fault_storage, 8U, 0U, 0U),
              NX_SUCCESS);
    nx_reset_record_t boot{};
    ASSERT_EQ(nx_reset_record_read(&f103_fault_storage, 0x30U, &boot),
              NX_SUCCESS);
    EXPECT_EQ(boot.reset_causes, 0x30U);
    EXPECT_TRUE(boot.has_fault);
    EXPECT_EQ(boot.fault.sequence, 8U);
    f103_fault_storage.bytes[40] ^= 1U;
    EXPECT_EQ(nx_reset_record_read(&f103_fault_storage, 0x40U, &boot),
              NX_ERROR_IO);
    EXPECT_EQ(boot.reset_causes, 0x30U);
    EXPECT_EQ(boot.fault.sequence, 8U);
}
