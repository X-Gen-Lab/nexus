/**
 * \file            infrared_exti_test.cpp
 * \brief           Actual PE2 EXTI observations through the NEC source policy
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32_exti_model.h"
extern "C" {
#include "nexus/components/infrared.h"
#include "stm32f407_provider.h"
nx_stm32_exti_nvic_model_t g_stm32_exti_nvic;
}
#include <cstring>
#include <gtest/gtest.h>

namespace {
nx_time_us_t observed_time;
}
extern "C" nx_time_us_t nx_time_now_us(void) {
    return observed_time;
}
extern "C" bool nx_stm32_exti_model_privileged(void) {
    return true;
}
uint32_t nx_stm32_exti_model_read(volatile uint32_t* reg) {
    return *reg;
}
void nx_stm32_exti_model_write(volatile uint32_t* reg, uint32_t value) {
    *reg = value;
}
void nx_stm32_exti_model_clear(volatile uint32_t* reg, uint32_t mask) {
    *reg &= ~mask;
}
void nx_stm32_exti_model_disable(int irq) {
    const unsigned vector = static_cast<unsigned>(irq);
    g_stm32_exti_nvic.ISER[vector / 32U] &= ~(1U << (vector % 32U));
}
void nx_stm32_exti_model_clear_irq(int irq) {
    const unsigned vector = static_cast<unsigned>(irq);
    g_stm32_exti_nvic.ISPR[vector / 32U] &= ~(1U << (vector % 32U));
}

namespace {
class InfraredExti : public testing::Test {
  protected:
    EXTI_TypeDef registers{};
    GPIO_TypeDef gpio{};
    SYSCFG_TypeDef mux{};
    nx_exti_event_t events[2]{};
    nx_stm32_exti_state_t source{};
    nx_infrared_nec_t decoder{};
    nx_infrared_nec_config_t config{{8800U, 9200U},
                                    {4300U, 4700U},
                                    {2000U, 2500U},
                                    {400U, 750U},
                                    {400U, 750U},
                                    {1400U, 1900U},
                                    120000U,
                                    200000U,
                                    NX_INFRARED_NEC_STANDARD,
                                    NX_INFRARED_SOURCE_STRICT};
    const nx_exti_port_t face{&nx_stm32_exti_ops, &source};
    unsigned dispatches = 0U;
    void SetUp() override {
        observed_time = 1000U;
        g_stm32_exti_nvic = {};
        source.registers = &registers;
        source.gpio = &gpio;
        source.storage = events;
        source.capacity = 2U;
        source.line = 2U;
        source.edge = NX_EXTI_BOTH;
        ASSERT_EQ(NX_SUCCESS, nx_stm32_exti_initialize(&source, &mux, 4U));
        EXPECT_EQ(4U, (mux.EXTICR[0] >> 8U) & 15U);
    }
    void Init(nx_infrared_source_policy_t policy) {
        config.source_policy = policy;
        ASSERT_EQ(NX_SUCCESS, nx_infrared_nec_init(&decoder, &config));
    }
    void Inject(nx_exti_edge_t edge, uint32_t after = 0U) {
        observed_time += after;
        gpio.IDR = edge == NX_EXTI_RISING ? 1U << 2U : 0U;
        registers.PR |= 1U << 2U;
        nx_stm32_exti_state_t* const ports[] = {&source};
        nx_stm32_exti_dispatch(ports, 1U, 1U << 2U);
        ++dispatches;
    }
    nx_infrared_nec_result_t Edge(nx_exti_edge_t edge, uint32_t after = 0U) {
        Inject(edge, after);
        EXPECT_EQ(1U, source.count);
        EXPECT_EQ(NX_EXTI_EVENT_COALESCED, events[source.tail].flags);
        EXPECT_EQ(edge, events[source.tail].edge);
        return nx_infrared_nec_poll(&decoder, &face);
    }
    nx_infrared_nec_result_t Word(uint32_t word) {
        auto result = Edge(NX_EXTI_FALLING);
        result = Edge(NX_EXTI_RISING, 9000U);
        result = Edge(NX_EXTI_FALLING, 4500U);
        for (unsigned i = 0U; i < 32U; ++i) {
            result = Edge(NX_EXTI_RISING, 560U);
            result = Edge(NX_EXTI_FALLING,
                          (word & (UINT32_C(1) << i)) != 0U ? 1690U : 560U);
        }
        return Edge(NX_EXTI_RISING, 560U);
    }
    nx_infrared_nec_result_t Repeat(bool flag_free = false) {
        if (!flag_free) {
            (void)Edge(NX_EXTI_FALLING, 40000U);
            (void)Edge(NX_EXTI_RISING, 9000U);
            (void)Edge(NX_EXTI_FALLING, 2250U);
            return Edge(NX_EXTI_RISING, 560U);
        }
        nx_exti_event_t event{observed_time + 40000U, 0U, NX_EXTI_FALLING};
        (void)nx_infrared_nec_feed(&decoder, &event);
        event.timestamp_us += 9000U;
        event.edge = NX_EXTI_RISING;
        (void)nx_infrared_nec_feed(&decoder, &event);
        event.timestamp_us += 2250U;
        event.edge = NX_EXTI_FALLING;
        (void)nx_infrared_nec_feed(&decoder, &event);
        event.timestamp_us += 560U;
        event.edge = NX_EXTI_RISING;
        observed_time = event.timestamp_us;
        return nx_infrared_nec_feed(&decoder, &event);
    }
};

TEST_F(InfraredExti, StrictDefaultRejectsActualCoalescedProviderEvents) {
    Init(NX_INFRARED_SOURCE_STRICT);
    EXPECT_EQ(NX_ERROR_OVERFLOW, Word(UINT32_C(0xCB34ED12)).result);
    EXPECT_EQ(68U, dispatches);
    EXPECT_FALSE(decoder.last_valid);
    EXPECT_FALSE(decoder.clock_fault);
}

TEST_F(InfraredExti, ExplicitIrqReferenceDecodesButRetainsSampledUncertainty) {
    Init(NX_INFRARED_SOURCE_IRQ_OBSERVATION);
    const auto result = Word(UINT32_C(0xCB34ED12));
    ASSERT_EQ(NX_SUCCESS, result.result);
    EXPECT_EQ(0x12U, result.frame.address);
    EXPECT_EQ(0x34U, result.frame.command);
    EXPECT_EQ(NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN, result.frame.quality);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, result.frame.source_flags);
    EXPECT_EQ(68U, dispatches);
    EXPECT_EQ(0U, source.count);
}

TEST_F(InfraredExti, RepeatCannotUpgradeItsObservedCommandToAnExactSource) {
    Init(NX_INFRARED_SOURCE_IRQ_OBSERVATION);
    ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
    const auto sampled = Repeat();
    ASSERT_EQ(NX_SUCCESS, sampled.result);
    EXPECT_TRUE(sampled.frame.repeat);
    EXPECT_EQ(NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN, sampled.frame.quality);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, sampled.frame.source_flags);
    const auto unflagged = Repeat(true);
    ASSERT_EQ(NX_SUCCESS, unflagged.result);
    EXPECT_EQ(NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN, unflagged.frame.quality);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, unflagged.frame.source_flags);
}

TEST_F(InfraredExti, RealRingLossAlwaysInvalidatesThePriorCommand) {
    Init(NX_INFRARED_SOURCE_IRQ_OBSERVATION);
    ASSERT_EQ(NX_SUCCESS, Word(UINT32_C(0xCB34ED12)).result);
    Inject(NX_EXTI_FALLING, 40000U);
    Inject(NX_EXTI_RISING, 9000U);
    Inject(NX_EXTI_FALLING, 2250U);
    ASSERT_EQ(2U, source.count);
    ASSERT_NE(0U, source.loss);
    EXPECT_EQ(NX_ERROR_EMPTY, nx_infrared_nec_poll(&decoder, &face).result);
    EXPECT_EQ(NX_ERROR_EMPTY, nx_infrared_nec_poll(&decoder, &face).result);
    EXPECT_EQ(NX_ERROR_OVERFLOW, nx_infrared_nec_poll(&decoder, &face).result);
    EXPECT_FALSE(decoder.last_valid);
    EXPECT_NE(NX_SUCCESS, Repeat().result);
}

TEST_F(InfraredExti, IrqSamplingNeverRepairsBadAddressOrCommandInverse) {
    Init(NX_INFRARED_SOURCE_IRQ_OBSERVATION);
    EXPECT_EQ(NX_ERROR_IO, Word(UINT32_C(0xCB343412)).result);
    EXPECT_FALSE(decoder.last_valid);
    ASSERT_EQ(NX_SUCCESS, nx_infrared_nec_stop(&decoder));
    Init(NX_INFRARED_SOURCE_IRQ_OBSERVATION);
    EXPECT_EQ(NX_ERROR_IO, Word(UINT32_C(0x3434ED12)).result);
    EXPECT_FALSE(decoder.last_valid);
}

TEST_F(InfraredExti, SampledTimestampsStillLatchARegressionUntilRebind) {
    Init(NX_INFRARED_SOURCE_IRQ_OBSERVATION);
    EXPECT_EQ(NX_ERROR_EMPTY, Edge(NX_EXTI_FALLING).result);
    EXPECT_EQ(NX_ERROR_IO, Edge(NX_EXTI_RISING).result);
    EXPECT_TRUE(decoder.clock_fault);
    EXPECT_EQ(NX_ERROR_IO, Edge(NX_EXTI_FALLING, 200000U).result);
}

TEST_F(InfraredExti, ExtendedAddressStillRequiresItsOwnExplicitPolicy) {
    config.protocol = NX_INFRARED_NEC_EXTENDED;
    Init(NX_INFRARED_SOURCE_IRQ_OBSERVATION);
    const auto result = Word(UINT32_C(0xCB343412));
    ASSERT_EQ(NX_SUCCESS, result.result);
    EXPECT_EQ(0x3412U, result.frame.address);
    EXPECT_EQ(0x34U, result.frame.command);
    EXPECT_EQ(NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN, result.frame.quality);
    EXPECT_EQ(NX_EXTI_EVENT_COALESCED, result.frame.source_flags);
}
} /* namespace */
