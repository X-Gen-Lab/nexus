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
void nx_hal_test_dsb_hook(void (*hook)(void));
}
#include <array>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
extern "C" {
nx_stm32_system_t g_nx_stm32_system;
uint32_t SystemCoreClock = NEXUS_CORE_HZ;
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
#if NEXUS_STM32_CRYPTO_DMA
bool hold_dma_status;
#endif
void HardwareReset() {
    std::memset(HASH, 0, sizeof(*HASH));
    HASH->SR = HASH_SR_DINIS;
#if NEXUS_STM32_CRYPTO_DMA
    if (hold_dma_status)
        HASH->SR |= HASH_SR_DMAS;
#endif
}
class Hash : public testing::Test {
  protected:
    nx_stm32f4_hash_state_t state{};
    nx_stm32f4_hash_config_t config{{NX_IRQ_KERNEL_NONE, 82U, 4U, 0U}, nullptr};
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
#if !NEXUS_STM32_CRYPTO_DMA
TEST_F(Hash, ExactLimitsPermitEmptyAndUnalignedByteSpansWithoutPadding) {
    Start();
    nx_hash_constraints_t limits{};
    ASSERT_EQ(NX_SUCCESS,
              nx_hash_port_query(&face, NX_HASH_HMAC_SHA1, &limits));
    EXPECT_EQ(1U, limits.input_alignment);
    EXPECT_EQ(1U, limits.key_alignment);
    EXPECT_EQ(1U, limits.digest_alignment);
    EXPECT_EQ(1U, limits.input_length_multiple);
    EXPECT_EQ(1U, limits.key_length_multiple);
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
#endif
#if NEXUS_STM32_CRYPTO_DMA
bool refuse_dma_stop;
bool check_poll_mask;
bool advance_epoch_on_poll;
unsigned poll_checks;
unsigned settle_checks;
void FlushDmaFlags() {
    if (advance_epoch_on_poll && DMA2->HIFCR != 0U) {
        advance_epoch_on_poll = false;
        ++g_nx_stm32_system.time_epoch;
    }
    if (check_poll_mask && DMA2->HIFCR != 0U) {
        ++poll_checks;
        EXPECT_FALSE(nx_arch_irq_is_masked());
    }
    DMA2->HISR &= ~DMA2->HIFCR;
    DMA2->HIFCR = 0U;
}
class HashDma : public Hash {
  protected:
    nx_stm32f4_hash_dma_state_t dma_state{};
    const nx_dma_memory_region_t region{0x20000000U, 4096U, NX_DMA_MEMORY_READ};
    const nx_stm32f4_hash_dma_config_t dma{&dma_state, &region, 1U};
    void* dma_map = MAP_FAILED;
    void* data_map = MAP_FAILED;
    void SetUp() override {
        hold_dma_status = false;
        Hash::SetUp();
        dma_map = mmap(
            reinterpret_cast<void*>(0x40026000U), 4096U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, dma_map);
        data_map = mmap(
            reinterpret_cast<void*>(0x20000000U), 4096U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, data_map);
        nx_hal_test_dsb_hook(FlushDmaFlags);
        refuse_dma_stop = false;
        check_poll_mask = false;
        advance_epoch_on_poll = false;
        poll_checks = settle_checks = 0U;
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
        DMA2_Stream7->FCR = 0x21U;
        config.dma = &dma;
        std::memcpy(data_map, input.data(), input.size());
        std::memcpy(static_cast<uint8_t*>(data_map) + 1024U, key.data(),
                    key.size());
        operation.input = data_map;
        operation.bytes = 64U;
    }
    void TearDown() override {
        hold_dma_status = false;
        refuse_dma_stop = refuse_reset = refuse_source = false;
        if (state.active != nullptr) {
            (void)nx_hash_port_cancel(&face, &request);
            (void)nx_hash_port_service(&face);
        }
        if (state.config != nullptr) {
            EXPECT_EQ(NX_SUCCESS, nx_hash_port_stop(&face));
        }
        if (state.active != nullptr) {
            ADD_FAILURE() << "Cannot reclaim a retained hash DMA fixture";
            std::abort();
        }
        if (data_map != MAP_FAILED) {
            EXPECT_EQ(0, munmap(data_map, 4096U));
        }
        if (dma_map != MAP_FAILED) {
            nx_hal_test_dsb_hook(nullptr);
            EXPECT_EQ(0, munmap(dma_map, 4096U));
        }
        Hash::TearDown();
    }
    void InputComplete() {
        DMA2_Stream7->NDTR = 0U;
        DMA2_Stream7->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        DMA2->HISR |= DMA_HISR_TCIF7;
        ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    }
};
TEST_F(HashDma, QueryMakesWholeWordAndNonemptyDmaConstraintsExplicit) {
    Start();
    nx_hash_constraints_t limits{};
    ASSERT_EQ(NX_SUCCESS,
              nx_hash_port_query(&face, NX_HASH_HMAC_SHA1, &limits));
    EXPECT_EQ(4U, limits.input_alignment);
    EXPECT_EQ(4U, limits.key_alignment);
    EXPECT_EQ(1U, limits.digest_alignment);
    EXPECT_EQ(4U, limits.input_length_multiple);
    EXPECT_EQ(4U, limits.key_length_multiple);
    EXPECT_FALSE(limits.empty_message);
    EXPECT_FALSE(limits.empty_key);
    EXPECT_EQ(262140U, limits.maximum_bytes);
}
TEST_F(HashDma, InputCompletionDoesNotPublishAnUnavailableDigest) {
    Submit();
    EXPECT_EQ(16U, DMA2_Stream7->NDTR);
    EXPECT_NE(0U, DMA2_Stream7->CR & DMA_SxCR_EN);
    EXPECT_EQ(DMA_CHANNEL_2, DMA2_Stream7->CR & DMA_SxCR_CHSEL);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(operation.input), DMA2_Stream7->M0AR);
    EXPECT_EQ(0U, HASH->IMR & HASH_IMR_DINIE);
    InputComplete();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0xa5U, digest[0]);
    HASH->SR = HASH_SR_DCIS | HASH_SR_BUSY;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    HASH->SR = HASH_SR_DCIS | HASH_SR_DMAS;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0xa5U, digest[0]);
    HASH->SR = HASH_SR_DCIS;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(20U, request.base.transferred);
    EXPECT_EQ(0U, DMA2_Stream7->M0AR);
    EXPECT_EQ(nullptr, dma_state.input.handle.XferCpltCallback);
    EXPECT_EQ(nullptr, dma_state.handle.Init.pKey);
}
TEST_F(HashDma, HmacUsesTheOfficialKeyMessageKeyTransitions) {
    operation.algorithm = NX_HASH_HMAC_MD5;
    operation.key = static_cast<uint8_t*>(data_map) + 1024U;
    operation.key_bytes = 128U;
    Submit();
    EXPECT_EQ(reinterpret_cast<uintptr_t>(operation.key), DMA2_Stream7->M0AR);
    EXPECT_EQ(32U, DMA2_Stream7->NDTR);
    InputComplete();
    EXPECT_EQ(reinterpret_cast<uintptr_t>(operation.input), DMA2_Stream7->M0AR);
    EXPECT_EQ(16U, DMA2_Stream7->NDTR);
    InputComplete();
    EXPECT_EQ(reinterpret_cast<uintptr_t>(operation.key), DMA2_Stream7->M0AR);
    EXPECT_EQ(32U, DMA2_Stream7->NDTR);
    InputComplete();
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    HASH->SR = HASH_SR_DCIS;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(16U, request.base.transferred);
    EXPECT_EQ(nullptr, dma_state.handle.pHashMsgBuffPtr);
    EXPECT_EQ(nullptr, dma_state.handle.pHashKeyBuffPtr);
}
TEST_F(HashDma, NonwordTailRejectsBeforeAnyDmaBorrow) {
    Start();
    operation.bytes = 67U;
    ASSERT_EQ(NX_SUCCESS, nx_hash_prepare(&request, &operation, 100U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_hash_port_submit(&face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(0U, DMA2_Stream7->NDTR);
}
TEST_F(HashDma, CpuOnlySourceRejectsBeforeAnyDmaEffects) {
    Start();
    operation.input = reinterpret_cast<void*>(0x10000000U);
    ASSERT_EQ(NX_SUCCESS, nx_hash_prepare(&request, &operation, 100U));
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_hash_port_submit(&face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, state.active);
    EXPECT_EQ(0U, DMA2_Stream7->NDTR);
}
TEST_F(HashDma, EmptyAndNonwordHmacKeysRejectWithoutEngineEffects) {
    Start();
    operation.algorithm = NX_HASH_HMAC_SHA1;
    operation.key = static_cast<uint8_t*>(data_map) + 1024U;
    for (size_t count : {0U, 3U}) {
        operation.key_bytes = count;
        ASSERT_EQ(NX_SUCCESS, nx_hash_prepare(&request, &operation, 100U));
        EXPECT_EQ(NX_ERROR_INVALID, nx_hash_port_submit(&face, &request));
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
        EXPECT_EQ(0U, DMA2_Stream7->NDTR);
    }
}
TEST_F(HashDma, FailedJoinRetainsHmacKeyAndCallerStorage) {
    operation.algorithm = NX_HASH_HMAC_SHA1;
    operation.key = static_cast<uint8_t*>(data_map) + 1024U;
    operation.key_bytes = 64U;
    Submit();
    refuse_dma_stop = true;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_cancel(&face, &request));
    EXPECT_EQ(NX_ERROR_IO, nx_hash_port_service(&face));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_EQ(operation.key, dma_state.handle.Init.pKey);
    EXPECT_NE(0U, DMA2_Stream7->M0AR);
    refuse_dma_stop = false;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(nullptr, dma_state.handle.Init.pKey);
    EXPECT_EQ(0U, DMA2_Stream7->M0AR);
    EXPECT_EQ(0x42U, static_cast<const uint8_t*>(operation.key)[0]);
}
TEST_F(HashDma, DmaErrorCannotPublishDigestOrLeaveTheNextHmacPhaseQueued) {
    operation.algorithm = NX_HASH_HMAC_MD5;
    operation.key = static_cast<uint8_t*>(data_map) + 1024U;
    operation.key_bytes = 64U;
    Submit();
    DMA2->HISR |= DMA_HISR_TEIF7;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0xa5U, digest[0]);
    EXPECT_EQ(0U, DMA2_Stream7->M0AR);
    EXPECT_EQ(nullptr, dma_state.input.handle.XferCpltCallback);
    EXPECT_EQ(nullptr, dma_state.handle.pHashMsgBuffPtr);
}
TEST_F(HashDma, DigestIrqOnlyLatchesAWakeAndNeverAdvancesDmaCallbacks) {
    Submit();
    const uint32_t address = DMA2_Stream7->M0AR;
    Irq(HASH_SR_DCIS);
    EXPECT_EQ(address, DMA2_Stream7->M0AR);
    EXPECT_NE(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(0U, HASH->IMR & HASH_IMR_DCIE);
    EXPECT_EQ(0xa5U, digest[0]);
}
TEST_F(HashDma, QueryCannotOverwriteTheOwnedDmaRegisters) {
    Start();
    const uint32_t before = DMA2_Stream7->CR;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_hash_port_query(
                  &face, NX_HASH_SHA1,
                  reinterpret_cast<nx_hash_constraints_t*>(DMA2_Stream7)));
    EXPECT_EQ(before, DMA2_Stream7->CR);
}
TEST_F(HashDma, AllFourAlgorithmsReuseTheOwnerAndByteAlignedDigestContract) {
    Start();
    operation.digest = digest.data() + 1U;
    operation.digest_capacity = digest.size() - 1U;
    for (unsigned algorithm = 0U; algorithm <= NX_HASH_HMAC_MD5; ++algorithm) {
        SCOPED_TRACE(algorithm);
        operation.algorithm = static_cast<nx_hash_algorithm_t>(algorithm);
        bool keyed = algorithm >= NX_HASH_HMAC_SHA1;
        operation.key =
            keyed ? static_cast<uint8_t*>(data_map) + 1024U : nullptr;
        operation.key_bytes = keyed ? 64U : 0U;
        digest.fill(0xa5U);
        ASSERT_EQ(NX_SUCCESS, nx_hash_prepare(&request, &operation, 100U));
        ASSERT_EQ(NX_SUCCESS, nx_hash_port_submit(&face, &request));
        for (unsigned phase = 0U; phase < (keyed ? 3U : 1U); ++phase)
            InputComplete();
        HASH->HR[0] = 0x01234567U;
        HASH->SR = HASH_SR_DCIS;
        ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
        nx_result_t result;
        size_t count;
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_result(&request.base, &result, &count));
        EXPECT_EQ(NX_SUCCESS, result);
        EXPECT_EQ(algorithm == NX_HASH_SHA1 || algorithm == NX_HASH_HMAC_SHA1
                      ? 20U
                      : 16U,
                  count);
        EXPECT_EQ(0xa5U, digest[0]);
        EXPECT_EQ(0x01U, digest[1]);
        EXPECT_EQ(0x23U, digest[2]);
        EXPECT_EQ(0x45U, digest[3]);
        EXPECT_EQ(0x67U, digest[4]);
        EXPECT_EQ(0xa5U, digest[count + 1U]);
    }
}
TEST_F(HashDma, DmaPollingIsUnmaskedAndFinalPublicationRemainsProtected) {
    Submit();
    DMA2_Stream7->NDTR = 0U;
    DMA2_Stream7->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    DMA2->HISR |= DMA_HISR_TCIF7;
    HASH->SR = HASH_SR_DCIS;
    check_poll_mask = true;
    nx_result_t result = nx_hash_port_service(&face);
    check_poll_mask = false;
    EXPECT_EQ(NX_SUCCESS, result);
    EXPECT_GT(poll_checks, 0U);
    EXPECT_EQ(1U, settle_checks);
}
TEST_F(HashDma, RefusedInterfaceResetRetainsTheOriginalBorrow) {
    Submit();
    hold_dma_status = true;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_cancel(&face, &request));
    EXPECT_EQ(NX_ERROR_IO, nx_hash_port_service(&face));
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, state.active);
    EXPECT_EQ(0U, DMA2_Stream7->M0AR);
    hold_dma_status = false;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}
TEST_F(HashDma, ChangedClockEpochDuringPollCannotCopyOrPublishDigest) {
    Submit();
    DMA2_Stream7->NDTR = 0U;
    DMA2_Stream7->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
    DMA2->HISR |= DMA_HISR_TCIF7;
    HASH->SR = HASH_SR_DCIS;
    advance_epoch_on_poll = true;
    ASSERT_EQ(NX_SUCCESS, nx_hash_port_service(&face));
    EXPECT_FALSE(advance_epoch_on_poll);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0xa5U, digest[0]);
    EXPECT_EQ(0U, DMA2_Stream7->M0AR);
    EXPECT_EQ(nullptr, dma_state.input.handle.Parent);
    EXPECT_EQ(nullptr, dma_state.handle.pHashMsgBuffPtr);
}
#endif
} /* namespace */
#if NEXUS_STM32_CRYPTO_DMA
extern "C" void __real_nx_request_settle(nx_request_t*, nx_result_t, size_t);
extern "C" void __wrap_nx_request_settle(nx_request_t* request,
                                         nx_result_t result, size_t count) {
    ++settle_checks;
    EXPECT_TRUE(nx_arch_irq_is_masked());
    __real_nx_request_settle(request, result, count);
}
#endif
extern "C" nx_time_us_t nx_time_now_us(void) {
    return now;
}
extern "C" uint32_t HAL_GetTick(void) {
    return tick_reads++;
}
extern "C" void nx_crypto_model_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
#if NEXUS_STM32_CRYPTO_DMA
    if (reg == &DMA2->HIFCR) {
        DMA2->HISR &= ~value;
        *reg = 0U;
        return;
    }
    if (refuse_dma_stop && reg == &DMA2_Stream7->CR &&
        (*reg & DMA_SxCR_EN) != 0U)
        value |= DMA_SxCR_EN;
#endif
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
extern "C" void nx_crypto_model_irq_clear(IRQn_Type irq) {
    unsigned number = static_cast<unsigned>(irq);
    NVIC->ISPR[number / 32U] &= ~(1U << (number % 32U));
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
