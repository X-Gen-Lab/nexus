/**
 * \file            flash_test.cpp
 * \brief           F103 physical halfword pulses through the public Flash API
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/flash/instance.h"
#include "nexus/arch/arch.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <new>

constexpr uint32_t page = 1024U;
constexpr uint32_t size = 64U * page;
constexpr nx_flash_erase_span_t span{0U, page, size / page};
constexpr nx_flash_geometry_t geometry{FLASH_BASE, size, 2U, &span, 1U, 0U};
constexpr uint32_t clock_ready = RCC_CR_HSION | RCC_CR_HSIRDY;
constexpr nx_time_us_t future = 10000U;

struct Model {
    FLASH_TypeDef flash{};
    alignas(2) std::array<uint8_t, size> memory{};
    uint32_t clock{};
    unsigned writes{};
    unsigned launches{};
    unsigned busy_reads{};
    nx_time_us_t now{};
    bool idle_irq{true};
    bool privileged{true};
    bool interrupt{};
    bool masked{};
    bool delayed_busy{};
    bool refuse_w1c{};
    bool refuse_relock{};
    bool omit_eop{};
};

static Model hardware;

extern "C" FLASH_TypeDef* nx_f1_flash_model_registers(void) {
    return &hardware.flash;
}

extern "C" volatile uint8_t* nx_f1_flash_model_memory(void) {
    return hardware.memory.data();
}

extern "C" uint32_t nx_f1_flash_model_read(const volatile uint32_t* reg) {
    if (reg == &hardware.flash.CR && hardware.delayed_busy) {
        hardware.delayed_busy = false;
        hardware.flash.SR |= FLASH_SR_BSY;
    }
    if (reg == &hardware.flash.SR) {
        EXPECT_FALSE(hardware.delayed_busy)
            << "ES096 2.2.11 requires a barrier and a later peripheral access";
        if ((hardware.flash.SR & FLASH_SR_BSY) != 0U) {
            hardware.now += 10U;
            if (--hardware.busy_reads == 0U) {
                hardware.flash.SR = hardware.omit_eop ? 0U : FLASH_SR_EOP;
            }
        }
    }
    return *reg;
}

extern "C" void nx_f1_flash_model_write(volatile uint32_t* reg,
                                        uint32_t value) {
    ++hardware.writes;
    if (reg == &hardware.flash.SR) {
        if (!hardware.refuse_w1c) {
            hardware.flash.SR &= ~value;
        }
        return;
    }
    if (reg == &hardware.flash.CR && value == FLASH_CR_LOCK &&
        hardware.refuse_relock) {
        return;
    }
    if (reg == &hardware.flash.KEYR && value == 0xcdef89abU) {
        EXPECT_EQ(hardware.flash.KEYR, 0x45670123U);
        hardware.flash.CR &= ~static_cast<uint32_t>(FLASH_CR_LOCK);
    }
    *reg = value;
    if (reg == &hardware.flash.CR && (value & FLASH_CR_STRT) != 0U) {
        EXPECT_EQ(value, FLASH_CR_PER | FLASH_CR_STRT);
        const uint32_t offset =
            hardware.flash.AR - static_cast<uint32_t>(FLASH_BASE);
        ASSERT_LT(offset, size);
        ASSERT_EQ(0U, offset % page);
        ++hardware.launches;
        std::fill_n(hardware.memory.data() + offset, page, UINT8_MAX);
        hardware.flash.CR &= ~static_cast<uint32_t>(FLASH_CR_STRT);
        hardware.delayed_busy = true;
        hardware.busy_reads = 4U;
    }
}

extern "C" void nx_f1_flash_model_store(volatile uint16_t* address,
                                        uint16_t value) {
    EXPECT_EQ(hardware.flash.CR, FLASH_CR_PG);
    EXPECT_EQ(hardware.clock, clock_ready);
    ++hardware.launches;
    auto* bytes = reinterpret_cast<volatile uint8_t*>(address);
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8U);
    hardware.flash.SR = FLASH_SR_BSY;
    hardware.busy_reads = 4U;
}

extern "C" bool nx_f1_flash_model_irq_idle(void) {
    return hardware.idle_irq;
}

extern "C" uint32_t nx_f1_flash_model_clock(void) {
    return hardware.clock;
}

extern "C" bool nx_arch_is_privileged(void) {
    return hardware.privileged;
}

extern "C" bool nx_arch_in_isr(void) {
    return hardware.interrupt;
}

extern "C" bool nx_arch_irq_is_masked(void) {
    return hardware.masked;
}

extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    return hardware.now;
}

class F1Flash : public testing::Test {
  protected:
    nx_stm32f1_flash_state_t state{};
    const nx_flash_port_t face{&nx_stm32f1_flash_ops, &state};

    void SetUp() override {
        hardware.~Model();
        new (&hardware) Model{};
        hardware.memory.fill(UINT8_MAX);
        hardware.flash.CR = FLASH_CR_LOCK;
        hardware.clock = clock_ready;
        ASSERT_EQ(NX_SUCCESS,
                  nx_stm32f1_flash_prepare(&state, &geometry, 3300U));
        ASSERT_EQ(0U, hardware.writes);
    }
};

TEST_F(F1Flash, GeometryAndBoundsDoNotAuthorizeAnErase) {
    EXPECT_EQ(&geometry, nx_flash_port_geometry(&face));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_erase(&face, 1U, page, future));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_erase(&face, size, page, future));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_erase(&face, 0U, page - 1U, future));
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_read(&face, size, nullptr, 0U));
    EXPECT_EQ(0U, hardware.writes);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
    EXPECT_EQ(nullptr, nx_flash_port_geometry(&face));
}

TEST_F(F1Flash, NeedsHsiAndSoleControllerBeforeAWrite) {
    hardware.clock = RCC_CR_HSION;
    EXPECT_EQ(NX_ERROR_STATE, nx_flash_port_erase(&face, 0U, page, future));
    hardware.clock = clock_ready;
    hardware.idle_irq = false;
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_port_erase(&face, 0U, page, future));
    hardware.idle_irq = true;
    hardware.interrupt = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_flash_port_erase(&face, 0U, page, future));
    hardware.interrupt = false;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_flash_port_erase(&face, 0U, page, 0U));
    EXPECT_EQ(0U, hardware.writes);
}

TEST_F(F1Flash, MatchesTheCompiledDensityGroupAndSupply) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
    const nx_flash_erase_span_t rb_span{0U, page, 128U};
    const nx_flash_geometry_t rb_geometry{FLASH_BASE, 128U * page, 2U,
                                          &rb_span,   1U,          0U};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f1_flash_prepare(&state, &rb_geometry, 1999U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f1_flash_prepare(&state, &rb_geometry, 3601U));
    const nx_flash_erase_span_t wrong_pages{0U, page * 2U, 64U};
    const nx_flash_geometry_t wrong_geometry{FLASH_BASE,   128U * page, 2U,
                                             &wrong_pages, 1U,          0U};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_flash_prepare(&state, &wrong_geometry, 3300U));
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f1_flash_prepare(&state, &rb_geometry, 2000U));
    EXPECT_EQ(&rb_geometry, nx_flash_port_geometry(&face));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32f1_flash_prepare(&state, &rb_geometry, 3600U));
    EXPECT_EQ(0U, hardware.writes);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
}

TEST_F(F1Flash, AcceptsByteAlignedSourceForAnAlignedHalfword) {
    alignas(2) const std::array<uint8_t, 3U> data{{0U, 0x35U, 0x68U}};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_program(&face, 1U, data.data() + 1U, 2U, future));
    EXPECT_EQ(0U, hardware.writes);
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_program(&face, 0U, data.data() + 1U, 2U, future));
    EXPECT_EQ(0x35U, hardware.memory[0U]);
    EXPECT_EQ(0x68U, hardware.memory[1U]);
    EXPECT_EQ(1U, hardware.launches);
}

TEST_F(F1Flash, ProgramsHalfwordAndSkipsErasedInput) {
    const std::array<uint8_t, 4U> data{{0x65, 0x12, 0xff, 0xff}};
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, 2U, data.data(),
                                                data.size(), future));
    std::array<uint8_t, 4U> output{};
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_read(&face, 2U, output.data(), output.size()));
    EXPECT_EQ(data, output);
    EXPECT_EQ(1U, hardware.launches);
    EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_LOCK), hardware.flash.CR);
    EXPECT_EQ(0U, hardware.flash.SR);
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 2U, data.data(),
                                                 data.size(), future));
    EXPECT_EQ(1U, hardware.launches);
}

TEST_F(F1Flash, ErasesExactlyOnePageAfterDelayedBusy) {
    std::fill_n(hardware.memory.data(), 3U * page, 0U);
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_erase(&face, page, page, future));
    EXPECT_EQ(1U, hardware.launches);
    EXPECT_EQ(0U, hardware.memory[page - 1U]);
    EXPECT_EQ(UINT8_MAX, hardware.memory[page]);
    EXPECT_EQ(UINT8_MAX, hardware.memory[2U * page - 1U]);
    EXPECT_EQ(0U, hardware.memory[2U * page]);
    EXPECT_EQ(0U, hardware.flash.SR);
}

TEST_F(F1Flash, WaitsForCurrentPulseAndDoesNotLaunchAnotherAfterDeadline) {
    std::fill_n(hardware.memory.data(), 2U * page, 0U);
    ASSERT_EQ(NX_ERROR_TIMEOUT, nx_flash_port_erase(&face, 0U, 2U * page, 15U));
    EXPECT_GE(hardware.now, 40U);
    EXPECT_EQ(1U, hardware.launches);
    EXPECT_EQ(UINT8_MAX, hardware.memory[0U]);
    EXPECT_EQ(0U, hardware.memory[page]);
    EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_LOCK), hardware.flash.CR);
    EXPECT_EQ(0U, hardware.flash.SR);
}

TEST_F(F1Flash, DoesNotUseStaleEopOrReplayUnprovenPulse) {
    hardware.flash.SR = FLASH_SR_EOP;
    hardware.refuse_w1c = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, page, future));
    EXPECT_EQ(0U, hardware.launches);

    hardware.refuse_w1c = false;
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_recover(&face));
    hardware.omit_eop = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, page, future));
    EXPECT_EQ(1U, hardware.launches);
}

TEST_F(F1Flash, FailureToRelockRetainsOwnerUntilRecovery) {
    hardware.refuse_relock = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, page, future));
    EXPECT_EQ(1U, hardware.launches);
    EXPECT_EQ(NX_ERROR_STATE, nx_flash_port_erase(&face, page, page, future));
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f1_flash_close(&state));
    EXPECT_EQ(&geometry, nx_flash_port_geometry(&face));
    hardware.refuse_relock = false;
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_recover(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
    EXPECT_EQ(1U, hardware.launches);
}
