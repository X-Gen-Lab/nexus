/**
 * \file            cipher_test.cpp
 * \brief           Real CRYP HAL ownership, bounded IRQ and secret withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
#include "stm32f4_cipher.h"
void nx_hal_test_cpu_reset(void);
bool nx_hal_test_isr(bool value);
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
nx_stm32_system_t g_nx_stm32_system;
alignas(16) uint8_t g_nx_stm32_scs_model[0x2000U] = {};
}
namespace {
nx_time_us_t now;
bool refuse_reset;
bool refuse_gate;
bool refuse_irq_quiet;
unsigned writes;
unsigned tick_reads;
std::array<uint32_t, 32> input_words;
unsigned input_count;
unsigned output_count;
std::array<uint32_t, 8> hidden_keys;
void HardwareReset() {
    hidden_keys.fill(0U);
    std::memset(CRYP, 0, sizeof(*CRYP));
    *const_cast<volatile uint32_t*>(&CRYP->SR) = CRYP_SR_IFEM | CRYP_SR_IFNF;
}
class Cipher : public testing::Test {
  protected:
    nx_stm32f4_cipher_state_t state{};
    const nx_stm32f4_cipher_config_t config{{NX_IRQ_KERNEL_NONE, 82U, 4U, 0U}};
    nx_cipher_port_t face{&nx_stm32f4_cipher_ops, &state};
    alignas(4) std::array<uint8_t, 64> input{};
    alignas(4) std::array<uint8_t, 64> output{};
    std::array<uint8_t, 32> key{};
    std::array<uint8_t, 16> iv{};
    nx_cipher_request_t request{};
    nx_cipher_operation_t operation{};
    std::array<void*, 2> maps{};
    void SetUp() override {
        const uintptr_t addresses[] = {CRYP_BASE, RCC_BASE & ~uintptr_t{4095U}};
        for (size_t i = 0U; i < maps.size(); ++i) {
            maps[i] =
                mmap(reinterpret_cast<void*>(addresses[i]), 4096U,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            ASSERT_EQ(reinterpret_cast<void*>(addresses[i]), maps[i]);
        }
        std::memset(g_nx_stm32_scs_model, 0, sizeof(g_nx_stm32_scs_model));
        HardwareReset();
        nx_hal_test_cpu_reset();
        now = 10U;
        refuse_reset = refuse_gate = refuse_irq_quiet = false;
        writes = tick_reads = input_count = output_count = 0U;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        g_nx_stm32_system.flash = FLASH;
        g_nx_stm32_system.core_hz = NEXUS_CORE_HZ;
        g_nx_stm32_system.started = true;
        g_nx_stm32_system.time_epoch = 7U;
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        RCC->PLLCFGR = NEXUS_CLOCK_PLLCFGR;
        RCC->CFGR = NEXUS_CLOCK_CFGR | 2U | 8U;
        FLASH->ACR = NEXUS_FLASH_WAIT_STATES;
        for (size_t i = 0U; i < key.size(); ++i)
            key[i] = static_cast<uint8_t>(i + 1U);
        for (size_t i = 0U; i < input.size(); ++i)
            input[i] = static_cast<uint8_t>(i);
        iv.fill(0x42U);
        nx_request_initialize(&request.base);
        operation = {NX_CIPHER_AES_CBC,
                     NX_CIPHER_ENCRYPT,
                     input.data(),
                     output.data(),
                     16U,
                     key.data(),
                     16U,
                     iv.data(),
                     16U};
    }
    void TearDown() override {
        for (void* map : maps) {
            if (map != nullptr && map != MAP_FAILED) {
                EXPECT_EQ(0, munmap(map, 4096U));
            }
        }
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f4_cipher_start(&state, &config));
    }
    void Prepare() {
        ASSERT_EQ(NX_SUCCESS, nx_cipher_prepare(&request, &operation, 100U));
    }
    void Submit() {
        Start();
        Prepare();
        ASSERT_EQ(NX_SUCCESS, nx_cipher_port_submit(&face, &request));
    }
    void Irq(uint32_t flags) {
        *const_cast<volatile uint32_t*>(&CRYP->MISR) = flags;
        *const_cast<volatile uint32_t*>(&CRYP->RISR) = flags;
        *const_cast<volatile uint32_t*>(&CRYP->SR) =
            CRYP_SR_IFEM | CRYP_SR_IFNF |
            ((flags & CRYP_IT_OUTI) != 0U ? CRYP_SR_OFNE : 0U);
        bool previous = nx_hal_test_isr(true);
        nx_stm32f4_cipher_irq(&state);
        nx_hal_test_isr(previous);
    }
    void Complete() {
        Irq(CRYP_IT_INI);
        Irq(CRYP_IT_OUTI);
    }
};
TEST_F(Cipher, QueryReportsExactBlockLimitsAndPreservesOutputOnFailure) {
    Start();
    nx_cipher_constraints_t limits{};
    ASSERT_EQ(NX_SUCCESS,
              nx_cipher_port_query(&face, NX_CIPHER_AES_CTR, &limits));
    EXPECT_EQ(4U, limits.alignment);
    EXPECT_EQ(16U, limits.length_multiple);
    EXPECT_EQ(65520U, limits.maximum_bytes);
    EXPECT_TRUE(limits.exact_in_place);
    auto before = limits;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_cipher_port_query(&face, static_cast<nx_cipher_algorithm_t>(7),
                                   &limits));
    EXPECT_EQ(0, std::memcmp(&before, &limits, sizeof(limits)));
}
TEST_F(Cipher, ForeignGateRejectsBeforeRetainingConfiguration) {
    RCC->AHB2ENR |= RCC_AHB2ENR_CRYPEN;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32f4_cipher_start(&state, &config));
    EXPECT_EQ(nullptr, state.config);
    EXPECT_EQ(0U, writes);
}
TEST_F(Cipher, GateRefusalRetainsResponsibilityUntilSuccessfulStop) {
    refuse_gate = true;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32f4_cipher_start(&state, &config));
    EXPECT_TRUE(state.gate_owned);
    refuse_gate = false;
    EXPECT_EQ(NX_SUCCESS, nx_cipher_port_stop(&face));
    EXPECT_EQ(nullptr, state.config);
}
TEST_F(Cipher, ExpiredAndInvalidTimeRejectWithoutBorrowOrRegisterWrites) {
    Start();
    Prepare();
    unsigned before = writes;
    now = 100U;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_cipher_port_submit(&face, &request));
    now = NX_DEADLINE_NEVER;
    EXPECT_EQ(NX_ERROR_IO, nx_cipher_port_submit(&face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(before, writes);
    EXPECT_EQ(nullptr, state.active);
}
TEST_F(Cipher, PrivateAliasAndUnalignedInputRejectWithoutHardwareEffects) {
    Start();
    operation.input = input.data() + 1U;
    Prepare();
    unsigned before = writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_cipher_port_submit(&face, &request));
    operation.input = &state;
    Prepare();
    EXPECT_EQ(NX_ERROR_INVALID, nx_cipher_port_submit(&face, &request));
    EXPECT_EQ(before, writes);
}
TEST_F(Cipher, StandardByteKeyAndIvAreLoadedThenHandlePointersAreCleared) {
    Submit();
    EXPECT_EQ(UINT32_C(0x01020304), hidden_keys[4]);
    EXPECT_EQ(UINT32_C(0x0d0e0f10), hidden_keys[7]);
    EXPECT_EQ(UINT32_C(0x42424242), CRYP->IV0LR);
    EXPECT_EQ(nullptr, state.handle.Init.pKey);
    EXPECT_EQ(nullptr, state.handle.Init.pInitVect);
    EXPECT_EQ(0U, tick_reads);
}
TEST_F(Cipher, OfficialHalMovesOnlyFullBlocksAndSettlementRequiresTaskCleanup) {
    Submit();
    Complete();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(4U, input_count);
    EXPECT_EQ(4U, output_count);
    EXPECT_EQ(0U, tick_reads);
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_service(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(16U, request.base.transferred);
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(nullptr, state.handle.pCrypInBuffPtr);
    EXPECT_EQ(nullptr, state.handle.pCrypOutBuffPtr);
    EXPECT_EQ(0U, hidden_keys[4] | hidden_keys[7] | CRYP->IV0LR);
    EXPECT_EQ(1U, key[0]);
}
TEST_F(Cipher, RefusedResetKeepsKeysAndBorrowQuarantinedUntilRecovery) {
    Submit();
    Complete();
    refuse_reset = true;
    EXPECT_EQ(NX_ERROR_IO, nx_cipher_port_service(&face));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_NE(0U, hidden_keys[4]);
    refuse_reset = false;
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_service(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, hidden_keys[4]);
}
TEST_F(Cipher, CancelIsFirstTerminalCauseAndPreservesCallerKey) {
    Submit();
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_cancel(&face, &request));
    now = 101U;
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_service(&face));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(1U, key[0]);
}
TEST_F(Cipher, DeadlineAndLostEpochDrainThroughSecretCleanup) {
    Submit();
    now = 100U;
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_service(&face));
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
    Prepare();
    now = 20U;
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_submit(&face, &request));
    ++g_nx_stm32_system.time_epoch;
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_service(&face));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, hidden_keys[4]);
}
TEST_F(Cipher, StopJoinsActiveAndLateIrqDoesNotReadRetiredConfigOrRequest) {
    Submit();
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_stop(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(nullptr, state.config);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN);
    state.config = reinterpret_cast<const nx_stm32f4_cipher_config_t*>(1U);
    state.active = reinterpret_cast<nx_cipher_request_t*>(1U);
    unsigned before = writes;
    Irq(CRYP_IT_INI | CRYP_IT_OUTI);
    EXPECT_EQ(before, writes);
}
TEST_F(Cipher, TwoKeyTdesExpandsK1K2K1AndUsesEightByteBlocks) {
    operation.algorithm = NX_CIPHER_TDES_CBC;
    operation.bytes = 8U;
    operation.iv_bytes = 8U;
    Submit();
    EXPECT_EQ(hidden_keys[2], hidden_keys[6]);
    EXPECT_EQ(hidden_keys[3], hidden_keys[7]);
    Complete();
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_service(&face));
    EXPECT_EQ(8U, request.base.transferred);
    EXPECT_EQ(2U, input_count);
    EXPECT_EQ(2U, output_count);
}
TEST_F(Cipher, QueryCannotOverwriteTheBorrowedDescriptorOrKey) {
    Submit();
    auto before = request;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_cipher_port_query(
                  &face, NX_CIPHER_AES_ECB,
                  reinterpret_cast<nx_cipher_constraints_t*>(&request)));
    EXPECT_EQ(0, std::memcmp(&before, &request, sizeof(request)));
}
TEST_F(Cipher, RefusedSourceWithdrawalQuietsDedicatedVectorAndRetainsBorrow) {
    Submit();
    const unsigned bank = static_cast<unsigned>(CRYP_IRQn) / 32U;
    const uint32_t bit = UINT32_C(1)
                         << (static_cast<unsigned>(CRYP_IRQn) % 32U);
    NVIC->ISER[bank] = bit;
    refuse_irq_quiet = true;
    now = 100U;
    Irq(CRYP_IT_INI);
    EXPECT_EQ(0U, NVIC->ISER[bank] & bit);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_TRUE(state.faulted);
    refuse_irq_quiet = false;
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_stop(&face));
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
}
TEST_F(Cipher, HalKeyPreparationFailureIsAcceptedAndFullyWithdrawn) {
    operation.direction = NX_CIPHER_DECRYPT;
    Start();
    Prepare();
    *const_cast<volatile uint32_t*>(&CRYP->SR) = CRYP_SR_BUSY;
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_submit(&face, &request));
    EXPECT_NE(NX_REQUEST_READY, nx_request_state(&request.base));
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_service(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, CRYP->CR & CRYP_CR_CRYPEN);
    EXPECT_EQ(0U, hidden_keys[4]);
}
TEST_F(Cipher, CancelAfterObservedSettlementIsAnIdempotentNoOp) {
    Submit();
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_cancel(&face, &request));
    ASSERT_EQ(NX_SUCCESS, nx_cipher_port_service(&face));
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    unsigned before = writes;
    EXPECT_EQ(NX_SUCCESS, nx_cipher_port_cancel(&face, &request));
    EXPECT_EQ(before, writes);
}
} /* namespace */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" uint32_t HAL_GetTick(void) {
    return tick_reads++;
}
extern "C" void nx_crypto_model_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
    if (reg == &CRYP->IMSCR && value == 0U && refuse_irq_quiet)
        return;
    for (unsigned bank = 0U; bank < 3U; ++bank) {
        if (reg == &NVIC->ICER[bank])
            NVIC->ISER[bank] &= ~value;
    }
    if (reg == &RCC->AHB2ENR && refuse_gate)
        return;
    if (reg == &RCC->AHB2RSTR && (value & RCC_AHB2RSTR_CRYPRST) != 0U) {
        if (refuse_reset)
            return;
        HardwareReset();
    }
    *reg = value;
}
extern "C" void nx_crypto_model_cryp_input(void* instance, uint32_t word) {
    EXPECT_EQ(CRYP, instance);
    ASSERT_LT(input_count, input_words.size());
    input_words[input_count++] = word;
}
extern "C" uint32_t nx_crypto_model_cryp_output(void* instance) {
    EXPECT_EQ(CRYP, instance);
    ++output_count;
    return UINT32_C(0x12345678);
}
extern "C" void nx_crypto_model_hash_input(uint32_t) {
}
extern "C" uint32_t nx_crypto_model_hash_digest(unsigned) {
    return 0U;
}

extern "C" void nx_crypto_model_cryp_key(volatile uint32_t* reg,
                                         uint32_t word) {
    size_t index = (reinterpret_cast<uintptr_t>(reg) -
                    reinterpret_cast<uintptr_t>(&CRYP->K0LR)) /
                   sizeof(uint32_t);
    ASSERT_LT(index, hidden_keys.size());
    hidden_keys[index] = word;
    *reg = 0U;
}
