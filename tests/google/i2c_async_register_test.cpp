/**
 * \file            i2c_async_register_test.cpp
 * \brief           Real V1 asynchronous register algorithms and borrowed
 * storage \author          Nexus Team \version         1.0.0 \date 2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_v1_async.h"
#if defined(NX_I2C_TEST_STM32)
#include "stm32f407_i2c_async.h"
#elif defined(NX_I2C_TEST_GD32)
extern "C" {
#include "gd32f470_i2c_async.h"
NVIC_Type g_gd32_model_nvic;
}
#endif
#if defined(NX_I2C_TEST_STM32) || defined(NX_I2C_TEST_GD32)
#include <sys/mman.h>
#endif
#include "nexus/arch/arch.h"
#include <deque>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

namespace {
uint64_t model_time;
std::deque<uint8_t> input;
std::vector<uint32_t> address_windows;
std::vector<uint32_t> address_dma_windows;
std::vector<uint8_t> output;
unsigned data_reads;
bool hold_dma;

class I2CAsyncRegisters : public testing::Test {
  protected:
    uint32_t backing[9] = {};
    uint32_t* registers = backing;
    void* mapping = nullptr;
    uint32_t lines = 3U;
    nx_i2c_v1_state_t state = {};
    nx_i2c_v1_endpoint_t endpoint_state = {&state, 0x50};
    const nx_i2c_port_t bus = {&nx_i2c_v1_async_ops, &state};
    const nx_i2c_endpoint_t endpoint = {&nx_i2c_v1_async_endpoint_ops,
                                        &endpoint_state};
    nx_i2c_request_t request = {};
    uint8_t bytes[4] = {};
    nx_i2c_message_t message = {bytes, 1U, true};

    nx_result_t Initialize() {
#if defined(NX_I2C_TEST_STM32)
        return nx_stm32_i2c_async_initialize(&state, 5U);
#elif defined(NX_I2C_TEST_GD32)
        return nx_gd32_i2c_async_initialize(&state, 5U);
#else
        return nx_i2c_v1_initialize(&state);
#endif
    }
    void SetUp() override {
        model_time = 100U;
        data_reads = 0U;
        hold_dma = false;
        input.clear();
        output.clear();
        address_windows.clear();
        address_dma_windows.clear();
#if defined(NX_I2C_TEST_STM32) || defined(NX_I2C_TEST_GD32)
        mapping =
            mmap(reinterpret_cast<void*>(UINT32_C(0x40000000)), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(UINT32_C(0x40000000)));
        registers = reinterpret_cast<uint32_t*>(UINT32_C(0x40005400));
        state.line_mask = (1U << 6U) | (1U << 7U);
#if defined(NX_I2C_TEST_STM32)
        state.line_inputs = &GPIOB->IDR;
        GPIOB->IDR = state.line_mask;
        state.peripheral_mhz = 42U;
#else
        state.line_inputs = &GPIO_ISTAT(GPIOB);
        GPIO_ISTAT(GPIOB) = state.line_mask;
        state.peripheral_mhz = 50U;
        g_gd32_model_nvic = {};
#endif
#else
        state.line_inputs = &lines;
        state.line_mask = 3U;
        state.peripheral_mhz = 42U;
#endif
        state.registers = registers;
        ASSERT_EQ(Initialize(), NX_SUCCESS);
        nx_request_initialize(&request.base);
    }
    void TearDown() override {
        if (state.active != nullptr) {
            (void)nx_i2c_port_cancel(&bus, &request);
            if (state.dma != nullptr) {
                *state.dma->rx.control = 0U;
                *state.dma->tx.control = 0U;
            }
            registers[0] &= ~((1U << 8U) | (1U << 9U));
            registers[6] = 0U;
            nx_i2c_port_service(&bus);
        }
        EXPECT_EQ(nx_i2c_port_stop(&bus), NX_SUCCESS);
#if defined(NX_I2C_TEST_STM32) || defined(NX_I2C_TEST_GD32)
        if (mapping != MAP_FAILED) {
            EXPECT_EQ(munmap(mapping, 0x80000U), 0);
        }
#endif
    }
    void Begin() {
        ASSERT_EQ(nx_i2c_request_prepare(&request, &message, 1U, 1000U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_i2c_endpoint_submit(&endpoint, &request), NX_SUCCESS);
        registers[6] = 3U;
        registers[5] = 1U;
        nx_i2c_v1_irq(&state);
        EXPECT_EQ(output, std::vector<uint8_t>({static_cast<uint8_t>(
                              message.read ? 0xA1U : 0xA0U)}));
        registers[5] = 2U;
        nx_i2c_v1_irq(&state);
    }
    void ReleaseWire() {
        registers[0] &= ~(1U << 9U);
        registers[6] &= ~1U;
        nx_i2c_port_service(&bus);
    }
};

TEST_F(I2CAsyncRegisters, OneByteNacksBeforeAddressClearAndWaitsStop) {
    input.push_back(0x35U);
    Begin();
    ASSERT_EQ(address_windows.size(), 1U);
    EXPECT_EQ(address_windows[0] & ((1U << 10U) | (1U << 11U)), 0U);
    EXPECT_NE(registers[0] & (1U << 9U), 0U);
    registers[5] = 1U << 6U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(data_reads, 1U);
    EXPECT_EQ(bytes[0], 0x35U);
    nx_i2c_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(state.active, &request);
    ReleaseWire();
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 1U);
    EXPECT_EQ(state.active, nullptr);
    registers[5] = (1U << 6U) | 2U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(data_reads, 1U);
}

TEST_F(I2CAsyncRegisters, FinalThreeWithdrawsAckBeforeNMinusTwo) {
    message.length = 3U;
    input = {11U, 12U, 13U};
    Begin();
    EXPECT_NE(address_windows[0] & (1U << 10U), 0U);
    EXPECT_EQ(address_windows[0] & (1U << 11U), 0U);
    registers[5] = 1U << 2U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(registers[0] & (1U << 10U), 0U);
    EXPECT_EQ(registers[0] & (1U << 9U), 0U);
    EXPECT_EQ(data_reads, 1U);
    registers[5] = 1U << 2U;
    nx_i2c_v1_irq(&state);
    EXPECT_NE(registers[0] & (1U << 9U), 0U);
    EXPECT_EQ(data_reads, 3U);
    EXPECT_EQ(bytes[0], 11U);
    EXPECT_EQ(bytes[1], 12U);
    EXPECT_EQ(bytes[2], 13U);
    ReleaseWire();
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 3U);
}

TEST_F(I2CAsyncRegisters, ThreeByteAddressDisablesBufferInterruptBeforeBtf) {
    message.length = 3U;
    Begin();
    EXPECT_EQ(registers[1] & (1U << 10U), 0U);
    registers[5] = 1U << 6U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(data_reads, 0U);
}

TEST_F(I2CAsyncRegisters, TwoBytesUsePosAndNackBeforeAddressClear) {
    message.length = 2U;
    input = {0x12U, 0x13U};
    Begin();
    ASSERT_EQ(address_windows.size(), 1U);
    EXPECT_EQ(address_windows[0] & (1U << 10U), 0U);
    EXPECT_NE(address_windows[0] & (1U << 11U), 0U);
    registers[5] = 1U << 2U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(data_reads, 2U);
    EXPECT_NE(registers[0] & (1U << 9U), 0U);
    ReleaseWire();
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 2U);
}

TEST_F(I2CAsyncRegisters, FourBytesConsumesOneThenFinalOneAndTwo) {
    message.length = 4U;
    input = {1U, 2U, 3U, 4U};
    Begin();
    registers[5] = 1U << 6U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(data_reads, 1U);
    EXPECT_EQ(registers[1] & (1U << 10U), 0U);
    registers[5] = 1U << 2U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(data_reads, 2U);
    registers[5] = 1U << 2U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(data_reads, 4U);
    ReleaseWire();
    EXPECT_EQ(request.base.transferred, 4U);
}

TEST_F(I2CAsyncRegisters, TransmitTxeDoesNotConfirmWirePrefix) {
    message.length = 2U;
    message.read = false;
    bytes[0] = 4U;
    bytes[1] = 5U;
    Begin();
    registers[5] = 1U << 7U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(state.transferred, 0U);
    EXPECT_EQ(output.back(), 4U);
    registers[5] = 1U << 2U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(state.transferred, 1U);
    EXPECT_EQ(output.back(), 5U);
    registers[5] = 1U << 10U;
    nx_i2c_v1_irq(&state);
    ReleaseWire();
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_NACK);
    EXPECT_EQ(request.base.transferred, 1U);
}

TEST_F(I2CAsyncRegisters,
       CancelBeforeStartDrainsLateMasterWithoutRevokingLoan) {
    ASSERT_EQ(nx_i2c_request_prepare(&request, &message, 1U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_i2c_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    ASSERT_EQ(nx_i2c_port_cancel(&bus, &request), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    nx_i2c_port_service(&bus);
    EXPECT_EQ(state.active, &request);
    registers[0] &= ~(1U << 8U);
    registers[6] = 3U;
    nx_i2c_port_service(&bus);
    EXPECT_NE(registers[0] & (1U << 9U), 0U);
    ReleaseWire();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
}

TEST_F(I2CAsyncRegisters, ArbitrationNeverInjectsStopIntoOtherMaster) {
    Begin();
    registers[0] &= ~(1U << 9U);
    registers[6] = 2U;
    registers[5] = 1U << 9U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(registers[0] & (1U << 9U), 0U);
    nx_i2c_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_ARBITRATION);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(I2CAsyncRegisters, DrainTimeoutQuarantinesThenOnlyActualReleaseSettles) {
    Begin();
    ASSERT_EQ(nx_i2c_port_cancel(&bus, &request), NX_SUCCESS);
    model_time = 1100U;
    nx_i2c_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_EQ(nx_i2c_port_stop(&bus), NX_ERROR_BUSY);
    ReleaseWire();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(I2CAsyncRegisters, ExpiredAdmissionAndForeignIdentityHaveNoEffects) {
    ASSERT_EQ(nx_i2c_request_prepare(&request, &message, 1U, model_time),
              NX_SUCCESS);
    const auto before = std::vector<uint32_t>(registers, registers + 9U);
    EXPECT_EQ(nx_i2c_endpoint_submit(&endpoint, &request), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(std::vector<uint32_t>(registers, registers + 9U), before);
    nx_i2c_v1_state_t other = {};
    nx_i2c_port_t foreign = {&nx_i2c_v1_async_ops, &other};
    EXPECT_FALSE(nx_i2c_endpoint_on_port(&endpoint, &foreign));
    EXPECT_TRUE(nx_i2c_endpoint_on_port(&endpoint, &bus));
}

class I2CDmaRegisters : public I2CAsyncRegisters {
  protected:
    uint32_t engines[2][7] = {};
    nx_dma_memory_region_t region = {};
    nx_i2c_v1_dma_t dma = {};
    void SetUp() override {
        I2CAsyncRegisters::SetUp();
        ASSERT_EQ(nx_i2c_port_stop(&bus), NX_SUCCESS);
        region = {reinterpret_cast<uintptr_t>(bytes), sizeof(bytes),
                  NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        for (unsigned i = 0U; i < 2U; ++i) {
            nx_i2c_v1_dma_engine_t engine = {&engines[i][0],
                                             &engines[i][1],
                                             &engines[i][2],
                                             &engines[i][3],
                                             &engines[i][4],
                                             &engines[i][5],
                                             &engines[i][6],
                                             (1U << 25U) | (1U << 10U) |
                                                 (1U << 16U) | 0x16U,
                                             0U};
            if (i == 0U) {
                engine.configuration |= 1U << 6U;
                dma.tx = engine;
            } else {
                dma.rx = engine;
            }
        }
        dma.regions = &region;
        dma.region_count = 1U;
#if defined(NX_I2C_TEST_STM32)
        dma = NX_STM32_I2C1_DMA(&region, 1U);
#elif defined(NX_I2C_TEST_GD32)
        dma = NX_GD32_I2C0_DMA(&region, 1U);
#endif
        state.dma = &dma;
        ASSERT_EQ(Initialize(), NX_SUCCESS);
    }
    void Complete(bool read) {
        const unsigned i = read ? 1U : 0U;
        const nx_i2c_v1_dma_engine_t* engine = read ? &dma.rx : &dma.tx;
        (void)i;
        *engine->count = 0U;
        *const_cast<volatile uint32_t*>(engine->flags) = (1U << 5U)
                                                         << engine->flag_shift;
        nx_i2c_v1_dma_irq(&state, read);
    }
};

TEST_F(I2CDmaRegisters,
       ReceiveLastIsBeforeAddressAndStopFollowsMemoryCompletion) {
    message.length = 3U;
    Begin();
    EXPECT_NE(address_dma_windows[0] & (1U << 12U), 0U);
    EXPECT_EQ(address_dma_windows[0] & (1U << 11U), 0U);
    EXPECT_NE(registers[1] & (1U << 12U), 0U);
    EXPECT_NE(address_windows[0] & (1U << 10U), 0U);
    EXPECT_EQ(registers[0] & (1U << 9U), 0U);
    EXPECT_EQ(*dma.rx.count, 3U);
    EXPECT_EQ(*dma.rx.memory,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(bytes)));
    Complete(true);
    EXPECT_EQ(state.transferred, 3U);
    EXPECT_EQ(*dma.rx.control & 0x1FU, 0U);
    EXPECT_EQ(registers[1] & ((1U << 11U) | (1U << 12U)), 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    ReleaseWire();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 3U);
}

TEST_F(I2CDmaRegisters, TxCounterZeroStillWaitsForBtf) {
    message.read = false;
    message.length = 3U;
    Begin();
    Complete(false);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(state.transferred, 0U);
    EXPECT_EQ(registers[0] & (1U << 9U), 0U);
    registers[5] = 1U << 2U;
    nx_i2c_v1_irq(&state);
    ReleaseWire();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.transferred, 3U);
}

TEST_F(I2CDmaRegisters, SimultaneousPeripheralNackOutranksDmaCompletion) {
    message.length = 3U;
    Begin();
    registers[5] = 1U << 10U;
    Complete(true);
    ReleaseWire();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_NACK);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(I2CDmaRegisters, CounterNotZeroNeverReportsPayloadCompleted) {
    message.length = 3U;
    Begin();
    *dma.rx.count = 1U;
    *const_cast<volatile uint32_t*>(dma.rx.flags) = (1U << 5U)
                                                    << dma.rx.flag_shift;
    nx_i2c_v1_dma_irq(&state, true);
    ReleaseWire();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(I2CDmaRegisters, RefusedEngineDisableQuarantinesStorage) {
    message.length = 3U;
    Begin();
    hold_dma = true;
    Complete(true);
    ReleaseWire();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    model_time = 1100U;
    nx_i2c_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(state.active, &request);
    EXPECT_NE(*dma.rx.control & 1U, 0U);
    EXPECT_EQ(nx_i2c_port_recover(&bus), NX_ERROR_BUSY);
    hold_dma = false;
    *dma.rx.control &= ~1U;
    nx_i2c_port_service(&bus);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 0U);
}

TEST_F(I2CDmaRegisters, AllDomainsValidateBeforeAdmissionOrMmio) {
    message.length = 3U;
    region.size = 2U;
    ASSERT_EQ(nx_i2c_request_prepare(&request, &message, 1U, 1000U),
              NX_SUCCESS);
    const auto before = std::vector<uint32_t>(registers, registers + 9U);
    EXPECT_EQ(nx_i2c_endpoint_submit(&endpoint, &request), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(state.active, nullptr);
    EXPECT_EQ(*dma.rx.control, 0U);
    EXPECT_EQ(std::vector<uint32_t>(registers, registers + 9U), before);
}

TEST_F(I2CAsyncRegisters, RepeatedStartKeepsOneLoanAndResetsReceiveWindow) {
    bytes[0] = 0x10U;
    nx_i2c_message_t sequence[2] = {{bytes, 1U, false}, {bytes + 1U, 2U, true}};
    ASSERT_EQ(nx_i2c_request_prepare(&request, sequence, 2U, 1000U),
              NX_SUCCESS);
    ASSERT_EQ(nx_i2c_endpoint_submit(&endpoint, &request), NX_SUCCESS);
    registers[6] = 3U;
    registers[5] = 1U;
    nx_i2c_v1_irq(&state);
    registers[5] = 2U;
    nx_i2c_v1_irq(&state);
    registers[5] = 1U << 7U;
    nx_i2c_v1_irq(&state);
    registers[5] = 1U << 2U;
    nx_i2c_v1_irq(&state);
    EXPECT_NE(registers[0] & (1U << 8U), 0U);
    EXPECT_EQ(registers[0] & (1U << 9U), 0U);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_ACTIVE);
    registers[5] = 1U;
    nx_i2c_v1_irq(&state);
    registers[5] = 2U;
    nx_i2c_v1_irq(&state);
    ASSERT_EQ(address_windows.size(), 2U);
    EXPECT_EQ(address_windows[1] & (1U << 10U), 0U);
    EXPECT_NE(address_windows[1] & (1U << 11U), 0U);
    input = {0x45U, 0x46U};
    registers[5] = 1U << 2U;
    nx_i2c_v1_irq(&state);
    ReleaseWire();
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 3U);
    EXPECT_EQ(output, std::vector<uint8_t>({0xA0U, 0x10U, 0xA1U}));
    EXPECT_EQ(bytes[1], 0x45U);
    EXPECT_EQ(bytes[2], 0x46U);
    EXPECT_EQ(registers[0] & ((1U << 10U) | (1U << 11U)), 0U);
}

TEST_F(I2CAsyncRegisters,
       ReadWriteAliasesAndDescriptorWrapRejectBeforeMutation) {
    nx_i2c_message_t sequence[2] = {{bytes, 2U, false}, {bytes + 1U, 2U, true}};
    EXPECT_EQ(nx_i2c_request_prepare(&request, sequence, 2U, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(request.messages, nullptr);
    auto* wrap =
        reinterpret_cast<nx_i2c_message_t*>(UINTPTR_MAX & ~uintptr_t(7U));
    EXPECT_EQ(nx_i2c_request_prepare(&request, wrap, 1U, 1000U),
              NX_ERROR_INVALID);
    nx_i2c_message_t control = {reinterpret_cast<uint8_t*>(&request), 1U, true};
    EXPECT_EQ(nx_i2c_request_prepare(&request, &control, 1U, 1000U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_i2c_request_prepare(&request, &message, 9U, 1000U),
              NX_ERROR_UNSUPPORTED);
    message.length = 257U;
    EXPECT_EQ(nx_i2c_request_prepare(&request, &message, 1U, 1000U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(state.active, nullptr);
}

TEST_F(I2CAsyncRegisters, MaskedAdmissionAndAsyncPollingNeverTouchHardware) {
    ASSERT_EQ(nx_i2c_request_prepare(&request, &message, 1U, 1000U),
              NX_SUCCESS);
    const auto saved = nx_arch_irq_save();
    EXPECT_EQ(nx_i2c_endpoint_submit(&endpoint, &request), NX_ERROR_CONTEXT);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    nx_arch_irq_restore(saved);
    size_t transferred = 23U;
    EXPECT_EQ(nx_i2c_endpoint_transaction(&endpoint, &message, 1U, 1000U,
                                          &transferred),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(transferred, 23U);
    EXPECT_EQ(output.size(), 0U);
}

TEST_F(I2CAsyncRegisters, CancelRequiresExplicitLocalRecoveryBeforeReuse) {
    Begin();
    ASSERT_EQ(nx_i2c_port_cancel(&bus, &request), NX_SUCCESS);
    ReleaseWire();
    ASSERT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    ASSERT_EQ(nx_i2c_request_prepare(&request, &message, 1U, 2000U),
              NX_SUCCESS);
    registers[6] = 0U;
    EXPECT_EQ(nx_i2c_endpoint_submit(&endpoint, &request), NX_ERROR_STATE);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_READY);
    EXPECT_EQ(nx_i2c_port_recover(&bus), NX_SUCCESS);
    EXPECT_EQ(state.faulted, false);
    EXPECT_EQ(nx_i2c_endpoint_submit(&endpoint, &request), NX_SUCCESS);
}

class WakeObserver {
  public:
    MOCK_METHOD(nx_result_t, notify, ());
    static nx_result_t Callback(void* context) {
        EXPECT_FALSE(nx_arch_irq_is_masked());
        return static_cast<WakeObserver*>(context)->notify();
    }
};

TEST_F(I2CAsyncRegisters, WakeFailureCannotUnlatchCompletionOrReleaseBorrow) {
    testing::StrictMock<WakeObserver> observer;
    const nx_irq_wake_t wake = {&observer, WakeObserver::Callback, false};
    state.validate_wake = [](void*, const nx_irq_wake_t*, uint8_t) {
        return NX_SUCCESS;
    };
    ASSERT_EQ(nx_i2c_port_attach_wake(&bus, &wake, 0U), NX_SUCCESS);
    EXPECT_CALL(observer, notify())
        .Times(3)
        .WillRepeatedly(testing::Return(NX_ERROR_BUSY));
    Begin();
    input = {0x44U};
    registers[5] = 1U << 6U;
    nx_i2c_v1_irq(&state);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_DRAINING);
    EXPECT_EQ(nx_i2c_port_attach_wake(&bus, nullptr, 0U), NX_ERROR_STATE);
    ReleaseWire();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(bytes[0], 0x44U);
    EXPECT_EQ(nx_i2c_port_stop(&bus), NX_SUCCESS);
    EXPECT_EQ(state.wake, nullptr);
    registers[5] = 1U << 6U;
    nx_i2c_v1_irq(&state);
}

TEST_F(I2CDmaRegisters, PeripheralErrorDuringStopDrainCannotPublishSuccess) {
    message.length = 3U;
    Begin();
    Complete(true);
    registers[5] = 1U << 8U;
    ReleaseWire();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_IO);
    EXPECT_EQ(request.base.transferred, 3U);
}

#if defined(NX_I2C_TEST_STM32) || defined(NX_I2C_TEST_GD32)
TEST_F(I2CDmaRegisters, ColdRouteTamperingRejectsBeforeRegistersOrPublishers) {
    ASSERT_EQ(nx_i2c_port_stop(&bus), NX_SUCCESS);
    dma.rx.control = &engines[1][0];
    const auto before = std::vector<uint32_t>(registers, registers + 9U);
    EXPECT_EQ(Initialize(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(state.initialized, false);
    EXPECT_EQ(std::vector<uint32_t>(registers, registers + 9U), before);
}
#endif

#if defined(NX_I2C_TEST_GD32)
TEST_F(I2CDmaRegisters, UnsafeDmaPublisherPreventsKernelWakeBinding) {
    testing::StrictMock<WakeObserver> observer;
    const nx_irq_wake_t wake = {&observer, WakeObserver::Callback, true};
    NVIC_SetPriority(DMA0_Channel6_IRQn, 4U);
    EXPECT_EQ(nx_i2c_port_attach_wake(&bus, &wake, 5U), NX_ERROR_PERMISSION);
    EXPECT_EQ(state.wake, nullptr);
    NVIC_SetPriority(DMA0_Channel6_IRQn, 5U);
    EXPECT_EQ(nx_i2c_port_attach_wake(&bus, &wake, 5U), NX_SUCCESS);
    EXPECT_EQ(nx_i2c_port_stop(&bus), NX_SUCCESS);
    EXPECT_EQ(NVIC->ISER[0] & ((1U << 11U) | (1U << 17U) | (1U << 31U)), 0U);
    EXPECT_EQ(NVIC->ISER[1] & 1U, 0U);
}
#endif

} /* namespace */

extern "C" nx_time_us_t nx_time_now_us(void) {
    return model_time;
}
extern "C" uint8_t nx_i2c_v1_model_read(void* raw) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(raw);
    ++data_reads;
    EXPECT_FALSE(input.empty());
    if (input.empty()) {
        return 0U;
    }
    const auto value = input.front();
    input.pop_front();
    if (input.empty()) {
        state->registers[5] &= ~((1U << 6U) | (1U << 2U));
    }
    return value;
}
extern "C" void nx_i2c_v1_model_write(void* raw, uint32_t value) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(raw);
    output.push_back(static_cast<uint8_t>(value));
    state->registers[5] &= ~(1U | (1U << 2U) | (1U << 7U));
}
extern "C" void nx_i2c_v1_model_address_cleared(void* raw) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(raw);
    address_windows.push_back(static_cast<uint32_t>(state->registers[0]));
    address_dma_windows.push_back(static_cast<uint32_t>(state->registers[1]));
    state->registers[5] &= ~2U;
}
extern "C" void nx_i2c_v1_model_disable(volatile uint32_t* control,
                                        uint32_t mask) {
    *control &= hold_dma ? ~(mask & ~1U) : ~mask;
}
