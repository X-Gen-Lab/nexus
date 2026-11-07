/**
 * \file            flash_program_test.cpp
 * \brief           Real STM32F4 Flash no-op pulse regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "nexus/io/native/model.h"
#include "stm32f407_provider.h"
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

/** \brief External GPIOA register bank required by the shared cold helpers. */
GPIO_TypeDef g_nx_stm32_gpioa_model;

static unsigned program_pulses;
static unsigned erase_pulses, erase_attempts, refused_erase_attempt;
static bool refuse_start, refuse_flags, refuse_second_flags, missing_end;
static bool refuse_relock, irq_enabled, irq_pending, refuse_pending_clear;
static bool using_write_ports;
static bool irq_active;
static bool privileged = true;
/** \brief Independent RM0090 density oracle for the production engine. */
static const nx_flash_sector_t s_sectors[] = {
    {0x00000U, 0x04000U}, {0x04000U, 0x04000U}, {0x08000U, 0x04000U},
    {0x0C000U, 0x04000U}, {0x10000U, 0x10000U}, {0x20000U, 0x20000U},
    {0x40000U, 0x20000U}, {0x60000U, 0x20000U}, {0x80000U, 0x20000U},
    {0xA0000U, 0x20000U}, {0xC0000U, 0x20000U}, {0xE0000U, 0x20000U}};
const nx_flash_geometry_t g_nx_stm32_flash_ve = {0x08000000U, 524288U, 4U,
                                                 s_sectors,   8U,      0U};

static nx_stm32_flash_state_t* model;
static void complete(bool erase) {
    model->registers->SR &= ~static_cast<uint32_t>(FLASH_SR_BSY);
    if (!missing_end && (model->registers->CR & FLASH_CR_EOPIE) != 0U) {
        model->registers->SR |= FLASH_SR_EOP;
        irq_pending = true;
    }
    if (erase) {
        size_t sector = (model->registers->CR & FLASH_CR_SNB) >> 3U;
        std::memset(const_cast<uint8_t*>(model->memory) +
                        model->geometry->sectors[sector].offset,
                    0xff, model->geometry->sectors[sector].size);
    }
}
extern "C" void nx_flash_program_model_poll(unsigned kind, void* context) {
    if (kind != 3U)
        return;
    auto& state = *static_cast<nx_stm32_flash_state_t*>(context);
    if (state.registers->KEYR == 0xcdef89abU)
        state.registers->CR &= ~static_cast<uint32_t>(FLASH_CR_LOCK);
    if (!using_write_ports && (state.registers->CR & FLASH_CR_PG) != 0U) {
        ++program_pulses;
        complete(false);
    }
    if (!using_write_ports && (state.registers->CR & FLASH_CR_STRT) != 0U) {
        ++erase_attempts;
        state.registers->CR &= ~static_cast<uint32_t>(FLASH_CR_STRT);
        if (!refuse_start && erase_attempts != refused_erase_attempt) {
            ++erase_pulses;
            complete(true);
        }
    }
}
extern "C" uint32_t nx_flash_program_model_read(const volatile uint32_t* reg) {
    using_write_ports = true;
    return *reg;
}
extern "C" void nx_flash_program_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    if (reg == &model->registers->SR) {
        if (!refuse_flags && !(refuse_second_flags && erase_pulses != 0U))
            *reg &= ~value;
        return;
    }
    using_write_ports = true;
    if (reg == &model->registers->CR) {
        if (refuse_relock && (value & FLASH_CR_LOCK) != 0U)
            return;
        *reg = value;
        if ((value & FLASH_CR_STRT) != 0U) {
            ++erase_attempts;
            *reg &= ~static_cast<uint32_t>(FLASH_CR_STRT);
            if (!refuse_start && erase_attempts != refused_erase_attempt) {
                ++erase_pulses;
                complete(true);
            }
        }
        return;
    }
    *reg = value;
    if (reg == &model->registers->KEYR && value == 0xcdef89abU)
        model->registers->CR &= ~static_cast<uint32_t>(FLASH_CR_LOCK);
}
extern "C" void nx_flash_program_model_store(volatile uint32_t* address,
                                             uint32_t value) {
    *address &= value;
    ++program_pulses;
    complete(false);
}
extern "C" bool nx_flash_program_model_irq_idle(void) {
    return !irq_enabled && !irq_pending;
}
extern "C" bool nx_flash_program_model_irq_clear(void) {
    if (!refuse_pending_clear)
        irq_pending = false;
    return !irq_enabled && !irq_pending;
}
extern "C" bool nx_flash_program_model_irq_active(void) {
    return irq_active;
}
extern "C" bool nx_flash_program_model_privileged(void) {
    return privileged;
}

/** \brief Exercise production methods using actual private provider state. */
class PhysicalFlashProgram : public testing::Test {
  protected:
    FLASH_TypeDef registers{};
    alignas(4) std::array<uint8_t, 32768> storage;
    nx_stm32_flash_state_t state{};
    const nx_flash_port_t face{&nx_stm32_flash_ops, &state};
    static constexpr size_t unit = 4U;
    uint8_t* memory = storage.data();
    void SetUp() override {
        ASSERT_EQ(NX_SUCCESS, nx_native_clock_configure(true, 0U));
        program_pulses = 0U;
        erase_pulses = erase_attempts = refused_erase_attempt = 0U;
        refuse_start = refuse_flags = refuse_second_flags = missing_end = false;
        refuse_relock = irq_enabled = irq_pending = refuse_pending_clear =
            false;
        using_write_ports = false;
        irq_active = false;
        privileged = true;
        storage.fill(0xffU);
        state.registers = &registers;
        state.geometry = &g_nx_stm32_flash_ve;
        state.memory = memory;
        state.supply_mv = 3300U;
        registers.CR = FLASH_CR_LOCK;
        model = &state;
    }
    void TearDown() override {
    }
    size_t EraseUnit() const {
        return 16384U;
    }
    void OldEnd() {
        registers.SR |= FLASH_SR_EOP;
    }
    uint32_t Control() const {
        return registers.CR;
    }
    void Control(uint32_t value) {
        registers.CR = value;
    }
};
TEST_F(PhysicalFlashProgram, AllFfUnitsNeverIssueProgramPulse) {
    const uint8_t erased[8]{0xffU, 0xffU, 0xffU, 0xffU,
                            0xffU, 0xffU, 0xffU, 0xffU};
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_program(&face, 0U, erased, sizeof(erased), 1000U));
    EXPECT_EQ(0U, program_pulses);
    EXPECT_EQ(0xffU, memory[0]);
    EXPECT_EQ(0U, nx_flash_port_geometry(&face)->program_flags);
}
TEST_F(PhysicalFlashProgram, MixedInputSkipsOnlyWholeErasedUnits) {
    std::array<uint8_t, 16> input;
    input.fill(0xffU);
    input[unit] = 0x35U;
    input[3U * unit] = 0x46U;
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_program(&face, 0U, input.data(), 4U * unit, 1000U));
    EXPECT_EQ(2U, program_pulses);
    EXPECT_EQ(0, std::memcmp(memory, input.data(), 4U * unit));
}
TEST_F(PhysicalFlashProgram, OddTailRejectsBeforeAnyProgramPulse) {
    const uint8_t input[4]{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_port_program(&face, 0U, input, unit - 1U, 1000U));
    EXPECT_EQ(0U, program_pulses);
    EXPECT_EQ(0xffU, memory[0]);
}
TEST_F(PhysicalFlashProgram, RefusedEraseStartNeverSucceedsOnBlankPage) {
    refuse_start = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_EQ(0U, erase_pulses);
}
TEST_F(PhysicalFlashProgram, IdleWithoutFreshEndIsNotCompletion) {
    missing_end = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_EQ(1U, erase_pulses);
}
TEST_F(PhysicalFlashProgram, StaleEndAndRefusedW1cCannotLaunchPulse) {
    OldEnd();
    refuse_flags = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_EQ(0U, erase_pulses);
}
TEST_F(PhysicalFlashProgram, SecondEraseNeedsItsOwnFreshCompletion) {
    refused_erase_attempt = 2U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_erase(&face, 0U, 2U * EraseUnit(), 1000U));
    EXPECT_EQ(2U, erase_attempts);
    EXPECT_EQ(1U, erase_pulses);
}
TEST_F(PhysicalFlashProgram, ForeignEnabledFlashIrqRejectsBeforeAnyPulse) {
    irq_enabled = true;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_EQ(0U, erase_pulses);
    EXPECT_TRUE(irq_enabled);
    irq_enabled = false;
}
TEST_F(PhysicalFlashProgram, TwoPagesEachCompleteAndWithdrawTheirIrqSource) {
    std::memset(memory, 0U, 2U * EraseUnit());
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_port_erase(&face, 0U, 2U * EraseUnit(), 1000U));
    EXPECT_EQ(2U, erase_pulses);
    EXPECT_FALSE(irq_enabled);
    EXPECT_FALSE(irq_pending);
    EXPECT_EQ(0U, Control() & (1U << 24U));
    EXPECT_EQ(0xffU, memory[2U * EraseUnit() - 1U]);
}
TEST_F(PhysicalFlashProgram, SecondPageCannotReuseUnclearedEnd) {
    refuse_second_flags = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_port_erase(&face, 0U, 2U * EraseUnit(), 1000U));
    EXPECT_EQ(1U, erase_attempts);
    EXPECT_EQ(1U, erase_pulses);
}
TEST_F(PhysicalFlashProgram, ProgramAlsoRequiresFreshEnd) {
    const uint8_t zero[4]{};
    missing_end = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_program(&face, 0U, zero, unit, 1000U));
    EXPECT_EQ(1U, program_pulses);
}
TEST_F(PhysicalFlashProgram, ForeignPendingFlashIrqIsPreservedOnRejection) {
    irq_pending = true;
    uint32_t before = Control();
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_EQ(0U, erase_pulses);
    EXPECT_TRUE(irq_pending);
    EXPECT_EQ(before, Control());
    irq_pending = false;
}
TEST_F(PhysicalFlashProgram, ForeignProgramModeRejectsWithoutRewritingIt) {
    uint32_t before = Control() | 1U;
    Control(before);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_EQ(0U, erase_pulses);
    EXPECT_EQ(before, Control());
    Control(before & ~1U);
}
TEST_F(PhysicalFlashProgram, FailedRelockNeverReleasesConcreteLease) {
    refuse_relock = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_TRUE(state.active);
    refuse_relock = false;
}
TEST_F(PhysicalFlashProgram, FailedPendingWithdrawalCannotReportSuccess) {
    refuse_pending_clear = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_TRUE(irq_pending);
    EXPECT_TRUE(state.active);
    refuse_pending_clear = false;
}
TEST_F(PhysicalFlashProgram, ActiveFlashVectorIsNotAColdPollingLease) {
    irq_active = true;
    uint32_t before = Control();
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_EQ(0U, erase_pulses);
    EXPECT_EQ(before, Control());
    irq_active = false;
}
TEST_F(PhysicalFlashProgram, UnprivilegedControlsCannotTouchTheController) {
    privileged = false;
    const uint8_t zero[4]{};
    uint32_t before = Control();
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    EXPECT_EQ(NX_ERROR_CONTEXT,
              nx_flash_port_program(&face, 0U, zero, unit, 1000U));
    EXPECT_EQ(0U, erase_pulses);
    EXPECT_EQ(0U, program_pulses);
    EXPECT_EQ(before, Control());
    privileged = true;
}
