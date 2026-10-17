/**
 * \file            h743_flash_image_admission_test.cpp
 * \brief           Actual H743 public Flash calls protect the running image
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_image_admission_model.h"
#include "nexus/io/flash.h"
#include "stm32h743_provider.h"
#include <array>
#include <gtest/gtest.h>

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
