/**
 * \file            uart_fifo_acquisition_probe_test.cpp
 * \brief           Internal wrapper dispatch survives observation failures.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/cycle_probe.h"
#if defined(FIFO_FIXTURE_H743)
#include "stm32h743_uart_fifo.h"
using State = nx_stm32h743_uart_fifo_state_t;
#define FIFO_REAL __real_nx_stm32h743_uart_fifo_irq
#define FIFO_WRAP __wrap_nx_stm32h743_uart_fifo_irq
#else
#include "stm32u575_uart_fifo.h"
using State = nx_stm32u575_uart_fifo_state_t;
#define FIFO_REAL __real_nx_stm32u575_uart_fifo_irq
#define FIFO_WRAP __wrap_nx_stm32u575_uart_fifo_irq
#endif
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
class Ports {
  public:
    MOCK_METHOD(nx_result_t, begin,
                (nx_io_cycle_probe_t*, nx_io_cycle_token_t*));
    MOCK_METHOD(nx_result_t, end, (nx_io_cycle_probe_t*, nx_io_cycle_token_t*));
    MOCK_METHOD(void, real, (State*));
    MOCK_METHOD(void, enter, ());
    MOCK_METHOD(void, leave, ());
};
Ports* current;
} /* namespace */

extern "C" nx_io_cycle_probe_t g_fifo_fixture_irq_probe;
extern "C" void FIFO_WRAP(State* state);
extern "C" void FIFO_REAL(State* state) {
    current->real(state);
}
extern "C" nx_result_t nx_io_cycle_probe_begin(nx_io_cycle_probe_t* probe,
                                               nx_io_cycle_token_t* token) {
    return current->begin(probe, token);
}
extern "C" nx_result_t nx_io_cycle_probe_end(nx_io_cycle_probe_t* probe,
                                             nx_io_cycle_token_t* token) {
    return current->end(probe, token);
}
extern "C" void fifo_station_dispatch_enter(void) {
    current->enter();
}
extern "C" void fifo_station_dispatch_leave(void) {
    current->leave();
}

TEST(FifoFixtureProbe, RealDispatchOccursOnceWhenCounterIsUnavailable) {
    testing::StrictMock<Ports> ports;
    current = &ports;
    State state{};
    nx_io_cycle_token_t* started = nullptr;
    testing::InSequence sequence;
    EXPECT_CALL(ports, begin(&g_fifo_fixture_irq_probe, testing::_))
        .WillOnce(testing::Invoke(
            [&](nx_io_cycle_probe_t* probe, nx_io_cycle_token_t* token) {
                EXPECT_EQ(token->owner, nullptr);
                token->owner = probe;
                token->available = false;
                started = token;
                return NX_SUCCESS;
            }));
    EXPECT_CALL(ports, enter());
    EXPECT_CALL(ports, real(&state)).Times(1);
    EXPECT_CALL(ports, leave());
    EXPECT_CALL(ports, end(&g_fifo_fixture_irq_probe, testing::_))
        .WillOnce(testing::Invoke(
            [&](nx_io_cycle_probe_t* probe, nx_io_cycle_token_t* token) {
                EXPECT_EQ(token, started);
                EXPECT_EQ(token->owner, probe);
                EXPECT_FALSE(token->available);
                token->owner = nullptr;
                return NX_SUCCESS;
            }));
    FIFO_WRAP(&state);
    current = nullptr;
}

TEST(FifoFixtureProbe, ReentrantObservationCannotDropOrRepeatProviderWork) {
    testing::StrictMock<Ports> ports;
    current = &ports;
    State state{};
    testing::InSequence sequence;
    EXPECT_CALL(ports, begin(&g_fifo_fixture_irq_probe, testing::_))
        .WillOnce(testing::Return(NX_ERROR_BUSY));
    EXPECT_CALL(ports, enter());
    EXPECT_CALL(ports, real(&state)).Times(1);
    EXPECT_CALL(ports, leave());
    EXPECT_CALL(ports, end(&g_fifo_fixture_irq_probe, testing::_))
        .WillOnce(testing::Return(NX_ERROR_STATE));
    FIFO_WRAP(&state);
    current = nullptr;
}
