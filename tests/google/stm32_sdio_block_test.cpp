/**
 * \file            stm32_sdio_block_test.cpp
 * \brief           Real polling SDIO, card protocol and retained cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "stm32f407_sdio.h"
nx_stm32_system_t g_nx_stm32_system;
NVIC_Type g_f4_model_nvic;
SCB_Type g_f4_model_scb;
SysTick_Type g_f4_model_systick;
uint32_t g_f4_model_mask;
bool g_f4_model_isr;
uint32_t SystemCoreClock = 168000000U;
}
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

namespace {
bool privileged;
nx_time_us_t now;
unsigned time_calls;
unsigned writes;
volatile uint32_t* refused_write;
std::vector<uint32_t> commands;
std::vector<uint32_t> arguments;
bool card_enabled;
bool card_sdsc;
bool legacy_illegal;
bool card_selected;
nx_time_us_t expire_after_deselect;
bool control_seen;
bool control_spacing_failed;
nx_time_us_t last_control_write;
bool clock_advances;
bool silence_commands;
bool r3_crc;
bool card_four_bit;
bool bad_csd;
bool write_busy;
bool host_stuck;
bool dataend_early;
unsigned terminal_tail;
int fifo_delta;
bool missing_crc_end;
bool card_read_only;
unsigned card_recovery_state;
unsigned fail_command;
uint32_t command_error;
const nx_block_port_t* reentrant_port;
nx_result_t reentrant_result;
alignas(4) std::array<uint8_t, 512> reentrant_data{};
uint32_t csd_size;
unsigned read_number;
unsigned fail_read_number;
unsigned fifo_words;
unsigned fifo_limit;
bool fifo_reading;
bool fifo_writing;
std::vector<uint32_t> written_words;

uint32_t WordAt(const uint8_t* data, size_t index) {
    uint32_t word;
    std::memcpy(&word, data + index * sizeof(word), sizeof(word));
    return word;
}

void SetResponse(const std::array<uint8_t, 16>& bytes) {
    volatile uint32_t* response = const_cast<volatile uint32_t*>(&SDIO->RESP1);
    for (size_t i = 0U; i < 4U; ++i)
        response[i] = (static_cast<uint32_t>(bytes[i * 4U]) << 24U) |
                      (static_cast<uint32_t>(bytes[i * 4U + 1U]) << 16U) |
                      (static_cast<uint32_t>(bytes[i * 4U + 2U]) << 8U) |
                      static_cast<uint32_t>(bytes[i * 4U + 3U]);
}

void DataStarted(unsigned count, bool reading) {
    fifo_words = 0U;
    fifo_limit = count;
    fifo_reading = reading;
    fifo_writing = !reading;
    *const_cast<volatile uint32_t*>(&SDIO->DCOUNT) = count * 4U;
    *const_cast<volatile uint32_t*>(&SDIO->STA) |=
        reading ? SDIO_STA_RXACT | SDIO_STA_RXDAVL
                : SDIO_STA_TXACT | SDIO_STA_TXFIFOHE;
}

void CardCommand(uint32_t cmd) {
    if (!card_enabled || silence_commands)
        return;
    *const_cast<volatile uint32_t*>(&SDIO->RESPCMD) = cmd;
    *const_cast<volatile uint32_t*>(&SDIO->STA) |=
        (cmd == 0U || (cmd == 7U && SDIO->ARG == 0U))
            ? static_cast<uint32_t>(SDIO_STA_CMDSENT)
            : static_cast<uint32_t>(SDIO_STA_CMDREND);
    *const_cast<volatile uint32_t*>(&SDIO->RESP1) = 0U;
    if (cmd == 7U) {
        card_selected = SDIO->ARG != 0U;
        if (!card_selected && expire_after_deselect != 0U)
            now = expire_after_deselect;
    }
    if (cmd == fail_command && fail_command != 0U) {
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) = command_error;
    } else if (cmd == 8U) {
        if (card_sdsc && legacy_illegal) {
            *const_cast<volatile uint32_t*>(&SDIO->RESP1) = 1U << 22U;
        } else if (card_sdsc) {
            *const_cast<volatile uint32_t*>(&SDIO->STA) &=
                ~static_cast<uint32_t>(SDIO_STA_CMDREND);
            *const_cast<volatile uint32_t*>(&SDIO->STA) |= SDIO_STA_CTIMEOUT;
        } else {
            *const_cast<volatile uint32_t*>(&SDIO->RESP1) = 0x1AAU;
        }
    } else if (cmd == 41U) {
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) =
            UINT32_C(0x80FF8000) | (card_sdsc ? 0U : UINT32_C(0x40000000));
        if (r3_crc) {
            *const_cast<volatile uint32_t*>(&SDIO->STA) &=
                ~static_cast<uint32_t>(SDIO_STA_CMDREND);
            *const_cast<volatile uint32_t*>(&SDIO->STA) |= SDIO_STA_CCRCFAIL;
        }
    } else if (cmd == 3U) {
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) = UINT32_C(0x12340000);
    } else if (cmd == 9U) {
        std::array<uint8_t, 16> bytes{};
        bytes[0] = bad_csd ? 0xC0U : (card_sdsc ? 0U : 0x40U);
        if (card_sdsc) {
            bytes[5] = 9U;
            bytes[6] = 3U;
            bytes[7] = 255U;
            bytes[8] = 0xC0U;
            bytes[9] = 3U;
            bytes[10] = 0x80U;
        } else {
            bytes[7] = static_cast<uint8_t>((csd_size >> 16U) & 63U);
            bytes[8] = static_cast<uint8_t>(csd_size >> 8U);
            bytes[9] = static_cast<uint8_t>(csd_size);
        }
        if (card_read_only)
            bytes[14] = 0x20U;
        SetResponse(bytes);
    } else if (cmd == 55U) {
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) = 1U << 5U;
    } else if (cmd == 13U) {
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) =
            card_recovery_state != 0U ? card_recovery_state << 9U
            : write_busy              ? (7U << 9U)
                         : (((card_selected ? 4U : 3U) << 9U) | (1U << 8U));
    } else if (cmd == 12U) {
        card_recovery_state = 0U;
    } else if (cmd == 17U) {
        ++read_number;
        DataStarted(static_cast<unsigned>(128 + fifo_delta), true);
    } else if (cmd == 24U) {
        if ((SDIO->DCTRL & SDIO_DCTRL_DTEN) != 0U)
            DataStarted(128U, false);
    } else if (cmd == 51U) {
        DataStarted(2U, true);
    }
}

class SdioBlock : public testing::Test {
  protected:
    void* mmio = MAP_FAILED;
    nx_stm32_sdio_state_t owner{};
    nx_stm32_sdio_receipt_t receipt{};
    uint16_t snapshots[6]{};
    std::array<nx_stm32_platform_pin_config_t, 6> pins{};
    nx_stm32_sdio_config_t config{};
    nx_block_port_t face{&nx_stm32_sdio_ops, &owner};
    void SetUp() override {
        mmio = mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mmio, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        g_f4_model_nvic = {};
        g_f4_model_scb = {};
        g_f4_model_systick = {};
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        g_nx_stm32_system = {};
        g_nx_stm32_system.rcc = RCC;
        privileged = true;
        now = 1000U;
        time_calls = writes = 0U;
        refused_write = nullptr;
        commands.clear();
        arguments.clear();
        written_words.clear();
        card_enabled = false;
        card_sdsc = card_selected = false;
        expire_after_deselect = 0U;
        legacy_illegal = control_seen = control_spacing_failed = false;
        last_control_write = 0U;
        clock_advances = true;
        silence_commands = r3_crc = bad_csd = write_busy = host_stuck = false;
        card_four_bit = true;
        dataend_early = missing_crc_end = card_read_only = false;
        terminal_tail = 0U;
        fifo_delta = 0;
        card_recovery_state = fail_command = 0U;
        command_error = 0U;
        reentrant_port = nullptr;
        reentrant_result = NX_SUCCESS;
        csd_size = 1023U;
        read_number = fail_read_number = fifo_words = fifo_limit = 0U;
        fifo_reading = fifo_writing = false;
        RCC->CR = RCC_CR_HSERDY | RCC_CR_PLLRDY;
        RCC->CFGR =
            RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
        RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE | 8U | (336U << 6U) | (7U << 24U);
        RCC->AHB1ENR = RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN;
        RCC->APB2ENR = RCC_APB2ENR_USART6EN;
        GPIOC->MODER = 0x55U;
        GPIOD->MODER = 0x01U;
        for (size_t i = 0U; i < 5U; ++i)
            pins[i] = {GPIOC, &snapshots[i], 2U, static_cast<uint8_t>(8U + i)};
        pins[5] = {GPIOD, &snapshots[5], 3U, 2U};
        config.registers = SDIO;
        config.pins = pins.data();
        config.max_blocks = 32U;
        config.poll_budget = 50000U;
        config.stop_timeout_us = 10000U;
        config.bus_width = 4U;
    }
    void TearDown() override {
        privileged = true;
        refused_write = nullptr;
        g_f4_model_mask = 0U;
        g_f4_model_isr = false;
        if (mmio != MAP_FAILED) {
            silence_commands = write_busy = host_stuck = false;
            expire_after_deselect = 0U;
            clock_advances = true;
            *const_cast<volatile uint32_t*>(&SDIO->STA) &=
                ~static_cast<uint32_t>(SDIO_STA_RXACT | SDIO_STA_TXACT |
                                       SDIO_STA_CMDACT);
            g_f4_model_nvic = {};
            if (receipt.owned || receipt.pin_mask != 0U ||
                receipt.clock_added != 0U) {
                EXPECT_EQ(NX_SUCCESS,
                          nx_stm32_sdio_close(&owner, &config, &receipt));
            }
            EXPECT_EQ(0, munmap(mmio, 0x80000U));
        }
    }
    void Activate() {
        Prepare();
        card_enabled = true;
        ASSERT_EQ(NX_SUCCESS, nx_block_port_activate(&face, 1000000U));
    }
    void Prepare() {
        ASSERT_EQ(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    }
};
} /* namespace */

TEST_F(SdioBlock, GeometryUnavailableUntilExplicitCardActivation) {
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(NX_BLOCK_UNAVAILABLE, nx_block_port_state(&face));
    EXPECT_EQ(0U, writes);
    Prepare();
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(NX_BLOCK_STOPPED, nx_block_port_state(&face));
    EXPECT_TRUE(commands.empty());
    EXPECT_EQ(0U, SDIO->MASK);
    EXPECT_EQ(0U, SDIO->DCTRL & SDIO_DCTRL_DMAEN);
    EXPECT_EQ(0U, time_calls);
}

TEST_F(SdioBlock,
       ColdPadAndGateOwnershipRestoresWithoutTouchingOtherPeripherals) {
    const uint32_t before_c = GPIOC->MODER;
    const uint32_t before_d = GPIOD->MODER;
    Prepare();
    EXPECT_EQ(RCC_APB2ENR_USART6EN | RCC_APB2ENR_SDIOEN, RCC->APB2ENR);
    for (const auto& pin : pins) {
        EXPECT_EQ(2U, (pin.registers->MODER >> (2U * pin.pin)) & 3U);
        EXPECT_EQ(12U,
                  (pin.registers->AFR[pin.pin / 8U] >> (4U * (pin.pin % 8U))) &
                      15U);
    }
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(RCC_APB2ENR_USART6EN, RCC->APB2ENR);
    EXPECT_EQ(before_c, GPIOC->MODER);
    EXPECT_EQ(before_d, GPIOD->MODER);
    EXPECT_FALSE(receipt.owned);
    EXPECT_EQ(0U, receipt.pin_mask);
    EXPECT_EQ(0U, receipt.clock_added);
}

TEST_F(SdioBlock, WrongPllOrMissingGpioGateRejectsBeforeAnyWriteOrClockCall) {
    const uint32_t pll = RCC->PLLCFGR;
    RCC->PLLCFGR =
        (pll & ~static_cast<uint32_t>(RCC_PLLCFGR_PLLQ)) | (6U << 24U);
    EXPECT_NE(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, time_calls);
    EXPECT_FALSE(receipt.owned);
    RCC->PLLCFGR = pll;
    RCC->AHB1ENR &= ~static_cast<uint32_t>(RCC_AHB1ENR_GPIODEN);
    EXPECT_NE(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, time_calls);
}

TEST_F(SdioBlock, ForeignLiveControllerOrPublisherCannotBeTakenOver) {
    NVIC_EnableIRQ(SDIO_IRQn);
    EXPECT_NE(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(receipt.owned);
    NVIC_DisableIRQ(SDIO_IRQn);
    RCC->APB2ENR |= RCC_APB2ENR_SDIOEN;
    SDIO->DCTRL = SDIO_DCTRL_DTEN;
    EXPECT_NE(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(receipt.owned);
    SDIO->DCTRL = 0U;
}

TEST_F(SdioBlock, EmptyAlienReceiptCannotStopThePreparedOwner) {
    Prepare();
    const auto before = owner;
    const auto before_apb = RCC->APB2ENR;
    const auto before_moderc = GPIOC->MODER;
    nx_stm32_sdio_receipt_t alien{};
    const unsigned before_writes = writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdio_close(&owner, &config, &alien));
    EXPECT_EQ(0, std::memcmp(&owner, &before, sizeof(before)));
    EXPECT_EQ(before_apb, RCC->APB2ENR);
    EXPECT_EQ(before_moderc, GPIOC->MODER);
    EXPECT_EQ(before_writes, writes);
}

TEST_F(SdioBlock, InvalidFaceDispatchHasNoControllerEffects) {
    alignas(4) std::array<uint8_t, 512> data{};
    nx_block_port_t invalid{};
    EXPECT_EQ(nullptr, nx_block_port_geometry(&invalid));
    EXPECT_EQ(NX_BLOCK_UNAVAILABLE, nx_block_port_state(&invalid));
    EXPECT_EQ(NX_ERROR_INVALID, nx_block_port_activate(&invalid, 10000U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_block_port_read(&invalid, 0U, data.data(),
                                                   data.size(), 1U, 10000U)
                                    .result);
    EXPECT_EQ(NX_ERROR_INVALID, nx_block_port_write(&invalid, 0U, data.data(),
                                                    data.size(), 1U, 10000U)
                                    .result);
    EXPECT_EQ(NX_ERROR_INVALID, nx_block_port_sync(&invalid, 10000U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_block_port_stop(&invalid, 10000U));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, time_calls);
}

TEST_F(SdioBlock,
       ExplicitActivationIdentifiesHighCapacityAndNegotiatesFourBits) {
    Activate();
    ASSERT_NE(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(UINT64_C(1048576), nx_block_port_geometry(&face)->block_count);
    EXPECT_EQ(512U, nx_block_port_geometry(&face)->block_bytes);
    EXPECT_EQ(4U, nx_block_port_geometry(&face)->buffer_alignment);
    EXPECT_EQ(NX_BLOCK_READY, nx_block_port_state(&face));
    EXPECT_NE(commands.end(), std::find(commands.begin(), commands.end(), 51U));
    EXPECT_EQ(2U | SDIO_CLKCR_CLKEN | SDIO_CLKCR_WIDBUS_0, SDIO->CLKCR);
    EXPECT_EQ(0U, SDIO->MASK);
    EXPECT_EQ(0U, SDIO->DCTRL & SDIO_DCTRL_DMAEN);
    const auto before = commands.size();
    EXPECT_EQ(NX_ERROR_STATE, nx_block_port_activate(&face, 1000000U));
    EXPECT_EQ(before, commands.size());
}

TEST_F(SdioBlock, V1ByteAddressingUsesCsdGeometryAndSets512ByteBlocks) {
    config.bus_width = 1U;
    card_sdsc = true;
    Prepare();
    card_enabled = true;
    ASSERT_EQ(NX_SUCCESS, nx_block_port_activate(&face, 1000000U));
    ASSERT_NE(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(UINT64_C(2097152), nx_block_port_geometry(&face)->block_count);
    EXPECT_NE(commands.end(), std::find(commands.begin(), commands.end(), 16U));
    alignas(4) std::array<uint8_t, 512> data{};
    ASSERT_EQ(NX_SUCCESS, nx_block_port_read(&face, 7U, data.data(),
                                             data.size(), 1U, 1000000U)
                              .result);
    const auto pos = std::find(commands.begin(), commands.end(), 17U);
    ASSERT_NE(commands.end(), pos);
    EXPECT_EQ(7U * 512U,
              arguments[static_cast<size_t>(pos - commands.begin())]);
}

TEST_F(SdioBlock, R3CrcExceptionAndMaximumV2CapacityAreHandledPrecisely) {
    Prepare();
    card_enabled = true;
    r3_crc = true;
    csd_size = 0x3FFFFFU;
    ASSERT_EQ(NX_SUCCESS, nx_block_port_activate(&face, 1000000U));
    ASSERT_NE(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(UINT64_C(4294967296), nx_block_port_geometry(&face)->block_count);
    alignas(4) std::array<uint8_t, 512> data{};
    EXPECT_EQ(NX_SUCCESS,
              nx_block_port_read(&face, UINT64_C(4294967295), data.data(),
                                 data.size(), 1U, 1000000U)
                  .result);
    const unsigned before = writes;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_block_port_read(&face, UINT64_C(4294967296), data.data(),
                                 data.size(), 1U, 1000000U)
                  .result);
    EXPECT_EQ(before, writes);
}

TEST_F(SdioBlock, InvalidTransferAndContextAreRejectedBeforeClockOrMmio) {
    Activate();
    alignas(4) std::array<uint8_t, 1024> data{};
    const unsigned before_writes = writes;
    const unsigned before_time = time_calls;
    EXPECT_EQ(NX_ERROR_INVALID, nx_block_port_read(&face, 0U, data.data() + 1U,
                                                   512U, 1U, 1000000U)
                                    .result);
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_block_port_read(&face, 0U, data.data(), 511U, 1U, 1000000U).result);
    EXPECT_EQ(NX_ERROR_INVALID, nx_block_port_write(&face, 0U, data.data(),
                                                    data.size(), 0U, 1000000U)
                                    .result);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_block_port_read(&face, 0U, &owner, 512U, 1U, 1000000U).result);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_block_port_read(&face, 0U, data.data(), data.size(), 1U,
                                 NX_DEADLINE_NEVER)
                  .result);
    g_f4_model_isr = true;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_block_port_read(&face, 0U, data.data(),
                                                   data.size(), 1U, 1000000U)
                                    .result);
    g_f4_model_isr = false;
    EXPECT_EQ(before_writes, writes);
    EXPECT_EQ(before_time, time_calls);
}

TEST_F(SdioBlock, CrcFailureReportsOnlyConfirmedReadPrefixAndNoBorrowRemains) {
    Activate();
    alignas(4) std::array<uint8_t, 1024> data{};
    fail_read_number = 2U;
    const auto result =
        nx_block_port_read(&face, 2U, data.data(), data.size(), 2U, 1000000U);
    EXPECT_EQ(NX_ERROR_IO, result.result);
    EXPECT_EQ(1U, result.completed_blocks);
    EXPECT_EQ(0xA5000000U, WordAt(data.data(), 0U));
    EXPECT_EQ(0U, SDIO->DCTRL & (SDIO_DCTRL_DTEN | SDIO_DCTRL_DMAEN));
    const auto before = data;
    EXPECT_EQ(NX_SUCCESS, nx_block_port_sync(&face, 1000000U));
    EXPECT_EQ(before, data);
}

TEST_F(SdioBlock, WriteProgrammingTimeoutRetainsGateAndPadsUntilRealReady) {
    Activate();
    alignas(4) std::array<uint8_t, 512> data{};
    std::memset(data.data(), 0x5AU, data.size());
    write_busy = true;
    const auto result = nx_block_port_write(&face, 8U, data.data(), data.size(),
                                            1U, now + 1000U);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.result);
    EXPECT_EQ(0U, result.completed_blocks);
    EXPECT_EQ(NX_BLOCK_UNCERTAIN, nx_block_port_state(&face));
    EXPECT_TRUE(owner.programming_pending);
    ASSERT_EQ(128U, written_words.size());
    EXPECT_EQ(0x5A5A5A5AU, written_words.front());
    EXPECT_EQ(0U, SDIO->DCTRL & (SDIO_DCTRL_DTEN | SDIO_DCTRL_DMAEN));
    EXPECT_NE(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(RCC_APB2ENR_USART6EN | RCC_APB2ENR_SDIOEN, RCC->APB2ENR);
    write_busy = false;
    EXPECT_EQ(NX_SUCCESS, nx_block_port_sync(&face, now + 10000U));
    EXPECT_FALSE(owner.programming_pending);
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(RCC_APB2ENR_USART6EN, RCC->APB2ENR);
}

TEST_F(SdioBlock, StalledClockStillTerminatesViaOneGlobalPollBudget) {
    Prepare();
    card_enabled = true;
    silence_commands = true;
    clock_advances = false;
    config.poll_budget = 20U;
    const unsigned before = time_calls;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_block_port_activate(&face, 1000000U));
    EXPECT_LE(time_calls - before, 24U);
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
}

TEST_F(SdioBlock, UnsupportedCsdAndUnavailableFourBitNeverPublishGeometry) {
    Prepare();
    card_enabled = true;
    bad_csd = true;
    EXPECT_NE(NX_SUCCESS, nx_block_port_activate(&face, 1000000U));
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_NE(NX_BLOCK_READY, nx_block_port_state(&face));
}

TEST_F(SdioBlock, RefusedHostDrainCannotAuthorizePinOrGateWithdrawal) {
    Activate();
    host_stuck = true;
    *const_cast<volatile uint32_t*>(&SDIO->STA) |= SDIO_STA_RXACT;
    const auto before_c = GPIOC->MODER;
    EXPECT_NE(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(before_c, GPIOC->MODER);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_SDIOEN);
}

TEST_F(SdioBlock, EarlyDataCountEndWaitsForFifoDrainAndCrcCompletion) {
    Activate();
    alignas(4) std::array<uint8_t, 512> data{};
    dataend_early = true;
    const auto result = nx_block_port_read(&face, 1U, data.data(), data.size(),
                                           1U, now + 10000U);
    EXPECT_EQ(NX_SUCCESS, result.result);
    EXPECT_EQ(1U, result.completed_blocks);
    EXPECT_EQ(0xA500007FU, WordAt(data.data(), 127U));
}

TEST_F(SdioBlock, DataEndWithoutCrcPassedCannotConfirmABlock) {
    Activate();
    alignas(4) std::array<uint8_t, 512> data{};
    missing_crc_end = true;
    const auto result = nx_block_port_read(&face, 1U, data.data(), data.size(),
                                           1U, now + 1000U);
    EXPECT_EQ(NX_ERROR_TIMEOUT, result.result);
    EXPECT_EQ(0U, result.completed_blocks);
    missing_crc_end = false;
}

TEST_F(SdioBlock, SyncAbortsAReportedSendingCardBeforeClaimingReady) {
    Activate();
    card_recovery_state = 5U;
    owner.state = NX_BLOCK_UNCERTAIN;
    EXPECT_EQ(NX_SUCCESS, nx_block_port_sync(&face, now + 10000U));
    EXPECT_NE(commands.end(), std::find(commands.begin(), commands.end(), 12U));
    EXPECT_EQ(NX_BLOCK_READY, nx_block_port_state(&face));
    card_recovery_state = 0U;
}

TEST_F(SdioBlock, ColdRecordsMustBeDisjointBeforeAnyEffects) {
    auto* aliased = reinterpret_cast<nx_stm32_sdio_receipt_t*>(&owner);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_sdio_prepare(&owner, &config, aliased));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, time_calls);
}

TEST_F(SdioBlock, LostSelectedClockCannotStartANewTransfer) {
    Activate();
    alignas(4) std::array<uint8_t, 512> data{};
    const uint32_t pll = RCC->PLLCFGR;
    RCC->PLLCFGR ^= 1U;
    const auto before_writes = writes;
    const auto before_time = time_calls;
    EXPECT_EQ(NX_ERROR_STATE, nx_block_port_read(&face, 0U, data.data(),
                                                 data.size(), 1U, now + 10000U)
                                  .result);
    EXPECT_EQ(before_writes, writes);
    EXPECT_EQ(before_time, time_calls);
    RCC->PLLCFGR = pll;
}

TEST_F(SdioBlock, WriteProtectedCardExposesGeometryAndRejectsBeforeMmio) {
    Prepare();
    card_enabled = true;
    card_read_only = true;
    ASSERT_EQ(NX_SUCCESS, nx_block_port_activate(&face, now + 10000U));
    ASSERT_NE(nullptr, nx_block_port_geometry(&face));
    EXPECT_FALSE(nx_block_port_geometry(&face)->writable);
    alignas(4) std::array<uint8_t, 512> data{};
    const auto before_writes = writes;
    const auto before_time = time_calls;
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_block_port_write(&face, 0U, data.data(), data.size(), 1U,
                                  now + 10000U)
                  .result);
    EXPECT_EQ(before_writes, writes);
    EXPECT_EQ(before_time, time_calls);
}

TEST_F(SdioBlock, IllegalCommandOrLockedCardCannotPublishReadySession) {
    Prepare();
    card_enabled = true;
    fail_command = 7U;
    command_error = 1U << 25U;
    EXPECT_EQ(NX_ERROR_IO, nx_block_port_activate(&face, now + 10000U));
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_NE(NX_BLOCK_READY, nx_block_port_state(&face));
    fail_command = 0U;
}

TEST_F(SdioBlock, AbsentCardRefusesStopWithoutHiddenRediscovery) {
    Activate();
    silence_commands = true;
    const auto command_count = commands.size();
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_block_port_stop(&face, now + 100U));
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(NX_BLOCK_UNCERTAIN, nx_block_port_state(&face));
    for (size_t i = command_count; i < commands.size(); ++i)
        EXPECT_NE(0U, commands[i]);
    silence_commands = false;
    EXPECT_EQ(NX_SUCCESS, nx_block_port_stop(&face, now + 10000U));
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(NX_BLOCK_STOPPED, nx_block_port_state(&face));
    EXPECT_EQ(NX_SUCCESS, nx_block_port_activate(&face, now + 10000U));
}

TEST_F(SdioBlock, ClockCallbackCannotReenterTheSameOwner) {
    Activate();
    alignas(4) std::array<uint8_t, 512> data{};
    reentrant_port = &face;
    EXPECT_EQ(NX_SUCCESS, nx_block_port_read(&face, 0U, data.data(),
                                             data.size(), 1U, now + 10000U)
                              .result);
    EXPECT_EQ(NX_ERROR_BUSY, reentrant_result);
}

TEST_F(SdioBlock, ReviewedClockDomainSettlesBetweenControlWrites) {
    Activate();
    alignas(4) std::array<uint8_t, 1024> data{};
    ASSERT_EQ(NX_SUCCESS, nx_block_port_read(&face, 0U, data.data(),
                                             data.size(), 2U, now + 10000U)
                              .result);
    ASSERT_EQ(NX_SUCCESS, nx_block_port_stop(&face, now + 10000U));
    EXPECT_FALSE(control_spacing_failed);
}

TEST_F(SdioBlock, LegacyIllegalCmd8CanAdmitAnOtherwiseValidV1Card) {
    config.bus_width = 1U;
    Prepare();
    card_enabled = card_sdsc = legacy_illegal = true;
    EXPECT_EQ(NX_SUCCESS, nx_block_port_activate(&face, now + 10000U));
    EXPECT_NE(nullptr, nx_block_port_geometry(&face));
}

TEST_F(SdioBlock, MissingFourBitCapabilityDoesNotSilentlyChangeSelectedWidth) {
    Prepare();
    card_enabled = true;
    card_four_bit = false;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_block_port_activate(&face, now + 10000U));
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_NE(commands.end(), std::find(commands.begin(), commands.end(), 51U));
    EXPECT_EQ(commands.end(), std::find(commands.begin(), commands.end(), 6U));
}

TEST_F(SdioBlock, ExpiredDeselectionCanFinishAfterObservedStandbyOnRetry) {
    Activate();
    const auto deadline = now + 10000U;
    expire_after_deselect = deadline;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_block_port_stop(&face, deadline));
    EXPECT_FALSE(card_selected);
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(NX_BLOCK_UNCERTAIN, nx_block_port_state(&face));
    expire_after_deselect = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_block_port_stop(&face, now + 10000U));
    EXPECT_EQ(NX_BLOCK_STOPPED, nx_block_port_state(&face));
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
}

TEST_F(SdioBlock, TerminalCrcPassedStillDrainsTheFinalEightFifoWords) {
    Activate();
    alignas(4) std::array<uint8_t, 512> data{};
    terminal_tail = 8U;
    const auto result = nx_block_port_read(&face, 1U, data.data(), data.size(),
                                           1U, now + 10000U);
    EXPECT_EQ(NX_SUCCESS, result.result);
    EXPECT_EQ(1U, result.completed_blocks);
    EXPECT_EQ(0xA500007FU, WordAt(data.data(), 127U));
}

TEST_F(SdioBlock, ShortTerminalFifoCannotPublishACompleteBlock) {
    Activate();
    alignas(4) std::array<uint8_t, 512> data{};
    terminal_tail = 8U;
    fifo_delta = -1;
    const auto result = nx_block_port_read(&face, 1U, data.data(), data.size(),
                                           1U, now + 10000U);
    EXPECT_EQ(NX_ERROR_IO, result.result);
    EXPECT_EQ(0U, result.completed_blocks);
}

TEST_F(SdioBlock, ExtraTerminalFifoCannotOverflowOrConfirmTheCallerExtent) {
    Activate();
    alignas(4) std::array<uint8_t, 516> data{};
    data[512] = 0x5AU;
    terminal_tail = 8U;
    fifo_delta = 1;
    const auto result =
        nx_block_port_read(&face, 1U, data.data(), 512U, 1U, now + 10000U);
    EXPECT_EQ(NX_ERROR_IO, result.result);
    EXPECT_EQ(0U, result.completed_blocks);
    EXPECT_EQ(0x5AU, data[512]);
}

TEST_F(SdioBlock, WrongAhbPrescalerRejectsBeforeEffectsOrClockCalls) {
    RCC->CFGR |= RCC_CFGR_HPRE_DIV2;
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, time_calls);
}

TEST_F(SdioBlock, SnapshotInPeripheralSpaceCannotBecomeWritableOwnerStorage) {
    RCC->APB1RSTR = UINT32_C(0x5A5A5A5A);
    pins[0].snapshot =
        reinterpret_cast<uint16_t*>(const_cast<uint32_t*>(&RCC->APB1RSTR));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(UINT32_C(0x5A5A5A5A), RCC->APB1RSTR);
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, time_calls);
}

TEST_F(SdioBlock, ActiveAdmissionCannotBeWithdrawnThroughColdClose) {
    Prepare();
    owner.state = NX_BLOCK_ACTIVE;
    const auto before_pads = GPIOC->MODER;
    const auto before_gate = RCC->APB2ENR;
    const auto before_writes = writes;
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(before_pads, GPIOC->MODER);
    EXPECT_EQ(before_gate, RCC->APB2ENR);
    EXPECT_EQ(before_writes, writes);
    owner.state = NX_BLOCK_STOPPED;
}

TEST_F(SdioBlock, InconsistentUnownedReceiptCannotAuthorizeAnyRelease) {
    receipt.clock_added = RCC_APB2ENR_SDIOEN;
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, time_calls);
    receipt = {};
}

TEST_F(SdioBlock, RefusedPadConfigurationCannotPublishPreparedInfrastructure) {
    refused_write = &GPIOC->AFR[1];
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_FALSE(owner.infrastructure);
    EXPECT_TRUE(receipt.owned);
    EXPECT_NE(0U, receipt.pin_mask);
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    refused_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(RCC_APB2ENR_USART6EN, RCC->APB2ENR);
}

TEST_F(SdioBlock, RefusedPadRestoreRetainsClockAndRetryReceipt) {
    Prepare();
    refused_write = &GPIOC->MODER;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_TRUE(receipt.owned);
    EXPECT_NE(0U, receipt.pin_mask);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_SDIOEN);
    refused_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(RCC_APB2ENR_USART6EN, RCC->APB2ENR);
}

TEST_F(SdioBlock, RefusedGateWithdrawalRetainsAuthorityForObservedRetry) {
    Prepare();
    refused_write = &RCC->APB2ENR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(0U, receipt.pin_mask);
    EXPECT_EQ(RCC_APB2ENR_SDIOEN, receipt.clock_added);
    refused_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(RCC_APB2ENR_USART6EN, RCC->APB2ENR);
}

extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return g_f4_model_isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return g_f4_model_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_dmb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    const nx_arch_irq_state_t prior{g_f4_model_mask};
    g_f4_model_mask = 1U;
    return prior;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t prior) {
    g_f4_model_mask = prior.value;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    ++time_calls;
    if (reentrant_port != nullptr) {
        const nx_block_port_t* port = reentrant_port;
        reentrant_port = nullptr;
        reentrant_result =
            nx_block_port_read(port, 0U, reentrant_data.data(),
                               reentrant_data.size(), 1U, now + 10000U)
                .result;
    }
    return clock_advances ? ++now : now;
}
extern "C" uint32_t nx_sdio_model_read(volatile const uint32_t* reg) {
    if (card_enabled && reg == &SDIO->STA && fifo_limit != 0U) {
        uint32_t status = *reg;
        if (fifo_reading && fail_read_number != 0U &&
            read_number == fail_read_number && fifo_words >= 16U) {
            status &= ~static_cast<uint32_t>(SDIO_STA_RXACT | SDIO_STA_RXDAVL);
            status |= SDIO_STA_DCRCFAIL;
        } else if (fifo_words >= fifo_limit) {
            status &=
                ~static_cast<uint32_t>(SDIO_STA_RXACT | SDIO_STA_TXACT |
                                       SDIO_STA_RXDAVL | SDIO_STA_TXFIFOHE);
            status |= SDIO_STA_DATAEND;
            if (!missing_crc_end)
                status |= SDIO_STA_DBCKEND;
        }
        if (terminal_tail != 0U && fifo_reading &&
            fifo_words >= fifo_limit - terminal_tail) {
            status &= ~static_cast<uint32_t>(SDIO_STA_RXACT);
            status |= SDIO_STA_DATAEND | SDIO_STA_DBCKEND;
            *const_cast<volatile uint32_t*>(&SDIO->DCOUNT) = 0U;
        }
        if (dataend_early && fifo_reading && fifo_words != 0U)
            status |= SDIO_STA_DATAEND;
        if (host_stuck)
            status |= SDIO_STA_RXACT;
        *const_cast<volatile uint32_t*>(reg) = status;
    }
    return *reg;
}
extern "C" void nx_sdio_model_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
    if (reg == refused_write)
        return;
    if (card_enabled && (reg == &SDIO->DCTRL || reg == &SDIO->CLKCR)) {
        if (control_seen && now - last_control_write < 1U)
            control_spacing_failed = true;
        control_seen = true;
        last_control_write = now;
    }
    *reg = value;
    if (reg == &SDIO->CMD && (value & SDIO_CMD_CPSMEN) != 0U) {
        commands.push_back(value & SDIO_CMD_CMDINDEX);
        arguments.push_back(static_cast<uint32_t>(SDIO->ARG));
        CardCommand(value & SDIO_CMD_CMDINDEX);
    }
    if (reg == &SDIO->DCTRL) {
        if ((value & SDIO_DCTRL_DTEN) == 0U) {
            if (!host_stuck)
                *const_cast<volatile uint32_t*>(&SDIO->STA) &=
                    ~static_cast<uint32_t>(SDIO_STA_RXACT | SDIO_STA_TXACT |
                                           SDIO_STA_RXDAVL | SDIO_STA_TXFIFOHE);
            fifo_limit = 0U;
            fifo_reading = fifo_writing = false;
        } else if (card_enabled && !commands.empty() &&
                   commands.back() == 24U) {
            DataStarted(128U, false);
        }
    }
    if (reg == &SDIO->ICR)
        *const_cast<volatile uint32_t*>(&SDIO->STA) &= ~value;
}
extern "C" void nx_platform_cold_model_write(volatile uint32_t* reg,
                                             uint32_t value) {
    nx_sdio_model_write(reg, value);
}
extern "C" uint32_t nx_sdio_model_fifo_read(SDIO_TypeDef* registers) {
    if (card_enabled) {
        uint32_t value = 0xA5000000U | fifo_words;
        if (!commands.empty() && commands.back() == 51U)
            value = fifo_words == 0U
                        ? (card_four_bit ? 0x00000502U : 0x00000102U)
                        : 0U;
        registers->FIFO = value;
        ++fifo_words;
        *const_cast<volatile uint32_t*>(&registers->DCOUNT) =
            (fifo_limit - fifo_words) * 4U;
    }
    return SDIO_ReadFIFO(registers);
}
extern "C" HAL_StatusTypeDef nx_sdio_model_fifo_write(SDIO_TypeDef* registers,
                                                      uint32_t* word) {
    const auto result = SDIO_WriteFIFO(registers, word);
    if (card_enabled) {
        written_words.push_back(static_cast<uint32_t>(registers->FIFO));
        ++fifo_words;
        *const_cast<volatile uint32_t*>(&registers->DCOUNT) =
            (fifo_limit - fifo_words) * 4U;
    }
    return result;
}
