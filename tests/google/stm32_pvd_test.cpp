/**
 * \file            stm32_pvd_test.cpp
 * \brief           PVD raw signal, bounded events and retained withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
extern "C" {
#include "stm32f407_pvd.h"
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <vector>

extern "C" {
NVIC_Type nx_pvd_model_nvic;
}
namespace {
constexpr uint32_t line = 1U << 16U;
constexpr uint32_t irq_bit = 1U << static_cast<unsigned>(PVD_IRQn);
uint32_t incoming_mask;
bool privileged;
bool isr;
volatile uint32_t* refused;
bool refused_pending;
bool refused_priority;
std::vector<volatile uint32_t*> writes;

class Pvd : public testing::Test {
  protected:
    void* mapping = nullptr;
    nx_stm32_pvd_state_t poll{};
    nx_stm32_pvd_events_state_t queued{};
    nx_exti_event_t ring[2]{};
    nx_stm32_pvd_events_config_t config{ring, 2U, NX_EXTI_BOTH, 5U, 6U};
    const nx_exti_port_t poll_face{&nx_stm32_pvd_ops, &poll};
    const nx_exti_port_t event_face{&nx_stm32_pvd_events_ops, &queued};
    void SetUp() override {
        mapping =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x80000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(mapping, reinterpret_cast<void*>(0x40000000U));
        std::memset(&nx_pvd_model_nvic, 0, sizeof(nx_pvd_model_nvic));
        privileged = true;
        isr = false;
        incoming_mask = 0U;
        refused = nullptr;
        refused_pending = false;
        refused_priority = false;
        writes.clear();
        RCC->APB1ENR = RCC_APB1ENR_PWREN;
        PWR->CR = PWR_CR_VOS | PWR_CR_DBP | PWR_CR_PLS_LEV2;
        NVIC->IPR[PVD_IRQn] = 3U << 4U;
    }
    void TearDown() override {
        if (mapping != nullptr && mapping != MAP_FAILED) {
            EXPECT_EQ(0, munmap(mapping, 0x80000U));
        }
    }
    void Dispatch(bool low_voltage) {
        PWR->CSR = low_voltage ? PWR_CSR_PVDO : 0U;
        EXTI->PR |= line;
        nx_stm32_exti_state_t* ports[] = {&queued.events};
        nx_stm32_exti_dispatch(ports, 1U, line);
    }
};
} /* namespace */

extern "C" void nx_pvd_model_write(volatile uint32_t* address, uint32_t value) {
    writes.push_back(address);
    if (address != refused)
        *address = value;
}
extern "C" void nx_pvd_model_clear(volatile uint32_t* address, uint32_t mask) {
    writes.push_back(address);
    if (refused_pending && address == &EXTI->PR) {
        *address |= mask;
        return;
    }
    if (address != refused)
        *address &= ~mask;
}
extern "C" void nx_pvd_model_irq(int irq, bool enabled, bool clear) {
    const unsigned index = static_cast<unsigned>(irq) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(irq) % 32U);
    volatile uint32_t* reg = clear ? &NVIC->ISPR[index] : &NVIC->ISER[index];
    if (enabled) {
        EXPECT_EQ(0U, EXTI->PR & line);
        EXPECT_NE(0U, PWR->CR & PWR_CR_PVDE);
    }
    nx_pvd_model_write(reg, clear || !enabled ? *reg & ~bit : *reg | bit);
}
extern "C" void nx_pvd_model_priority(int irq, uint32_t value) {
    if (!refused_priority)
        NVIC->IPR[static_cast<unsigned>(irq)] =
            static_cast<uint8_t>(value << 4U);
}
extern "C" nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous{incoming_mask};
    incoming_mask = 1U;
    return previous;
}
extern "C" void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    incoming_mask = previous.value;
}
extern "C" bool nx_arch_is_privileged(void) {
    return privileged;
}
extern "C" bool nx_arch_in_isr(void) {
    return isr;
}
extern "C" bool nx_arch_irq_is_masked(void) {
    return incoming_mask != 0U;
}
extern "C" void nx_arch_dsb(void) {
}
extern "C" void nx_arch_isb(void) {
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return 123U;
}

TEST_F(Pvd, PollAdmitsBeforeSystemButPrepareRequiresItsPowerClock) {
    RCC->APB1ENR = 0U;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_pvd_admit(&poll, 5U));
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(NX_ERROR_STATE, nx_stm32_pvd_prepare(&poll, 5U));
    EXPECT_TRUE(writes.empty());
    RCC->APB1ENR = RCC_APB1ENR_PWREN;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_prepare(&poll, 5U));
    EXPECT_EQ(PWR_CR_PLS_LEV5, PWR->CR & PWR_CR_PLS);
    EXPECT_NE(0U, PWR->CR & PWR_CR_PVDE);
    EXPECT_EQ(0U, EXTI->IMR | EXTI->RTSR | EXTI->FTSR);
}

TEST_F(Pvd, RawPvdoIsLowVoltageAndObservationNeverConsumesAnEdge) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_prepare(&poll, 7U));
    EXTI->PR = line | 1U;
    bool high = false;
    PWR->CSR = PWR_CSR_PVDO;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_level(&poll_face, &high));
    EXPECT_TRUE(high);
    PWR->CSR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_level(&poll_face, &high));
    EXPECT_FALSE(high);
    EXPECT_EQ(line | 1U, EXTI->PR);
    nx_exti_event_t event{};
    size_t count = 1U;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_exti_port_read(&poll_face, &event, 1U, &count));
    EXPECT_EQ(NX_ERROR_UNSUPPORTED,
              nx_exti_port_attach_wake(&poll_face, nullptr, 0U));
    EXTI->PR = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_stop(&poll_face));
    EXPECT_EQ(PWR_CR_VOS | PWR_CR_DBP | PWR_CR_PLS_LEV2, PWR->CR);
    EXPECT_EQ(RCC_APB1ENR_PWREN, RCC->APB1ENR);
    EXPECT_EQ(NX_ERROR_STATE, nx_exti_port_level(&poll_face, &high));
}

TEST_F(Pvd, ForeignComparatorLineOrVectorRejectsWithoutAcquisition) {
    for (unsigned source = 0U; source < 10U; ++source) {
        SCOPED_TRACE(source);
        PWR->CR = source == 0U ? PWR_CR_PVDE : 0U;
        volatile uint32_t* bits[] = {&EXTI->IMR,  &EXTI->EMR, &EXTI->RTSR,
                                     &EXTI->FTSR, &EXTI->PR,  &EXTI->SWIER};
        for (unsigned i = 0U; i < 6U; ++i)
            *bits[i] = source == i + 1U ? line : 0U;
        NVIC->ISER[0] = source == 7U ? irq_bit : 0U;
        NVIC->ISPR[0] = source == 8U ? irq_bit : 0U;
        const_cast<volatile uint32_t&>(NVIC->IABR[0]) =
            source == 9U ? irq_bit : 0U;
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_pvd_admit(&poll, 5U));
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_pvd_prepare(&poll, 5U));
        EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_pvd_events_admit(&queued, &config));
        EXPECT_TRUE(writes.empty());
        EXPECT_EQ(0U, poll.effects);
        EXPECT_EQ(0U, queued.pvd.effects);
    }
}

TEST_F(Pvd, InvalidColdContextsLevelsAndOutputAliasesPreserveState) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_pvd_admit(nullptr, 0U));
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_pvd_prepare(&poll, 8U));
    for (unsigned execution = 0U; execution < 3U; ++execution) {
        privileged = execution != 0U;
        isr = execution == 1U;
        incoming_mask = execution == 2U ? 1U : 0U;
        EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_pvd_prepare(&poll, 0U));
        EXPECT_EQ(NX_ERROR_CONTEXT, nx_stm32_pvd_stop(&poll));
        EXPECT_EQ(NX_ERROR_CONTEXT,
                  nx_stm32_pvd_events_prepare(&queued, &config));
    }
    privileged = true;
    isr = false;
    incoming_mask = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_prepare(&poll, 0U));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_exti_port_level(&poll_face, reinterpret_cast<bool*>(&poll)));
    EXPECT_EQ(
        NX_ERROR_INVALID,
        nx_exti_port_level(&poll_face, reinterpret_cast<bool*>(PWR_BASE)));
    bool sample = true;
    privileged = false;
    EXPECT_EQ(NX_ERROR_CONTEXT, nx_exti_port_level(&poll_face, &sample));
    EXPECT_TRUE(sample);
}

TEST_F(Pvd, ComparatorReadbackFailureRetainsOnlyItsOwnCleanup) {
    refused = &PWR->CR;
    ASSERT_EQ(NX_ERROR_IO, nx_stm32_pvd_prepare(&poll, 7U));
    EXPECT_NE(0U, poll.effects);
    bool sample = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_exti_port_level(&poll_face, &sample));
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_stop(&poll));
    EXPECT_EQ(0U, poll.effects);
    EXPECT_EQ(PWR_CR_VOS | PWR_CR_DBP | PWR_CR_PLS_LEV2, PWR->CR);
    EXPECT_EQ(RCC_APB1ENR_PWREN, RCC->APB1ENR);
}

TEST_F(Pvd, RingStorageCannotOverwriteItsOwnerOrColdConfiguration) {
    config.storage = reinterpret_cast<nx_exti_event_t*>(&queued);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_pvd_events_admit(&queued, &config));
    config.storage = reinterpret_cast<nx_exti_event_t*>(&config);
    EXPECT_EQ(NX_ERROR_INVALID, nx_stm32_pvd_events_admit(&queued, &config));
    EXPECT_TRUE(writes.empty());
    EXPECT_EQ(0U, queued.pvd.effects);
    struct {
        nx_stm32_pvd_events_state_t owner;
        nx_exti_event_t ring[2];
    } adjacent{};
    config.storage = adjacent.ring;
    EXPECT_EQ(NX_SUCCESS, nx_stm32_pvd_events_admit(&adjacent.owner, &config));
    auto* end = reinterpret_cast<unsigned char*>(&adjacent.owner) +
                sizeof(adjacent.owner);
    EXPECT_EQ(static_cast<void*>(end), static_cast<void*>(adjacent.ring));
    config.storage = reinterpret_cast<nx_exti_event_t*>(end - sizeof(ring[0]));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_stm32_pvd_events_admit(&adjacent.owner, &config));
    const nx_stm32_pvd_events_state_t untouched{};
    EXPECT_EQ(0, std::memcmp(&adjacent.owner, &untouched, sizeof(untouched)));
}

TEST_F(Pvd, EveryFailedEventPreparationRemainsWithdrawableWithoutAClockReset) {
    volatile uint32_t* const registers[] = {&EXTI->RTSR, &EXTI->FTSR,
                                            &EXTI->IMR, &NVIC->ISER[0]};
    for (unsigned phase = 0U; phase < 6U; ++phase) {
        SCOPED_TRACE(phase);
        refused = phase < 4U ? registers[phase] : nullptr;
        refused_pending = phase == 4U;
        refused_priority = phase == 5U;
        ASSERT_EQ(NX_ERROR_IO, nx_stm32_pvd_events_prepare(&queued, &config));
        EXPECT_NE(0U, queued.pvd.effects);
        bool sample = true;
        EXPECT_EQ(NX_ERROR_STATE, nx_exti_port_level(&event_face, &sample));
        EXPECT_TRUE(sample);
        refused = nullptr;
        refused_pending = false;
        refused_priority = false;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_stop(&queued));
        EXPECT_EQ(0U, queued.pvd.effects);
        EXPECT_EQ(PWR_CR_VOS | PWR_CR_DBP | PWR_CR_PLS_LEV2, PWR->CR);
        EXPECT_EQ(RCC_APB1ENR_PWREN, RCC->APB1ENR);
        EXPECT_EQ(3U, NVIC_GetPriority(PVD_IRQn));
    }
}

TEST_F(Pvd, EventStartupHasNoSyntheticHistoryAndDoesNotTouchOtherLines) {
    EXTI->IMR = 1U;
    EXTI->RTSR = 1U;
    EXTI->PR = 1U;
    PWR->CSR = PWR_CSR_PVDO;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_prepare(&queued, &config));
    EXPECT_EQ(1U | line, EXTI->IMR);
    EXPECT_EQ(1U | line, EXTI->RTSR);
    EXPECT_EQ(line, EXTI->FTSR);
    EXPECT_EQ(1U, EXTI->PR);
    EXPECT_EQ(0U, EXTI->EMR);
    EXPECT_EQ(6U, NVIC_GetPriority(PVD_IRQn));
    EXPECT_EQ(1U, NVIC_GetEnableIRQ(PVD_IRQn));
    nx_exti_event_t event{};
    size_t count = 99U;
    EXPECT_EQ(NX_ERROR_EMPTY,
              nx_exti_port_read(&event_face, &event, 1U, &count));
    EXPECT_EQ(0U, count);
    bool sample = false;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_level(&event_face, &sample));
    EXPECT_TRUE(sample);
}

TEST_F(Pvd, BothEdgesFollowPvdoAndAcknowledgeBeforePublishingWake) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_prepare(&queued, &config));
    testing::StrictMock<testing::MockFunction<nx_result_t()>> observer;
    const nx_irq_wake_t wake{
        &observer,
        [](void* context) {
            EXPECT_EQ(0U, EXTI->PR & line);
            return static_cast<testing::MockFunction<nx_result_t()>*>(context)
                ->Call();
        },
        false};
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&event_face, &wake, 0U));
    EXPECT_CALL(observer, Call())
        .Times(2)
        .WillRepeatedly(testing::Return(NX_SUCCESS));
    Dispatch(true);
    Dispatch(false);
    nx_exti_event_t events[2]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&event_face, events, 2U, &count));
    ASSERT_EQ(2U, count);
    EXPECT_EQ(NX_EXTI_RISING, events[0].edge);
    EXPECT_EQ(NX_EXTI_FALLING, events[1].edge);
    EXPECT_EQ(123U, events[0].timestamp_us);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, events[0].flags);
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_stop(&queued));
    EXPECT_EQ(nullptr, queued.events.wake);
}

TEST_F(Pvd, BoundedQueuePublishesLossBeforeLaterEvents) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_prepare(&queued, &config));
    Dispatch(true);
    Dispatch(false);
    Dispatch(true);
    nx_exti_event_t events[3]{};
    size_t count = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&event_face, events, 3U, &count));
    ASSERT_EQ(3U, count);
    EXPECT_EQ(NX_EXTI_EVENT_LOSS, events[2].flags);
    Dispatch(false);
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&event_face, events, 3U, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(NX_EXTI_FALLING, events[0].edge);
}

TEST_F(Pvd, StopJoinsTheActualPvdVectorBeforeChangingPowerFields) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_prepare(&queued, &config));
    const_cast<volatile uint32_t&>(NVIC->IABR[0]) = irq_bit;
    writes.clear();
    EXPECT_EQ(NX_ERROR_BUSY, nx_stm32_pvd_events_stop(&queued));
    EXPECT_TRUE(writes.empty());
    EXPECT_NE(0U, PWR->CR & PWR_CR_PVDE);
    bool sample = false;
    EXPECT_EQ(NX_ERROR_STATE, nx_exti_port_level(&event_face, &sample));
    const_cast<volatile uint32_t&>(NVIC->IABR[0]) = 0U;
    EXTI->PR = line | 1U;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_stop(&queued));
    EXPECT_EQ(0U, (EXTI->IMR | EXTI->RTSR | EXTI->FTSR) & line);
    EXPECT_EQ(1U, EXTI->PR);
    EXPECT_EQ(0U, NVIC_GetEnableIRQ(PVD_IRQn));
    EXPECT_EQ(3U, NVIC_GetPriority(PVD_IRQn));
    EXPECT_EQ(PWR_CR_VOS | PWR_CR_DBP | PWR_CR_PLS_LEV2, PWR->CR);
    EXPECT_EQ(RCC_APB1ENR_PWREN, RCC->APB1ENR);
}

TEST_F(Pvd, FailedIrqWithdrawalRetainsBorrowAndComparatorForRetry) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_prepare(&queued, &config));
    refused = &EXTI->IMR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_pvd_events_stop(&queued));
    EXPECT_TRUE(queued.events.initialized);
    EXPECT_NE(0U, queued.pvd.effects);
    EXPECT_NE(0U, PWR->CR & PWR_CR_PVDE);
    bool sample = true;
    EXPECT_EQ(NX_ERROR_STATE, nx_exti_port_level(&event_face, &sample));
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_stop(&queued));
    EXPECT_FALSE(queued.events.initialized);
    EXPECT_EQ(0U, queued.pvd.effects);
}

TEST_F(Pvd, FailedPowerRestoreRetainsReceiptAfterPublisherDrain) {
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_prepare(&queued, &config));
    refused = &PWR->CR;
    EXPECT_EQ(NX_ERROR_IO, nx_stm32_pvd_events_stop(&queued));
    EXPECT_FALSE(queued.events.initialized);
    EXPECT_NE(0U, queued.pvd.effects);
    EXPECT_EQ(0U, NVIC_GetEnableIRQ(PVD_IRQn));
    refused = nullptr;
    ASSERT_EQ(NX_SUCCESS, nx_stm32_pvd_events_stop(&queued));
    EXPECT_EQ(0U, queued.pvd.effects);
    EXPECT_EQ(PWR_CR_VOS | PWR_CR_DBP | PWR_CR_PLS_LEV2, PWR->CR);
}
