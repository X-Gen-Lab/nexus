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
bool card_mmc;
uint32_t mmc_sectors;
uint8_t mmc_version;
unsigned mmc_switch_write;
unsigned mmc_ready_write;
unsigned mmc_host_width_write;
bool mmc_status_switch_error;
bool card_io;
bool io_ready_stuck;
uint8_t io_functions;
std::array<std::array<uint8_t, 131072>, 8> io_registers{};
uint32_t io_address;
uint8_t io_function;
bool io_increment;
uint32_t fail_io_address;
bool enforce_dma_request_order;
bool dma_request_order_failed;
bool io_lost_ien_response;
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
    } else if (cmd == 5U && card_io) {
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) =
            UINT32_C(0x80ff8000) | (static_cast<uint32_t>(io_functions) << 28U);
        if (r3_crc) {
            *const_cast<volatile uint32_t*>(&SDIO->STA) &=
                ~static_cast<uint32_t>(SDIO_STA_CMDREND);
            *const_cast<volatile uint32_t*>(&SDIO->STA) |= SDIO_STA_CCRCFAIL;
        }
    } else if (cmd == 52U && card_io) {
        const auto function = static_cast<uint8_t>((SDIO->ARG >> 28U) & 7U);
        uint32_t address = (SDIO->ARG >> 9U) & 0x1ffffU;
        if (address == fail_io_address) {
            *const_cast<volatile uint32_t*>(&SDIO->RESP1) = UINT32_C(1) << 11U;
            return;
        }
        if ((SDIO->ARG & (UINT32_C(1) << 31U)) != 0U) {
            io_registers[function][address] = static_cast<uint8_t>(SDIO->ARG);
            if (function == 0U && address == 2U && !io_ready_stuck)
                io_registers[0][3] = io_registers[0][2];
            if (function == 0U && address == 6U && (SDIO->ARG & 8U) != 0U) {
                io_registers[0][2] = io_registers[0][3] = io_registers[0][4] =
                    0U;
            }
        }
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) =
            io_registers[function][address];
        if (io_lost_ien_response && function == 0U && address == 4U &&
            (SDIO->ARG & (UINT32_C(1) << 31U)) != 0U) {
            *const_cast<volatile uint32_t*>(&SDIO->STA) &=
                ~static_cast<uint32_t>(SDIO_STA_CMDREND);
            *const_cast<volatile uint32_t*>(&SDIO->STA) |= SDIO_STA_CTIMEOUT;
        }
    } else if (cmd == 53U && card_io) {
        io_function = static_cast<uint8_t>((SDIO->ARG >> 28U) & 7U);
        io_address = (SDIO->ARG >> 9U) & 0x1ffffU;
        io_increment = (SDIO->ARG & (UINT32_C(1) << 26U)) != 0U;
        DataStarted((SDIO->DLEN + 3U) / 4U,
                    (SDIO->ARG & (UINT32_C(1) << 31U)) == 0U);
        *const_cast<volatile uint32_t*>(&SDIO->DCOUNT) = SDIO->DLEN;
    } else if (cmd == 13U && card_io) {
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) = UINT32_C(1) << 22U;
    } else if (cmd == 1U && card_mmc) {
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) =
            UINT32_C(0x80ff8000) | (card_sdsc ? 0U : UINT32_C(0x40000000));
    } else if (cmd == 8U && card_mmc) {
        DataStarted(128U, true);
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
        *const_cast<volatile uint32_t*>(&SDIO->RESP1) =
            card_mmc ? 0U : UINT32_C(0x12340000);
    } else if (cmd == 9U) {
        std::array<uint8_t, 16> bytes{};
        bytes[0] = bad_csd ? 0xC0U : ((card_sdsc || card_mmc) ? 0U : 0x40U);
        if (card_mmc)
            bytes[0] |= static_cast<uint8_t>(mmc_version << 2U);
        if (card_sdsc || card_mmc) {
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
        if (card_mmc && mmc_switch_write != 0U && mmc_status_switch_error)
            *const_cast<volatile uint32_t*>(&SDIO->RESP1) |= 1U << 7U;
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
        card_mmc = false;
        mmc_sectors = 1048576U;
        mmc_version = 4U;
        mmc_switch_write = mmc_ready_write = mmc_host_width_write = 0U;
        mmc_status_switch_error = false;
        card_io = io_ready_stuck = false;
        io_functions = 2U;
        io_address = 0U;
        io_function = 0U;
        io_increment = false;
        fail_io_address = UINT32_MAX;
        enforce_dma_request_order = dma_request_order_failed = false;
        io_lost_ien_response = false;
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
        RCC->CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_PLLON | RCC_CR_PLLRDY;
        RCC->CFGR = RCC_CFGR_SW_PLL | RCC_CFGR_SWS_PLL | RCC_CFGR_PPRE1_DIV4 |
                    RCC_CFGR_PPRE2_DIV2;
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
            mmc_status_switch_error = false;
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

/** \brief Real finite IRQ engine; only the external FIFO/CPU are modeled. */
class SdioAsync : public SdioBlock {
  protected:
    nx_stm32_sdio_async_state_t execution{};
    nx_block_async_port_t async_face{&nx_stm32_sdio_async_ops, &owner};
    nx_block_request_t request{};
    alignas(4) std::array<uint8_t, 1024> data{};
    void SetUp() override {
        SdioBlock::SetUp();
        config.engine = NX_STM32_SDIO_IRQ;
        config.async = &execution;
        config.irq_priority = 6U;
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_request_prepare(&request.base, 1000000U));
        request.data = data.data();
        request.data_bytes = data.size();
        request.first_block = 1U;
        request.blocks = 1U;
    }
    void Irq() {
        g_f4_model_isr = true;
        nx_stm32_sdio_irq(&owner);
        g_f4_model_isr = false;
    }
    void Pump() {
        for (unsigned i = 0U; i < 20U; ++i)
            Irq();
        nx_block_async_service(&async_face);
    }
};

TEST_F(SdioAsync, ExactIrqLeaseIsAcquiredAndRestored) {
    NVIC_SetPriority(SDIO_IRQn, 11U);
    Prepare();
    EXPECT_EQ(1U, NVIC_GetEnableIRQ(SDIO_IRQn));
    EXPECT_EQ(6U, NVIC_GetPriority(SDIO_IRQn));
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(0U, NVIC_GetEnableIRQ(SDIO_IRQn));
    EXPECT_EQ(11U, NVIC_GetPriority(SDIO_IRQn));
}

TEST_F(SdioAsync, IrqDataCompletesOnlyAfterThreadDrainAndCrc) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    EXPECT_EQ(&request, execution.active);
    for (unsigned i = 0U; i < 16U; ++i)
        Irq();
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(512U, request.base.transferred);
    EXPECT_EQ(0xA500007FU, WordAt(data.data(), 127U));
    EXPECT_EQ(nullptr, execution.active);
    EXPECT_EQ(0U, SDIO->MASK);
    EXPECT_EQ(0U, SDIO->DCTRL);
    EXPECT_EQ(NX_BLOCK_READY, nx_block_port_state(&face));
}

TEST_F(SdioAsync, LateIrqCannotAccessASettledRequestOrBuffer) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    Pump();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    data.fill(0x6aU);
    request.data = nullptr;
    Irq();
    EXPECT_EQ(0x6aU, data[0]);
    EXPECT_EQ(0x6aU, data[511]);
    EXPECT_EQ(nullptr, execution.active);
}

TEST_F(SdioAsync, CancellationRetainsTheBorrowUntilHostWithdrawal) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    Irq();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_cancel(&async_face, &request));
    EXPECT_EQ(NX_REQUEST_DRAINING, nx_request_state(&request.base));
    EXPECT_EQ(&request, execution.active);
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
    EXPECT_EQ(nullptr, execution.active);
    EXPECT_EQ(NX_BLOCK_UNCERTAIN, nx_block_port_state(&face));
}

TEST_F(SdioAsync, RefusedWithdrawalQuarantinesAndRetainsColdOwner) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    ASSERT_EQ(NX_SUCCESS, nx_block_async_cancel(&async_face, &request));
    refused_write = &SDIO->DCTRL;
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, execution.active);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_TRUE(receipt.owned);
    refused_write = nullptr;
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}

TEST_F(SdioAsync, RemovedCardInvalidatesGeometryAndNeverAutoReprobes) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    card_enabled = false;
    ASSERT_EQ(NX_SUCCESS, nx_block_async_media_removed(&async_face));
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_NE(NX_SUCCESS, request.base.result);
    EXPECT_EQ(NX_ERROR_STATE, nx_block_port_activate(&face, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_block_port_stop(&face, 1000000U));
    card_enabled = true;
}

TEST_F(SdioAsync, CrcErrorNeverConfirmsTheCurrentBlock) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    *const_cast<volatile uint32_t*>(&SDIO->STA) |= SDIO_STA_DCRCFAIL;
    Irq();
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}

TEST_F(SdioAsync, TimeoutRetainsStorageThroughServiceDrain) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    now = request.base.deadline;
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_TIMEOUT, request.base.result);
    EXPECT_EQ(nullptr, execution.active);
}

TEST_F(SdioAsync, TwoBlocksUseConfirmedWholeBlockPrefix) {
    Activate();
    request.blocks = 2U;
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    Pump();
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    *const_cast<volatile uint32_t*>(&SDIO->STA) |= SDIO_STA_DCRCFAIL;
    Irq();
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(512U, request.base.transferred);
}

TEST_F(SdioAsync, InvalidAndConcurrentAdmissionHaveNoBorrowOrCommand) {
    Activate();
    const auto before = commands.size();
    request.blocks = 0U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_block_async_submit(&async_face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(before, commands.size());
    request.blocks = 1U;
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    nx_block_request_t other{};
    nx_request_initialize(&other.base);
    ASSERT_EQ(NX_SUCCESS, nx_request_prepare(&other.base, 1000000U));
    EXPECT_EQ(NX_ERROR_BUSY, nx_block_async_submit(&async_face, &other));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&other.base));
    EXPECT_EQ(NX_ERROR_STATE, nx_block_async_cancel(&async_face, &other));
    EXPECT_EQ(NX_ERROR_BUSY, nx_block_port_stop(&face, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_block_async_cancel(&async_face, &request));
    nx_block_async_service(&async_face);
}

TEST_F(SdioAsync, RequestAndBufferCannotOverlap) {
    Activate();
    request.data = &request;
    request.data_bytes = 512U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_block_async_submit(&async_face, &request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
    EXPECT_EQ(nullptr, execution.active);
}

/** \brief A model DMA master can reach only an explicitly mapped SRAM span. */
class SdioDma : public SdioAsync {
  protected:
    void* sram = MAP_FAILED;
    nx_dma_memory_region_t region{};
    void SetUp() override {
        SdioAsync::SetUp();
        sram = mmap(reinterpret_cast<void*>(0x20000000U), 4096U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, sram);
        region = {0x20000000U, 4096U, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        config.engine = NX_STM32_SDIO_DMA;
        config.dma_regions = &region;
        config.dma_region_count = 1U;
        request.data = sram;
        request.data_bytes = 4096U;
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    }
    void TearDown() override {
        SdioAsync::TearDown();
        if (sram != MAP_FAILED) {
            EXPECT_EQ(0, munmap(sram, 4096U));
        }
    }
    void DmaIrq() {
        g_f4_model_isr = true;
        nx_stm32_sdio_dma_irq(&owner);
        g_f4_model_isr = false;
    }
    void FinishDma() {
        DMA2_Stream3->NDTR = 0U;
        DMA2_Stream3->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
        DMA2->LISR |= UINT32_C(1) << 27U;
        DmaIrq();
    }
};

TEST_F(SdioDma, DmaCompletionWaitsForSdioCrcAndCounter) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    EXPECT_EQ(128U, DMA2_Stream3->NDTR);
    EXPECT_EQ(UINT32_C(0x20000000), DMA2_Stream3->M0AR);
    EXPECT_NE(0U, SDIO->DCTRL & SDIO_DCTRL_DMAEN);
    FinishDma();
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
    *const_cast<volatile uint32_t*>(&SDIO->DCOUNT) = 0U;
    *const_cast<volatile uint32_t*>(&SDIO->STA) =
        SDIO_STA_DATAEND | SDIO_STA_DBCKEND;
    Irq();
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(512U, request.base.transferred);
    EXPECT_EQ(0U, DMA2_Stream3->CR & DMA_SxCR_EN);
    EXPECT_EQ(0U, SDIO->DCTRL);
    EXPECT_EQ(nullptr, execution.active);
}

TEST_F(SdioDma, CcmAndExternalWindowsCannotBecomeDmaBuffers) {
    Activate();
    auto before = commands.size();
    for (uintptr_t address : {UINT32_C(0x10000000), UINT32_C(0x68000000)}) {
        request.data = reinterpret_cast<void*>(address);
        EXPECT_EQ(NX_ERROR_PERMISSION,
                  nx_block_async_submit(&async_face, &request));
        EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&request.base));
        EXPECT_EQ(before, commands.size());
    }
}

TEST_F(SdioDma, RefusedDmaDisableKeepsMemoryAndRequestQuarantined) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    ASSERT_EQ(NX_SUCCESS, nx_block_async_cancel(&async_face, &request));
    refused_write = &DMA2_Stream3->CR;
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED, nx_request_state(&request.base));
    EXPECT_EQ(&request, execution.active);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdio_close(&owner, &config, &receipt));
    refused_write = nullptr;
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, request.base.result);
}

TEST_F(SdioDma, DmaTransferErrorCannotBeHiddenBySdioDataEnd) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    DMA2->LISR |= UINT32_C(1) << 25U;
    DmaIrq();
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}

TEST_F(SdioBlock, ReadyFlagWithoutEnabledSourceIsNotAClockLease) {
    RCC->CR &= ~static_cast<uint32_t>(RCC_CR_HSEON);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_EQ(0U, time_calls);
}

TEST_F(SdioDma, LateDmaErrorIsObservedBeforeFlagsAreAcknowledged) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    FinishDma();
    *const_cast<volatile uint32_t*>(&SDIO->DCOUNT) = 0U;
    *const_cast<volatile uint32_t*>(&SDIO->STA) =
        SDIO_STA_DATAEND | SDIO_STA_DBCKEND;
    Irq();
    DMA2->LISR |= UINT32_C(1) << 25U;
    nx_block_async_service(&async_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_ERROR_IO, request.base.result);
    EXPECT_EQ(0U, request.base.transferred);
}

TEST_F(SdioAsync, IrqWritesWaitForMediaProgrammingReadiness) {
    Activate();
    request.write = true;
    data.fill(0x5aU);
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    Pump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
    EXPECT_EQ(NX_SUCCESS, request.base.result);
    EXPECT_EQ(512U, request.base.transferred);
    EXPECT_EQ(128U, written_words.size());
    EXPECT_FALSE(owner.programming_pending);
}

/** \brief MMC uses CMD1, host-assigned RCA, EXT_CSD and SWITCH semantics. */
class MmcBlock : public SdioBlock {
  protected:
    void SetUp() override {
        SdioBlock::SetUp();
        card_mmc = true;
        config.protocol = NX_STM32_SDIO_MMC;
    }
};

TEST_F(MmcBlock, SectorMmcUsesExtCsdWithoutSdApplicationCommands) {
    Activate();
    const auto* geometry = nx_block_port_geometry(&face);
    ASSERT_NE(nullptr, geometry);
    EXPECT_EQ(1048576U, geometry->block_count);
    EXPECT_EQ(1U, owner.rca);
    EXPECT_EQ(commands.end(), std::find(commands.begin(), commands.end(), 55U));
    EXPECT_EQ(commands.end(), std::find(commands.begin(), commands.end(), 41U));
    auto change = std::find(commands.begin(), commands.end(), 6U);
    ASSERT_NE(commands.end(), change);
    EXPECT_EQ(UINT32_C(0x03b70100),
              arguments[static_cast<size_t>(change - commands.begin())]);
    EXPECT_EQ(SDIO_BUS_WIDE_4B, SDIO->CLKCR & SDIO_CLKCR_WIDBUS);
}

TEST_F(MmcBlock, ByteAddressedMmcKeepsExplicitOneBitAndCmd16) {
    card_sdsc = true;
    config.bus_width = 1U;
    Activate();
    ASSERT_NE(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(2097152U, nx_block_port_geometry(&face)->block_count);
    EXPECT_NE(commands.end(), std::find(commands.begin(), commands.end(), 16U));
    EXPECT_EQ(commands.end(), std::find(commands.begin(), commands.end(), 6U));
    alignas(4) std::array<uint8_t, 512> output{};
    const auto result = nx_block_port_read(&face, 3U, output.data(),
                                           output.size(), 1U, 1000000U);
    EXPECT_EQ(NX_SUCCESS, result.result);
    auto command = std::find(commands.begin(), commands.end(), 17U);
    ASSERT_NE(commands.end(), command);
    EXPECT_EQ(1536U,
              arguments[static_cast<size_t>(command - commands.begin())]);
}

TEST_F(MmcBlock, ZeroExtCsdSectorCountCannotPublishGeometry) {
    Prepare();
    card_enabled = true;
    mmc_sectors = 0U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_block_port_activate(&face, 1000000U));
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
}

/** \brief Exact MMC-only silicon pads, never an eight-wire SD socket claim. */
class MmcEight : public MmcBlock {
  protected:
    uint16_t wide_snapshots[10]{};
    std::array<nx_stm32_platform_pin_config_t, 10> wide_pins{};
    void SetUp() override {
        MmcBlock::SetUp();
        for (size_t i = 0U; i < 6U; ++i) {
            wide_pins[i] = pins[i];
            wide_pins[i].snapshot = &wide_snapshots[i];
        }
        wide_pins[6] = {GPIOB, &wide_snapshots[6], 1U, 8U};
        wide_pins[7] = {GPIOB, &wide_snapshots[7], 1U, 9U};
        wide_pins[8] = {GPIOC, &wide_snapshots[8], 2U, 6U};
        wide_pins[9] = {GPIOC, &wide_snapshots[9], 2U, 7U};
        config.pins = wide_pins.data();
        config.bus_width = 8U;
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    }
};

TEST_F(MmcEight, CardSwitchAndReadyPrecedeHostEightBitClock) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(1023U, receipt.pin_mask);
    for (const auto& pin : wide_pins) {
        EXPECT_EQ(2U, (pin.registers->MODER >> (2U * pin.pin)) & 3U);
        EXPECT_EQ(12U,
                  (pin.registers->AFR[pin.pin / 8U] >> (4U * (pin.pin % 8U))) &
                      15U);
    }
    card_enabled = true;
    ASSERT_EQ(NX_SUCCESS, nx_block_port_activate(&face, 1000000U));
    const auto change = std::find(commands.begin(), commands.end(), 6U);
    ASSERT_NE(commands.end(), change);
    EXPECT_EQ(UINT32_C(0x03b70200),
              arguments[static_cast<size_t>(change - commands.begin())]);
    EXPECT_NE(0U, mmc_switch_write);
    EXPECT_LT(mmc_switch_write, mmc_ready_write);
    EXPECT_LT(mmc_ready_write, mmc_host_width_write);
    EXPECT_EQ(SDIO_BUS_WIDE_8B, SDIO->CLKCR & SDIO_CLKCR_WIDBUS);
    alignas(4) std::array<uint8_t, 512> data{};
    EXPECT_EQ(NX_SUCCESS, nx_block_port_read(&face, 0U, data.data(),
                                             data.size(), 1U, 1000000U)
                              .result);
}

TEST_F(MmcEight, RefusedLastPadRestoreRetainsAllTenLeasesForRetry) {
    const uint32_t before_b = GPIOB->MODER;
    const uint32_t before_c = GPIOC->MODER;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    refused_write = &GPIOC->MODER;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(1023U, receipt.pin_mask);
    EXPECT_TRUE(receipt.owned);
    EXPECT_NE(0U, RCC->APB2ENR & RCC_APB2ENR_SDIOEN);
    refused_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(0U, receipt.pin_mask);
    EXPECT_EQ(before_b, GPIOB->MODER);
    EXPECT_EQ(before_c, GPIOC->MODER);
}

TEST_F(MmcEight, PartialNinthPadFailureIsRecordedAndFullyReturned) {
    refused_write = &GPIOC->AFR[0];
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(511U, receipt.pin_mask);
    EXPECT_TRUE(receipt.owned);
    refused_write = nullptr;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
    EXPECT_EQ(0U, receipt.pin_mask);
    EXPECT_EQ(0U, RCC->APB2ENR & RCC_APB2ENR_SDIOEN);
}

TEST_F(MmcEight, ExtraPadSnapshotAliasOrMissingBankRejectsBeforeEffects) {
    wide_pins[9].snapshot = reinterpret_cast<uint16_t*>(&owner);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(0U, writes);
    wide_pins[9].snapshot = &wide_snapshots[9];
    RCC->AHB1ENR &= ~static_cast<uint32_t>(RCC_AHB1ENR_GPIOBEN);
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    EXPECT_EQ(0U, writes);
    EXPECT_FALSE(receipt.owned);
}

TEST_F(MmcEight, OldMmcVersionCannotReceiveTheExtendedBusSwitch) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    card_enabled = true;
    mmc_version = 3U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_block_port_activate(&face, 1000000U));
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(0U, mmc_switch_write);
    EXPECT_EQ(0U, mmc_host_width_write);
}

TEST_F(MmcEight, RejectedCardSwitchLeavesHostInIdentificationWidth) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    card_enabled = true;
    fail_command = 6U;
    command_error = UINT32_C(1) << 7U;
    EXPECT_EQ(NX_ERROR_IO, nx_block_port_activate(&face, 1000000U));
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(SDIO_BUS_WIDE_1B, SDIO->CLKCR & SDIO_CLKCR_WIDBUS);
    EXPECT_EQ(0U, mmc_host_width_write);
}

TEST_F(MmcEight, DeferredSwitchErrorInReadyStatusNeverWidensHost) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
    card_enabled = true;
    mmc_status_switch_error = true;
    EXPECT_EQ(NX_ERROR_IO, nx_block_port_activate(&face, 1000000U));
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(SDIO_BUS_WIDE_1B, SDIO->CLKCR & SDIO_CLKCR_WIDBUS);
    EXPECT_EQ(0U, mmc_host_width_write);
}

TEST_F(MmcEight, SdMemoryAndIoNeverAcceptAnEightBitBus) {
    for (const auto protocol : {NX_STM32_SDIO_SD_MEMORY, NX_STM32_SDIO_IO}) {
        config.protocol = protocol;
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_stm32_sdio_prepare(&owner, &config, &receipt));
        EXPECT_EQ(0U, writes);
        EXPECT_FALSE(receipt.owned);
    }
    config.protocol = NX_STM32_SDIO_MMC;
}

/** \brief Wire width reuses both finite engines and their actual IRQ leaves. */
class MmcEightAsync : public MmcEight {
  protected:
    void* sram = MAP_FAILED;
    nx_stm32_sdio_async_state_t execution{};
    nx_dma_memory_region_t region{};
    nx_block_request_t request{};
    void SetUp() override {
        MmcEight::SetUp();
        sram = mmap(reinterpret_cast<void*>(0x20000000U), 4096U,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_NE(MAP_FAILED, sram);
        region = {0x20000000U, 4096U, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        config.async = &execution;
        config.irq_priority = 6U;
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    }
    void TearDown() override {
        MmcEight::TearDown();
        if (sram != MAP_FAILED) {
            EXPECT_EQ(0, munmap(sram, 4096U));
        }
    }
};

TEST_F(MmcEightAsync, IrqAndDmaRetainTheSameEightBitDataAndDrainContract) {
    const nx_block_async_port_t async_face{&nx_stm32_sdio_async_ops, &owner};
    for (const auto engine : {NX_STM32_SDIO_IRQ, NX_STM32_SDIO_DMA}) {
        SCOPED_TRACE(engine);
        config.engine = engine;
        config.dma_regions = engine == NX_STM32_SDIO_DMA ? &region : nullptr;
        config.dma_region_count = engine == NX_STM32_SDIO_DMA ? 1U : 0U;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_sdio_prepare(&owner, &config, &receipt));
        card_enabled = true;
        ASSERT_EQ(NX_SUCCESS, nx_block_port_activate(&face, 1000000U));
        nx_request_initialize(&request.base);
        ASSERT_EQ(NX_SUCCESS, nx_request_prepare(&request.base, 1000000U));
        request.data = sram;
        request.data_bytes = 512U;
        request.blocks = 1U;
        ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
        EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&request.base));
        if (engine == NX_STM32_SDIO_DMA) {
            DMA2_Stream3->NDTR = 0U;
            DMA2_Stream3->CR &= ~static_cast<uint32_t>(DMA_SxCR_EN);
            DMA2->LISR |= UINT32_C(1) << 27U;
            g_f4_model_isr = true;
            nx_stm32_sdio_dma_irq(&owner);
            g_f4_model_isr = false;
            *const_cast<volatile uint32_t*>(&SDIO->DCOUNT) = 0U;
            *const_cast<volatile uint32_t*>(&SDIO->STA) =
                SDIO_STA_DATAEND | SDIO_STA_DBCKEND;
        }
        for (unsigned i = 0U; i < 20U; ++i) {
            g_f4_model_isr = true;
            nx_stm32_sdio_irq(&owner);
            g_f4_model_isr = false;
        }
        nx_block_async_service(&async_face);
        EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&request.base));
        EXPECT_EQ(NX_SUCCESS, request.base.result);
        EXPECT_EQ(512U, request.base.transferred);
        EXPECT_EQ(nullptr, execution.active);
        EXPECT_EQ(SDIO_BUS_WIDE_8B, SDIO->CLKCR & SDIO_CLKCR_WIDBUS);
        ASSERT_EQ(NX_SUCCESS, nx_block_port_stop(&face, 1000000U));
        ASSERT_EQ(NX_SUCCESS, nx_stm32_sdio_close(&owner, &config, &receipt));
        EXPECT_EQ(0U, receipt.pin_mask);
    }
}

TEST_F(SdioBlock, PollingRemovalStopsWithoutInventingAnAbsentCardResponse) {
    Activate();
    card_enabled = false;
    nx_result_t result = nx_block_port_media_removed(&face);
    EXPECT_EQ(NX_SUCCESS, result);
    if (result != NX_SUCCESS) {
        card_enabled = true;
        return;
    }
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(NX_SUCCESS, nx_block_port_stop(&face, 1000000U));
    EXPECT_EQ(NX_BLOCK_STOPPED, nx_block_port_state(&face));
    card_enabled = true;
}

TEST_F(SdioDma, GeometryDescribesTheSelectedDmaAlignment) {
    Activate();
    ASSERT_NE(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(16U, nx_block_port_geometry(&face)->buffer_alignment);
}

TEST_F(SdioAsync, MaskedPendingVectorCannotMovePayload) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    SDIO->MASK = 0U;
    Irq();
    EXPECT_EQ(0U, execution.words);
    EXPECT_EQ(0U, data[0]);
    ASSERT_EQ(NX_SUCCESS, nx_block_async_cancel(&async_face, &request));
    nx_block_async_service(&async_face);
}

TEST_F(SdioDma, MaskedDmaVectorCannotLatchAnUnarmedCompletion) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_block_async_submit(&async_face, &request));
    DMA2_Stream3->CR &=
        ~static_cast<uint32_t>(DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE);
    DMA2_Stream3->FCR &= ~static_cast<uint32_t>(DMA_SxFCR_FEIE);
    DMA2_Stream3->NDTR = 0U;
    DMA2->LISR |= UINT32_C(1) << 27U;
    DmaIrq();
    EXPECT_FALSE(execution.dma_complete);
    ASSERT_EQ(NX_SUCCESS, nx_block_async_cancel(&async_face, &request));
    nx_block_async_service(&async_face);
}

/** \brief Independent CCCR/FBR card image, no implementation under test mocked.
 */
static void InitializeIoCard() {
    card_io = true;
    for (auto& function : io_registers)
        function.fill(0U);
    io_registers[0][0] = 0x32U;
    io_registers[0][9] = 0x00U;
    io_registers[0][10] = 0x08U;
    for (unsigned function = 1U; function <= 2U; ++function) {
        io_registers[0][function * 256U] = static_cast<uint8_t>(function + 1U);
        io_registers[0][function * 256U + 9U] = 0U;
        io_registers[0][function * 256U + 10U] =
            static_cast<uint8_t>(function * 16U);
        io_registers[0][function * 256U + 16U] = 64U;
    }
}

class SdioIo : public SdioAsync {
  protected:
    nx_stm32_sdio_io_state_t io_state{};
    nx_sdio_io_port_t io_face{&nx_stm32_sdio_io_ops, &owner};
    nx_sdio_io_request_t io_request{};
    void SetUp() override {
        SdioAsync::SetUp();
        InitializeIoCard();
        config.protocol = NX_STM32_SDIO_IO;
        config.io = &io_state;
        nx_request_initialize(&io_request.transfer.base);
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_prepare(&io_request.transfer.base, 1000000U));
        io_request.transfer.data = data.data();
        io_request.transfer.data_bytes = data.size();
        io_request.transfer.blocks = 1U;
        io_request.function = 1U;
        io_request.address = 0x100U;
        io_request.unit_bytes = 5U;
        io_request.increment = true;
    }
    void Enable() {
        ASSERT_EQ(NX_SUCCESS, nx_sdio_io_enable(&io_face, 1U, true, 1000000U));
    }
    void IoPump() {
        for (unsigned count = 0U; count < 32U; ++count) {
            Irq();
            nx_sdio_io_service(&io_face);
        }
    }
};

TEST_F(SdioIo, Cmd5CccrFbrAndCisAreDistinctFromMemoryGeometry) {
    r3_crc = true;
    Activate();
    nx_sdio_io_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_inspect(&io_face, &info, 1000000U));
    EXPECT_EQ(2U, info.function_count);
    EXPECT_EQ(0x800U, info.common_cis_address);
    EXPECT_EQ(0x1000U, info.functions[0].cis_address);
    EXPECT_EQ(2U, info.functions[0].interface_code);
    EXPECT_EQ(nullptr, nx_block_port_geometry(&face));
    EXPECT_EQ(commands.end(), std::find(commands.begin(), commands.end(), 55U));
    EXPECT_EQ(commands.end(), std::find(commands.begin(), commands.end(), 13U));
    EXPECT_EQ(2U, io_registers[0][7] & 3U);
    EXPECT_EQ(SDIO_BUS_WIDE_4B, SDIO->CLKCR & SDIO_CLKCR_WIDBUS);
}

TEST_F(SdioIo, EnableAndExplicitBlockSizeOwnTheCccrFbrFields) {
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_block_size(&io_face, 1U, 512U, 1000000U));
    EXPECT_EQ(2U, io_registers[0][2]);
    EXPECT_EQ(2U, io_registers[0][3]);
    EXPECT_EQ(0U, io_registers[0][0x110]);
    EXPECT_EQ(2U, io_registers[0][0x111]);
    EXPECT_EQ(512U, io_state.block_bytes[0]);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_sdio_io_block_size(&io_face, 1U, 300U, 1000000U));
}

TEST_F(SdioIo, DirectCmd52NeverExposesFunctionZeroWritesOrAddressWrap) {
    Activate();
    Enable();
    const auto before = commands.size();
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_sdio_io_write_byte(&io_face, 0U, 7U, 0U, 1000000U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_sdio_io_write_byte(&io_face, 3U, 0U, 0U, 1000000U));
    uint8_t value = 0x5aU;
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_sdio_io_read_byte(&io_face, 1U, 0x20000U, &value, 1000000U));
    EXPECT_EQ(before, commands.size());
    ASSERT_EQ(NX_SUCCESS,
              nx_sdio_io_write_byte(&io_face, 1U, 0x1ffffU, 0x45U, 1000000U));
    ASSERT_EQ(NX_SUCCESS,
              nx_sdio_io_read_byte(&io_face, 1U, 0x1ffffU, &value, 1000000U));
    EXPECT_EQ(0x45U, value);
}

TEST_F(SdioIo, R5FunctionErrorIsNotAnSdStatusOrSuccessfulByte) {
    Activate();
    Enable();
    fail_command = 52U;
    command_error = UINT32_C(1) << 9U;
    uint8_t output = 0x76U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_sdio_io_read_byte(&io_face, 1U, 4U, &output, 1000000U));
    EXPECT_EQ(0x76U, output);
    EXPECT_EQ(NX_BLOCK_UNCERTAIN, nx_block_port_state(&face));
    fail_command = 0U;
}

TEST_F(SdioIo, ByteCmd53CopiesOnlyTheExactOddExtentAndNeverRounds) {
    Activate();
    Enable();
    for (unsigned index = 0U; index < 5U; ++index)
        io_registers[1][0x100U + index] = static_cast<uint8_t>(index + 1U);
    data[5] = 0xa5U;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&io_request.transfer.base));
    IoPump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_SUCCESS, io_request.transfer.base.result);
    EXPECT_EQ(5U, io_request.transfer.base.transferred);
    EXPECT_EQ(1U, data[0]);
    EXPECT_EQ(5U, data[4]);
    EXPECT_EQ(0xa5U, data[5]);
    auto command = std::find(commands.begin(), commands.end(), 53U);
    ASSERT_NE(commands.end(), command);
    EXPECT_EQ(UINT32_C(0x14020005),
              arguments[static_cast<size_t>(command - commands.begin())]);
}

TEST_F(SdioIo, BlockCmd53UsesConfiguredUnitsAndConfirmedPrefixes) {
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_block_size(&io_face, 1U, 64U, 1000000U));
    io_request.unit_bytes = 64U;
    io_request.transfer.blocks = 2U;
    io_request.block_mode = true;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    IoPump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_SUCCESS, io_request.transfer.base.result);
    EXPECT_EQ(128U, io_request.transfer.base.transferred);
    std::vector<uint32_t> io_args;
    for (size_t index = 0U; index < commands.size(); ++index)
        if (commands[index] == 53U)
            io_args.push_back(arguments[index]);
    ASSERT_EQ(2U, io_args.size());
    EXPECT_EQ(UINT32_C(0x1c020001), io_args[0]);
    EXPECT_EQ(UINT32_C(0x1c028001), io_args[1]);
}

TEST_F(SdioIo, FixedAddressWritesKeepFunctionFifoSemantics) {
    Activate();
    Enable();
    data[0] = 0x12U;
    data[1] = 0x34U;
    data[2] = 0x56U;
    io_request.unit_bytes = 3U;
    io_request.increment = false;
    io_request.transfer.write = true;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    IoPump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_SUCCESS, io_request.transfer.base.result);
    EXPECT_EQ(3U, io_request.transfer.base.transferred);
    EXPECT_EQ(0x56U, io_registers[1][0x100U]);
}

TEST_F(SdioIo, CancelledFunctionRequiresExplicitAbortBeforeRecovery) {
    Activate();
    Enable();
    io_request.unit_bytes = 128U;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_cancel(&io_face, &io_request));
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, io_request.transfer.base.result);
    EXPECT_EQ(NX_BLOCK_UNCERTAIN, nx_block_port_state(&face));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_sync(&io_face, 1000000U));
    EXPECT_EQ(1U, io_registers[0][6] & 7U);
    EXPECT_EQ(NX_BLOCK_READY, nx_block_port_state(&face));
}

TEST_F(SdioIo, WholeDescriptorAndIncrementingAddressArePreflighted) {
    Activate();
    Enable();
    const auto before = commands.size();
    io_request.address = 0x1fffeU;
    EXPECT_EQ(NX_ERROR_INVALID, nx_sdio_io_submit(&io_face, &io_request));
    io_request.address = 0x100U;
    io_request.transfer.data = &io_request.address;
    EXPECT_EQ(NX_ERROR_INVALID, nx_sdio_io_submit(&io_face, &io_request));
    EXPECT_EQ(before, commands.size());
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&io_request.transfer.base));
}

TEST_F(SdioIo, FunctionInterruptIsOnlyAHintUntilPendingAndDeviceAck) {
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_interrupts(&io_face, 2U, 1000000U));
    EXPECT_EQ(3U, io_registers[0][4]);
    io_registers[0][5] = 2U;
    *const_cast<volatile uint32_t*>(&SDIO->STA) |= SDIO_STA_SDIOIT;
    Irq();
    EXPECT_TRUE(io_state.interrupt_latched);
    EXPECT_EQ(0U, SDIO->MASK & SDIO_STA_SDIOIT);
    uint8_t pending = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_pending(&io_face, &pending, 1000000U));
    EXPECT_EQ(2U, pending);
    EXPECT_EQ(2U, io_registers[0][5]);
    io_registers[0][5] = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_interrupts(&io_face, 2U, 1000000U));
    EXPECT_FALSE(io_state.interrupt_latched);
    EXPECT_NE(0U, SDIO->MASK & SDIO_STA_SDIOIT);
}

TEST_F(SdioIo, StoppingDisablesFunctionsAndNeverSendsMemoryStatus) {
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_block_port_stop(&face, 1000000U));
    EXPECT_EQ(0U, io_registers[0][2]);
    EXPECT_EQ(0U, io_registers[0][4]);
    EXPECT_EQ(commands.end(), std::find(commands.begin(), commands.end(), 13U));
}

TEST_F(SdioIo, PollingFunctionsUseTheSameFiniteLoanWithoutIrqOrWorker) {
    config.engine = NX_STM32_SDIO_POLL;
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    EXPECT_EQ(0U, NVIC_GetEnableIRQ(SDIO_IRQn));
    EXPECT_EQ(0U, SDIO->MASK);
    for (unsigned count = 0U; count < 8U; ++count)
        nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_SUCCESS, io_request.transfer.base.result);
    EXPECT_EQ(5U, io_request.transfer.base.transferred);
}

TEST_F(SdioIo, FailedIoResetCannotReleasePreviouslyEnabledFunctions) {
    Prepare();
    card_enabled = true;
    io_registers[0][2] = io_registers[0][3] = 2U;
    io_registers[0][4] = 3U;
    fail_command = 52U;
    command_error = UINT32_C(1) << 9U;
    EXPECT_EQ(NX_ERROR_IO, nx_block_port_activate(&face, 1000000U));
    EXPECT_EQ(NX_ERROR_IO, nx_block_port_stop(&face, 1000000U));
    EXPECT_TRUE(owner.protocol_started);
    EXPECT_TRUE(receipt.owned);
    EXPECT_EQ(2U, io_registers[0][2]);
    fail_command = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_block_port_stop(&face, 1000000U));
    EXPECT_EQ(0U, io_registers[0][2]);
    EXPECT_EQ(0U, io_registers[0][4]);
}

TEST_F(SdioIo, PollingFunctionDoesNotConsumeAnUnleasedVectorPendingBit) {
    config.engine = NX_STM32_SDIO_POLL;
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    g_f4_model_nvic.ISPR[static_cast<unsigned>(SDIO_IRQn) / 32U] |=
        UINT32_C(1) << (static_cast<unsigned>(SDIO_IRQn) % 32U);
    for (unsigned count = 0U; count < 8U; ++count)
        nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(1U, NVIC_GetPendingIRQ(SDIO_IRQn));
}

TEST_F(SdioIo, EnableTimeoutRetainsTheAlreadyWrittenFunctionState) {
    Activate();
    io_ready_stuck = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_sdio_io_enable(&io_face, 1U, true, now + 30U));
    EXPECT_EQ(2U, io_registers[0][2]);
    EXPECT_EQ(0U, io_registers[0][3]);
    EXPECT_EQ(NX_BLOCK_UNCERTAIN, nx_block_port_state(&face));
    io_ready_stuck = false;
    io_registers[0][3] = 2U;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_sync(&io_face, 1000000U));
    EXPECT_EQ(NX_BLOCK_READY, nx_block_port_state(&face));
}

TEST_F(SdioIo, PartialFbrUpdateRequiresExplicitObservationAndRepair) {
    Activate();
    Enable();
    fail_io_address = 0x111U;
    EXPECT_EQ(NX_ERROR_IO, nx_sdio_io_block_size(&io_face, 1U, 512U, 1000000U));
    EXPECT_EQ(0U, io_state.block_bytes[0]);
    EXPECT_EQ(NX_BLOCK_UNCERTAIN, nx_block_port_state(&face));
    fail_io_address = UINT32_MAX;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_sync(&io_face, 1000000U));
    io_request.block_mode = true;
    io_request.unit_bytes = 64U;
    EXPECT_EQ(NX_ERROR_STATE, nx_sdio_io_submit(&io_face, &io_request));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_block_size(&io_face, 1U, 64U, 1000000U));
}

TEST_F(SdioIo, LateVectorCannotTouchAReusedEnclosingFunctionRequest) {
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    IoPump();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    data.fill(0xa6U);
    io_request.transfer.data = nullptr;
    io_request.function = 7U;
    Irq();
    EXPECT_EQ(0xa6U, data[0]);
    EXPECT_EQ(nullptr, execution.active);
}

TEST_F(SdioIo, KnownRomCannotBecomeACmd53ReceiveDestination) {
    Activate();
    Enable();
    io_request.transfer.data = reinterpret_cast<void*>(0x08000000U);
    const auto before = commands.size();
    nx_result_t result = nx_sdio_io_submit(&io_face, &io_request);
    EXPECT_EQ(NX_ERROR_INVALID, result);
    EXPECT_EQ(before, commands.size());
    if (result == NX_SUCCESS) {
        ASSERT_EQ(NX_SUCCESS, nx_sdio_io_cancel(&io_face, &io_request));
        nx_sdio_io_service(&io_face);
    }
}

TEST_F(SdioBlock, KnownRomReceiveRejectionPrecedesAnyMediaCommand) {
    Activate();
    silence_commands = true;
    const auto before = commands.size();
    auto result = nx_block_port_read(
        &face, 0U, reinterpret_cast<void*>(0x08000000U), 512U, 1U, now + 50U);
    EXPECT_EQ(NX_ERROR_INVALID, result.result);
    EXPECT_EQ(before, commands.size());
    silence_commands = false;
}

TEST_F(SdioIo, Exact512ByteModeUsesTheStandardZeroCountEncoding) {
    Activate();
    Enable();
    io_request.unit_bytes = 512U;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    IoPump();
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(512U, io_request.transfer.base.transferred);
    auto command = std::find(commands.begin(), commands.end(), 53U);
    ASSERT_NE(commands.end(), command);
    EXPECT_EQ(0U, arguments[static_cast<size_t>(command - commands.begin())] &
                      511U);
}

TEST_F(SdioIo, DataCompletionPreservesAnUnconsumedFunctionInterruptArm) {
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_interrupts(&io_face, 2U, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    IoPump();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_NE(0U, SDIO->MASK & SDIO_STA_SDIOIT);
    nx_sdio_io_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_inspect(&io_face, &info, 1000000U));
    EXPECT_TRUE(info.interrupt_armed);
    io_registers[0][5] = 2U;
    *const_cast<volatile uint32_t*>(&SDIO->STA) |= SDIO_STA_SDIOIT;
    Irq();
    EXPECT_TRUE(io_state.interrupt_latched);
}

TEST_F(SdioIo, SyncRequiresAnExplicitNewInterruptArm) {
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_interrupts(&io_face, 2U, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_sync(&io_face, 1000000U));
    EXPECT_FALSE(io_state.interrupt_armed);
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    IoPump();
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(0U, SDIO->MASK & SDIO_STA_SDIOIT);
}

TEST_F(SdioIo, SyncObservesIenAfterAnAppliedWriteLosesItsResponse) {
    Activate();
    Enable();
    io_lost_ien_response = true;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_sdio_io_interrupts(&io_face, 2U, 1000000U));
    EXPECT_EQ(3U, io_registers[0][4]);
    io_lost_ien_response = false;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_sync(&io_face, 1000000U));
    nx_sdio_io_info_t info{};
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_inspect(&io_face, &info, 1000000U));
    EXPECT_EQ(2U, info.interrupt_functions);
    EXPECT_FALSE(info.interrupt_armed);
}

TEST_F(SdioIo, LowSpeedOneBitKeepsTheIdentificationClock) {
    io_registers[0][8] = 64U;
    config.bus_width = 1U;
    Activate();
    EXPECT_EQ(118U, SDIO->CLKCR & SDIO_CLKCR_CLKDIV);
    EXPECT_EQ(0U, SDIO->CLKCR & SDIO_CLKCR_WIDBUS);
}

TEST_F(SdioIo, LowSpeedFourBitRequiresTheActualCardCapability) {
    io_registers[0][8] = 64U;
    Prepare();
    card_enabled = true;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_block_port_activate(&face, 1000000U));
    EXPECT_EQ(NX_BLOCK_UNCERTAIN, nx_block_port_state(&face));
    ASSERT_EQ(NX_SUCCESS, nx_block_port_stop(&face, 1000000U));
    io_registers[0][8] = 64U | 128U;
    ASSERT_EQ(NX_SUCCESS, nx_block_port_activate(&face, 1000000U));
    EXPECT_EQ(118U, SDIO->CLKCR & SDIO_CLKCR_CLKDIV);
    EXPECT_EQ(static_cast<uint32_t>(SDIO_CLKCR_WIDBUS_0),
              SDIO->CLKCR & SDIO_CLKCR_WIDBUS);
}

TEST_F(SdioIo, RemovalDrainsTheBorrowWithoutInventingAnAbsentResponse) {
    Activate();
    Enable();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    ASSERT_EQ(NX_SUCCESS, nx_block_port_media_removed(&face));
    const auto before = commands.size();
    silence_commands = true;
    nx_sdio_io_service(&io_face);
    ASSERT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_ERROR_IO, io_request.transfer.base.result);
    ASSERT_EQ(NX_SUCCESS, nx_block_port_stop(&face, 1000000U));
    EXPECT_EQ(before, commands.size());
    silence_commands = false;
}

class SdioIoDma : public SdioDma {
  protected:
    nx_stm32_sdio_io_state_t io_state{};
    nx_sdio_io_port_t io_face{&nx_stm32_sdio_io_ops, &owner};
    nx_sdio_io_request_t io_request{};
    void SetUp() override {
        SdioDma::SetUp();
        InitializeIoCard();
        config.protocol = NX_STM32_SDIO_IO;
        config.io = &io_state;
        nx_request_initialize(&io_request.transfer.base);
        ASSERT_EQ(NX_SUCCESS,
                  nx_request_prepare(&io_request.transfer.base, 1000000U));
        io_request.transfer.data = sram;
        io_request.transfer.data_bytes = 4096U;
        io_request.transfer.blocks = 1U;
        io_request.function = 1U;
        io_request.address = 0x100U;
        io_request.unit_bytes = 64U;
        io_request.increment = true;
    }
};

TEST_F(SdioIoDma, DmaCmd53StillRequiresCrcBeforeReleasingTheExactBorrow) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_enable(&io_face, 1U, true, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    EXPECT_EQ(16U, DMA2_Stream3->NDTR);
    FinishDma();
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_ACTIVE, nx_request_state(&io_request.transfer.base));
    *const_cast<volatile uint32_t*>(&SDIO->DCOUNT) = 0U;
    *const_cast<volatile uint32_t*>(&SDIO->STA) =
        SDIO_STA_DATAEND | SDIO_STA_DBCKEND;
    Irq();
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_SUCCESS, io_request.transfer.base.result);
    EXPECT_EQ(64U, io_request.transfer.base.transferred);
    EXPECT_EQ(nullptr, execution.active);
}

TEST_F(SdioIoDma, OddDmaExtentIsRejectedWithoutBounceStorageOrCommands) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_enable(&io_face, 1U, true, 1000000U));
    auto before = commands.size();
    io_request.unit_bytes = 5U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_sdio_io_submit(&io_face, &io_request));
    EXPECT_EQ(NX_REQUEST_READY, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(before, commands.size());
}

TEST_F(SdioIoDma, RefusedDmaWithdrawalQuarantinesTheWholeFunctionRequest) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_enable(&io_face, 1U, true, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_cancel(&io_face, &io_request));
    refused_write = &DMA2_Stream3->CR;
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED,
              nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(&io_request.transfer, execution.active);
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_sdio_close(&owner, &config, &receipt));
    refused_write = nullptr;
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_ERROR_CANCELLED, io_request.transfer.base.result);
}

TEST_F(SdioIoDma, RefusedCrInterruptWithdrawalAfterTcRetainsTheBorrow) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_enable(&io_face, 1U, true, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    FinishDma();
    *const_cast<volatile uint32_t*>(&SDIO->DCOUNT) = 0U;
    *const_cast<volatile uint32_t*>(&SDIO->STA) =
        SDIO_STA_DATAEND | SDIO_STA_DBCKEND;
    Irq();
    refused_write = &DMA2_Stream3->CR;
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED,
              nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(&io_request.transfer, execution.active);
    refused_write = nullptr;
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_ERROR_IO, io_request.transfer.base.result);
}

TEST_F(SdioIoDma, RefusedFifoInterruptWithdrawalAfterTcRetainsTheBorrow) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_enable(&io_face, 1U, true, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    FinishDma();
    *const_cast<volatile uint32_t*>(&SDIO->DCOUNT) = 0U;
    *const_cast<volatile uint32_t*>(&SDIO->STA) =
        SDIO_STA_DATAEND | SDIO_STA_DBCKEND;
    Irq();
    refused_write = &DMA2_Stream3->FCR;
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_QUARANTINED,
              nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(&io_request.transfer, execution.active);
    refused_write = nullptr;
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(NX_ERROR_IO, io_request.transfer.base.result);
}

TEST_F(SdioIoDma, HalfTransferInterruptsNeverBelongToTheFiniteOwner) {
    DMA2_Stream3->CR = DMA_SxCR_HTIE;
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_enable(&io_face, 1U, true, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    EXPECT_EQ(0U, DMA2_Stream3->CR & DMA_SxCR_HTIE);
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_cancel(&io_face, &io_request));
    nx_sdio_io_service(&io_face);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    EXPECT_EQ(0U, DMA2_Stream3->CR & DMA_SxCR_HTIE);
}

TEST_F(SdioIoDma, ProducerRequestsStopBeforeTheDmaMemoryEngine) {
    Activate();
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_enable(&io_face, 1U, true, 1000000U));
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_submit(&io_face, &io_request));
    enforce_dma_request_order = true;
    ASSERT_EQ(NX_SUCCESS, nx_sdio_io_cancel(&io_face, &io_request));
    nx_sdio_io_service(&io_face);
    EXPECT_FALSE(dma_request_order_failed);
    EXPECT_EQ(NX_REQUEST_SETTLED, nx_request_state(&io_request.transfer.base));
    enforce_dma_request_order = false;
    nx_sdio_io_service(&io_face);
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
    if (enforce_dma_request_order && reg == &DMA2_Stream3->CR &&
        (DMA2_Stream3->CR & DMA_SxCR_EN) != 0U && (value & DMA_SxCR_EN) == 0U &&
        (SDIO->DCTRL & SDIO_DCTRL_DMAEN) != 0U) {
        dma_request_order_failed = true;
        return;
    }
    if (card_enabled && (reg == &SDIO->DCTRL || reg == &SDIO->CLKCR)) {
        if (control_seen && now - last_control_write < 1U)
            control_spacing_failed = true;
        control_seen = true;
        last_control_write = now;
    }
    if (card_mmc && reg == &SDIO->CLKCR &&
        (value & SDIO_CLKCR_WIDBUS) == SDIO_BUS_WIDE_8B)
        mmc_host_width_write = writes;
    *reg = value;
    if (reg == &SDIO->CMD && (value & SDIO_CMD_CPSMEN) != 0U) {
        commands.push_back(value & SDIO_CMD_CMDINDEX);
        arguments.push_back(static_cast<uint32_t>(SDIO->ARG));
        if (card_mmc && (value & SDIO_CMD_CMDINDEX) == 6U)
            mmc_switch_write = writes;
        CardCommand(value & SDIO_CMD_CMDINDEX);
        if (card_mmc && mmc_switch_write != 0U &&
            (value & SDIO_CMD_CMDINDEX) == 13U &&
            (SDIO->RESP1 & (UINT32_C(1) << 8U)) != 0U &&
            ((SDIO->RESP1 >> 9U) & 15U) == 4U)
            mmc_ready_write = writes;
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
    if (reg == &DMA2->LIFCR)
        DMA2->LISR &= ~value;
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
        if (card_mmc && !commands.empty() && commands.back() == 8U)
            value = fifo_words == 48U   ? 8U
                    : fifo_words == 53U ? mmc_sectors
                                        : 0U;
        if (card_io && !commands.empty() && commands.back() == 53U) {
            value = 0U;
            for (unsigned byte = 0U; byte < 4U; ++byte) {
                unsigned offset = fifo_words * 4U + byte;
                if (offset < SDIO->DLEN) {
                    uint32_t address =
                        io_address + (io_increment ? offset : 0U);
                    value |= static_cast<uint32_t>(
                                 io_registers[io_function][address])
                             << (8U * byte);
                }
            }
        }
        registers->FIFO = value;
        ++fifo_words;
        *const_cast<volatile uint32_t*>(&registers->DCOUNT) =
            card_io ? (fifo_words * 4U >= registers->DLEN
                           ? 0U
                           : registers->DLEN - fifo_words * 4U)
                    : (fifo_limit - fifo_words) * 4U;
    }
    return SDIO_ReadFIFO(registers);
}
extern "C" HAL_StatusTypeDef nx_sdio_model_fifo_write(SDIO_TypeDef* registers,
                                                      uint32_t* word) {
    const auto result = SDIO_WriteFIFO(registers, word);
    if (card_enabled) {
        written_words.push_back(static_cast<uint32_t>(registers->FIFO));
        if (card_io && !commands.empty() && commands.back() == 53U) {
            for (unsigned byte = 0U; byte < 4U; ++byte) {
                unsigned offset = fifo_words * 4U + byte;
                if (offset < registers->DLEN) {
                    uint32_t address =
                        io_address + (io_increment ? offset : 0U);
                    io_registers[io_function][address] =
                        static_cast<uint8_t>(*word >> (8U * byte));
                }
            }
        }
        ++fifo_words;
        *const_cast<volatile uint32_t*>(&registers->DCOUNT) =
            card_io ? (fifo_words * 4U >= registers->DLEN
                           ? 0U
                           : registers->DLEN - fifo_words * 4U)
                    : (fifo_limit - fifo_words) * 4U;
    }
    return result;
}
