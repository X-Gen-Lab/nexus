/**
 * \file            h743_flash_image_admission_test.cpp
 * \brief           Actual H743 public Flash calls protect the running image
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_fixture.h"
#include "h743_flash_image_admission_model.h"
#include "nexus/io/flash.h"
#include "stm32h743_provider.h"
#include <array>
#include <cstddef>
#include <cstring>
#include <gtest/gtest.h>
#include <new>

extern "C" const nx_stm32h743_flash_image_t*
nx_h743_flash_image_admission_model(void) {
    /* Physical image metadata is separate from the host backing memory. It
     * models linker/load ownership, not host execution from that address. */
    static const nx_stm32h743_flash_image_t image{0x08000000U, 0x08005c20U,
                                                  0x08000000U, 0x08005c6cU};
    return &image;
}
class H743FlashImageAdmission : public testing::Test {
  protected:
    std::array<uint32_t, 524288U> memory{};
    std::array<uint8_t, 32U> input{};
    nx_stm32h743_flash_state_t state{};
    nx_flash_port_t face{&nx_stm32h743_flash_ops, &state};
    alignas(1024) nx_stm32h743_flash_ram_workspace_t execution{};
    void SetUp() override {
        nx_h743_model_reset();
        /* Erased model bytes deliberately isolate image ownership from the
         * existing visibly-non-erased heuristic. This is not an ARM image. */
        memory.fill(UINT32_MAX);
        input.fill(0x5aU);
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.core_hz = 64000000U;
        g_h743_model.power.D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
        g_h743_model.power.CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
        state.registers = &g_h743_model.flash;
        state.registers->ACR = FLASH_ACR_LATENCY_7WS | FLASH_ACR_WRHIGHFREQ_0;
        state.registers->CR1 = state.registers->CR2 = FLASH_CR_LOCK;
        state.memory = reinterpret_cast<uint8_t*>(memory.data());
        state.supply_mv = 3300U;
        state.poll_limit = 8U;
        g_h743_model.physical_flash = &state;
        nexus_test::h743_fixture::ResetExternalCpu();
        nexus_test::h743_fixture::SetIsolatedTimer(64000000U);
        ASSERT_EQ(NX_SUCCESS,
                  nexus_test::h743_fixture::Prepare(&state, &execution));
    }
    void TearDown() override {
        nexus_test::h743_fixture::CheckedStop(&execution);
        nexus_test::h743_fixture::StopIsolatedTimer();
    }
    void ExpectRejectedWithoutPulse(uint32_t offset, bool erase) {
        const nx_result_t result =
            erase ? nx_flash_port_erase(&face, offset, 131072U, 10000U)
                  : nx_flash_port_program(&face, offset, input.data(),
                                          input.size(), 10000U);
        EXPECT_EQ(NX_ERROR_STATE, result);
        EXPECT_EQ(0U, g_h743_model.writes);
        EXPECT_EQ(0U, g_h743_model.flash_store_count);
        EXPECT_EQ(FLASH_CR_LOCK, state.registers->CR1);
        EXPECT_EQ(FLASH_CR_LOCK, state.registers->CR2);
        EXPECT_FALSE(state.active);
    }
};
TEST_F(H743FlashImageAdmission, ProgramCannotChangeLiveVectorOrCodeOwnership) {
    ExpectRejectedWithoutPulse(0U, false);
}
TEST_F(H743FlashImageAdmission, EraseCannotRemoveTheRunningImageSector) {
    ExpectRejectedWithoutPulse(0U, true);
}
TEST_F(H743FlashImageAdmission, ProgramCannotChangeLoadOnlyInitializerBytes) {
    ExpectRejectedWithoutPulse(0x5c20U, false);
}
TEST_F(H743FlashImageAdmission, SpareBankOneRegionStillExecutesActualStores) {
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 131072U, input.data(),
                                                input.size(), 10000U));
    EXPECT_EQ(8U, g_h743_model.flash_store_count);
    EXPECT_EQ(0x5a5a5a5aU, memory[131072U / sizeof(uint32_t)]);
    EXPECT_FALSE(state.active);
}
TEST_F(H743FlashImageAdmission, BankTwoIsNotRemovedFromThePublicGeometry) {
    const auto* geometry = nx_flash_port_geometry(&face);
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(2097152U, geometry->size);
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 1048576U, input.data(),
                                                input.size(), 10000U));
    EXPECT_EQ(8U, g_h743_model.flash_store_count);
    EXPECT_EQ(0x5a5a5a5aU, memory[1048576U / sizeof(uint32_t)]);
    EXPECT_FALSE(state.active);
}

namespace {
template <typename T> auto ControlBytes(const T& value) {
    std::array<unsigned char, sizeof(T)> bytes{};
    std::memcpy(bytes.data(), &value, bytes.size());
    return bytes;
}
} /* namespace */
TEST_F(H743FlashImageAdmission, ReadCannotOverwriteTheFirstSystemControlByte) {
    const auto before = ControlBytes(g_nx_stm32h743_system);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_read(&face, 131072U, &g_nx_stm32h743_system, 1U));
    EXPECT_EQ(
        0, std::memcmp(before.data(), &g_nx_stm32h743_system, before.size()));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743FlashImageAdmission, ReadCannotOverwriteTheLastSystemControlByte) {
    const auto before = ControlBytes(g_nx_stm32h743_system);
    auto* bytes = reinterpret_cast<unsigned char*>(&g_nx_stm32h743_system);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_read(&face, 131072U,
                                 bytes + sizeof(g_nx_stm32h743_system) - 1U,
                                 1U));
    EXPECT_EQ(
        0, std::memcmp(before.data(), &g_nx_stm32h743_system, before.size()));
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashImageAdmission, ReadCannotOverwriteTheLiveMonotonicEpoch) {
    const uint64_t before = g_nx_stm32h743_system.overflow_us;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_read(&face, 131072U,
                                 &g_nx_stm32h743_system.overflow_us,
                                 sizeof(g_nx_stm32h743_system.overflow_us)));
    EXPECT_EQ(before, g_nx_stm32h743_system.overflow_us);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashImageAdmission, AStoppedSystemIsStillNotAnApplicationBuffer) {
    g_nx_stm32h743_system.started = false;
    const uint64_t before = g_nx_stm32h743_system.overflow_us;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_read(&face, 131072U,
                                 &g_nx_stm32h743_system.overflow_us,
                                 sizeof(g_nx_stm32h743_system.overflow_us)));
    EXPECT_EQ(before, g_nx_stm32h743_system.overflow_us);
    EXPECT_FALSE(g_nx_stm32h743_system.started);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashImageAdmission, ProgramCannotBorrowSystemControlAsPayload) {
    const auto before = ControlBytes(g_nx_stm32h743_system);
    static_assert(sizeof(g_nx_stm32h743_system) >= 32U);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_program(&face, 131072U, &g_nx_stm32h743_system, 32U,
                                    10000U));
    EXPECT_EQ(
        0, std::memcmp(before.data(), &g_nx_stm32h743_system, before.size()));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_FALSE(state.active);
}
TEST_F(H743FlashImageAdmission, ProgramCannotBorrowClockRegistersAsPayload) {
    static_assert(sizeof(g_h743_model.rcc) >= 32U);
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_flash_port_program(&face, 131072U, &g_h743_model.rcc, 32U, 10000U));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    EXPECT_FALSE(state.active);
}
TEST_F(H743FlashImageAdmission, ReadCannotWriteTheSystemClockRegisters) {
    const uint32_t before = g_h743_model.rcc.CR;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_read(&face, 131072U,
                                 const_cast<uint32_t*>(&g_h743_model.rcc.CR),
                                 sizeof(uint32_t)));
    EXPECT_EQ(before, g_h743_model.rcc.CR);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashImageAdmission, ReadCannotWriteTheSystemPowerRegisters) {
    const uint32_t before = g_h743_model.power.D3CR;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_flash_port_read(&face, 131072U,
                           const_cast<uint32_t*>(&g_h743_model.power.D3CR),
                           sizeof(uint32_t)));
    EXPECT_EQ(before, g_h743_model.power.D3CR);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashImageAdmission, ReadCannotWriteTheSystemTimebaseRegisters) {
    const uint32_t before = g_h743_model.timer.CNT;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_read(&face, 131072U,
                                 const_cast<uint32_t*>(&g_h743_model.timer.CNT),
                                 sizeof(uint32_t)));
    EXPECT_EQ(before, g_h743_model.timer.CNT);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashImageAdmission, ReadCannotWriteTheSystemVectorSelector) {
    const uint32_t before = g_h743_model.core->VTOR;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_flash_port_read(&face, 131072U,
                           const_cast<uint32_t*>(&g_h743_model.core->VTOR),
                           sizeof(uint32_t)));
    EXPECT_EQ(before, g_h743_model.core->VTOR);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashImageAdmission, ReadCannotUseFlashBackedControllerMetadata) {
    alignas(nx_stm32h743_flash_state_t) std::array<uint32_t, 524288U> backing{};
    backing.fill(UINT32_MAX);
    auto* raw = reinterpret_cast<unsigned char*>(backing.data());
    auto* intrusive = ::new (raw + 131072U) nx_stm32h743_flash_state_t(state);
    intrusive->memory = reinterpret_cast<uint8_t*>(backing.data());
    const nx_flash_port_t invalid_face{&nx_stm32h743_flash_ops, intrusive};
    uint8_t output = 0x5aU;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_read(&invalid_face, 0U, &output, sizeof(output)));
    EXPECT_EQ(0x5aU, output);
    EXPECT_EQ(0U, g_h743_model.writes);
}
TEST_F(H743FlashImageAdmission, EraseCannotTargetItsOwnControllerMetadata) {
    alignas(nx_stm32h743_flash_state_t) std::array<uint32_t, 524288U> backing{};
    backing.fill(UINT32_MAX);
    auto* raw = reinterpret_cast<unsigned char*>(backing.data());
    auto* intrusive = ::new (raw + 131072U) nx_stm32h743_flash_state_t(state);
    intrusive->memory = reinterpret_cast<uint8_t*>(backing.data());
    const nx_flash_port_t invalid_face{&nx_stm32h743_flash_ops, intrusive};
    g_h743_model.physical_flash = intrusive;
    /* The external device refuses START so an expected bad implementation
     * cannot destroy its C++ fixture or cause a test-process bus fault. This
     * does not hide attempted configuration/trigger writes from the oracle. */
    g_h743_model.hold_flash_start = true;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_erase(&invalid_face, 131072U, 131072U, 10000U));
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
    /* The rejected operation retained no metadata. Withdraw the external
     * hardware model's temporary pointer before its stack backing expires. */
    g_h743_model.physical_flash = &state;
}
TEST_F(H743FlashImageAdmission, ExplicitControlBoundariesRemainHalfOpen) {
    struct AdjacentStorage {
        unsigned char before[alignof(nx_stm32h743_flash_state_t)];
        nx_stm32h743_flash_state_t controller;
        unsigned char after[alignof(nx_stm32h743_flash_state_t)];
    } storage{};
    storage.controller = state;
    const nx_flash_port_t owned_face{&nx_stm32h743_flash_ops,
                                     &storage.controller};
    auto* raw = reinterpret_cast<unsigned char*>(&storage);
    constexpr size_t start = offsetof(AdjacentStorage, controller);
    constexpr size_t end = start + sizeof(storage.controller);
    ASSERT_GE(start, 1U);
    ASSERT_LT(end, sizeof(storage));
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_read(&owned_face, 131072U, raw + start - 1U, 1U));
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_read(&owned_face, 131072U, raw + end, 1U));
    const auto before = ControlBytes(storage.controller);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_read(&owned_face, 131072U, raw + start - 1U, 2U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_read(&owned_face, 131072U, raw + end - 1U, 2U));
    EXPECT_EQ(0,
              std::memcmp(before.data(), &storage.controller, before.size()));
    EXPECT_EQ(0U, g_h743_model.writes);
}
