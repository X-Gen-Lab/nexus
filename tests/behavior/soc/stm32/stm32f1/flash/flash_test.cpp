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

#if defined(STM32F103xG)
constexpr uint32_t page = 2048U;
constexpr uint32_t bank_size = 512U * 1024U;
constexpr uint32_t size = 2U * bank_size;
#elif defined(STM32F103xE)
constexpr uint32_t page = 2048U;
constexpr uint32_t size = 512U * 1024U;
constexpr uint32_t alternate_size = size;
#else
constexpr uint32_t page = 1024U;
constexpr uint32_t size = 64U * page;
constexpr uint32_t alternate_size = 128U * page;
#endif
#if defined(STM32F103xG)
constexpr nx_flash_erase_span_t spans[]{{0U, page, bank_size / page},
                                        {bank_size, page, bank_size / page}};
#else
constexpr nx_flash_erase_span_t spans[]{{0U, page, size / page}};
#endif
constexpr nx_flash_geometry_t geometry{FLASH_BASE,       size, 2U, spans,
                                       std::size(spans), 0U};
constexpr uint32_t clock_ready = RCC_CR_HSION | RCC_CR_HSIRDY;
constexpr nx_time_us_t future = 10000U;

struct Model {
    FLASH_TypeDef flash{};
    alignas(2) std::array<uint8_t, size> memory{};
    uint32_t clock{};
    unsigned writes{};
    std::array<unsigned, std::size(spans)> bank_writes{};
    uint32_t pulse_error{};
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

struct Bank {
    volatile uint32_t* status;
    volatile uint32_t* control;
    volatile uint32_t* key;
    volatile uint32_t* address;
};

static Bank bank(unsigned index) {
#if defined(STM32F103xG)
    if (index == 1U) {
        return {&hardware.flash.SR2, &hardware.flash.CR2, &hardware.flash.KEYR2,
                &hardware.flash.AR2};
    }
#else
    (void)index;
#endif
    return {&hardware.flash.SR, &hardware.flash.CR, &hardware.flash.KEYR,
            &hardware.flash.AR};
}

static unsigned register_bank(const volatile uint32_t* reg) {
#if defined(STM32F103xG)
    const Bank second = bank(1U);
    if (reg == second.status || reg == second.control || reg == second.key ||
        reg == second.address) {
        return 1U;
    }
#else
    (void)reg;
#endif
    return 0U;
}

extern "C" uint32_t nx_f1_flash_model_read(const volatile uint32_t* reg) {
    const Bank selected = bank(register_bank(reg));
    if (reg == selected.address) {
        ADD_FAILURE() << "The FPEC address register is write-only";
        return 0U;
    }
    if (reg == selected.control && hardware.delayed_busy) {
        hardware.delayed_busy = false;
        *selected.status |= FLASH_SR_BSY;
    }
    if (reg == selected.status) {
        EXPECT_FALSE(hardware.delayed_busy)
            << "A later peripheral access is required before sampling BSY";
        if ((*selected.status & FLASH_SR_BSY) != 0U &&
            hardware.busy_reads != 0U) {
            hardware.now += 10U;
            if (--hardware.busy_reads == 0U) {
                *selected.status =
                    hardware.pulse_error != 0U
                        ? hardware.pulse_error
                        : (hardware.omit_eop ? 0U : FLASH_SR_EOP);
            }
        }
    }
    return *reg;
}

extern "C" void nx_f1_flash_model_write(volatile uint32_t* reg,
                                        uint32_t value) {
    ++hardware.writes;
    const unsigned index = register_bank(reg);
    ++hardware.bank_writes[index];
    const Bank selected = bank(index);
    if (reg == selected.status) {
        if (!hardware.refuse_w1c) {
            *selected.status &= ~value;
        }
        return;
    }
    if (reg == selected.control && value == FLASH_CR_LOCK &&
        hardware.refuse_relock) {
        return;
    }
    if (reg == selected.key && value == 0xcdef89abU) {
        EXPECT_EQ(*selected.key, 0x45670123U);
        *selected.control &= ~static_cast<uint32_t>(FLASH_CR_LOCK);
    }
    *reg = value;
    if (reg == selected.control && (value & FLASH_CR_STRT) != 0U) {
        EXPECT_EQ(value, FLASH_CR_PER | FLASH_CR_STRT);
        const uint32_t offset =
            *selected.address - static_cast<uint32_t>(FLASH_BASE);
        ASSERT_LT(offset, size);
        ASSERT_EQ(0U, offset % page);
#if defined(STM32F103xG)
        EXPECT_EQ(index, offset / bank_size);
#endif
        ++hardware.launches;
        if (hardware.pulse_error == 0U) {
            std::fill_n(hardware.memory.data() + offset, page, UINT8_MAX);
        }
        *selected.control &= ~static_cast<uint32_t>(FLASH_CR_STRT);
        hardware.delayed_busy = true;
        hardware.busy_reads = 4U;
    }
}

extern "C" void nx_f1_flash_model_store(volatile uint16_t* address,
                                        uint16_t value) {
    unsigned index = 0U;
    auto* bytes = reinterpret_cast<volatile uint8_t*>(address);
#if defined(STM32F103xG)
    index = static_cast<unsigned>((bytes - hardware.memory.data()) / bank_size);
#endif
    const Bank selected = bank(index);
    EXPECT_EQ(*selected.control, FLASH_CR_PG);
    EXPECT_EQ(hardware.clock, clock_ready);
    ++hardware.launches;
    if (hardware.pulse_error == 0U) {
        bytes[0] = static_cast<uint8_t>(value);
        bytes[1] = static_cast<uint8_t>(value >> 8U);
    }
    *selected.status = FLASH_SR_BSY;
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
        for (unsigned index = 0U; index < std::size(spans); ++index) {
            *bank(index).control = FLASH_CR_LOCK;
        }
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
#if defined(STM32F103xG)
    const nx_flash_geometry_t alternate_geometry = geometry;
#else
    const nx_flash_erase_span_t alternate_span{0U, page, alternate_size / page};
    const nx_flash_geometry_t alternate_geometry{
        FLASH_BASE, alternate_size, 2U, &alternate_span, 1U, 0U};
#endif
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f1_flash_prepare(&state, &alternate_geometry, 1999U));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_stm32f1_flash_prepare(&state, &alternate_geometry, 3601U));
    constexpr uint32_t wrong_page = page == 1024U ? 2048U : 1024U;
    const nx_flash_erase_span_t wrong_pages{
        0U, wrong_page, alternate_geometry.size / wrong_page};
    const nx_flash_geometry_t wrong_geometry{
        FLASH_BASE, alternate_geometry.size, 2U, &wrong_pages, 1U, 0U};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_flash_prepare(&state, &wrong_geometry, 3300U));
    ASSERT_EQ(NX_SUCCESS,
              nx_stm32f1_flash_prepare(&state, &alternate_geometry, 2000U));
    EXPECT_EQ(&alternate_geometry, nx_flash_port_geometry(&face));
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
    EXPECT_EQ(NX_SUCCESS,
              nx_stm32f1_flash_prepare(&state, &alternate_geometry, 3600U));
    EXPECT_EQ(0U, hardware.writes);
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
}

#if defined(STM32F103xE)
TEST_F(F1Flash, ZeRejectsSmallerHighDensityCapacity) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
    const nx_flash_erase_span_t smaller{0U, page, 128U};
    const nx_flash_geometry_t wrong{FLASH_BASE, 256U * 1024U, 2U,
                                    &smaller,   1U,           0U};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_flash_prepare(&state, &wrong, 3300U));
    EXPECT_EQ(0U, hardware.writes);
}

TEST_F(F1Flash, ZeProgramsAndErasesAtTheLastSingleBankPage) {
    constexpr uint32_t last_page = size - page;
    hardware.memory[last_page - 1U] = 0U;
    const std::array<uint8_t, 2U> data{0x29U, 0xa8U};
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, size - 2U, data.data(),
                                                data.size(), future));
    EXPECT_EQ(data[0], hardware.memory[size - 2U]);
    EXPECT_EQ(data[1], hardware.memory[size - 1U]);
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_erase(&face, last_page, page, future));
    EXPECT_EQ(UINT32_C(0x0807f800), hardware.flash.AR);
    EXPECT_EQ(UINT8_MAX, hardware.memory[last_page]);
    EXPECT_EQ(UINT8_MAX, hardware.memory[size - 1U]);
    EXPECT_EQ(0U, hardware.memory[last_page - 1U]);
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_erase(&face, size, page, future));
    EXPECT_EQ(2U, hardware.launches);
}
#endif

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

#if defined(STM32F103xG)
TEST_F(F1Flash, ZgRequiresTheExactTwoBankLayout) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
    const nx_flash_erase_span_t merged{0U, page, size / page};
    const nx_flash_geometry_t merged_banks{FLASH_BASE, size, 2U,
                                           &merged,    1U,   0U};
    const nx_flash_geometry_t single_bank{FLASH_BASE, bank_size, 2U,
                                          spans,      1U,        0U};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_flash_prepare(&state, &merged_banks, 3300U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32f1_flash_prepare(&state, &single_bank, 3300U));
    EXPECT_EQ(0U, hardware.writes);
}

TEST_F(F1Flash, ZgRejectsCrossBankPulsesBeforeUnlock) {
    const std::array<uint8_t, 4U> input{0x13U, 0x24U, 0x35U, 0x46U};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_program(&face, bank_size - 2U, input.data(),
                                    input.size(), future));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_port_erase(&face, bank_size - page, 2U * page, future));
    EXPECT_EQ(0U, hardware.writes);
    hardware.memory[bank_size - 1U] = input[0];
    hardware.memory[bank_size] = input[1];
    std::array<uint8_t, 2U> output{};
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_read(&face, bank_size - 1U,
                                             output.data(), output.size()));
    EXPECT_EQ(input[0], output[0]);
    EXPECT_EQ(input[1], output[1]);
}

TEST_F(F1Flash, ZgProgramsAndErasesBank2ThroughItsOwnRegisters) {
    const std::array<uint8_t, 2U> data{0x91U, 0xbdU};
    hardware.memory[size - page - 1U] = 0U;
    hardware.flash.SR = FLASH_SR_EOP;
    ASSERT_EQ(NX_SUCCESS, nx_flash_port_program(&face, size - 2U, data.data(),
                                                data.size(), future));
    EXPECT_EQ(data[0], hardware.memory[size - 2U]);
    EXPECT_EQ(data[1], hardware.memory[size - 1U]);
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_port_erase(&face, size - page, page, future));
    EXPECT_EQ(UINT32_C(0x080ff800), hardware.flash.AR2);
    EXPECT_EQ(UINT8_MAX, hardware.memory[size - 1U]);
    EXPECT_EQ(0U, hardware.memory[size - page - 1U]);
    EXPECT_EQ(0U, hardware.bank_writes[0]);
    EXPECT_EQ(static_cast<uint32_t>(FLASH_SR_EOP), hardware.flash.SR);
    EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_LOCK), hardware.flash.CR2);
}

TEST_F(F1Flash, ZgProtectionAndTimeoutSettleTheSelectedBank) {
    const std::array<uint8_t, 2U> data{0x91U, 0xbdU};
    hardware.flash.SR = FLASH_SR_EOP;
    hardware.pulse_error = FLASH_SR2_WRPRTERR;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, bank_size, data.data(),
                                                 data.size(), future));
    EXPECT_EQ(UINT8_MAX, hardware.memory[bank_size]);
    EXPECT_EQ(0U, hardware.flash.SR2);
    hardware.pulse_error = 0U;
    hardware.memory[bank_size] = 0U;
    hardware.memory[bank_size + page] = 0U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_flash_port_erase(&face, bank_size, 2U * page,
                                                    hardware.now + 15U));
    EXPECT_EQ(UINT8_MAX, hardware.memory[bank_size]);
    EXPECT_EQ(0U, hardware.memory[bank_size + page]);
    EXPECT_EQ(2U, hardware.launches);
    EXPECT_EQ(0U, hardware.bank_writes[0]);
}

TEST_F(F1Flash, ZgRecoveryNeverTakesOverTheOtherBank) {
    hardware.flash.SR = FLASH_SR_BSY;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_flash_port_erase(&face, bank_size, page, future));
    EXPECT_EQ(0U, hardware.writes);
    hardware.flash.SR = 0U;
    hardware.refuse_relock = true;
    ASSERT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, bank_size, page, future));
    EXPECT_EQ(NX_ERROR_STATE, nx_flash_port_erase(&face, 0U, page, future));
    hardware.refuse_relock = false;
    hardware.flash.CR = 0U;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_recover(&face));
    EXPECT_EQ(0U, hardware.bank_writes[0]);
    EXPECT_EQ(0U, hardware.flash.CR);
    EXPECT_EQ(static_cast<uint32_t>(FLASH_CR_LOCK), hardware.flash.CR2);
    hardware.flash.CR = FLASH_CR_LOCK;
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_recover(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stm32f1_flash_close(&state));
    EXPECT_EQ(0U, hardware.bank_writes[0]);
    EXPECT_EQ(1U, hardware.launches);
}
#endif
