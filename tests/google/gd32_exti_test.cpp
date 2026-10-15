/**
 * \file            gd32_exti_test.cpp
 * \brief           Production EXTI admission and selected-domain isolation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
extern "C" {
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
extern uint64_t g_gd32_model_now;
extern bool g_gd32_model_isr;
extern uint32_t g_gd32_model_mask;
extern uint32_t g_gd32_model_barrier_advance;
/** \brief The EXTI fixture never injects a UART completion. */
void USART0_IRQHandler(void) {
}
static uint32_t s_refused_inten;
static uint32_t s_refused_source_enable;
static bool s_refused_nvic_enable;
static nx_gd32_exti_state_t* s_publishing;
static bool s_published_before_enable;
static uint64_t s_enable_observed_time;
static uint32_t s_refused_pending;
static uint32_t s_refused_triggers;
static bool s_refused_nvic_disable;
static bool s_refused_nvic_pending;
static unsigned s_exti_clear_calls;
static unsigned s_nvic_disable_calls;
static unsigned s_nvic_pending_calls;
/** \brief Model an INTEN enable that is not observable after the write. */
volatile uint32_t* nx_gd32_exti_model_inten(void) {
    volatile uint32_t* reg = (volatile uint32_t*)(uintptr_t)(EXTI + 0x00U);
    *reg &= ~s_refused_source_enable;
    return reg;
}
/** \brief Observe state and hardware at the external IRQ publication leaf. */
void nx_gd32_exti_model_enable_irq(unsigned irq) {
    if (s_publishing != nullptr) {
        s_published_before_enable =
            s_publishing->initialized && s_publishing->domain != nullptr &&
            s_publishing->events != nullptr && s_publishing->capacity > 0;
        s_enable_observed_time = g_gd32_model_now;
    }
    if (!s_refused_nvic_enable) {
        NVIC->ISER[irq / 32U] |= UINT32_C(1) << (irq % 32U);
    }
}
/** \brief Model W1C and a source mask that rejected the earlier write. */
void nx_gd32_exti_model_clear(uint32_t mask) {
    ++s_exti_clear_calls;
    EXTI_INTEN |= mask & s_refused_inten;
    EXTI_RTEN |= mask & s_refused_triggers;
    EXTI_PD &= ~(mask & ~s_refused_pending);
}
/** \brief An injected NVIC write failure keeps the enabled bit observable. */
void nx_gd32_exti_model_disable_irq(unsigned irq) {
    ++s_nvic_disable_calls;
    uint32_t bit = UINT32_C(1) << (irq % 32U);
    NVIC->ICER[irq / 32U] = bit;
    if (!s_refused_nvic_disable) {
        NVIC->ISER[irq / 32U] &= ~bit;
    }
}
/** \brief An injected pending clear failure never makes the IRQ disappear. */
void nx_gd32_exti_model_clear_irq(unsigned irq) {
    ++s_nvic_pending_calls;
    uint32_t bit = UINT32_C(1) << (irq % 32U);
    NVIC->ICPR[irq / 32U] = bit;
    if (!s_refused_nvic_pending) {
        NVIC->ISPR[irq / 32U] &= ~bit;
    }
}
}
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

class Gd32Exti : public testing::Test {
  protected:
    nx_gd32_exti_state_t first{};
    nx_gd32_exti_state_t second{};
    nx_gd32_exti_state_t* const selected[2]{&first, &second};
    const nx_gd32_exti_domain_t domain{selected, 2};
    nx_exti_event_t first_events[2]{};
    nx_exti_event_t second_events[2]{};
    const nx_exti_port_t first_face{&nx_gd32_exti_ops, &first};
    const nx_exti_port_t second_face{&nx_gd32_exti_ops, &second};
    static void SetUpTestSuite() {
        void* p =
            mmap(reinterpret_cast<void*>(0x40000000U), 0x40000U,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        ASSERT_EQ(reinterpret_cast<void*>(0x40000000U), p);
    }
    static void TearDownTestSuite() {
        EXPECT_EQ(0, munmap(reinterpret_cast<void*>(0x40000000U), 0x40000U));
    }
    void SetUp() override {
        std::memset(reinterpret_cast<void*>(0x40000000U), 0, 0x40000U);
        std::memset(&g_gd32_model_nvic, 0, sizeof(g_gd32_model_nvic));
        g_gd32_model_now = 0;
        g_gd32_model_isr = false;
        g_gd32_model_mask = 0;
        s_refused_inten = 0;
        s_refused_source_enable = 0;
        s_refused_nvic_enable = false;
        s_publishing = nullptr;
        s_published_before_enable = false;
        s_enable_observed_time = 0;
        g_gd32_model_barrier_advance = 0;
        s_refused_pending = 0;
        s_refused_triggers = 0;
        s_refused_nvic_disable = false;
        s_refused_nvic_pending = false;
        s_exti_clear_calls = 0;
        s_nvic_disable_calls = 0;
        s_nvic_pending_calls = 0;
    }
    void TearDown() override {
        s_refused_inten = 0;
        s_refused_source_enable = 0;
        s_refused_nvic_enable = false;
        s_publishing = nullptr;
        s_published_before_enable = false;
        s_enable_observed_time = 0;
        g_gd32_model_barrier_advance = 0;
        s_refused_pending = 0;
        s_refused_triggers = 0;
        s_refused_nvic_disable = false;
        s_refused_nvic_pending = false;
        if (first.initialized) {
            EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
        }
        if (second.initialized) {
            EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&second_face));
        }
    }
};
TEST_F(Gd32Exti, ForeignEnabledLineRejectsBeforePinOrMuxChanges) {
    EXTI_INTEN = 1U << 3U;
    GPIO_CTL(GPIOE) = 0x12345678U;
    const uint32_t clock = RCU_AHB1EN;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    EXPECT_EQ(0x12345678U, GPIO_CTL(GPIOE));
    EXPECT_EQ(clock, RCU_AHB1EN);
    EXPECT_EQ(1U << 3U, EXTI_INTEN);
}
TEST_F(Gd32Exti, UnownedEnabledSharedVectorRejectsPriorityTakeover) {
    NVIC_EnableIRQ(EXTI5_9_IRQn);
    NVIC_SetPriority(EXTI5_9_IRQn, 2);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32_exti_initialize(&first, &domain, 5, 0, NX_EXTI_RISING,
                                      first_events, 2, 5));
    EXPECT_EQ(2U, NVIC_GetPriority(EXTI5_9_IRQn));
    EXPECT_EQ(0U, EXTI_INTEN);
    EXPECT_FALSE(first.initialized);
}
TEST_F(Gd32Exti, DomainRejectsMissingDuplicateOrForeignStorageBeforeWrites) {
    const nx_gd32_exti_domain_t empty{selected, 0};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32_exti_initialize(&first, &empty, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    nx_gd32_exti_state_t* const duplicate[]{&first, &first};
    const nx_gd32_exti_domain_t repeated{duplicate, 2};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32_exti_initialize(&first, &repeated, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    nx_gd32_exti_state_t* const null_member[]{&first, nullptr};
    const nx_gd32_exti_domain_t invalid{null_member, 2};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32_exti_initialize(&first, &invalid, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    nx_gd32_exti_state_t foreign{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32_exti_initialize(&foreign, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    const nx_gd32_exti_domain_t excessive{selected, 17};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_gd32_exti_initialize(&first, &excessive, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    EXPECT_EQ(0U, RCU_AHB1EN);
    EXPECT_EQ(0U, EXTI_INTEN);
}
TEST_F(Gd32Exti, DuplicateLineAndSharedPriorityRejectBeforeTakingPins) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 5, 0, NX_EXTI_RISING,
                                      first_events, 2, 5));
    const uint32_t first_control = GPIO_CTL(GPIOA);
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32_exti_initialize(&second, &domain, 5, 1, NX_EXTI_RISING,
                                      second_events, 2, 5));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32_exti_initialize(&second, &domain, 7, 1, NX_EXTI_FALLING,
                                      second_events, 2, 6));
    EXPECT_EQ(first_control, GPIO_CTL(GPIOA));
    EXPECT_EQ(0U, GPIO_CTL(GPIOB));
    EXPECT_EQ(5U, NVIC_GetPriority(EXTI5_9_IRQn));
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&second, &domain, 7, 1, NX_EXTI_FALLING,
                                      second_events, 2, 5));
}
TEST_F(Gd32Exti, ForeignSharedLineCannotBeTakenByAnotherDomain) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 5, 0, NX_EXTI_RISING,
                                      first_events, 2, 5));
    nx_gd32_exti_state_t* const foreign_selection[]{&second};
    const nx_gd32_exti_domain_t foreign{foreign_selection, 1};
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32_exti_initialize(&second, &foreign, 7, 1, NX_EXTI_FALLING,
                                      second_events, 2, 5));
    EXPECT_EQ(1U << 5U, EXTI_INTEN);
    EXPECT_FALSE(second.initialized);
}
TEST_F(Gd32Exti, PendingAndActiveUnownedVectorRejectWithoutClearingIt) {
    const unsigned word = static_cast<unsigned>(EXTI3_IRQn) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(EXTI3_IRQn) % 32U);
    NVIC->ISPR[word] = bit;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    EXPECT_EQ(bit, NVIC->ISPR[word]);
    NVIC->ISPR[word] = 0;
    NVIC->IABR[word] = bit;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    NVIC->IABR[word] = 0;
    EXPECT_EQ(0U, RCU_AHB1EN);
}
TEST_F(Gd32Exti, SharedStopPreservesOtherPendingLineAndFinalOwnerClearsNvic) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 5, 0, NX_EXTI_RISING,
                                      first_events, 2, 5));
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&second, &domain, 7, 1, NX_EXTI_FALLING,
                                      second_events, 2, 5));
    const unsigned word = static_cast<unsigned>(EXTI5_9_IRQn) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(EXTI5_9_IRQn) % 32U);
    NVIC->ISPR[word] = bit;
    EXTI_PD = (1U << 5U) | (1U << 7U);
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
    EXPECT_EQ(1U << 7U, EXTI_INTEN);
    EXPECT_EQ(1U << 7U, EXTI_PD);
    EXPECT_NE(0U, NVIC->ISER[word] & bit);
    EXPECT_EQ(bit, NVIC->ISPR[word]);
    g_gd32_model_isr = true;
    nx_gd32_exti_dispatch(&domain, 0x3E0U);
    g_gd32_model_isr = false;
    EXPECT_EQ(0U, first.count);
    EXPECT_EQ(1U, second.count);
    EXPECT_EQ(0U, EXTI_PD);
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&second_face));
    EXPECT_EQ(0U, NVIC->ISER[word] & bit);
    EXPECT_EQ(0U, NVIC->ISPR[word] & bit);
    EXPECT_EQ(nullptr, first.domain);
    EXPECT_EQ(nullptr, second.domain);
}
TEST_F(Gd32Exti, ActiveSharedVectorRetainsQueueAndWakeUntilExit) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 5, 0, NX_EXTI_RISING,
                                      first_events, 2, 5));
    const unsigned word = static_cast<unsigned>(EXTI5_9_IRQn) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(EXTI5_9_IRQn) % 32U);
    NVIC->IABR[word] = bit;
    EXPECT_EQ(NX_ERROR_BUSY, nx_exti_port_stop(&first_face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&domain, first.domain);
    EXPECT_EQ(1U << 5U, EXTI_INTEN);
    NVIC->IABR[word] = 0;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
}
class ExtiWake {
  public:
    MOCK_METHOD(nx_result_t, signal, ());
    static nx_result_t Signal(void* context) {
        return static_cast<ExtiWake*>(context)->signal();
    }
};
TEST_F(Gd32Exti, SelectedDispatchSignalsOnceAndLeavesUnownedPendingUntouched) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    testing::StrictMock<ExtiWake> observer;
    const nx_irq_wake_t wake{&observer, ExtiWake::Signal, false};
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&first_face, &wake, 0));
    EXPECT_CALL(observer, signal()).WillOnce(testing::Return(NX_SUCCESS));
    GPIO_ISTAT(GPIOE) = 1U << 3U;
    EXTI_INTEN |= 1U << 4U;
    EXTI_PD = (1U << 3U) | (1U << 4U);
    g_gd32_model_isr = true;
    nx_gd32_exti_dispatch(&domain, 0x1FU);
    g_gd32_model_isr = false;
    EXPECT_EQ(1U, first.count);
    EXPECT_EQ(NX_EXTI_RISING, first_events[0].edge);
    EXPECT_EQ(1U << 4U, EXTI_PD);
    EXPECT_EQ(1U << 4U, EXTI_INTEN & (1U << 4U));
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
}
TEST_F(Gd32Exti, QueueOverflowOrdersOlderFactsThenLossBeforeLaterReception) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    for (unsigned i = 0; i < 3; ++i) {
        EXTI_PD = 1U << 3U;
        g_gd32_model_isr = true;
        nx_gd32_exti_dispatch(&domain, 1U << 3U);
        g_gd32_model_isr = false;
    }
    nx_exti_event_t output[3]{};
    size_t count = 0;
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&first_face, output, 1, &count));
    EXPECT_EQ(1U, count);
    EXPECT_EQ(0U, output[0].flags & NX_EXTI_EVENT_LOSS);
    EXTI_PD = 1U << 3U;
    nx_gd32_exti_dispatch(&domain, 1U << 3U);
    EXPECT_EQ(1U, first.count);
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_read(&first_face, output, 3, &count));
    EXPECT_EQ(2U, count);
    EXPECT_EQ(0U, output[0].flags & NX_EXTI_EVENT_LOSS);
    EXPECT_NE(0U, output[1].flags & NX_EXTI_EVENT_LOSS);
    EXTI_PD = 1U << 3U;
    nx_gd32_exti_dispatch(&domain, 1U << 3U);
    EXPECT_EQ(1U, first.count);
}
TEST_F(Gd32Exti, FullSixteenLineDomainHasIndependentStorageAndBoundedDispatch) {
    nx_gd32_exti_state_t states[16]{};
    nx_exti_event_t events[16]{};
    nx_gd32_exti_state_t* members[16]{};
    for (size_t i = 0; i < 16; ++i)
        members[i] = &states[i];
    const nx_gd32_exti_domain_t complete{members, 16};
    for (unsigned i = 0; i < 16; ++i) {
        ASSERT_EQ(NX_SUCCESS,
                  nx_gd32_exti_initialize(&states[i], &complete, i, 0,
                                          NX_EXTI_RISING, &events[i], 1, 5));
    }
    EXTI_PD = 0xFFFFU;
    g_gd32_model_isr = true;
    nx_gd32_exti_dispatch(&complete, 0xFFFFU);
    g_gd32_model_isr = false;
    for (unsigned i = 0; i < 16; ++i) {
        EXPECT_EQ(1U, states[i].count);
        EXPECT_EQ(i, events[i].line);
        EXPECT_EQ(events[0].timestamp_us, events[i].timestamp_us);
        EXPECT_EQ(NX_SUCCESS, nx_gd32_exti_stop(&states[i]));
    }
    EXPECT_EQ(0U, EXTI_INTEN);
    EXPECT_EQ(0U, EXTI_PD);
}
TEST_F(Gd32Exti, UnselectedOrMalformedDomainDoesNotTouchPendingHardware) {
    EXTI_INTEN = 8;
    EXTI_PD = 8;
    const nx_gd32_exti_domain_t empty{selected, 0};
    nx_gd32_exti_dispatch(nullptr, 8);
    nx_gd32_exti_dispatch(&empty, 8);
    nx_gd32_exti_dispatch(&domain, 8);
    EXPECT_EQ(8U, EXTI_PD);
    EXPECT_EQ(0U, first.count);
    EXPECT_EQ(0U, second.count);
}

TEST_F(Gd32Exti, RefusedSourceMaskRetainsQueueWakeDomainAndRetry) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 5, 0, NX_EXTI_RISING,
                                      first_events, 2, 5));
    testing::StrictMock<ExtiWake> observer;
    const nx_irq_wake_t wake{&observer, ExtiWake::Signal, false};
    ASSERT_EQ(NX_SUCCESS, nx_exti_port_attach_wake(&first_face, &wake, 0));
    first.count = 1;
    first.lost = 2;
    s_refused_inten = 1U << 5U;
    const unsigned clears = s_exti_clear_calls;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&first_face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&domain, first.domain);
    EXPECT_EQ(&wake, first.wake);
    EXPECT_EQ(first_events, first.events);
    EXPECT_EQ(1U, first.count);
    EXPECT_EQ(2U, first.lost);
    EXPECT_EQ(clears + 1, s_exti_clear_calls);
    EXPECT_EQ(0U, s_nvic_disable_calls);
    EXPECT_EQ(0U, g_gd32_model_mask);
    EXTI_PD = 1U << 5U;
    EXPECT_CALL(observer, signal()).WillOnce(testing::Return(NX_SUCCESS));
    g_gd32_model_isr = true;
    nx_gd32_exti_dispatch(&domain, 0x3E0U);
    g_gd32_model_isr = false;
    EXPECT_EQ(3U, first.lost);
    s_refused_inten = 0;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
    EXPECT_FALSE(first.initialized);
    EXPECT_EQ(nullptr, first.domain);
    EXPECT_EQ(nullptr, first.wake);
}
TEST_F(Gd32Exti, RefusedPeripheralPendingClearRetainsOwnerWithoutClearingPeer) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 5, 0, NX_EXTI_RISING,
                                      first_events, 2, 5));
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&second, &domain, 7, 1, NX_EXTI_FALLING,
                                      second_events, 2, 5));
    const unsigned word = static_cast<unsigned>(EXTI5_9_IRQn) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(EXTI5_9_IRQn) % 32U);
    NVIC->ISPR[word] = bit;
    EXTI_PD = (1U << 5U) | (1U << 7U);
    first.count = 1;
    s_refused_pending = 1U << 5U;
    const unsigned clears = s_exti_clear_calls;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&first_face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&domain, first.domain);
    EXPECT_EQ(1U, first.count);
    EXPECT_EQ(1U << 7U, EXTI_INTEN);
    EXPECT_EQ((1U << 5U) | (1U << 7U), EXTI_PD);
    EXPECT_EQ(bit, NVIC->ISPR[word]);
    EXPECT_NE(0U, NVIC->ISER[word] & bit);
    EXPECT_EQ(0U, s_nvic_disable_calls);
    EXPECT_EQ(clears + 1, s_exti_clear_calls);
    g_gd32_model_isr = true;
    nx_gd32_exti_dispatch(&domain, 0x3E0U);
    g_gd32_model_isr = false;
    EXPECT_EQ(1U, first.count);
    EXPECT_EQ(1U, second.count);
    EXPECT_EQ(1U << 5U, EXTI_PD);
    s_refused_pending = 0;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
    EXPECT_EQ(0U, EXTI_PD);
    EXPECT_EQ(1U << 7U, EXTI_INTEN);
    EXPECT_NE(0U, NVIC->ISER[word] & bit);
}
TEST_F(Gd32Exti, RefusedLastOwnerNvicDisableRetainsStateUntilActualWithdrawal) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    const unsigned word = static_cast<unsigned>(EXTI3_IRQn) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(EXTI3_IRQn) % 32U);
    first.count = 1;
    s_refused_nvic_disable = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&first_face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&domain, first.domain);
    EXPECT_EQ(1U, first.count);
    EXPECT_NE(0U, NVIC->ISER[word] & bit);
    EXPECT_EQ(1U, s_nvic_disable_calls);
    EXPECT_EQ(1U, s_nvic_pending_calls);
    EXPECT_EQ(0U, EXTI_INTEN);
    EXPECT_EQ(0U, g_gd32_model_mask);
    s_refused_nvic_disable = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
    EXPECT_EQ(0U, NVIC->ISER[word] & bit);
    EXPECT_FALSE(first.initialized);
}
TEST_F(Gd32Exti, RefusedLastOwnerNvicPendingClearRetainsOwnerAndRetry) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    const unsigned word = static_cast<unsigned>(EXTI3_IRQn) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(EXTI3_IRQn) % 32U);
    NVIC->ISPR[word] = bit;
    s_refused_nvic_pending = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&first_face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&domain, first.domain);
    EXPECT_EQ(bit, NVIC->ISPR[word]);
    EXPECT_EQ(0U, NVIC->ISER[word] & bit);
    EXPECT_EQ(1U, s_nvic_disable_calls);
    EXPECT_EQ(1U, s_nvic_pending_calls);
    s_refused_nvic_pending = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
    EXPECT_EQ(0U, NVIC->ISPR[word] & bit);
    EXPECT_EQ(nullptr, first.domain);
}

TEST_F(Gd32Exti, RefusedTriggerWithdrawalRetainsOwnerAndPreservesPeer) {
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 5, 0, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&second, &domain, 7, 1, NX_EXTI_FALLING,
                                      second_events, 2, 5));
    s_refused_triggers = 1U << 5U;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&first_face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&domain, first.domain);
    EXPECT_TRUE(second.initialized);
    EXPECT_EQ(1U << 5U, EXTI_RTEN);
    EXPECT_EQ(1U << 7U, EXTI_FTEN);
    EXPECT_EQ(1U << 7U, EXTI_INTEN);
    EXPECT_EQ(0U, s_nvic_disable_calls);
    s_refused_triggers = 0;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
    EXPECT_EQ(0U, EXTI_RTEN);
    EXPECT_EQ(1U << 7U, EXTI_FTEN);
    EXPECT_TRUE(second.initialized);
}

TEST_F(Gd32Exti, RefusedNvicEnableDuringInitRetainsOwnerForActualStop) {
    s_refused_nvic_enable = true;
    EXPECT_EQ(NX_ERROR_IO,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&domain, first.domain);
    EXPECT_EQ(first_events, first.events);
    EXPECT_EQ(2U, first.capacity);
    s_refused_nvic_enable = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
    EXPECT_FALSE(first.initialized);
    EXPECT_EQ(nullptr, first.domain);
}
TEST_F(Gd32Exti, RefusedOwnSourceEnableRetainsInitAndFailedCleanupUntilRetry) {
    s_refused_source_enable = 1U << 3U;
    EXPECT_EQ(NX_ERROR_IO,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&domain, first.domain);
    EXPECT_EQ(first_events, first.events);
    EXPECT_EQ(0U, EXTI_INTEN & (1U << 3U));
    const unsigned word = static_cast<unsigned>(EXTI3_IRQn) / 32U;
    const uint32_t bit = 1U << (static_cast<unsigned>(EXTI3_IRQn) % 32U);
    NVIC->ISPR[word] = bit;
    s_refused_nvic_pending = true;
    EXPECT_EQ(NX_ERROR_IO, nx_exti_port_stop(&first_face));
    EXPECT_TRUE(first.initialized);
    EXPECT_EQ(&domain, first.domain);
    s_refused_nvic_pending = false;
    EXPECT_EQ(NX_SUCCESS, nx_exti_port_stop(&first_face));
    EXPECT_FALSE(first.initialized);
    EXPECT_EQ(nullptr, first.domain);
}
TEST_F(Gd32Exti, WholeStateAndBarrierPrecedeInterruptPublication) {
    s_publishing = &first;
    g_gd32_model_barrier_advance = 1;
    ASSERT_EQ(NX_SUCCESS,
              nx_gd32_exti_initialize(&first, &domain, 3, 4, NX_EXTI_BOTH,
                                      first_events, 2, 5));
    EXPECT_TRUE(s_published_before_enable);
    EXPECT_GE(s_enable_observed_time, 1U);
    EXPECT_EQ(&domain, first.domain);
    EXPECT_EQ(first_events, first.events);
    EXPECT_EQ(3U, first.line);
    EXPECT_EQ(4U, first.gpio_index);
    EXPECT_EQ(NX_EXTI_BOTH, first.edges);
    EXPECT_EQ(0U, g_gd32_model_mask);
}
