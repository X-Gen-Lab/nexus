/**
 * \file            freertos_mpu_port_test.cpp
 * \brief           Actual port SVC restore and MPU register writes under mocks
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
extern "C" {
#include "guard.h"
#include "mpu_port_model.h"
extern nx_freertos_mpu_start_t xNexusMpuStart;
uint32_t __privileged_functions_start__[1];
uint32_t __privileged_functions_end__[1];
uint32_t __privileged_data_start__[1];
uint32_t __privileged_data_end__[1];
uint8_t __nexus_user_flash_start__[1] = {};
uint8_t __nexus_user_flash_end__[1] = {};
uint8_t __nexus_syscall_flash_start__[1] = {};
uint8_t __nexus_syscall_flash_end__[1] = {};
}
#include <algorithm>
#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

class PortHardware {
  public:
    MOCK_METHOD(void, restore, ());
    MOCK_METHOD(void, yield, ());
    MOCK_METHOD(uint8_t, opcode, (uint32_t));
    MOCK_METHOD(nx_freertos_mpu_start_facts_t, snapshot, ());
    MOCK_METHOD(volatile uint32_t*, reg, (unsigned));
};
static PortHardware* s_hardware;
extern "C" void prvRestoreContextOfFirstTask(void) {
    s_hardware->restore();
}
extern "C" void vRestoreContextOfFirstTask(void) {
    s_hardware->restore();
}
extern "C" void vPortYield(void) {
    s_hardware->yield();
}
extern "C" uint8_t nx_mpu_model_opcode(uint32_t pc) {
    return s_hardware->opcode(pc);
}
extern "C" void nx_mpu_model_barrier(void) {
}
extern "C" volatile uint32_t* nx_mpu_model_register(unsigned index) {
    return s_hardware->reg(index);
}
extern "C" nx_freertos_mpu_start_facts_t nx_freertos_mpu_start_facts(void) {
    return s_hardware->snapshot();
}
extern "C" uint32_t nx_mpu_model_link_address(const void* symbol) {
    if (symbol == __nexus_user_flash_start__) {
        return 0x08080000;
    }
    if (symbol == __nexus_user_flash_end__) {
        return 0x08090000;
    }
    if (symbol == __nexus_syscall_flash_start__) {
        return 0x08090000;
    }
    if (symbol == __nexus_syscall_flash_end__) {
        return 0x080a0000;
    }
    if (symbol == __privileged_functions_start__) {
        return 0x08000000;
    }
    if (symbol == __privileged_functions_end__) {
        return 0x08080000;
    }
    if (symbol == __privileged_data_start__) {
        return 0x20000000;
    }
    if (symbol == __privileged_data_end__) {
        return 0x20020000;
    }
    ADD_FAILURE() << "Unexpected actual linker boundary";
    return 0;
}

#if NEXUS_ARCH_MPU_VERSION == 7 || configRUN_FREERTOS_SECURE_ONLY == 1
static constexpr uint32_t BOOT_RETURN = 0xfffffff9U;
#else
static constexpr uint32_t BOOT_RETURN = 0xffffffb8U;
#endif
alignas(8) static uint32_t s_frame[8];

class ActualMpuPort : public testing::Test {
  protected:
    testing::NiceMock<PortHardware> hardware;
    nx_freertos_mpu_start_facts_t facts = {};
    std::array<uint32_t, NX_MPU_REG_COUNT> regs = {};
    std::vector<std::array<uint32_t, 2>> regions;
    bool pending = false;
    void flush() {
        if (pending) {
            regions.push_back(
                {regs[NX_MPU_REG_BASE], regs[NX_MPU_REG_ATTRIBUTES]});
            pending = false;
        }
    }
    void SetUp() override {
        s_hardware = &hardware;
        xNexusMpuStart.phase = 0;
        std::fill(std::begin(s_frame), std::end(s_frame), 0U);
        const uintptr_t lease = reinterpret_cast<uintptr_t>(&xNexusMpuStart);
        const uintptr_t frame = reinterpret_cast<uintptr_t>(s_frame);
        facts = {
            0,
            0,
            0x08000102,
            0x08000000,
            0x08080000,
            std::min(lease, frame),
            std::max(lease + sizeof(xNexusMpuStart), frame + sizeof(s_frame))};
        s_frame[6] = static_cast<uint32_t>(facts.startup_pc);
        s_frame[7] = 1U << 24;
        regs[NX_MPU_REG_TYPE] = 8U << 8;
        ON_CALL(hardware, opcode(testing::_))
            .WillByDefault(testing::Return(uint8_t{portSVC_START_SCHEDULER}));
        ON_CALL(hardware, snapshot()).WillByDefault(testing::Invoke([this]() {
            return facts;
        }));
        ON_CALL(hardware, reg(testing::_))
            .WillByDefault(
                testing::Invoke([this](unsigned index) -> volatile uint32_t* {
                    flush();
                    if (index == NX_MPU_REG_ATTRIBUTES) {
                        pending = true;
                    }
                    return &regs.at(index);
                }));
    }
    void TearDown() override {
        s_hardware = nullptr;
    }
};

TEST_F(ActualMpuPort, OriginalSpecialSvcRestoresOnlyForProtectedOneShotOrigin) {
    EXPECT_EQ(nx_freertos_mpu_start_arm(&xNexusMpuStart), pdTRUE);
    facts.exception = 11;
    EXPECT_CALL(hardware, restore()).Times(1);
    nx_mpu_model_dispatch(s_frame, BOOT_RETURN);
    EXPECT_EQ(xNexusMpuStart.phase, 2U);
    nx_mpu_model_dispatch(s_frame, BOOT_RETURN);
    EXPECT_EQ(xNexusMpuStart.phase, 2U);
}

class InvalidSpecialSvc : public ActualMpuPort,
                          public testing::WithParamInterface<int> {};
TEST_P(InvalidSpecialSvc,
       OriginalFallbackDoesNotWriteRestoreStateForRawUserSvc) {
    xNexusMpuStart.phase = 1;
    facts.exception = 11;
    uint32_t return_state = BOOT_RETURN;
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
            s_frame[6] = 0x08080002;
            break;
        case 4:
            return_state &= ~(1U << 4);
            break;
        case 5:
            return_state |= 1U << 2;
            break;
        case 6:
            facts.exception = 15;
            break;
        case 7:
            s_frame[7] = 0;
            break;
        case 8:
            facts.ram_end = reinterpret_cast<uintptr_t>(s_frame) + 28;
            break;
        case 9:
            return_state ^= 1U << 6;
            break;
        default:
            FAIL() << "Invalid parameter";
    }
    EXPECT_CALL(hardware, restore()).Times(0);
    EXPECT_CALL(hardware, reg(testing::_)).Times(0);
    nx_mpu_model_dispatch(s_frame, return_state);
    EXPECT_EQ(xNexusMpuStart.phase, 1U);
}
INSTANTIATE_TEST_SUITE_P(UserRawStart, InvalidSpecialSvc,
                         testing::Range(0, 10));

TEST_F(ActualMpuPort, UnarmedAndUnknownCallsCannotRestore) {
    facts.exception = 11;
    EXPECT_CALL(hardware, restore()).Times(0);
    nx_mpu_model_dispatch(s_frame, BOOT_RETURN);
    EXPECT_CALL(hardware, opcode(testing::_))
        .WillOnce(testing::Return(uint8_t{99}));
    nx_mpu_model_dispatch(s_frame, BOOT_RETURN);
    EXPECT_EQ(xNexusMpuStart.phase, 0U);
}

#if NEXUS_ARCH_MPU_VERSION == 7
TEST_F(ActualMpuPort,
       ActualSetupMapsOnlyExactNormalRxWindowsAndPrivilegedStorage) {
    nx_mpu_model_setup();
    flush();
    ASSERT_EQ(regions.size(), 4U);
    EXPECT_EQ(regions[0][0], 0x08080015U);
    EXPECT_EQ(regions[1][0], 0x08000016U);
    EXPECT_EQ(regions[2][0], 0x20000017U);
    EXPECT_EQ(regions[3][0], 0x08090014U);
    EXPECT_EQ(regions[0][1], (6U << 24) | (7U << 16) | (15U << 1) | 1U);
    EXPECT_EQ(regions[1][1], (5U << 24) | (7U << 16) | (18U << 1) | 1U);
    EXPECT_EQ(regions[2][1],
              (1U << 28) | (1U << 24) | (7U << 16) | (16U << 1) | 1U);
    EXPECT_EQ(regions[3][1], regions[0][1]);
    EXPECT_EQ(regs[NX_MPU_REG_CONTROL], 5U);
    EXPECT_EQ(regs[NX_MPU_REG_FAULT], 1U << 16);
    /* Decode actual register writes to check default user permissions. */
    auto user_readable = [this](uint32_t address) {
        const std::array<uint32_t, 2>* selected = nullptr;
        for (const auto& region : regions) {
            const uint32_t base = region[0] & ~31U;
            const uint32_t bytes = 1U << (((region[1] >> 1) & 31U) + 1U);
            if (address >= base && address - base < bytes &&
                (selected == nullptr ||
                 (region[0] & 15U) > ((*selected)[0] & 15U))) {
                selected = &region;
            }
        }
        return selected != nullptr && (((*selected)[1] >> 24) & 7U) == 6U;
    };
    EXPECT_TRUE(user_readable(0x08080000));
    EXPECT_TRUE(user_readable(0x0809ffff));
    EXPECT_FALSE(user_readable(0x08000100));
    EXPECT_FALSE(user_readable(0x080a0000));
    EXPECT_FALSE(user_readable(0x080fffff));
    EXPECT_FALSE(user_readable(0x20000000));
    EXPECT_FALSE(user_readable(0x40000000));
}

TEST_F(ActualMpuPort, ActualSetupRejectsMismatchedMpuWithoutAnyWrites) {
    regs[NX_MPU_REG_TYPE] = 16U << 8;
    EXPECT_CALL(hardware, reg(NX_MPU_REG_TYPE)).Times(1);
    EXPECT_CALL(hardware, reg(NX_MPU_REG_BASE)).Times(0);
    EXPECT_CALL(hardware, reg(NX_MPU_REG_ATTRIBUTES)).Times(0);
    nx_mpu_model_setup();
    EXPECT_TRUE(regions.empty());
    EXPECT_EQ(regs[NX_MPU_REG_CONTROL], 0U);
}
#endif
