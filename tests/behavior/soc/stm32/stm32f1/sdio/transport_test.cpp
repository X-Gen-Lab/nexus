/**
 * \file            transport_test.cpp
 * \brief           Exercise the actual F1 CPSM, PIO RX/TX and withdrawal code.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/sdio/instance.h"

#include "nexus/arch/arch.h"
#include "system/platform.h"
#include <gtest/gtest.h>
#include <vector>

RCC_TypeDef nx_sdio_rcc;
SDIO_TypeDef nx_sdio_registers;
GPIO_TypeDef nx_sdio_gpio_c;
GPIO_TypeDef nx_sdio_gpio_d;

namespace {
constexpr uint32_t ready_clock = SDIO_CLKCR_CLKEN | 20U;
constexpr unsigned words_per_sector = 128U;
uint32_t pending_flags;
uint32_t pending_response;
uint32_t final_data_flags;
bool wrong_index;
bool hold_command;
bool hold_data;
bool freeze_time;
bool system_live;
bool blocked_icr;
uint32_t generated_words;
uint32_t words_read;
std::vector<uint32_t> output_words;
uint32_t writer_last_command;
uint32_t send_result_flags;
unsigned fail_after_words;
uint32_t writes;
nx_time_us_t now;
constexpr uint32_t detect_pin = 1U << 13U;
constexpr uint32_t rca_address = 0x45670000U;
constexpr uint32_t voltage = 0x00300000U;
constexpr uint32_t capacity_flag = 1U << 30U;
constexpr uint32_t ready_flag = 1U << 31U;
constexpr uint32_t transfer_ready = (4U << 9U) | (1U << 8U);
struct Command {
    uint32_t index;
    uint32_t arg;
    uint32_t flags;
    uint32_t response;
    uint32_t response2;
    uint32_t response3;
    uint32_t response4 = 0U;
};
std::vector<Command> program;
size_t command_cursor;
bool scripted;
bool lose_detect_after_status;
nx_time_us_t detect_clock_start;
nx_time_us_t reset_command_at;

class F1Sdio : public testing::Test {
  protected:
    nx_stm32f1_sdio_t host{};

    void SetUp() override {
        nx_sdio_rcc = {};
        nx_sdio_registers = {};
        nx_sdio_gpio_c = {};
        nx_sdio_gpio_d = {};
        GPIOC->CRH = 0x44444444U;
        GPIOD->CRL = 0x44444444U;
        program.clear();
        command_cursor = 0U;
        scripted = lose_detect_after_status = false;
        detect_clock_start = reset_command_at = 0U;
        RCC->AHBENR = RCC_AHBENR_DMA2EN;
        pending_flags = 0U;
        pending_response = 0U;
        final_data_flags = SDIO_STA_DBCKEND;
        wrong_index = hold_command = hold_data = freeze_time = blocked_icr =
            false;
        system_live = true;
        writes = generated_words = words_read = writer_last_command = 0U;
        send_result_flags = SDIO_STA_DBCKEND;
        fail_after_words = 0U;
        output_words.clear();
        now = 1U;
    }
};

TEST_F(F1Sdio, OwnsOneHostAndInvokesNoInterruptOrDma) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    EXPECT_EQ(SDIO->CLKCR, ready_clock);
    EXPECT_EQ(SDIO->MASK, 0U);
    EXPECT_EQ(SDIO->DCTRL, 0U);
    nx_stm32f1_sdio_t other{};
    const uint32_t before = writes;
    EXPECT_EQ(nx_stm32f1_sdio_open(&other, 100U), NX_ERROR_BUSY);
    EXPECT_EQ(writes, before);
    ASSERT_EQ(nx_stm32f1_sdio_close(&host, 100U), NX_SUCCESS);
    EXPECT_EQ(SDIO->POWER, 0U);
    EXPECT_EQ(SDIO->CLKCR, 0U);
    EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA2EN);
}

TEST_F(F1Sdio, RejectsForeignControllerStateAndReleasesOnlyTheProbeGate) {
    SDIO->MASK = SDIO_MASK_CMDRENDIE;
    EXPECT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_ERROR_BUSY);
    EXPECT_EQ(SDIO->MASK, SDIO_MASK_CMDRENDIE);
    EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA2EN);
    EXPECT_EQ(writes, 2U);
}

TEST_F(F1Sdio, SendsCommandsAndChecksIndexCrcAndOcrSeparately) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    pending_flags = SDIO_STA_CMDSENT;
    ASSERT_EQ(nx_stm32f1_sdio_command(&host, 0U, 0U, NX_STM32F1_SDIO_NONE,
                                      nullptr, 100U),
              NX_SUCCESS);
    pending_flags = SDIO_STA_CMDREND;
    pending_response = 0x1aaU;
    nx_stm32f1_sdio_reply_t reply{};
    ASSERT_EQ(nx_stm32f1_sdio_command(&host, 8U, 0x1aaU, NX_STM32F1_SDIO_SHORT,
                                      &reply, 100U),
              NX_SUCCESS);
    EXPECT_EQ(reply.words[0], 0x1aaU);
    EXPECT_EQ(SDIO->ARG, 0x1aaU);
    wrong_index = true;
    EXPECT_EQ(nx_stm32f1_sdio_command(&host, 8U, 0x1aaU, NX_STM32F1_SDIO_SHORT,
                                      &reply, 100U),
              NX_ERROR_IO);
    wrong_index = false;
    pending_flags = SDIO_STA_CCRCFAIL;
    pending_response = 0x80ff8000U;
    EXPECT_EQ(nx_stm32f1_sdio_command(&host, 8U, 0x1aaU, NX_STM32F1_SDIO_SHORT,
                                      &reply, 100U),
              NX_ERROR_IO);
    EXPECT_EQ(reply.words[0], 0x1aaU);
    ASSERT_EQ(nx_stm32f1_sdio_command(&host, 41U, 0U, NX_STM32F1_SDIO_OCR,
                                      &reply, 100U),
              NX_SUCCESS);
    EXPECT_EQ(reply.words[0], pending_response);
    pending_flags = SDIO_STA_CMDREND;
    SDIO->RESP2 = 2U;
    SDIO->RESP3 = 3U;
    SDIO->RESP4 = 4U;
    EXPECT_EQ(nx_stm32f1_sdio_command(&host, 2U, 0U, NX_STM32F1_SDIO_LONG,
                                      &reply, 100U),
              NX_SUCCESS);
    EXPECT_EQ(reply.words[3], 4U);
}

TEST_F(F1Sdio, ReceiverRequiresExactlyOneWholeSectorAndRetiresDataPath) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    generated_words = words_per_sector;
    pending_flags = SDIO_STA_CMDREND;
    uint32_t output[words_per_sector]{};
    ASSERT_EQ(nx_stm32f1_sdio_read_sector(&host, 1000U, output,
                                          words_per_sector, 500U),
              NX_SUCCESS);
    for (unsigned i = 0; i < words_per_sector; ++i)
        EXPECT_EQ(output[i], i + 1U);
    EXPECT_EQ(SDIO->ARG, 1000U);
    EXPECT_EQ(SDIO->DCTRL, 0U);
    EXPECT_EQ(SDIO->DCOUNT, 0U);
    EXPECT_EQ(SDIO->MASK, 0U);
    EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA2EN | RCC_AHBENR_SDIOEN);
}

TEST_F(F1Sdio, OversizedAndShortReceiveCannotSucceed) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    pending_flags = SDIO_STA_CMDREND;
    generated_words = words_per_sector + 1U;
    uint32_t output[words_per_sector + 1U]{};
    output[words_per_sector] = 0xcafebabeU;
    EXPECT_EQ(
        nx_stm32f1_sdio_read_sector(&host, 0U, output, words_per_sector, 500U),
        NX_ERROR_IO);
    EXPECT_EQ(output[words_per_sector], 0xcafebabeU);
    EXPECT_EQ(words_read, words_per_sector);
    ASSERT_EQ(nx_stm32f1_sdio_close(&host, 800U), NX_SUCCESS);
    EXPECT_EQ(words_read, words_per_sector + 1U);
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 800U), NX_SUCCESS);
    generated_words = words_per_sector - 1U;
    EXPECT_EQ(
        nx_stm32f1_sdio_read_sector(&host, 0U, output, words_per_sector, 1100U),
        NX_ERROR_IO);
}

TEST_F(F1Sdio, FailedWithdrawalRetainsClockUntilRetry) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    hold_command = true;
    EXPECT_EQ(nx_stm32f1_sdio_command(&host, 0U, 0U, NX_STM32F1_SDIO_NONE,
                                      nullptr, 35U),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_stm32f1_sdio_close(&host, 65U), NX_ERROR_TIMEOUT);
    EXPECT_NE(RCC->AHBENR & RCC_AHBENR_SDIOEN, 0U);
    EXPECT_EQ(SDIO->CLKCR, ready_clock);
    EXPECT_EQ(nx_stm32f1_sdio_open(&host, 150U), NX_ERROR_STATE);
    hold_command = false;
    ASSERT_EQ(nx_stm32f1_sdio_close(&host, 150U), NX_SUCCESS);
    EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA2EN);
}

TEST_F(F1Sdio, ADeadClockStillHasABoundedPollBudget) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    freeze_time = true;
    const uint32_t before = writes;
    EXPECT_EQ(nx_stm32f1_sdio_command(&host, 0U, 0U, NX_STM32F1_SDIO_NONE,
                                      nullptr, 1000U),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(writes, before);
}

TEST_F(F1Sdio, EndOfDataWithCrcErrorIsNotSuccess) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    pending_flags = SDIO_STA_CMDREND;
    generated_words = words_per_sector;
    final_data_flags = SDIO_STA_DCRCFAIL;
    uint32_t output[words_per_sector]{};
    EXPECT_EQ(
        nx_stm32f1_sdio_read_sector(&host, 0U, output, words_per_sector, 500U),
        NX_ERROR_IO);
    EXPECT_EQ(words_read, words_per_sector);
    EXPECT_EQ(
        nx_stm32f1_sdio_read_sector(&host, 0U, output, words_per_sector, 600U),
        NX_ERROR_STATE);
    EXPECT_EQ(nx_stm32f1_sdio_close(&host, 700U), NX_SUCCESS);
}

TEST_F(F1Sdio, CounterCompleteWithoutBlockCrcProofCannotSucceed) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    pending_flags = SDIO_STA_CMDREND;
    generated_words = words_per_sector;
    final_data_flags = 0U;
    uint32_t output[words_per_sector]{};
    EXPECT_EQ(
        nx_stm32f1_sdio_read_sector(&host, 0U, output, words_per_sector, 500U),
        NX_ERROR_TIMEOUT);
    EXPECT_EQ(words_read, words_per_sector);
    EXPECT_NE(RCC->AHBENR & RCC_AHBENR_SDIOEN, 0U);
    ASSERT_EQ(nx_stm32f1_sdio_close(&host, 700U), NX_SUCCESS);
}

TEST_F(F1Sdio, InvalidBufferAndUnexpectedClockWithdrawalAreSideEffectFree) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    uint32_t output[words_per_sector]{};
    const unsigned before = writes;
    EXPECT_EQ(nx_stm32f1_sdio_read_sector(&host, 0U, output,
                                          words_per_sector - 1U, 500U),
              NX_ERROR_INVALID);
    RCC->AHBENR = RCC_AHBENR_DMA2EN;
    EXPECT_EQ(
        nx_stm32f1_sdio_read_sector(&host, 0U, output, words_per_sector, 500U),
        NX_ERROR_IO);
    EXPECT_EQ(writes, before);
}

TEST_F(F1Sdio, StuckW1cStatusCannotAcknowledgeANewCommand) {
    ASSERT_EQ(nx_stm32f1_sdio_open(&host, 100U), NX_SUCCESS);
    SDIO->STA = SDIO_STA_CMDREND;
    SDIO->RESPCMD = 8U;
    SDIO->RESP1 = 0x123U;
    blocked_icr = true;
    nx_stm32f1_sdio_reply_t reply{};
    EXPECT_EQ(nx_stm32f1_sdio_command(&host, 8U, 0x1aaU, NX_STM32F1_SDIO_SHORT,
                                      &reply, 100U),
              NX_ERROR_IO);
    EXPECT_EQ(SDIO->ARG, 0U);
    EXPECT_EQ(reply.words[0], 0U);
}
class F1SdBlock : public F1Sdio {
  protected:
    nx_stm32f1_sdio_block_t block{};
    const nx_block_port_t port{&nx_stm32f1_sdio_block_ops, &block};
    const nx_block_port_t writable{&nx_stm32f1_sdio_block_write_ops, &block};
    alignas(uint32_t) uint8_t output[512]{};

    void prepare() {
        ASSERT_EQ(nx_stm32f1_sdio_block_prepare(&block), NX_SUCCESS);
        EXPECT_EQ(nx_block_port_state(&port), NX_BLOCK_STOPPED);
        EXPECT_EQ(nx_block_port_geometry(&port), nullptr);
        EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA2EN);
        EXPECT_EQ(SDIO->CLKCR, 0U);
        EXPECT_EQ(SDIO->CMD, 0U);
        scripted = true;
    }

    void init(uint32_t echo = 0x1AAU,
              uint32_t ocr = ready_flag | capacity_flag | voltage,
              uint32_t csd = 0x40000000U) {
        program = {
            {0U, 0U, SDIO_STA_CMDSENT, 0U, 0U, 0U},
            {8U, 0x1AAU, SDIO_STA_CMDREND, echo, 0U, 0U},
            {55U, 0U, SDIO_STA_CMDREND, 1U << 5U, 0U, 0U},
            {41U, capacity_flag | voltage, SDIO_STA_CCRCFAIL, 0U, 0U, 0U},
            {55U, 0U, SDIO_STA_CMDREND, 1U << 5U, 0U, 0U},
            {41U, capacity_flag | voltage, SDIO_STA_CCRCFAIL, ocr, 0U, 0U},
            {2U, 0U, SDIO_STA_CMDREND, 0x01ee0000U, 0U, 0U},
            {3U, 0U, SDIO_STA_CMDREND, rca_address, 0U, 0U},
            {9U, rca_address, SDIO_STA_CMDREND, csd, 0x00090001U, 0x23450000U},
            {7U, rca_address, SDIO_STA_CMDREND, 0U, 0U, 0U},
            {13U, rca_address, SDIO_STA_CMDREND, transfer_ready, 0U, 0U}};
    }

    void read_commands(uint32_t lba, uint32_t status = transfer_ready) {
        program.push_back({17U, lba, SDIO_STA_CMDREND, transfer_ready, 0U, 0U});
        program.push_back({13U, rca_address, SDIO_STA_CMDREND, status, 0U, 0U});
        generated_words = words_per_sector;
    }

    void init_write(uint32_t wp = 0U) {
        ASSERT_EQ(nx_stm32f1_sdio_block_prepare_write(&block), NX_SUCCESS);
        scripted = true;
        init();
        /* SDHC C_SIZE=0x1fff, CCC class 4 and WRITE_BL_LEN=9. */
        program[8].response2 = (1U << 24U) | (9U << 16U);
        program[8].response3 = 0x1fff0000U;
        program[8].response4 = (9U << 22U) | wp;
        ASSERT_EQ(nx_block_port_activate(&writable, 5000U), NX_SUCCESS);
    }

    void write_command(uint32_t lba, uint32_t status = transfer_ready) {
        program.push_back({24U, lba, SDIO_STA_CMDREND, status, 0U, 0U});
    }

    void ready_command(uint32_t status) {
        program.push_back({13U, rca_address, SDIO_STA_CMDREND, status, 0U, 0U});
    }

    void stop_commands() {
        program.push_back(
            {13U, rca_address, SDIO_STA_CMDREND, transfer_ready, 0U, 0U});
        program.push_back({7U, 0U, SDIO_STA_CMDSENT, 0U, 0U, 0U});
        program.push_back({13U, rca_address, SDIO_STA_CMDREND,
                           (3U << 9U) | (1U << 8U), 0U, 0U});
    }
};

TEST_F(F1SdBlock, NegotiatesV2AndReturnsOneConfirmedReadOnlySector) {
    prepare();
    init();
    ASSERT_EQ(nx_block_port_activate(&port, 5000U), NX_SUCCESS);
    EXPECT_GE(reset_command_at - detect_clock_start, 2001U);
    ASSERT_NE(nx_block_port_geometry(&port), nullptr);
    const nx_block_geometry_t* shape = nx_block_port_geometry(&port);
    EXPECT_EQ(shape->block_count, (0x12345ULL + 1ULL) * 1024ULL);
    EXPECT_EQ(shape->block_bytes, 512U);
    EXPECT_EQ(shape->buffer_alignment, 4U);
    EXPECT_FALSE(shape->writable);
    const unsigned before = writes;
    EXPECT_EQ(nx_block_port_read(&port, shape->block_count, output,
                                 sizeof(output), 1U, 6000U)
                  .result,
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_block_port_read(&port, 0U, output, sizeof(output), 2U, 6000U).result,
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_block_port_write(&port, 0U, output, sizeof(output), 1U, 6000U)
                  .result,
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(writes, before);

    read_commands(19U);
    const auto result =
        nx_block_port_read(&port, 19U, output, sizeof(output), 1U, 7000U);
    EXPECT_EQ(result.result, NX_SUCCESS);
    EXPECT_EQ(result.completed_blocks, 1U);
    EXPECT_EQ(output[0], 1U);
    EXPECT_EQ(output[4], 2U);
    EXPECT_EQ(output[508], 128U);
    EXPECT_EQ(SDIO->MASK, 0U);
    EXPECT_EQ(SDIO->DCTRL, 0U);
    EXPECT_EQ(GPIOD->CRL & 0xf00U, 0xa00U);
    EXPECT_EQ(GPIOC->CRH & 0x00ff000fU, 0x004a000aU);

    stop_commands();
    ASSERT_EQ(nx_block_port_stop(&port, 9000U), NX_SUCCESS);
    EXPECT_EQ(command_cursor, program.size());
    EXPECT_EQ(nx_block_port_geometry(&port), nullptr);
    EXPECT_EQ(SDIO->CLKCR | SDIO->POWER, 0U);
    EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA2EN);
    ASSERT_EQ(nx_stm32f1_sdio_block_close(&block), NX_SUCCESS);
    EXPECT_EQ(GPIOD->CRL, 0x44444444U);
    EXPECT_EQ(GPIOC->CRH, 0x44444444U);
    EXPECT_EQ(RCC->APB2ENR, 0U);
}

TEST_F(F1SdBlock, Cmd24ConfirmsOnlyAfterAcceptedBlockAndProgrammingReady) {
    ASSERT_NO_FATAL_FAILURE(init_write());
    ASSERT_NE(nx_block_port_geometry(&writable), nullptr);
    ASSERT_TRUE(nx_block_port_geometry(&writable)->writable);
    write_command(42U);
    ready_command(7U << 9U);
    ready_command(transfer_ready);
    output[0] = 0x11U;
    output[1] = 0x22U;
    output[2] = 0x33U;
    output[3] = 0x44U;
    output[508] = 0x5aU;
    const auto done =
        nx_block_port_write(&writable, 42U, output, sizeof(output), 1U, 7000U);
    EXPECT_EQ(done.result, NX_SUCCESS);
    EXPECT_EQ(done.completed_blocks, 1U);
    EXPECT_EQ(command_cursor, program.size());
    ASSERT_EQ(output_words.size(), words_per_sector);
    EXPECT_EQ(output_words.front(), 0x44332211U);
    EXPECT_EQ(output_words.back(), 0x5aU);
    EXPECT_EQ(SDIO->DCTRL, 0U);
    EXPECT_EQ(SDIO->MASK, 0U);
}

TEST_F(F1SdBlock, WriteProtectionRefusesBeforeAnyCommand) {
    ASSERT_NO_FATAL_FAILURE(init_write(1U << 12U));
    const auto* geometry = nx_block_port_geometry(&writable);
    ASSERT_NE(geometry, nullptr);
    EXPECT_FALSE(geometry->writable);
    const unsigned before = writes;
    EXPECT_EQ(
        nx_block_port_write(&writable, 0U, output, sizeof(output), 1U, 7000U)
            .result,
        NX_ERROR_PERMISSION);
    EXPECT_EQ(writes, before);
    EXPECT_TRUE(output_words.empty());
    EXPECT_EQ(nx_block_port_state(&writable), NX_BLOCK_READY);
}

TEST_F(F1SdBlock, UnderrunDoesNotReplayOrConfirmTheSector) {
    ASSERT_NO_FATAL_FAILURE(init_write());
    const uint64_t outside = nx_block_port_geometry(&writable)->block_count;
    const unsigned untouched = writes;
    EXPECT_EQ(nx_block_port_write(&writable, outside, output, sizeof(output),
                                  1U, 7000U)
                  .result,
              NX_ERROR_INVALID);
    EXPECT_EQ(writes, untouched);
    write_command(2U);
    fail_after_words = 12U;
    const auto failed =
        nx_block_port_write(&writable, 2U, output, sizeof(output), 1U, 7000U);
    EXPECT_EQ(failed.result, NX_ERROR_IO);
    EXPECT_EQ(failed.completed_blocks, 0U);
    EXPECT_EQ(output_words.size(), 12U);
    EXPECT_EQ(nx_block_port_state(&writable), NX_BLOCK_UNCERTAIN);
    EXPECT_EQ(
        nx_block_port_write(&writable, 2U, output, sizeof(output), 1U, 9000U)
            .result,
        NX_ERROR_STATE);
    EXPECT_EQ(command_cursor, program.size());
}

TEST_F(F1SdBlock, DcountAloneCannotConfirmRejectedCardData) {
    ASSERT_NO_FATAL_FAILURE(init_write());
    write_command(7U);
    send_result_flags = SDIO_STA_DCRCFAIL;
    const auto failed =
        nx_block_port_write(&writable, 7U, output, sizeof(output), 1U, 7000U);
    EXPECT_EQ(failed.result, NX_ERROR_IO);
    EXPECT_EQ(failed.completed_blocks, 0U);
    EXPECT_EQ(output_words.size(), words_per_sector);
    EXPECT_EQ(command_cursor, program.size());
    EXPECT_EQ(nx_block_port_state(&writable), NX_BLOCK_UNCERTAIN);
}

TEST_F(F1SdBlock, RejectsLegacyCapacityAndNeverSilentlyReprobes) {
    prepare();
    init(0x1ABU);
    EXPECT_EQ(nx_block_port_activate(&port, 5000U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(command_cursor, 2U);
    EXPECT_EQ(nx_block_port_geometry(&port), nullptr);
    EXPECT_EQ(nx_block_port_state(&port), NX_BLOCK_UNCERTAIN);
    EXPECT_EQ(nx_block_port_activate(&port, 6000U), NX_ERROR_STATE);
    ASSERT_EQ(nx_block_port_stop(&port, 7000U), NX_SUCCESS);
    command_cursor = 0U;
    init(0x1AAU, ready_flag | voltage);
    EXPECT_EQ(nx_block_port_activate(&port, 12000U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(command_cursor, 6U);
    EXPECT_EQ(nx_block_port_geometry(&port), nullptr);
    EXPECT_EQ(nx_block_port_state(&port), NX_BLOCK_UNCERTAIN);
}

TEST_F(F1SdBlock, KeepsGeometryPrivateUntilReadyAndRetainsStoppedPadFailure) {
    prepare();
    init(0x1AAU, ready_flag | capacity_flag | voltage, 0U);
    EXPECT_EQ(nx_block_port_activate(&port, 5000U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(command_cursor, 9U);
    EXPECT_EQ(nx_block_port_geometry(&port), nullptr);
    hold_command = true;
    EXPECT_EQ(nx_block_port_stop(&port, 5500U), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_block_port_state(&port), NX_BLOCK_UNCERTAIN);
    EXPECT_NE(RCC->AHBENR & RCC_AHBENR_SDIOEN, 0U);
    EXPECT_NE(RCC->APB2ENR & RCC_APB2ENR_IOPCEN, 0U);
    EXPECT_EQ(nx_block_port_media_removed(&port), NX_ERROR_STATE);
    GPIOC->IDR = detect_pin;
    ASSERT_EQ(nx_block_port_media_removed(&port), NX_SUCCESS);
    hold_command = false;
    ASSERT_EQ(nx_block_port_stop(&port, 8000U), NX_SUCCESS);
    ASSERT_EQ(nx_stm32f1_sdio_block_close(&block), NX_SUCCESS);
    EXPECT_EQ(RCC->AHBENR, RCC_AHBENR_DMA2EN);
}

TEST_F(F1SdBlock, CardDisappearanceBeforeActivationDoesNotPublishGeometry) {
    prepare();
    init();
    lose_detect_after_status = true;
    EXPECT_EQ(nx_block_port_activate(&port, 5000U), NX_ERROR_IO);
    EXPECT_EQ(nx_block_port_state(&port), NX_BLOCK_UNCERTAIN);
    EXPECT_EQ(nx_block_port_geometry(&port), nullptr);
    ASSERT_EQ(nx_block_port_media_removed(&port), NX_SUCCESS);
    ASSERT_EQ(nx_block_port_stop(&port, 5500U), NX_SUCCESS);
}

TEST_F(F1SdBlock, RejectsInheritedAlternateOutputsOnUnusedDataPins) {
    for (const unsigned shift : {4U, 8U}) {
        GPIOC->CRH = (0x44444444U & ~(0xfU << shift)) | (0xaU << shift);
        const uint32_t original = GPIOC->CRH;
        EXPECT_EQ(nx_stm32f1_sdio_block_prepare(&block), NX_ERROR_BUSY);
        EXPECT_EQ(GPIOC->CRH, original);
        EXPECT_EQ(SDIO->CLKCR, 0U);
        ASSERT_EQ(nx_stm32f1_sdio_block_close(&block), NX_SUCCESS);
        EXPECT_EQ(RCC->APB2ENR, 0U);
    }
}

TEST_F(F1SdBlock, RequiresPhysicalPresenceAndCardReadinessEvidence) {
    prepare();
    GPIOC->IDR = detect_pin;
    const unsigned before = writes;
    EXPECT_EQ(nx_block_port_activate(&port, 1000U), NX_ERROR_STATE);
    EXPECT_EQ(writes, before);
    GPIOC->IDR = 0U;
    init();
    ASSERT_EQ(nx_block_port_activate(&port, 5000U), NX_SUCCESS);
    read_commands(2U, transfer_ready | (1U << 19U));
    EXPECT_EQ(nx_block_port_read(&port, 2U, output, sizeof(output), 1U, 6000U)
                  .completed_blocks,
              0U);
    EXPECT_EQ(nx_block_port_state(&port), NX_BLOCK_UNCERTAIN);
    EXPECT_EQ(
        nx_block_port_read(&port, 2U, output, sizeof(output), 1U, 7000U).result,
        NX_ERROR_STATE);
    program.push_back(
        {13U, rca_address, SDIO_STA_CMDREND, transfer_ready, 0U, 0U});
    ASSERT_EQ(nx_block_port_sync(&port, 7000U), NX_SUCCESS);
    EXPECT_EQ(nx_block_port_state(&port), NX_BLOCK_READY);
    GPIOC->IDR = detect_pin;
    ASSERT_EQ(nx_block_port_media_removed(&port), NX_SUCCESS);
    EXPECT_EQ(nx_block_port_geometry(&port), nullptr);
    EXPECT_EQ(nx_block_port_activate(&port, 7000U), NX_ERROR_STATE);
    ASSERT_EQ(nx_block_port_stop(&port, 7000U), NX_SUCCESS);
    EXPECT_EQ(command_cursor, program.size());
}
} /* namespace */

extern "C" bool nx_stm32f1_system_live(void) {
    return system_live;
}

extern "C" nx_time_us_t nx_time_now_us(void) {
    return freeze_time ? now : now++;
}

extern "C" void nx_sdio_model_write(volatile uint32_t* reg, uint32_t value) {
    ++writes;
    if (reg == &SDIO->CLKCR && (value & SDIO_CLKCR_CLKEN) != 0U)
        detect_clock_start = now;
    if (reg == &SDIO->ICR && !blocked_icr)
        SDIO->STA &= ~value;
    if (reg == &SDIO->CMD && (value & SDIO_CMD_CPSMEN) != 0U) {
        if (hold_command)
            SDIO->STA |= SDIO_STA_CMDACT;
        else {
            const uint32_t command = value & SDIO_CMD_CMDINDEX;
            if (scripted) {
                ASSERT_LT(command_cursor, program.size());
                const auto& next = program[command_cursor++];
                EXPECT_EQ(command, next.index);
                EXPECT_EQ(SDIO->ARG, next.arg);
                if (command == 0U)
                    reset_command_at = now;
                if (command == 13U && lose_detect_after_status)
                    GPIOC->IDR = detect_pin;
                pending_flags = next.flags;
                pending_response = next.response;
                SDIO->RESP2 = next.response2;
                SDIO->RESP3 = next.response3;
                SDIO->RESP4 = next.response4;
            }
            writer_last_command = command;
            if (command == 24U) {
                EXPECT_EQ(SDIO->DCTRL, 0U);
                EXPECT_EQ(SDIO->DLEN, 512U);
            }
            SDIO->STA |= pending_flags;
            SDIO->RESPCMD = command + (wrong_index ? 1U : 0U);
            SDIO->RESP1 = pending_response;
            if (command == 17U) {
                words_read = 0U;
                SDIO->DCOUNT = 512U;
                SDIO->STA |= SDIO_STA_RXACT;
                if (generated_words != 0U)
                    SDIO->STA |= SDIO_STA_RXDAVL;
            }
        }
    } else if (reg == &SDIO->CMD && !hold_command) {
        SDIO->STA &= ~static_cast<uint32_t>(SDIO_STA_CMDACT);
    }
    if (reg == &SDIO->DCTRL && (value & SDIO_DCTRL_DTEN) != 0U &&
        (value & SDIO_DCTRL_DTDIR) == 0U) {
        EXPECT_EQ(writer_last_command, 24U);
        EXPECT_EQ(value & SDIO_DCTRL_DMAEN, 0U);
        SDIO->DCOUNT = 512U;
        SDIO->STA |= SDIO_STA_TXFIFOHE | SDIO_STA_TXACT;
    }
    if (reg == &SDIO->FIFO) {
        EXPECT_EQ(writer_last_command, 24U);
        EXPECT_EQ(SDIO->DCTRL & (SDIO_DCTRL_DTEN | SDIO_DCTRL_DTDIR),
                  SDIO_DCTRL_DTEN);
        output_words.push_back(value);
        ASSERT_GE(SDIO->DCOUNT, 4U);
        SDIO->DCOUNT -= 4U;
        if ((fail_after_words != 0U &&
             output_words.size() == fail_after_words) ||
            SDIO->DCOUNT == 0U) {
            SDIO->STA &= ~static_cast<uint32_t>(
                SDIO_STA_TXACT | SDIO_STA_TXDAVL | SDIO_STA_TXFIFOHE);
            SDIO->STA |= SDIO->DCOUNT == 0U
                             ? static_cast<uint32_t>(SDIO_STA_DATAEND) |
                                   send_result_flags
                             : static_cast<uint32_t>(SDIO_STA_TXUNDERR);
        }
    }
    if (reg == &SDIO->DCTRL && value == 0U && !hold_data)
        SDIO->STA &=
            ~static_cast<uint32_t>(SDIO_STA_RXACT | SDIO_STA_TXACT |
                                   SDIO_STA_TXDAVL | SDIO_STA_TXFIFOHE);
    *reg = value;
}

extern "C" uint32_t nx_sdio_model_fifo(void) {
    ++words_read;
    if (SDIO->DCOUNT >= 4U)
        SDIO->DCOUNT -= 4U;
    if (words_read >= generated_words) {
        SDIO->STA &= ~static_cast<uint32_t>(SDIO_STA_RXDAVL | SDIO_STA_RXACT);
        SDIO->STA |= SDIO_STA_DATAEND | final_data_flags;
    }
    return words_read;
}

extern "C" void nx_sdio_model_bsrr(GPIO_TypeDef* gpio, uint32_t value) {
    gpio->ODR = (gpio->ODR | (value & UINT16_MAX)) & ~(value >> 16U);
}
