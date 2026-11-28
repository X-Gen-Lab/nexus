/**
 * \file            crc_uid_test.cpp
 * \brief           F103 fixed CRC, retained gate and standalone silicon UID.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/crc/instance.h"
#include "nexus/io/chip.h"
#include "system/platform.h"
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>

namespace {
constexpr uintptr_t bus_base = UINT32_C(0x40021000);
constexpr uintptr_t uid_page = UINT32_C(0x1FFFF000);
constexpr size_t bus_bytes = 0x3000U;
constexpr size_t page_bytes = 0x1000U;
constexpr uint32_t marker = 0x01234567U;
bool system_live;
bool block_gate;
bool block_reset;
unsigned resets;
unsigned fed;
nx_time_us_t now;
nx_time_us_t expire_when_fed;

void* map_at(uintptr_t address, size_t bytes) {
    return mmap(reinterpret_cast<void*>(address), bytes, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
}

class F1CrcUid : public testing::Test {
  protected:
    void* bus = MAP_FAILED;
    void* uid = MAP_FAILED;
    nx_stm32f1_crc_t crc{};
    nx_crc_port_t face{&nx_stm32f1_crc_ops, &crc};
    uint32_t output = marker;

    void SetUp() override {
        bus = map_at(bus_base, bus_bytes);
        ASSERT_EQ(bus, reinterpret_cast<void*>(bus_base));
        uid = map_at(uid_page, page_bytes);
        ASSERT_EQ(uid, reinterpret_cast<void*>(uid_page));
        system_live = true;
        block_gate = block_reset = false;
        resets = fed = 0U;
        now = 10U;
        expire_when_fed = NX_DEADLINE_NEVER;
        CRC->IDR = 0x5AU;
    }

    void TearDown() override {
        block_gate = block_reset = false;
        if (bus != MAP_FAILED) {
            EXPECT_EQ(nx_stm32f1_crc_close(&crc), NX_SUCCESS);
            EXPECT_EQ(munmap(bus, bus_bytes), 0);
        }
        if (uid != MAP_FAILED) {
            EXPECT_EQ(munmap(uid, page_bytes), 0);
        }
    }
};

TEST_F(F1CrcUid, CrcUsesWordOrderAndResetsWithoutTouchingIdr) {
    ASSERT_EQ(nx_stm32f1_crc_prepare(&crc), NX_SUCCESS);
    const uint32_t words[] = {0x12345678U, 0x90ABCDEFU};
    ASSERT_EQ(nx_crc_port_compute(&face, words, 1U, 100U, &output), NX_SUCCESS);
    EXPECT_EQ(output, 0xDF8A8A2BU);
    ASSERT_EQ(nx_crc_port_compute(&face, words, 2U, 100U, &output), NX_SUCCESS);
    EXPECT_EQ(output, 0x36B13E20U);
    ASSERT_EQ(nx_crc_port_compute(&face, nullptr, 0U, 100U, &output),
              NX_SUCCESS);
    EXPECT_EQ(output, UINT32_MAX);
    EXPECT_EQ(CRC->IDR, 0x5AU);
    EXPECT_EQ(resets, 3U);
    EXPECT_EQ(fed, 3U);
}

TEST_F(F1CrcUid, CrcTimeoutAndResetFaultKeepOutputAndPermitRetry) {
    const uint32_t word = 0x12345678U;
    ASSERT_EQ(nx_stm32f1_crc_prepare(&crc), NX_SUCCESS);
    EXPECT_EQ(nx_crc_port_compute(&face, &word, 1U, now, &output),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(resets, 0U);
    expire_when_fed = 50U;
    EXPECT_EQ(nx_crc_port_compute(&face, &word, 1U, 50U, &output),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(output, marker);
    CRC->DR = 0U;
    block_reset = true;
    EXPECT_EQ(nx_crc_port_compute(&face, &word, 1U, 100U, &output),
              NX_ERROR_IO);
    EXPECT_EQ(output, marker);
    block_reset = false;
    expire_when_fed = NX_DEADLINE_NEVER;
    EXPECT_EQ(nx_crc_port_compute(&face, &word, 1U, 100U, &output), NX_SUCCESS);
    EXPECT_EQ(output, 0xDF8A8A2BU);
}

TEST_F(F1CrcUid, CrcOwnsOnlyItsAcquiredGateAndRetainsFailedRelease) {
    RCC->AHBENR = RCC_AHBENR_DMA1EN | RCC_AHBENR_CRCEN;
    ASSERT_EQ(nx_stm32f1_crc_prepare(&crc), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_crc_close(&crc), NX_SUCCESS);
    EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA1EN | RCC_AHBENR_CRCEN);
    RCC->AHBENR = RCC_AHBENR_DMA1EN;
    ASSERT_EQ(nx_stm32f1_crc_prepare(&crc), NX_SUCCESS);
    block_gate = true;
    EXPECT_EQ(nx_stm32f1_crc_close(&crc), NX_ERROR_IO);
    EXPECT_EQ(nx_crc_port_compute(&face, nullptr, 0U, 100U, &output),
              NX_ERROR_STATE);
    EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA1EN | RCC_AHBENR_CRCEN);
    block_gate = false;
    ASSERT_EQ(nx_stm32f1_crc_close(&crc), NX_SUCCESS);
    EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA1EN);
}

TEST_F(F1CrcUid, UidQueryIsPureAndCopiesExactLittleEndianWords) {
    auto* words = reinterpret_cast<uint32_t*>(UID_BASE);
    words[0] = 0x01234567U;
    words[1] = 0x89ABCDEFU;
    words[2] = 0xFF008040U;
    ASSERT_EQ(mprotect(uid, page_bytes, PROT_NONE), 0);
    size_t bytes = 0U;
    ASSERT_EQ(nx_chip_uid_read(nullptr, 0U, &bytes), NX_SUCCESS);
    ASSERT_EQ(bytes, 12U);
    ASSERT_EQ(mprotect(uid, page_bytes, PROT_READ), 0);
    uint8_t output_bytes[12]{};
    EXPECT_EQ(nx_chip_uid_read(output_bytes, sizeof(output_bytes), &bytes),
              NX_SUCCESS);
    const uint8_t expected[] = {0x67U, 0x45U, 0x23U, 0x01U, 0xEFU, 0xCDU,
                                0xABU, 0x89U, 0x40U, 0x80U, 0x00U, 0xFFU};
    EXPECT_EQ(std::memcmp(output_bytes, expected, sizeof(expected)), 0);
}

TEST_F(F1CrcUid, UidRejectsAliasAndDiagnosticDoesNotReadDebugOnlyRegisters) {
    uint8_t short_buffer[11] = {0xA5U};
    size_t bytes = 62U;
    EXPECT_EQ(nx_chip_uid_read(short_buffer, sizeof(short_buffer), &bytes),
              NX_ERROR_OVERFLOW);
    EXPECT_EQ(short_buffer[0], 0xA5U);
    EXPECT_EQ(bytes, 62U);
    EXPECT_EQ(nx_chip_uid_read(reinterpret_cast<void*>(UID_BASE), 12U, &bytes),
              NX_ERROR_INVALID);
    nx_chip_info_t info{{1U, 2U, 3U}, 4U, 5U, 6U};
    EXPECT_EQ(nx_chip_info_read(&info), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(info.device_id, 4U);
    EXPECT_EQ(info.revision_id, 5U);
    EXPECT_EQ(info.flash_size_kib, 6U);
}
} /* namespace */

extern "C" {
bool nx_stm32f1_system_live(void) {
    return system_live;
}
nx_time_us_t nx_time_now_us(void) {
    return now;
}
void nx_f1_crc_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &CRC->CR) {
        EXPECT_EQ(value, CRC_CR_RESET);
        ++resets;
        if (!block_reset) {
            CRC->DR = UINT32_MAX;
        }
        *reg = 0U;
    } else if (reg != &RCC->AHBENR || !block_gate) {
        *reg = value;
    }
}
void nx_f1_crc_model_feed(CRC_TypeDef* reg, uint32_t value) {
    EXPECT_EQ(reg, CRC);
    EXPECT_NE(RCC->AHBENR & RCC_AHBENR_CRCEN, 0U);
    uint32_t result = reg->DR ^ value;
    for (unsigned bit = 0U; bit < 32U; ++bit) {
        result =
            (result << 1U) ^ ((result & 0x80000000U) != 0U ? 0x04C11DB7U : 0U);
    }
    reg->DR = result;
    ++fed;
    if (expire_when_fed != NX_DEADLINE_NEVER) {
        now = expire_when_fed;
    }
}
}
