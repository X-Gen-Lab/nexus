/**
 * \file            hash_test.cpp
 * \brief           Byte-exact hardware FIFO protocol and shared-IRQ ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
#include "stm32f4_hash.h"
void nx_hal_test_cpu_reset(void);
bool nx_hal_test_isr(bool value);
}
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
alignas(16) uint8_t g_nx_stm32_scs_model[0x2000U] = {};
}
namespace {
nx_time_us_t now;
bool refuse_reset;
bool refuse_source;
unsigned wake_calls;
unsigned writes;
unsigned tick_reads;
std::array<uint32_t, 128> words;
size_t word_count;
unsigned finalizations;
void HardwareReset() {
    std::memset(HASH, 0, sizeof(*HASH));
    HASH->SR = HASH_SR_DINIS;
}
class Hash : public testing::Test {
  protected:
    nx_stm32f4_hash_state_t state{};
    const nx_stm32f4_hash_config_t config{{NX_IRQ_KERNEL_NONE, 82U, 4U, 0U}};
    nx_hash_port_t face{&nx_stm32f4_hash_ops, &state};
    std::array<uint8_t, 128> input{};
    std::array<uint8_t, 128> key{};
    std::array<uint8_t, 32> digest{};
    nx_hash_request_t request{};
    nx_hash_operation_t operation{};
    std::array<void*, 3> maps{};
    void SetUp() override {
        const uintptr_t addresses[] = {CRYP_BASE, RCC_BASE & ~uintptr_t{4095U},
                                       UINT32_C(0x21000000)};
        for (size_t i = 0U; i < maps.size(); ++i) {
            maps[i] =
                mmap(reinterpret_cast<void*>(addresses[i]),
                     i == 2U ? 8192U : 4096U, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            ASSERT_EQ(reinterpret_cast<void*>(addresses[i]), maps[i]);
        }
        std::memset(g_nx_stm32_scs_model, 0, sizeof(g_nx_stm32_scs_model));
        HardwareReset();
        nx_hal_test_cpu_reset();
        now = 10U;
        refuse_reset = refuse_source = false;
        wake_calls = 0U;
        writes = tick_reads = finalizations = 0U;
        word_count = 0U;
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
        input.fill(0x31U);
        key.fill(0x42U);
        digest.fill(0xa5U);
        nx_request_initialize(&request.base);
        operation = {NX_HASH_SHA1, input.data(),  3U,           nullptr,
                     0U,           digest.data(), digest.size()};
    }
    void TearDown() override {
        for (size_t i = 0U; i < maps.size(); ++i) {
            if (maps[i] != nullptr && maps[i] != MAP_FAILED) {
                EXPECT_EQ(0, munmap(maps[i], i == 2U ? 8192U : 4096U));
            }
        }
    }
    void Start() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32f4_hash_start(&state, &config));
    }
    void Submit() {
        Start();
        ASSERT_EQ(NX_SUCCESS, nx_hash_prepare(&request, &operation, 100U));
        ASSERT_EQ(NX_SUCCESS, nx_hash_port_submit(&face, &request));
    }
    void Irq(uint32_t flags) {
        HASH->SR = flags;
        bool previous = nx_hal_test_isr(true);
        nx_stm32f4_hash_irq(&state);
        nx_hal_test_isr(previous);
    }
};
TEST_F(Hash, ExactLimitsPermitEmptyAndUnalignedByteSpansWithoutPadding) {
    Start();
    nx_hash_constraints_t limits{};
    ASSERT_EQ(NX_SUCCESS,
              nx_hash_port_query(&face, NX_HASH_HMAC_SHA1, &limits));
    EXPECT_EQ(1U, limits.input_alignment);
    EXPECT_EQ(1U, limits.key_alignment);
    EXPECT_EQ(1U, limits.digest_alignment);
    EXPECT_TRUE(limits.empty_message);
    EXPECT_TRUE(limits.empty_key);
    EXPECT_EQ(20U, limits.digest_bytes);
}
TEST_F(Hash,
       ActualHalInitSelectsByteOrderWithoutPrivateTickOrPersistentHandle) {
    Submit();
    EXPECT_EQ(HASH_DATATYPE_8B, HASH->CR & HASH_CR_DATATYPE);
    EXPECT_EQ(0U, tick_reads);
    EXPECT_EQ(0U, word_count);
}
class HashTail : public Hash, public testing::WithParamInterface<size_t> {};
TEST_P(HashTail, PartialWordNeverReadsPastGuardedMessage) {
    const size_t count = GetParam();
    auto* end = reinterpret_cast<uint8_t*>(UINT32_C(0x21001000));
    std::memset(end - count, 0x31, count);
    ASSERT_EQ(0, mprotect(end, 4096U, PROT_NONE));
    operation.input = end - count;
    operation.bytes = count;
    ASSERT_EXIT(
        {
            if (nx_stm32f4_hash_start(&state, &config) != NX_SUCCESS ||
                nx_hash_prepare(&request, &operation, 100U) != NX_SUCCESS ||
                nx_hash_port_submit(&face, &request) != NX_SUCCESS)
                _exit(2);
            HASH->SR = HASH_SR_DINIS;
            nx_stm32f4_hash_irq(&state);
            if (count > 64U) {
                HASH->SR = HASH_SR_DINIS;
                nx_stm32f4_hash_irq(&state);
            }
            if (word_count != (count + 3U) / 4U || finalizations != 1U)
                _exit(3);
            const uint32_t mask = count % 4U == 1U   ? 0xffU
                                  : count % 4U == 2U ? 0xffffU
                                                     : 0xffffffU;
            if (words[word_count - 1U] != (UINT32_C(0x31313131) & mask))
                _exit(4);
            _exit(0);
        },
        testing::ExitedWithCode(0), "");
}
INSTANTIATE_TEST_SUITE_P(GuardedEnds, HashTail,
                         testing::Values(1U, 2U, 3U, 65U, 66U, 67U));
TEST_F(Hash, FirstFifoObservationIsSeventeenWordsThenAtMostSixteen) {
    operation.bytes = 128U;
    Submit();
    Irq(HASH_SR_DINIS);
    EXPECT_EQ(17U, word_count);
    EXPECT_EQ(0U, finalizations);
    Irq(HASH_SR_BUSY);
    EXPECT_EQ(17U, word_count);
    Irq(HASH_SR_DINIS);
    EXPECT_EQ(32U, word_count);
    EXPECT_EQ(1U, finalizations);
    EXPECT_EQ(0U, tick_reads);
}
TEST_F(Hash, HmacHasThreeSeparatePhasesAndNeverWaitsInsideIrq) {
    operation.algorithm = NX_HASH_HMAC_SHA1;
    operation.key = key.data();
    operation.key_bytes = 3U;
    Submit();
    Irq(HASH_SR_DINIS);
    EXPECT_EQ(1U, finalizations);
    EXPECT_EQ(1U, word_count);
    EXPECT_EQ(0U, HASH->IMR & HASH_IMR_DINIE);
    EXPECT_EQ(0U, tick_reads);
    for (unsigned i = 0U; i < 10U; ++i)
        ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(1U, word_count);
    HASH->SR = HASH_SR_DINIS;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(2U, finalizations);
    EXPECT_EQ(2U, word_count);
    HASH->SR = HASH_SR_DINIS;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(3U, finalizations);
    EXPECT_EQ(3U, word_count);
    EXPECT_EQ(UINT32_C(0x424242), words[0]);
    EXPECT_EQ(UINT32_C(0x313131), words[1]);
    EXPECT_EQ(UINT32_C(0x424242), words[2]);
}
TEST_F(Hash, EmptyHmacMessageAndKeyWriteNoFabricatedInputBytes) {
    operation = {NX_HASH_HMAC_MD5, nullptr,      0U, nullptr, 0U,
                 digest.data(),    digest.size()};
    Submit();
    for (unsigned i = 0U; i < 3U; ++i) {
        HASH->SR = HASH_SR_DINIS;
        ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    }
    EXPECT_EQ(0U, word_count);
    EXPECT_EQ(3U, finalizations);
}
TEST_F(Hash, DigestUsesStandardByteOrderAndWritesOnlyItsDeclaredLength) {
    operation.algorithm = NX_HASH_MD5;
    Submit();
    Irq(HASH_SR_DINIS);
    for (unsigned i = 0U; i < 5U; ++i)
        HASH->HR[i] = UINT32_C(0x01020304) + i;
    Irq(HASH_SR_DCIS);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(16U, request.base.transferred);
    EXPECT_EQ(1U, digest[0]);
    EXPECT_EQ(2U, digest[1]);
    EXPECT_EQ(3U, digest[2]);
    EXPECT_EQ(4U, digest[3]);
    EXPECT_EQ(0xa5U, digest[16]);
    EXPECT_EQ(0U, HASH->CR | HASH->IMR);
}
TEST_F(Hash, HashStopPreservesTheRngGateSourceAndSharedNvicLease) {
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    RNG->CR = RNG_CR_RNGEN | RNG_CR_IE;
    const unsigned bank = static_cast<unsigned>(HASH_RNG_IRQn) / 32U;
    const uint32_t bit = UINT32_C(1)
                         << (static_cast<unsigned>(HASH_RNG_IRQn) % 32U);
    NVIC->ISER[bank] = bit;
    Submit();
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_stop(&face));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_NE(0U, RCC->AHB2ENR & RCC_AHB2ENR_RNGEN);
    EXPECT_EQ(RNG_CR_RNGEN | RNG_CR_IE, RNG->CR);
    EXPECT_EQ(bit, NVIC->ISER[bank] & bit);
    EXPECT_EQ(0U, RCC->AHB2ENR & RCC_AHB2ENR_HASHEN);
}
TEST_F(Hash, CleanupRefusalRetainsBorrowAndFirstCancelCause) {
    Submit();
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_cancel(&face, &request));
    refuse_reset = true;
    EXPECT_EQ(NX_ERROR_IO, nx_hash_port_service(&face));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    now = 101U;
    refuse_reset = false;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}
TEST_F(Hash, InvalidTimeRejectsBeforeBorrowAndDeadlineCleansAcceptedRequest) {
    Start();
    ASSERT_EQ(NX_SUCCESS, nx_hash_prepare(&request, &operation, 100U));
    unsigned before = writes;
    now = NX_DEADLINE_NEVER;
    EXPECT_EQ(NX_ERROR_IO, nx_hash_port_submit(&face, &request));
    EXPECT_EQ(before, writes);
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    now = 10U;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_submit(&face, &request));
    now = 100U;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
}
TEST_F(Hash, RngOnlyIrqDoesNotPublishAHashWakeOrTouchItsEngine) {
    Start();
    const nx_irq_wake_t wake{nullptr,
                             [](void*) {
                                 ++wake_calls;
                                 return NX_SUCCESS;
                             },
                             false};
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_attach_wake(&face, &wake, 0U));
    ASSERT_EQ(NX_SUCCESS, nx_hash_prepare(&request, &operation, 100U));
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_submit(&face, &request));
    unsigned before = writes;
    Irq(0U);
    EXPECT_EQ(before, writes);
    EXPECT_EQ(0U, wake_calls);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
}
TEST_F(Hash, RefusedSourceMaskUsesOnlyHashResetAndRetainsTheBorrow) {
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    RNG->CR = RNG_CR_RNGEN | RNG_CR_IE;
    Submit();
    refuse_source = true;
    now = 100U;
    Irq(HASH_SR_DINIS);
    EXPECT_TRUE(state.faulted);
    EXPECT_TRUE(state.reset_owned);
    EXPECT_NE(0U, RCC->AHB2RSTR & RCC_AHB2RSTR_HASHRST);
    EXPECT_EQ(0U, HASH->IMR);
    EXPECT_EQ(&request, state.active);
    EXPECT_EQ(RNG_CR_RNGEN | RNG_CR_IE, RNG->CR);
    EXPECT_NE(0U, RCC->AHB2ENR & RCC_AHB2ENR_RNGEN);
    refuse_source = false;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_stop(&face));
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
}
TEST_F(Hash, LongHmacKeyUsesLkeyAndRepeatsExactlyTheOriginalBytes) {
    operation.algorithm = NX_HASH_HMAC_SHA1;
    operation.key = key.data();
    operation.key_bytes = 70U;
    Submit();
    EXPECT_NE(0U, HASH->CR & HASH_CR_LKEY);
    Irq(HASH_SR_DINIS);
    EXPECT_EQ(17U, word_count);
    EXPECT_EQ(0U, finalizations);
    Irq(HASH_SR_DINIS);
    EXPECT_EQ(18U, word_count);
    EXPECT_EQ(1U, finalizations);
    HASH->SR = HASH_SR_DINIS;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(2U, finalizations);
    HASH->SR = HASH_SR_DINIS;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(36U, word_count);
    EXPECT_EQ(2U, finalizations);
    Irq(HASH_SR_DINIS);
    EXPECT_EQ(37U, word_count);
    EXPECT_EQ(3U, finalizations);
    EXPECT_EQ(UINT32_C(0x4242), words[17]);
    EXPECT_EQ(UINT32_C(0x4242), words[36]);
    EXPECT_EQ(16U, HASH->STR & HASH_STR_NBLW);
}
TEST_F(Hash, PrivateAliasAndMetadataQueryNeverOverwriteBorrowedObjects) {
    Start();
    operation.input = &state;
    ASSERT_EQ(NX_SUCCESS, nx_hash_prepare(&request, &operation, 100U));
    unsigned before = writes;
    EXPECT_EQ(NX_ERROR_INVALID, nx_hash_port_submit(&face, &request));
    EXPECT_EQ(before, writes);
    operation.input = input.data();
    ASSERT_EQ(NX_SUCCESS, nx_hash_prepare(&request, &operation, 100U));
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_submit(&face, &request));
    auto snapshot = request;
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_hash_port_query(&face, NX_HASH_SHA1,
                           reinterpret_cast<nx_hash_constraints_t*>(&request)));
    EXPECT_EQ(0, std::memcmp(&snapshot, &request, sizeof(request)));
}
TEST_F(Hash, LateSharedIrqAfterStopDoesNotAccessRetiredStorage) {
    Submit();
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_stop(&face));
    state.config = reinterpret_cast<const nx_stm32f4_hash_config_t*>(1U);
    state.active = reinterpret_cast<nx_hash_request_t*>(1U);
    unsigned before = writes;
    Irq(HASH_SR_DINIS | HASH_SR_DCIS);
    EXPECT_EQ(before, writes);
}
TEST_F(Hash, CancelAfterObservedSettlementIsAnIdempotentNoOp) {
    Submit();
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_cancel(&face, &request));
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    unsigned before = writes;
    EXPECT_EQ(NX_SUCCESS, nx_hash_port_cancel(&face, &request));
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
    if (reg == &HASH->IMR && value == 0U && refuse_source)
        return;
    if (reg == &RCC->AHB2RSTR && (value & RCC_AHB2RSTR_HASHRST) != 0U) {
        if (refuse_reset)
            return;
        HardwareReset();
    }
    if (reg == &HASH->DIN) {
        ASSERT_LT(word_count, words.size());
        words[word_count++] = value;
    }
    if (reg == &HASH->CR && (value & HASH_CR_INIT) != 0U) {
        HASH->SR = HASH_SR_DINIS;
        value &= ~static_cast<uint32_t>(HASH_CR_INIT);
    }
    if (reg == &HASH->STR && (value & HASH_STR_DCAL) != 0U) {
        ++finalizations;
        HASH->SR = HASH_SR_BUSY;
        value &= ~static_cast<uint32_t>(HASH_STR_DCAL);
    }
    *reg = value;
}
extern "C" void nx_crypto_model_cryp_key(volatile uint32_t*, uint32_t) {
}
extern "C" void nx_crypto_model_cryp_input(void*, uint32_t) {
}
extern "C" uint32_t nx_crypto_model_cryp_output(void*) {
    return 0U;
}
extern "C" void nx_crypto_model_hash_input(uint32_t word) {
    nx_crypto_model_write(&HASH->DIN, word);
}
extern "C" uint32_t nx_crypto_model_hash_digest(unsigned index) {
    return HASH->HR[index];
}
