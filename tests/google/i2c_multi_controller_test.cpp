/**
 * \file            i2c_multi_controller_test.cpp
 * \brief           Two real asynchronous controllers and exact IRQ/DMA
 * isolation \author          Nexus Team \version         1.0.0 \date 2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#if defined(NX_I2C_MULTI_STM32)
#include "stm32f407_i2c_async.h"
#define PRIMARY_REGS   ((volatile uint32_t*)I2C1)
#define SECONDARY_REGS ((volatile uint32_t*)I2C2)
#define LINE_INPUT     GPIOB->IDR
#define PRIMARY_DMA    NX_STM32_I2C1_DMA
#define SECONDARY_DMA  NX_STM32_I2C2_DMA
#define INITIALIZE     nx_stm32_i2c_async_initialize
#define EV0            I2C1_EV_IRQn
#define ER0            I2C1_ER_IRQn
#define EV1            I2C2_EV_IRQn
#define ER1            I2C2_ER_IRQn
#define RX0            DMA1_Stream0_IRQn
#define TX0            DMA1_Stream6_IRQn
#define RX1            DMA1_Stream2_IRQn
#define TX1            DMA1_Stream7_IRQn
#define MHZ            42U
#else
#include "gd32f470_i2c_async.h"
#define PRIMARY_REGS   ((volatile uint32_t*)(uintptr_t)I2C0)
#define SECONDARY_REGS ((volatile uint32_t*)(uintptr_t)I2C1)
#define LINE_INPUT     GPIO_ISTAT(GPIOB)
#define PRIMARY_DMA    NX_GD32_I2C0_DMA
#define SECONDARY_DMA  NX_GD32_I2C1_DMA
#define INITIALIZE     nx_gd32_i2c_async_initialize
#define EV0            I2C0_EV_IRQn
#define ER0            I2C0_ER_IRQn
#define EV1            I2C1_EV_IRQn
#define ER1            I2C1_ER_IRQn
#define RX0            DMA0_Channel0_IRQn
#define TX0            DMA0_Channel6_IRQn
#define RX1            DMA0_Channel2_IRQn
#define TX1            DMA0_Channel7_IRQn
#define MHZ            50U
#endif
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

extern "C" {
NVIC_Type g_gd32_model_nvic;
}
namespace {
uint32_t mask;
bool isr;
nx_time_us_t now;
volatile uint32_t* held_engine;
testing::MockFunction<nx_time_us_t()>* clock_observer;
bool enabled(IRQn_Type irq) {
    return (NVIC->ISER[(unsigned)irq / 32U] & (1U << ((unsigned)irq % 32U))) !=
           0U;
}
class I2CMultiple : public testing::Test {
  protected:
    nx_i2c_v1_state_t states[2]{};
    nx_i2c_v1_endpoint_t endpoints[2]{{&states[0], 0x50U}, {&states[1], 0x51U}};
    nx_i2c_port_t buses[2]{{&nx_i2c_v1_async_ops, &states[0]},
                           {&nx_i2c_v1_async_ops, &states[1]}};
    nx_i2c_endpoint_t devices[2]{
        {&nx_i2c_v1_async_endpoint_ops, &endpoints[0]},
        {&nx_i2c_v1_async_endpoint_ops, &endpoints[1]}};
    uint8_t buffers[2][4]{};
    nx_i2c_message_t messages[2]{{buffers[0], 3U, true},
                                 {buffers[1], 3U, true}};
    nx_i2c_request_t requests[2]{};
    nx_dma_memory_region_t regions[1]{};
    nx_i2c_v1_dma_t dma[2]{};
    void SetUp() override {
        void* mapping = mmap(
            (void*)(uintptr_t)0x40000000U, 0x80000U, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, (void*)(uintptr_t)0x40000000U);
        mask = 0U;
        isr = false;
        now = 100U;
        held_engine = nullptr;
        clock_observer = nullptr;
        g_gd32_model_nvic = {};
        LINE_INPUT = 0xFFFFU;
        regions[0] = {(uintptr_t)buffers, sizeof(buffers),
                      NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
        dma[0] = nx_i2c_v1_dma_t PRIMARY_DMA(regions, 1U);
        dma[1] = nx_i2c_v1_dma_t SECONDARY_DMA(regions, 1U);
        states[0].registers = PRIMARY_REGS;
        states[1].registers = SECONDARY_REGS;
        for (unsigned i = 0U; i < 2U; ++i) {
            states[i].line_inputs = &LINE_INPUT;
            states[i].line_mask = i == 0U ? 0xC0U : 0xC00U;
            states[i].peripheral_mhz = MHZ;
            states[i].dma = &dma[i];
            nx_request_initialize(&requests[i].base);
        }
    }
    void TearDown() override {
        held_engine = nullptr;
        clock_observer = nullptr;
        for (unsigned i = 0U; i < 2U; ++i) {
            if (states[i].initialized) {
                if (states[i].active != nullptr) {
                    EXPECT_EQ(nx_i2c_port_cancel(&buses[i], &requests[i]),
                              NX_SUCCESS);
                }
                *dma[i].rx.control = 0U;
                *dma[i].tx.control = 0U;
                states[i].registers[0] &= ~((1U << 8U) | (1U << 9U));
                states[i].registers[6] = 0U;
                nx_i2c_port_service(&buses[i]);
                EXPECT_EQ(nx_i2c_port_stop(&buses[i]), NX_SUCCESS);
            }
        }
        EXPECT_EQ(munmap((void*)(uintptr_t)0x40000000U, 0x80000U), 0);
    }
    void Both() {
        ASSERT_EQ(INITIALIZE(&states[0], 5U), NX_SUCCESS);
        ASSERT_EQ(INITIALIZE(&states[1], 6U), NX_SUCCESS);
    }
    void Submit(unsigned i) {
        ASSERT_EQ(nx_i2c_request_prepare(&requests[i], &messages[i], 1U, 1000U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_i2c_endpoint_submit(&devices[i], &requests[i]),
                  NX_SUCCESS);
        states[i].registers[6] = 3U;
        states[i].registers[5] = 1U;
        nx_i2c_v1_irq(&states[i]);
        states[i].registers[5] = 2U;
        nx_i2c_v1_irq(&states[i]);
        EXPECT_EQ(*dma[i].rx.control & 1U, 1U);
    }
    void Wire(unsigned i) {
        states[i].registers[0] &= ~(1U << 9U);
        states[i].registers[6] &= ~1U;
        nx_i2c_port_service(&buses[i]);
    }
};

TEST_F(I2CMultiple, SecondaryColdInitLeavesPrimaryRegistersAndIrqsIntact) {
    ASSERT_EQ(INITIALIZE(&states[0], 5U), NX_SUCCESS);
    const auto before = std::vector<uint32_t>(PRIMARY_REGS, PRIMARY_REGS + 9U);
    ASSERT_EQ(INITIALIZE(&states[1], 6U), NX_SUCCESS);
    EXPECT_EQ(std::vector<uint32_t>(PRIMARY_REGS, PRIMARY_REGS + 9U), before);
    EXPECT_TRUE(enabled(EV0));
    EXPECT_TRUE(enabled(ER0));
    EXPECT_TRUE(enabled(RX0));
    EXPECT_TRUE(enabled(TX0));
    EXPECT_TRUE(enabled(EV1));
    EXPECT_TRUE(enabled(ER1));
    EXPECT_TRUE(enabled(RX1));
    EXPECT_TRUE(enabled(TX1));
    EXPECT_EQ(NVIC_GetPriority(EV0), 5U);
    EXPECT_EQ(NVIC_GetPriority(EV1), 6U);
}

TEST_F(I2CMultiple, DuplicateLiveControllerRefusesBeforePeerMutation) {
    ASSERT_EQ(INITIALIZE(&states[0], 5U), NX_SUCCESS);
    nx_i2c_v1_state_t duplicate = states[0];
    duplicate.initialized = false;
    const auto before = std::vector<uint32_t>(PRIMARY_REGS, PRIMARY_REGS + 9U);
    EXPECT_EQ(INITIALIZE(&duplicate, 3U), NX_ERROR_BUSY);
    EXPECT_FALSE(duplicate.initialized);
    EXPECT_EQ(std::vector<uint32_t>(PRIMARY_REGS, PRIMARY_REGS + 9U), before);
    EXPECT_EQ(NVIC_GetPriority(EV0), 5U);
}

TEST_F(I2CMultiple, SecondaryCannotStealPrimaryDmaTupleOrLineRoute) {
    states[1].dma = &dma[0];
    EXPECT_EQ(INITIALIZE(&states[1], 6U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(SECONDARY_REGS[0], 0U);
    EXPECT_FALSE(enabled(EV1));
    states[1].dma = &dma[1];
    states[1].line_mask = 0xC0U;
    EXPECT_EQ(INITIALIZE(&states[1], 6U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(SECONDARY_REGS[0], 0U);
}

TEST_F(I2CMultiple, OneStopCannotWithdrawAnotherControllersPublishers) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_EQ(nx_i2c_port_stop(&buses[0]), NX_SUCCESS);
    EXPECT_FALSE(enabled(EV0));
    EXPECT_FALSE(enabled(ER0));
    EXPECT_FALSE(enabled(RX0));
    EXPECT_FALSE(enabled(TX0));
    EXPECT_TRUE(enabled(EV1));
    EXPECT_TRUE(enabled(ER1));
    EXPECT_TRUE(enabled(RX1));
    EXPECT_TRUE(enabled(TX1));
    EXPECT_TRUE(states[1].initialized);
    EXPECT_EQ(SECONDARY_REGS[0] & 1U, 1U);
}

TEST_F(I2CMultiple, StoppingOneInstancePreservesPendingPeerIrqs) {
    ASSERT_NO_FATAL_FAILURE(Both());
    const IRQn_Type sources[] = {EV0, ER0, RX0, TX0, EV1, ER1, RX1, TX1};
    for (IRQn_Type irq : sources) {
        NVIC->ISPR[(unsigned)irq / 32U] |= 1U << ((unsigned)irq % 32U);
    }
    ASSERT_EQ(nx_i2c_port_stop(&buses[0]), NX_SUCCESS);
    for (IRQn_Type irq : {EV0, ER0, RX0, TX0}) {
        EXPECT_EQ(NVIC->ISPR[(unsigned)irq / 32U] &
                      (1U << ((unsigned)irq % 32U)),
                  0U);
    }
    for (IRQn_Type irq : {EV1, ER1, RX1, TX1}) {
        EXPECT_NE(NVIC->ISPR[(unsigned)irq / 32U] &
                      (1U << ((unsigned)irq % 32U)),
                  0U);
    }
}

TEST_F(I2CMultiple, IrqOnlyInstancesHaveSeparateReceivePipelineAndNoDmaLease) {
    states[0].dma = states[1].dma = nullptr;
    ASSERT_NO_FATAL_FAILURE(Both());
    EXPECT_FALSE(enabled(RX0));
    EXPECT_FALSE(enabled(TX0));
    EXPECT_FALSE(enabled(RX1));
    EXPECT_FALSE(enabled(TX1));
    for (unsigned i = 0U; i < 2U; ++i) {
        messages[i].length = 1U;
        ASSERT_EQ(nx_i2c_request_prepare(&requests[i], &messages[i], 1U, 1000U),
                  NX_SUCCESS);
        ASSERT_EQ(nx_i2c_endpoint_submit(&devices[i], &requests[i]),
                  NX_SUCCESS);
        states[i].registers[6] = 3U;
        states[i].registers[5] = 1U;
        nx_i2c_v1_irq(&states[i]);
        states[i].registers[5] = 2U;
        nx_i2c_v1_irq(&states[i]);
    }
    PRIMARY_REGS[4] = 0x91U;
    PRIMARY_REGS[5] = 1U << 6U;
    nx_i2c_v1_irq(&states[0]);
    Wire(0U);
    EXPECT_EQ(buffers[0][0], 0x91U);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_ACTIVE);
    SECONDARY_REGS[4] = 0x63U;
    SECONDARY_REGS[5] = 1U << 6U;
    nx_i2c_v1_irq(&states[1]);
    Wire(1U);
    EXPECT_EQ(buffers[1][0], 0x63U);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_SETTLED);
}

TEST_F(I2CMultiple, SecondaryRejectsChangedSelectorAndInterruptBitGroup) {
    dma[1].rx.configuration ^= 1U << 25U;
    EXPECT_EQ(INITIALIZE(&states[1], 5U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(SECONDARY_REGS[0], 0U);
    dma[1].rx.configuration ^= 1U << 25U;
    dma[1].tx.flag_shift = 16U;
    EXPECT_EQ(INITIALIZE(&states[1], 5U), NX_ERROR_UNSUPPORTED);
    EXPECT_FALSE(enabled(EV1));
}

TEST_F(I2CMultiple, NonselectedControllerCannotAcquireKnownLineOrDmaFacts) {
    states[1].registers = (volatile uint32_t*)(uintptr_t)0x40005C00U;
    EXPECT_EQ(INITIALIZE(&states[1], 5U), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(states[1].registers[0], 0U);
    EXPECT_FALSE(states[1].initialized);
    EXPECT_FALSE(enabled(EV1));
}

TEST_F(I2CMultiple, IsrInitializationCannotMutateDisabledControllerOrIrqs) {
    isr = true;
    EXPECT_EQ(INITIALIZE(&states[1], 5U), NX_ERROR_CONTEXT);
    isr = false;
    EXPECT_EQ(SECONDARY_REGS[0], 0U);
    EXPECT_FALSE(enabled(EV1));
    EXPECT_FALSE(enabled(RX1));
}

TEST_F(I2CMultiple, FiniteDmaCompletionPreservesPeerFlagsEngineAndBorrow) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(Submit(0U));
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    auto* first_flags = const_cast<volatile uint32_t*>(dma[0].rx.flags);
    auto* next_flags = const_cast<volatile uint32_t*>(dma[1].rx.flags);
    *first_flags |= 0x20U << dma[0].rx.flag_shift;
    *next_flags |= 0x20U << dma[1].rx.flag_shift;
    *dma[0].rx.count = 0U;
    nx_i2c_v1_dma_irq(&states[0], true);
    EXPECT_EQ(*first_flags & (0x3DU << dma[0].rx.flag_shift), 0U);
    EXPECT_NE(*next_flags & (0x20U << dma[1].rx.flag_shift), 0U);
    EXPECT_EQ(*dma[1].rx.control & 1U, 1U);
    EXPECT_EQ(states[1].active, &requests[1]);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_DRAINING);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_ACTIVE);
    Wire(0U);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(states[0].active, nullptr);
    EXPECT_EQ(states[1].active, &requests[1]);
    EXPECT_EQ(requests[0].base.result, NX_SUCCESS);
}

TEST_F(I2CMultiple, FailedSecondaryDmaDrainRetainsItsBorrowOnly) {
    ASSERT_NO_FATAL_FAILURE(Both());
    ASSERT_NO_FATAL_FAILURE(Submit(0U));
    ASSERT_NO_FATAL_FAILURE(Submit(1U));
    held_engine = dma[1].rx.control;
    EXPECT_EQ(nx_i2c_port_cancel(&buses[1], &requests[1]), NX_SUCCESS);
    Wire(1U);
    now = 2000U;
    nx_i2c_port_service(&buses[1]);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_QUARANTINED);
    EXPECT_EQ(states[1].active, &requests[1]);
    EXPECT_EQ(states[0].active, &requests[0]);
    EXPECT_EQ(nx_request_state(&requests[0].base), NX_REQUEST_ACTIVE);
    EXPECT_EQ(*dma[0].rx.control & 1U, 1U);
    held_engine = nullptr;
    *dma[1].rx.control = 0U;
    nx_i2c_port_service(&buses[1]);
    EXPECT_EQ(nx_request_state(&requests[1].base), NX_REQUEST_SETTLED);
    EXPECT_EQ(requests[1].base.result, NX_ERROR_CANCELLED);
}

TEST_F(I2CMultiple, SecondaryPublisherCeilingUsesActualSelectedDmaIrq) {
    ASSERT_NO_FATAL_FAILURE(Both());
    testing::StrictMock<testing::MockFunction<nx_result_t()>> wake;
    const nx_irq_wake_t sink{
        &wake,
        [](void* context) {
            return static_cast<testing::MockFunction<nx_result_t()>*>(context)
                ->Call();
        },
        true};
    NVIC_SetPriority(RX1, 4U);
    EXPECT_EQ(nx_i2c_port_attach_wake(&buses[1], &sink, 5U),
              NX_ERROR_PERMISSION);
    EXPECT_EQ(states[1].wake, nullptr);
    NVIC_SetPriority(RX1, 6U);
    EXPECT_EQ(nx_i2c_port_attach_wake(&buses[1], &sink, 5U), NX_SUCCESS);
    EXPECT_EQ(states[1].wake, &sink);
    EXPECT_EQ(states[0].wake, nullptr);
}
} /* namespace */

/** \brief Supply the external monotonic clock, never request logic. */
extern "C" nx_time_us_t nx_time_now_us(void) {
    return clock_observer != nullptr ? clock_observer->Call() : now;
}
/** \brief Inject actual task versus configurable handler context. */
extern "C" bool nx_arch_in_isr(void) {
    return isr;
}
/** \brief Expose the external incoming mask admission boundary. */
extern "C" bool nx_arch_irq_is_masked(void) {
    return mask != 0U;
}
/** \brief Preserve incoming metadata exclusion on the single model CPU. */
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous{mask};
    mask = 1U;
    return previous;
}
/** \brief Restore the exact saved model mask. */
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    mask = previous.value;
}
/** \brief Single-thread model MMIO ordering; no physical latency claim. */
extern "C" void nx_arch_dsb(void) {
}
/** \brief Single-thread external memory-ordering boundary. */
extern "C" void nx_arch_dmb(void) {
}
/** \brief Read actual per-instance DATA and consume receive status. */
extern "C" uint8_t nx_i2c_v1_model_read(void* context) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(context);
    state->registers[5] &= ~((1U << 6U) | (1U << 2U));
    return (uint8_t)state->registers[4];
}
/** \brief Write actual selected DATA without changing peer registers. */
extern "C" void nx_i2c_v1_model_write(void* context, uint32_t value) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(context);
    state->registers[4] = value;
    state->registers[5] &= ~(1U | (1U << 2U) | (1U << 7U));
}
/** \brief Apply actual address read-clear hardware behavior. */
extern "C" void nx_i2c_v1_model_address_cleared(void* context) {
    auto* state = static_cast<nx_i2c_v1_state_t*>(context);
    state->registers[5] &= ~2U;
}
/** \brief Model a refused enable-bit clear without releasing the loan. */
extern "C" void nx_i2c_v1_model_disable(volatile uint32_t* control,
                                        uint32_t bits) {
    *control &= control == held_engine ? ~(bits & ~1U) : ~bits;
}
/** \brief Apply exact W1C bits to the actual selected DMA flag word. */
extern "C" void nx_i2c_multi_dma_ack(volatile uint32_t* clear, uint32_t bits) {
    *clear = bits;
    /* Each reviewed DMA has status0/1 exactly eight bytes before clear0/1. */
    *(clear - 2U) &= ~bits;
}
