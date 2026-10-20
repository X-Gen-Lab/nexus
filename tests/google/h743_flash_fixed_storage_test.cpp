/**
 * \file            h743_flash_fixed_storage_test.cpp
 * \brief           Fixed physical workspace admission before effects
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_mpu_support.h"
#include "h743_flash_ram_closure_support.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>
extern "C" {
alignas(1024)
    nx_stm32h743_flash_ram_workspace_t g_nx_stm32h743_flash_ram_workspace;
}
using namespace nexus_test::h743_ram;
class H743FlashFixedStorage : public testing::Test {
  protected:
    std::array<uint32_t, 524288U> memory{};
    nx_stm32h743_flash_state_t port{};
    alignas(1024) nx_stm32h743_flash_ram_workspace_t foreign{};
    void SetUp() override {
        nx_h743_model_reset();
        g_nx_stm32h743_flash_ram_workspace = {};
        port.registers = &g_h743_model.flash;
        port.memory = reinterpret_cast<uint8_t*>(memory.data());
        port.poll_limit = 8U;
        port.supply_mv = 3300U;
        g_h743_model.physical_flash = &port;
        g_nx_stm32h743_system.started = true;
        g_nx_stm32h743_system.remaining_effects = NX_STM32H743_EFFECT_TIMER;
        ResetRamCpu(&foreign);
        cpu.owner_port = &port;
        cpu.primask = 0U;
        cpu.control = 0U;
        g_h743_model.core->VTOR = cpu.original_vtor;
        g_h743_model.flash.CR1 = g_h743_model.flash.CR2 = FLASH_CR_LOCK;
    }
    void TearDown() override {
        if (port.execution != nullptr) {
            EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(port.execution));
        }
        cpu.execution = nullptr;
        cpu.owner_port = nullptr;
    }
};
TEST_F(H743FlashFixedStorage, ForeignAlignedWorkspaceRejectsBeforeEffects) {
    const auto before = foreign;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_flash_ram_initialize(&port, &foreign));
    EXPECT_EQ(0, std::memcmp(&before, &foreign, sizeof(foreign)));
    EXPECT_EQ(nullptr, port.execution);
    EXPECT_EQ(0U, cpu.copy_writes);
    EXPECT_EQ(0U, cpu.vtor_writes);
    EXPECT_EQ(0U, g_h743_model.writes);
    EXPECT_EQ(0U, g_h743_model.flash_store_count);
}
TEST_F(H743FlashFixedStorage, ExistingSelectedWorkspacePreparesAndStops) {
    auto& selected = g_nx_stm32h743_flash_ram_workspace;
    ResetRamCpu(&selected);
    cpu.owner_port = &port;
    cpu.primask = 0U;
    cpu.control = 0U;
    g_h743_model.core->VTOR = cpu.original_vtor;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_initialize(&port, &selected));
    EXPECT_EQ(&selected, port.execution);
    EXPECT_TRUE(selected.prepared);
    EXPECT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_stop(&selected));
    EXPECT_EQ(nullptr, port.execution);
    EXPECT_FALSE(selected.prepared);
}

TEST_F(H743FlashFixedStorage, ForeignCleanupCannotDetachTheSelectedOwner) {
    auto& selected = g_nx_stm32h743_flash_ram_workspace;
    ResetRamCpu(&selected);
    cpu.owner_port = &port;
    cpu.primask = 0U;
    cpu.control = 0U;
    g_h743_model.core->VTOR = cpu.original_vtor;
    ASSERT_EQ(NX_SUCCESS, nx_stm32h743_flash_ram_initialize(&port, &selected));
    std::array<unsigned char, sizeof(selected)> before{};
    std::memcpy(before.data(), &selected, before.size());
    const unsigned copies = cpu.copy_writes;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32h743_flash_ram_stop(&foreign));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32h743_flash_ram_stop(
                  reinterpret_cast<nx_stm32h743_flash_ram_workspace_t*>(
                      uintptr_t{1U})));
    EXPECT_EQ(&selected, port.execution);
    EXPECT_EQ(0, std::memcmp(before.data(), &selected, before.size()));
    EXPECT_EQ(copies, cpu.copy_writes);
    EXPECT_EQ(writes, g_h743_model.writes);
}
TEST_F(H743FlashFixedStorage, ForeignBindingCannotAdmitOrExposeSelectedRam) {
    port.execution = &foreign;
    const uint32_t writes = g_h743_model.writes;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32h743_flash_ram_admission(&port));
    EXPECT_FALSE(nx_stm32h743_flash_ram_external(
        &port, g_nx_stm32h743_flash_ram_workspace.words, sizeof(uint32_t)));
    EXPECT_EQ(writes, g_h743_model.writes);
    EXPECT_EQ(&foreign, port.execution);
    /* No constructor or pulse was accepted; clear only this forged binding. */
    port.execution = nullptr;
}
