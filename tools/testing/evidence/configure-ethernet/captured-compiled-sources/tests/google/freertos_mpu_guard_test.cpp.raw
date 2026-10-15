/**
 * \file            freertos_mpu_guard_test.cpp
 * \brief           Cold scheduler lease and real SVC admission reject replay
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
extern "C" {
#include "guard.h"
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#if NEXUS_ARCH_MPU_VERSION == 7 || configRUN_FREERTOS_SECURE_ONLY == 1
static constexpr uint32_t BOOT_RETURN = 0xfffffff9U;
#else
static constexpr uint32_t BOOT_RETURN = 0xffffffb8U;
#endif

class StartRegisters {
  public:
    MOCK_METHOD(nx_freertos_mpu_start_facts_t, snapshot, ());
};
static StartRegisters* s_registers;
extern "C" nx_freertos_mpu_start_facts_t nx_freertos_mpu_start_facts(void) {
    return s_registers->snapshot();
}

class MpuStartGuard : public testing::Test {
  protected:
    testing::NiceMock<StartRegisters> registers;
    struct alignas(64) Protected {
        nx_freertos_mpu_start_t lease = {};
        alignas(8) uint32_t frame[8] = {};
    } memory;
    nx_freertos_mpu_start_facts_t facts = {};
    void SetUp() override {
        s_registers = &registers;
        facts = {0,
                 0,
                 0x08000102,
                 0x08000000,
                 0x08010000,
                 reinterpret_cast<uintptr_t>(&memory),
                 reinterpret_cast<uintptr_t>(&memory) + sizeof(memory)};
        memory.frame[6] = static_cast<uint32_t>(facts.startup_pc);
        memory.frame[7] = 1U << 24;
        ON_CALL(registers, snapshot()).WillByDefault(testing::Invoke([this]() {
            return facts;
        }));
    }
    void TearDown() override {
        s_registers = nullptr;
    }
};

TEST_F(MpuStartGuard, PrivilegedBootstrapArmsAndConsumesExactlyOnce) {
    EXPECT_EQ(nx_freertos_mpu_start_arm(&memory.lease), pdTRUE);
    EXPECT_EQ(memory.lease.phase, 1U);
    EXPECT_EQ(nx_freertos_mpu_start_arm(&memory.lease), pdFALSE);
    facts.exception = 11;
    EXPECT_EQ(
        nx_freertos_mpu_start_consume(&memory.lease, memory.frame, BOOT_RETURN),
        pdTRUE);
    EXPECT_EQ(memory.lease.phase, 2U);
    EXPECT_EQ(
        nx_freertos_mpu_start_consume(&memory.lease, memory.frame, BOOT_RETURN),
        pdFALSE);
    facts.exception = 0;
    EXPECT_EQ(nx_freertos_mpu_start_arm(&memory.lease), pdFALSE);
}

class InvalidStartFrame : public MpuStartGuard,
                          public testing::WithParamInterface<int> {};
TEST_P(InvalidStartFrame, RawSvcCannotRestoreOrConsumeTheProtectedLease) {
    memory.lease.phase = 1;
    facts.exception = 11;
    uint32_t return_state = BOOT_RETURN;
    const uint32_t* frame = memory.frame;
    nx_freertos_mpu_start_t* lease = &memory.lease;
    switch (GetParam()) {
        case 0:
            facts.control = 1;
            break;
        case 1:
            facts.control = 2;
            break;
        case 2:
            facts.control = 4;
            break;
        case 3:
            facts.exception = 0;
            break;
        case 4:
            facts.exception = 3;
            break;
        case 5:
            return_state = 0xfffffffdU;
            break;
        case 6:
            return_state = 0xfffffff1U;
            break;
        case 7:
            return_state = 0xffffffe9U;
            break;
        case 8:
            return_state = 0x12345679U;
            break;
        case 9:
            memory.frame[6] = 0x08008002U;
            break;
        case 10:
            memory.frame[7] = 0;
            break;
        case 11:
            memory.frame[7] |= 15;
            break;
        case 12:
            frame = nullptr;
            break;
        case 13:
            frame = memory.frame + 1;
            break;
        case 14:
            facts.ram_end = reinterpret_cast<uintptr_t>(memory.frame) + 28;
            break;
        case 15:
            facts.startup_pc = facts.flash_end + 2;
            break;
        case 16:
            lease = nullptr;
            break;
        case 17:
            memory.lease.phase = 0;
            break;
        case 18:
            memory.lease.phase = 2;
            break;
        default:
            FAIL();
    }
    const uint32_t incoming = memory.lease.phase;
    EXPECT_EQ(nx_freertos_mpu_start_consume(lease, frame, return_state),
              pdFALSE);
    EXPECT_EQ(memory.lease.phase, incoming);
}
INSTANTIATE_TEST_SUITE_P(NoRestoreEffects, InvalidStartFrame,
                         testing::Range(0, 19));

TEST_F(MpuStartGuard,
       ArmRejectsUnprivilegedOrHandlerOrProcessStackBeforeWrites) {
    for (uint32_t control : {1U, 2U, 3U, 4U}) {
        facts.control = control;
        EXPECT_EQ(nx_freertos_mpu_start_arm(&memory.lease), pdFALSE);
        EXPECT_EQ(memory.lease.phase, 0U);
    }
    facts.control = 0;
    facts.exception = 11;
    EXPECT_EQ(nx_freertos_mpu_start_arm(&memory.lease), pdFALSE);
    EXPECT_EQ(memory.lease.phase, 0U);
}

#if NEXUS_ARCH_MPU_VERSION == 8
TEST_F(MpuStartGuard, V8RejectsOtherWorldAndAdditionalCalleeFrameEncoding) {
    for (uint32_t return_state : {0xffffff99U, 0xffffff98U, 0xffffffb9U}) {
        memory.lease.phase = 1;
        facts.exception = 11;
        EXPECT_EQ(nx_freertos_mpu_start_consume(&memory.lease, memory.frame,
                                                return_state),
                  pdFALSE);
        EXPECT_EQ(memory.lease.phase, 1U);
    }
    memory.lease.phase = 1;
    facts.exception = 11;
    const uint32_t other_world =
        configRUN_FREERTOS_SECURE_ONLY == 1 ? 0xffffffb8U : 0xfffffff9U;
    EXPECT_EQ(
        nx_freertos_mpu_start_consume(&memory.lease, memory.frame, other_world),
        pdFALSE);
}
#endif
