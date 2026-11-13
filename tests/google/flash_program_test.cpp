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
#include "stm32f407_basic_cold.h"
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
static bool refuse_option_relock;
static bool using_write_ports;
static bool irq_active;
static bool privileged = true;
static uint32_t clock_hpre;
/** \brief Independent RM0090 density oracle for the production engine. */
static const nx_flash_sector_t s_sectors[] = {
    {0x00000U, 0x04000U}, {0x04000U, 0x04000U}, {0x08000U, 0x04000U},
    {0x0C000U, 0x04000U}, {0x10000U, 0x10000U}, {0x20000U, 0x20000U},
    {0x40000U, 0x20000U}, {0x60000U, 0x20000U}, {0x80000U, 0x20000U},
    {0xA0000U, 0x20000U}, {0xC0000U, 0x20000U}, {0xE0000U, 0x20000U}};
static const nx_flash_erase_span_t s_spans[] = {
    {0U, 0x4000U, 4U}, {0x10000U, 0x10000U, 1U}, {0x20000U, 0x20000U, 3U}};
const nx_flash_geometry_t g_nx_stm32_flash_ve = {0x08000000U, 524288U, 4U,
                                                 s_spans,     3U,      0U};

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
                        s_sectors[sector].offset,
                    0xff, s_sectors[sector].size);
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
    if (reg == &model->registers->OPTCR && refuse_option_relock &&
        (value & FLASH_OPTCR_OPTLOCK) != 0U)
        return;
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
    if (reg == &model->registers->OPTKEYR && value == 0x4c5d6e7fU)
        model->registers->OPTCR &= ~static_cast<uint32_t>(FLASH_OPTCR_OPTLOCK);
    if (reg == &model->registers->OPTCR &&
        (value & FLASH_OPTCR_OPTSTRT) != 0U) {
        model->registers->OPTCR &= ~static_cast<uint32_t>(FLASH_OPTCR_OPTSTRT);
        complete(false);
    }
}
extern "C" void nx_flash_program_model_store(volatile uint32_t* address,
                                             uint32_t value) {
    *address &= value;
    ++program_pulses;
    complete(false);
}
extern "C" void nx_flash_program_model_store_byte(volatile uint8_t* address,
                                                  uint8_t value) {
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
extern "C" bool nx_flash_program_model_irq_enabled(void) {
    return irq_enabled;
}
extern "C" bool nx_flash_program_model_privileged(void) {
    return privileged;
}
extern "C" uint32_t nx_flash_program_model_hpre(void) {
    return clock_hpre;
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
        refuse_option_relock = false;
        using_write_ports = false;
        irq_active = false;
        privileged = true;
        clock_hpre = 0U;
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
TEST_F(PhysicalFlashProgram, ExplicitCloseRetriesAnOwnedFailedRelock) {
    refuse_relock = true;
    ASSERT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    ASSERT_TRUE(state.active);
    unsigned pulses = erase_pulses;
    refuse_relock = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&state));
    EXPECT_FALSE(state.active);
    EXPECT_NE(0U, registers.CR & FLASH_CR_LOCK);
    EXPECT_EQ(pulses, erase_pulses);
    EXPECT_FALSE(irq_pending);
}
TEST_F(PhysicalFlashProgram, ExplicitCloseRetriesAnOwnedPendingWithdrawal) {
    refuse_pending_clear = true;
    ASSERT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    ASSERT_TRUE(state.active);
    unsigned pulses = erase_pulses;
    refuse_pending_clear = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&state));
    EXPECT_FALSE(state.active);
    EXPECT_FALSE(irq_pending);
    EXPECT_EQ(pulses, erase_pulses);
}
TEST_F(PhysicalFlashProgram, ExplicitCloseCannotWithdrawAForeignActiveVector) {
    refuse_relock = true;
    ASSERT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    refuse_relock = false;
    irq_active = true;
    uint32_t before = Control();
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_flash_close(&state));
    EXPECT_TRUE(state.active);
    EXPECT_EQ(before, Control());
    irq_active = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&state));
    EXPECT_FALSE(state.active);
}
TEST_F(PhysicalFlashProgram, ExplicitClosePreservesForeignUnlockedOptions) {
    registers.OPTCR = 0x0fffaaecU;
    uint32_t before = registers.OPTCR;
    refuse_relock = true;
    ASSERT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    refuse_relock = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&state));
    EXPECT_FALSE(state.active);
    EXPECT_EQ(before, registers.OPTCR);
}
TEST_F(PhysicalFlashProgram, PublicRecoveryRequiresActualHardwareWithdrawal) {
    refuse_relock = true;
    ASSERT_EQ(NX_ERROR_IO, nx_flash_port_erase(&face, 0U, EraseUnit(), 1000U));
    unsigned pulses = erase_pulses;
    uint32_t before = Control();
    registers.SR |= FLASH_SR_BSY;
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_port_recover(&face));
    EXPECT_EQ(before, Control());
    registers.SR &= ~static_cast<uint32_t>(FLASH_SR_BSY);
    irq_enabled = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_port_recover(&face));
    EXPECT_TRUE(state.active);
    EXPECT_EQ(before, Control());
    irq_enabled = false;
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_flash_port_recover(&face));
    privileged = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_port_recover(&face));
    EXPECT_TRUE(state.active);
    refuse_relock = false;
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_recover(&face));
    EXPECT_FALSE(state.active);
    EXPECT_EQ(pulses, erase_pulses);
}
TEST_F(PhysicalFlashProgram, IdleRecoveryPreservesForeignPendingEvidence) {
    irq_pending = true;
    uint32_t before = Control();
    EXPECT_EQ(NX_SUCCESS, nx_flash_port_recover(&face));
    EXPECT_TRUE(irq_pending);
    EXPECT_EQ(before, Control());
}
TEST_F(PhysicalFlashProgram, RecoveryIsAnOptionalCheckedPublicOperation) {
    nx_flash_ops_t methods = nx_stm32_flash_ops;
    methods.recover = nullptr;
    nx_flash_port_t unsupported{&methods, &state};
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_flash_port_recover(&unsupported));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_port_recover(nullptr));
    EXPECT_FALSE(state.active);
    EXPECT_EQ(0U, erase_pulses);
    EXPECT_EQ(0U, program_pulses);
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

/** \brief Independent fixed OTP address oracle from RM0090 table 19. */
class PhysicalFlashMaintenance : public PhysicalFlashProgram {
  protected:
    const nx_flash_maintenance_port_t maintenance{
        &nx_stm32_flash_maintenance_ops, &state};
    uint8_t* otp = reinterpret_cast<uint8_t*>(0x1fff7800U);
    void* mapping = MAP_FAILED;
    void SetUp() override {
        PhysicalFlashProgram::SetUp();
        mapping = mmap(
            reinterpret_cast<void*>(0x1fff7000U), 4096U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, mapping);
        std::memset(otp, 0xff, 528U);
        registers.OPTCR = 0x0fffaaedU;
    }
    void TearDown() override {
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mapping, 4096U));
        }
        PhysicalFlashProgram::TearDown();
    }
    nx_flash_options_t OpenOptions() const {
        return {NX_FLASH_READ_OPEN,
                0U,
                NX_FLASH_BROWNOUT_DISABLED,
                false,
                false,
                false};
    }
};

TEST_F(PhysicalFlashMaintenance, ExactOtpAndVeGeometryAreCopied) {
    nx_flash_maintenance_snapshot_t snapshot{};
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_maintenance_snapshot(&maintenance, &snapshot));
    EXPECT_EQ(512U, snapshot.otp_bytes);
    EXPECT_EQ(32U, snapshot.otp_block_bytes);
    EXPECT_EQ(8U, snapshot.sector_count);
    EXPECT_EQ(0U, snapshot.otp_locked_blocks);
    EXPECT_EQ(NX_FLASH_READ_OPEN, snapshot.options.read_protection);
    EXPECT_FALSE(snapshot.power_on_reset_required);
}

TEST_F(PhysicalFlashMaintenance, OtpReadCannotReachLockOrSystemMemory) {
    uint8_t output = 0U;
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_maintenance_otp_read(&maintenance, 511U, &output, 1U));
    EXPECT_EQ(0xffU, output);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_maintenance_otp_read(&maintenance, 512U, &output, 1U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_maintenance_otp_read(
                                    &maintenance, UINT32_MAX, &output, 1U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_maintenance_otp_read(
                                    &maintenance, 1U, &output, SIZE_MAX));
    EXPECT_EQ(0U, program_pulses);
}

TEST_F(PhysicalFlashMaintenance, EffectsAreRequiredBeforeUnlockOrPulse) {
    uint32_t word = 0x12345678U;
    auto desired = OpenOptions();
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_maintenance_otp_program(
                                    &maintenance, 0U, &word, 4U, 0U, 1000U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_maintenance_otp_lock(&maintenance, 0U, 0U, 1000U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_maintenance_set_options(
                                    &maintenance, &desired, 0U, 1000U));
    EXPECT_EQ(FLASH_CR_LOCK, registers.CR);
    EXPECT_EQ(0U, program_pulses);
}

TEST_F(PhysicalFlashMaintenance, OtpProgramsOnlySelectedWordsWithoutMainErase) {
    uint32_t words[2]{0x12345678U, 0xfeed0123U};
    ASSERT_EQ(NX_SUCCESS, nx_flash_maintenance_otp_program(
                              &maintenance, 24U, words, sizeof(words),
                              NX_FLASH_MAINTAIN_OTP_PROGRAM, 1000U));
    EXPECT_EQ(0, std::memcmp(otp + 24U, words, sizeof(words)));
    EXPECT_EQ(2U, program_pulses);
    EXPECT_EQ(0U, erase_pulses);
    EXPECT_EQ(0xffU, memory[0]);
    EXPECT_FALSE(state.active);
}

TEST_F(PhysicalFlashMaintenance,
       OtpPreflightRejectsLockedTailBeforeFirstPulse) {
    uint32_t words[2]{};
    otp[512U + 1U] = 0U;
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_maintenance_otp_program(
                                 &maintenance, 28U, words, sizeof(words),
                                 NX_FLASH_MAINTAIN_OTP_PROGRAM, 1000U));
    EXPECT_EQ(0U, program_pulses);
    EXPECT_EQ(0xffU, otp[28U]);
}

TEST_F(PhysicalFlashMaintenance, OtpRejectsZeroToOneBeforeAnyPulse) {
    uint32_t words[2]{0U, UINT32_MAX};
    otp[4U] = 0U;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_maintenance_otp_program(
                               &maintenance, 0U, words, sizeof(words),
                               NX_FLASH_MAINTAIN_OTP_PROGRAM, 1000U));
    EXPECT_EQ(0U, program_pulses);
}

TEST_F(PhysicalFlashMaintenance, LockWritesExactlyZeroToOneLockByte) {
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_maintenance_otp_lock(&maintenance, 15U,
                                            NX_FLASH_MAINTAIN_OTP_LOCK, 1000U));
    EXPECT_EQ(0U, otp[527U]);
    EXPECT_EQ(0xffU, otp[526U]);
    EXPECT_EQ(0xffU, otp[511U]);
    EXPECT_EQ(1U, program_pulses);
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_maintenance_otp_lock(&maintenance, 15U,
                                            NX_FLASH_MAINTAIN_OTP_LOCK, 1000U));
    EXPECT_EQ(1U, program_pulses);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_maintenance_otp_lock(&maintenance, 16U,
                                            NX_FLASH_MAINTAIN_OTP_LOCK, 1000U));
}

TEST_F(PhysicalFlashMaintenance, CorruptLockByteIsNotAnUnlockedBlock) {
    otp[512U] = 0x7fU;
    uint32_t word = 0U;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_maintenance_otp_program(
                               &maintenance, 0U, &word, sizeof(word),
                               NX_FLASH_MAINTAIN_OTP_PROGRAM, 1000U));
    EXPECT_EQ(NX_ERROR_IO,
              nx_flash_maintenance_otp_lock(&maintenance, 0U,
                                            NX_FLASH_MAINTAIN_OTP_LOCK, 1000U));
    EXPECT_EQ(0U, program_pulses);
}

TEST_F(PhysicalFlashMaintenance,
       DensitySpecificProtectionRejectsVeSectorEight) {
    auto desired = OpenOptions();
    desired.write_protected_sectors = 1U << 8U;
    uint32_t before = registers.OPTCR;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
    EXPECT_EQ(before, registers.OPTCR);
}

TEST_F(PhysicalFlashMaintenance, PermanentProtectionNeedsSeparateDeclaration) {
    auto desired = OpenOptions();
    desired.read_protection = NX_FLASH_READ_PERMANENT;
    uint32_t before = registers.OPTCR;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
    EXPECT_EQ(before, registers.OPTCR);
}

TEST_F(PhysicalFlashMaintenance, ReadProtectionRegressionNeedsMassEraseEffect) {
    registers.OPTCR = 0x0fff55edU;
    auto desired = OpenOptions();
    uint32_t before = registers.OPTCR;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
    EXPECT_EQ(before, registers.OPTCR);
    EXPECT_EQ(0U, erase_pulses);
}

TEST_F(PhysicalFlashMaintenance, ExistingPermanentProtectionCannotBeChanged) {
    registers.OPTCR = 0x0fffccedU;
    auto desired = OpenOptions();
    uint32_t before = registers.OPTCR;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_maintenance_set_options(&maintenance, &desired,
                                               NX_FLASH_MAINTAIN_OPTIONS |
                                                   NX_FLASH_MAINTAIN_MASS_ERASE,
                                               1000U));
    EXPECT_EQ(before, registers.OPTCR);
}

TEST_F(PhysicalFlashMaintenance, OptionsRequirePorAndNeverResetTheMachine) {
    auto desired = OpenOptions();
    desired.write_protected_sectors = 0x81U;
    desired.hardware_watchdog = true;
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
    nx_flash_maintenance_snapshot_t snapshot{};
    ASSERT_EQ(NX_SUCCESS,
              nx_flash_maintenance_snapshot(&maintenance, &snapshot));
    EXPECT_EQ(0x81U, snapshot.options.write_protected_sectors);
    EXPECT_TRUE(snapshot.options.hardware_watchdog);
    EXPECT_TRUE(snapshot.power_on_reset_required);
    EXPECT_NE(0U, registers.OPTCR & FLASH_OPTCR_OPTLOCK);
    EXPECT_FALSE(state.active);
}
TEST_F(PhysicalFlashMaintenance, BrownoutProfilesMapSupplyIntentToHardware) {
    const std::array<nx_flash_brownout_t, 4> profiles{
        NX_FLASH_BROWNOUT_DISABLED, NX_FLASH_BROWNOUT_SUPPLY_2V1,
        NX_FLASH_BROWNOUT_SUPPLY_2V4, NX_FLASH_BROWNOUT_SUPPLY_2V7};
    for (size_t index = 0U; index < profiles.size(); ++index) {
        auto desired = OpenOptions();
        desired.brownout = profiles[index];
        ASSERT_EQ(NX_SUCCESS, nx_flash_maintenance_set_options(
                                  &maintenance, &desired,
                                  NX_FLASH_MAINTAIN_OPTIONS, 1000U));
        EXPECT_EQ(3U - index, (registers.OPTCR & FLASH_OPTCR_BOR_LEV) >> 2U);
        nx_flash_maintenance_snapshot_t snapshot{};
        ASSERT_EQ(NX_SUCCESS,
                  nx_flash_maintenance_snapshot(&maintenance, &snapshot));
        EXPECT_EQ(profiles[index], snapshot.options.brownout);
    }
}

TEST_F(PhysicalFlashMaintenance, UnknownBrownoutProfileRejectsBeforeUnlock) {
    for (uint8_t invalid : std::array<uint8_t, 2>{0U, 0xffU}) {
        auto desired = OpenOptions();
        desired.brownout = invalid;
        const auto before = registers.OPTCR;
        EXPECT_EQ(NX_ERROR_INVALID, nx_flash_maintenance_set_options(
                                        &maintenance, &desired,
                                        NX_FLASH_MAINTAIN_OPTIONS, 1000U));
        EXPECT_EQ(before, registers.OPTCR);
        EXPECT_EQ(0U, registers.OPTKEYR);
        EXPECT_FALSE(state.active);
    }
}

TEST_F(PhysicalFlashMaintenance, BorChangeRejectsUnsafeAhbDividerBeforeUnlock) {
    auto desired = OpenOptions();
    desired.brownout = NX_FLASH_BROWNOUT_SUPPLY_2V1;
    clock_hpre = RCC_CFGR_HPRE_DIV4;
    uint32_t before = registers.OPTCR;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
    EXPECT_EQ(before, registers.OPTCR);
    EXPECT_EQ(0U, registers.KEYR);
    EXPECT_EQ(0U, registers.OPTKEYR);
    EXPECT_FALSE(state.active);
    EXPECT_FALSE(state.power_on_reset_required);
}
TEST_F(PhysicalFlashMaintenance, NonBorOptionsDoNotRequireAnAhbDividerChange) {
    auto desired = OpenOptions();
    desired.write_protected_sectors = 1U;
    clock_hpre = RCC_CFGR_HPRE_DIV4;
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
    EXPECT_EQ(RCC_CFGR_HPRE_DIV4, clock_hpre);
}
TEST_F(PhysicalFlashMaintenance, BorChangeSupportsBothSafeAhbDividers) {
    auto desired = OpenOptions();
    desired.brownout = NX_FLASH_BROWNOUT_SUPPLY_2V1;
    clock_hpre = RCC_CFGR_HPRE_DIV2;
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
    desired.brownout = NX_FLASH_BROWNOUT_SUPPLY_2V4;
    clock_hpre = RCC_CFGR_HPRE_DIV1;
    EXPECT_EQ(NX_SUCCESS,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
}

TEST_F(PhysicalFlashMaintenance, MissingPulseCompletionRetainsError) {
    uint32_t word = 0U;
    missing_end = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_maintenance_otp_program(
                               &maintenance, 0U, &word, sizeof(word),
                               NX_FLASH_MAINTAIN_OTP_PROGRAM, 1000U));
    EXPECT_EQ(1U, program_pulses);
}

TEST_F(PhysicalFlashMaintenance, FailedRelockRetainsTheSharedPhysicalOwner) {
    uint32_t word = 0U;
    refuse_relock = true;
    EXPECT_EQ(NX_ERROR_IO, nx_flash_maintenance_otp_program(
                               &maintenance, 0U, &word, sizeof(word),
                               NX_FLASH_MAINTAIN_OTP_PROGRAM, 1000U));
    EXPECT_TRUE(state.active);
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_port_erase(&face, 0U, 16384U, 1000U));
    refuse_relock = false;
}
TEST_F(PhysicalFlashMaintenance, ExplicitCloseRetriesItsOwnOptionRelock) {
    auto desired = OpenOptions();
    desired.write_protected_sectors = 1U;
    refuse_option_relock = true;
    ASSERT_EQ(NX_ERROR_IO,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
    ASSERT_TRUE(state.active);
    ASSERT_EQ(0U, registers.OPTCR & FLASH_OPTCR_OPTLOCK);
    unsigned pulses = program_pulses + erase_pulses;
    refuse_option_relock = false;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_flash_close(&state));
    EXPECT_FALSE(state.active);
    EXPECT_NE(0U, registers.OPTCR & FLASH_OPTCR_OPTLOCK);
    EXPECT_TRUE(state.power_on_reset_required);
    EXPECT_EQ(pulses, program_pulses + erase_pulses);
}

TEST_F(PhysicalFlashMaintenance, ForeignUnlockedOptionsCannotBeOverwritten) {
    auto desired = OpenOptions();
    desired.reset_on_stop = true;
    registers.OPTCR &= ~static_cast<uint32_t>(FLASH_OPTCR_OPTLOCK);
    uint32_t before = registers.OPTCR;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_flash_maintenance_set_options(
                  &maintenance, &desired, NX_FLASH_MAINTAIN_OPTIONS, 1000U));
    EXPECT_EQ(before, registers.OPTCR);
    EXPECT_EQ(FLASH_CR_LOCK, registers.CR);
}

TEST_F(PhysicalFlashMaintenance, InfiniteAndExpiredDeadlinesNeverUnlockOtp) {
    uint32_t word = 0U;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_maintenance_otp_program(
                  &maintenance, 0U, &word, sizeof(word),
                  NX_FLASH_MAINTAIN_OTP_PROGRAM, NX_DEADLINE_NEVER));
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_flash_maintenance_otp_lock(&maintenance, 0U,
                                            NX_FLASH_MAINTAIN_OTP_LOCK, 0U));
    EXPECT_EQ(0U, program_pulses);
    EXPECT_EQ(FLASH_CR_LOCK, registers.CR);
}

TEST_F(PhysicalFlashMaintenance, MaintenanceOutputCannotOverwriteItsOwner) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_flash_maintenance_otp_read(
                                    &maintenance, 0U, &state, sizeof(state)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_flash_maintenance_snapshot(
                  &maintenance,
                  reinterpret_cast<nx_flash_maintenance_snapshot_t*>(&state)));
    EXPECT_EQ(FLASH_CR_LOCK, registers.CR);
}

TEST_F(PhysicalFlashMaintenance, ForeignPendingVectorRemainsOwnedOnOtpReject) {
    uint32_t word = 0U;
    irq_pending = true;
    EXPECT_EQ(NX_ERROR_BUSY, nx_flash_maintenance_otp_program(
                                 &maintenance, 0U, &word, sizeof(word),
                                 NX_FLASH_MAINTAIN_OTP_PROGRAM, 1000U));
    EXPECT_TRUE(irq_pending);
    EXPECT_EQ(0U, program_pulses);
    irq_pending = false;
}
