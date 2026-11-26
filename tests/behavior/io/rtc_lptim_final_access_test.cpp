/**
 * \file            rtc_lptim_final_access_test.cpp
 * \brief           Real settlement final access and unread foreign identities
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "rtc_lptim_provider.h"
#include <cstdlib>
#include <gtest/gtest.h>
#include <new>
#include <sys/mman.h>
#include <unistd.h>

namespace {

nx_time_us_t Clock(void*) {
    return 0;
}

/** \brief Child failures produce a real nonzero result, never a swallowed
 * assert. */
void Require(bool condition) {
    if (!condition) {
        _exit(17);
    }
}

/** \brief A separate child owns and finally withdraws one real alarm. */
void RtcChild() {
    long bytes = sysconf(_SC_PAGESIZE);
    Require(bytes > 0);
    void* page =
        mmap(nullptr, static_cast<size_t>(bytes), PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void* foreign = mmap(nullptr, static_cast<size_t>(bytes), PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED && foreign != MAP_FAILED);
    auto* request = new (page) nx_rtc_request_t{};
    nx_request_initialize(&request->base);
    nx_native_rtc_state_t state{};
    const nx_clock_t clock{Clock, nullptr};
    const nx_rtc_port_t face{&nx_native_rtc_ops, &state};
    Require(nx_native_rtc_construct(&state, &clock) == NX_SUCCESS);
    const nx_rtc_calendar_t date{24, 1, 1, 1, 0, 0, 0};
    Require(nx_rtc_port_set(&face, &date) == NX_SUCCESS);
    nx_rtc_snapshot_t snapshot{};
    Require(nx_rtc_port_read(&face, &snapshot) == NX_SUCCESS);
    const nx_rtc_alarm_t alarm{1, 0, 0, 1};
    Require(nx_rtc_request_prepare(request, &snapshot.domain, &alarm,
                                   NX_DEADLINE_NEVER) == NX_SUCCESS);
    Require(nx_rtc_port_submit(&face, request) == NX_SUCCESS);
    Require(nx_rtc_port_cancel(&face, static_cast<nx_rtc_request_t*>(
                                          foreign)) == NX_ERROR_STATE);
    Require(nx_native_rtc_model_advance(&face, 1) == NX_SUCCESS);
    Require(nx_rtc_port_service(&face) == NX_SUCCESS);
    Require(nx_request_state(&request->base) == NX_REQUEST_SETTLED);
    Require(state.active == nullptr && !state.armed && !state.pending);
    Require(mprotect(page, static_cast<size_t>(bytes), PROT_NONE) == 0);
    Require(nx_rtc_port_service(&face) == NX_SUCCESS);
    Require(nx_rtc_port_cancel(&face, request) == NX_ERROR_STATE);
    Require(nx_native_rtc_model_advance(&face, 1) == NX_SUCCESS);
    Require(nx_rtc_port_stop(&face) == NX_SUCCESS);
    Require(nx_rtc_port_stop(&face) == NX_SUCCESS);
    Require(munmap(page, static_cast<size_t>(bytes)) == 0);
    Require(munmap(foreign, static_cast<size_t>(bytes)) == 0);
    _exit(0);
}

/** \brief A separate child proves finite-counter final access and identity. */
void CounterChild() {
    long bytes = sysconf(_SC_PAGESIZE);
    Require(bytes > 0);
    void* page =
        mmap(nullptr, static_cast<size_t>(bytes), PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void* foreign = mmap(nullptr, static_cast<size_t>(bytes), PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED && foreign != MAP_FAILED);
    auto* request = new (page) nx_lptim_request_t{};
    nx_request_initialize(&request->base);
    nx_native_lptim_state_t state{};
    const nx_clock_t clock{Clock, nullptr};
    const nx_lptim_port_t face{&nx_native_lptim_ops, &state};
    Require(nx_native_lptim_construct(&state, &clock, 32768) == NX_SUCCESS);
    nx_lptim_sample_t sample{};
    Require(nx_lptim_port_sample(&face, &sample) == NX_SUCCESS);
    Require(nx_lptim_request_prepare(request, &sample.domain, 10,
                                     NX_DEADLINE_NEVER) == NX_SUCCESS);
    Require(nx_lptim_port_submit(&face, request) == NX_SUCCESS);
    Require(nx_lptim_port_cancel(&face, static_cast<nx_lptim_request_t*>(
                                            foreign)) == NX_ERROR_STATE);
    Require(nx_native_lptim_model_advance(&face, 3) == NX_SUCCESS);
    Require(nx_lptim_port_cancel(&face, request) == NX_SUCCESS);
    Require(nx_request_state(&request->base) == NX_REQUEST_SETTLED);
    Require(state.active == nullptr && !state.armed && !state.pending);
    Require(mprotect(page, static_cast<size_t>(bytes), PROT_NONE) == 0);
    Require(nx_lptim_port_service(&face) == NX_SUCCESS);
    Require(nx_lptim_port_cancel(&face, request) == NX_ERROR_STATE);
    Require(nx_native_lptim_model_advance(&face, 7) == NX_SUCCESS);
    Require(nx_lptim_port_stop(&face) == NX_SUCCESS);
    Require(nx_lptim_port_stop(&face) == NX_SUCCESS);
    Require(munmap(page, static_cast<size_t>(bytes)) == 0);
    Require(munmap(foreign, static_cast<size_t>(bytes)) == 0);
    _exit(0);
}

TEST(NativeTimeFinalAccess, RTCSettledPageIsNeverReadByLaterOperations) {
    EXPECT_EXIT(RtcChild(), testing::ExitedWithCode(0), "");
}

TEST(NativeTimeFinalAccess,
     CounterFinalAccessAndForeignControlNeedNoRequestRead) {
    EXPECT_EXIT(CounterChild(), testing::ExitedWithCode(0), "");
}

} /* namespace */
