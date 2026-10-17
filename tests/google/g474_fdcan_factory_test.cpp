/**
 * \file            g474_fdcan_factory_test.cpp
 * \brief           Actual TOML factory, system startup and FDCAN retained loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "g474_fdcan_factory_model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "stm32g474_fdcan.h"
#include "stm32g474_system.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
FDCAN_GlobalTypeDef g_g474_factory_fdcan[3];
FDCAN_Config_TypeDef g_g474_factory_fdcan_config;
volatile uint32_t g_g474_factory_fdcan_ram[3U * 212U];
void FDCAN1_IT0_IRQHandler(void);
}
namespace {
size_t unclocked_reads;
testing::MockFunction<void(volatile uint32_t*, uint32_t)>* write_observer;
bool fdcan_address(const volatile uint32_t* address) {
    uintptr_t value = reinterpret_cast<uintptr_t>(address);
    uintptr_t first = reinterpret_cast<uintptr_t>(g_g474_factory_fdcan);
    uintptr_t config = reinterpret_cast<uintptr_t>(FDCAN_CONFIG);
    uintptr_t ram = reinterpret_cast<uintptr_t>(g_g474_factory_fdcan_ram);
    return (value >= first && value - first < sizeof(g_g474_factory_fdcan)) ||
           (value >= config && value - config < sizeof(*FDCAN_CONFIG)) ||
           (value >= ram && value - ram < sizeof(g_g474_factory_fdcan_ram));
}
} /* namespace */
extern "C" uint32_t
nx_g474_fdcan_factory_read(volatile const uint32_t* address) {
    ++g_g474_model.reads;
    if (fdcan_address(address) &&
        (RCC->APB1ENR1 & RCC_APB1ENR1_FDCANEN) == 0U) {
        ++unclocked_reads;
        return 0U;
    }
    if (address == &FDCAN1->TXFQS) {
        const uint32_t pending = (FDCAN1->TXBAR | FDCAN1->TXBRP) & 7U;
        unsigned index = 0U;
        while (index < 3U && (pending & (1U << index)) != 0U) {
            ++index;
        }
        return index == 3U ? static_cast<uint32_t>(FDCAN_TXFQS_TFQF)
                           : index << FDCAN_TXFQS_TFQPI_Pos;
    }
    return *address;
}
extern "C" void nx_g474_fdcan_factory_write(volatile uint32_t* address,
                                            uint32_t value) {
    if (write_observer != nullptr) {
        write_observer->Call(address, value);
    }
    if (!fdcan_address(address)) {
        nx_g474_model_write(address, value);
        return;
    }
    ++g_g474_model.writes;
    if (address == g_g474_model.ignore_write) {
        return;
    }
    if (address == &FDCAN1->IR) {
        *address &= ~value;
        return;
    }
    if (address == &FDCAN1->TXBAR) {
        const uint32_t added = value & ~FDCAN1->TXBRP & 7U;
        FDCAN1->TXBTO &= ~added;
        FDCAN1->TXBCF &= ~added;
        FDCAN1->TXBRP |= added;
        return;
    }
    if (address == &FDCAN1->TXBCR) {
        *address |= value & 7U;
        return;
    }
    if (address == &FDCAN1->CCCR && (value & FDCAN_CCCR_CCE) != 0U &&
        (*address & FDCAN_CCCR_CCE) == 0U) {
        FDCAN1->TXBRP = FDCAN1->TXBAR = FDCAN1->TXBCR = 0U;
        FDCAN1->TXBTO = FDCAN1->TXBCF = 0U;
        FDCAN1->RXF0S = FDCAN1->RXF1S = 0U;
    }
    *address = value;
}
extern "C" void nx_g474_fdcan_factory_priority_write(int irq,
                                                     uint32_t priority) {
    if (irq >= 0 && irq < 102 && priority < 16U) {
        g_g474_model.priority[irq] = static_cast<uint8_t>(priority);
    }
}
class G474FdcanFactory : public testing::Test {
  protected:
    testing::NiceMock<testing::MockFunction<void(volatile uint32_t*, uint32_t)>>
        observer;
    nx_can_fd_tx_request_t request = {};
    nx_can_fd_frame_t frame = {};
    static const nx_can_fd_port_t* port() {
        return nx_factory_can_fd(NX_CAN_FD_ID_FIELDBUS);
    }
    void SetUp() override {
        nx_g474_model_reset();
        std::memset(g_g474_factory_fdcan, 0, sizeof(g_g474_factory_fdcan));
        std::memset(FDCAN_CONFIG, 0, sizeof(*FDCAN_CONFIG));
        for (auto& word : g_g474_factory_fdcan_ram) {
            word = 0U;
        }
        for (auto& controller : g_g474_factory_fdcan) {
            controller.CREL = 0x32141218U;
            controller.ENDN = 0x87654321U;
            controller.CCCR = FDCAN_CCCR_INIT;
        }
        write_observer = &observer;
        unclocked_reads = 0U;
        nx_request_initialize(&request.base);
        frame.id = 0x100U;
        frame.format = NX_CAN_STANDARD;
        frame.kind = NX_CAN_DATA;
        frame.wire = NX_CAN_WIRE_FD;
        frame.payload_bytes = 64U;
        frame.brs = true;
        for (unsigned i = 0U; i < 64U; ++i) {
            frame.data[i] = static_cast<uint8_t>(i + 1U);
        }
    }
    void TearDown() override {
        g_g474_model.ignore_write = nullptr;
        g_g474_model.hold_irq_off = false;
        g_g474_model.irq_pending[21] = g_g474_model.irq_pending[22] = false;
        g_g474_model.irq_active[21] = g_g474_model.irq_active[22] = false;
        FDCAN1->TXBCF |= FDCAN1->TXBRP;
        FDCAN1->TXBRP = FDCAN1->TXBAR = FDCAN1->TXBCR = 0U;
        EXPECT_EQ(nx_platform_stop(), NX_SUCCESS);
        EXPECT_EQ(unclocked_reads, 0U);
        write_observer = nullptr;
    }
    void Start() {
        const auto result = nx_platform_start();
        ASSERT_EQ(result.primary, NX_SUCCESS);
        ASSERT_EQ(result.cleanup, NX_SUCCESS);
        ASSERT_EQ(result.remaining_effects, 0U);
        TIM2->CNT = 100U;
    }
};
TEST_F(G474FdcanFactory, TypedLookupHasNoClockPinRamOrIrqEffects) {
    const auto* face = port();
    ASSERT_NE(face, nullptr);
    EXPECT_EQ(face, port());
    EXPECT_EQ(nx_factory_can_fd(NX_CAN_FD_ID_COUNT), nullptr);
    EXPECT_EQ(g_g474_model.reads, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
}
TEST_F(G474FdcanFactory, RealSystemStartupAcquiresClockPinsAndOnlyLineZero) {
    ASSERT_NO_FATAL_FAILURE(Start());
    EXPECT_EQ(g_nx_stm32g474_system.core_hz, 170000000U);
    EXPECT_EQ(nx_time_now_us(), 100U);
    EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_FDCANEN, 0U);
    EXPECT_EQ((GPIOA->MODER >> 22U) & 15U, 10U);
    EXPECT_EQ((GPIOA->AFR[1] >> 12U) & 255U, 0x99U);
    EXPECT_NE(GPIOA->ODR & (1U << 12U), 0U);
    EXPECT_EQ(FDCAN1->NBTP, 0x02020B02U);
    EXPECT_EQ(FDCAN1->DBTP, 0x00000811U);
#if NX_G474_FDCAN_FACTORY_NORMAL
    EXPECT_EQ(FDCAN1->CCCR & (FDCAN_CCCR_TEST | FDCAN_CCCR_MON), 0U);
    EXPECT_EQ(FDCAN1->TEST & FDCAN_TEST_LBCK, 0U);
#else
    EXPECT_EQ(FDCAN1->CCCR & (FDCAN_CCCR_TEST | FDCAN_CCCR_MON),
              static_cast<uint32_t>(FDCAN_CCCR_TEST | FDCAN_CCCR_MON));
    EXPECT_NE(FDCAN1->TEST & FDCAN_TEST_LBCK, 0U);
#endif
    EXPECT_TRUE(g_g474_model.irq_enabled[21]);
    EXPECT_FALSE(g_g474_model.irq_enabled[22]);
    EXPECT_EQ(g_g474_model.priority[21], 5U);
}
TEST_F(G474FdcanFactory, GeneratedFaceSendsAllSixtyFourBytesThroughRealRam) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_submit(port(), &request), NX_SUCCESS);
    EXPECT_EQ(g_g474_factory_fdcan_ram[158U + 17U], 0x403F3E3DU);
    EXPECT_NE(FDCAN1->TXBRP & 1U, 0U);
    FDCAN1->TXBRP = 0U;
    FDCAN1->TXBTO = 1U;
    FDCAN1->IR |= FDCAN_IR_TC;
    FDCAN1_IT0_IRQHandler();
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_SUCCESS);
    EXPECT_EQ(request.base.transferred, 64U);
}
TEST_F(G474FdcanFactory,
       FailedAbortRetainsPlatformUntilRealOutcomeThenRestarts) {
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_EQ(nx_can_fd_tx_prepare(&request, &frame, 1000U), NX_SUCCESS);
    ASSERT_EQ(nx_can_fd_port_submit(port(), &request), NX_SUCCESS);
    EXPECT_EQ(nx_platform_stop(), NX_ERROR_BUSY);
    EXPECT_TRUE(g_nx_stm32g474_system.started);
    EXPECT_NE(RCC->APB1ENR1 & RCC_APB1ENR1_FDCANEN, 0U);
    EXPECT_EQ((GPIOA->MODER >> 22U) & 15U, 10U);
    EXPECT_NE(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    FDCAN1->TXBRP = FDCAN1->TXBCR = 0U;
    FDCAN1->TXBCF = 1U;
    ASSERT_EQ(nx_platform_stop(), NX_SUCCESS);
    EXPECT_EQ(nx_request_state(&request.base), NX_REQUEST_SETTLED);
    EXPECT_EQ(request.base.result, NX_ERROR_CANCELLED);
    EXPECT_EQ(RCC->APB1ENR1 & RCC_APB1ENR1_FDCANEN, 0U);
    EXPECT_FALSE(g_nx_stm32g474_system.started);
    ASSERT_NO_FATAL_FAILURE(Start());
}
TEST_F(G474FdcanFactory, ForeignDisabledLinePendingRejectsBeforeClockEffects) {
    g_g474_model.irq_pending[22] = true;
    EXPECT_CALL(observer, Call(testing::_, testing::_)).Times(0);
    const auto result = nx_platform_start();
    EXPECT_EQ(result.primary, NX_ERROR_BUSY);
    EXPECT_EQ(result.cleanup, NX_SUCCESS);
    EXPECT_EQ(result.remaining_effects, 0U);
    EXPECT_EQ(g_g474_model.writes, 0U);
    EXPECT_TRUE(g_g474_model.irq_pending[22]);
    testing::Mock::VerifyAndClearExpectations(&observer);
}
