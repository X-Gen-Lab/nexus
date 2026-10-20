/**
 * \file            native-time-empty.c
 * \brief           Empty static factory and repeatable Native platform
 * lifecycle \author          Nexus Team \version         1.0.0 \date 2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/native/model.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
/** \brief Failure always maps to a real nonzero process result. */
#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            return 1;                                                          \
        }                                                                      \
    } while (0)
/** \brief A new independent process owns this exact empty assembly scenario. */
/* The Google qualification target renames this public C entry point. */
int main(void);

int main(void) {
    _Static_assert(NX_RTC_ID_COUNT == 0, "No selected RTC state");
    _Static_assert(NX_LPTIM_ID_COUNT == 0, "No selected LPTIM state");
    CHECK(nx_native_clock_configure(true, 19) == NX_SUCCESS);
    CHECK(nx_factory_rtc(0) == NULL);
    CHECK(nx_factory_lptim(0) == NULL);
    CHECK(nx_factory_rtc_capability(0) == NULL);
    CHECK(nx_factory_lptim_capability(0) == NULL);
    CHECK(nx_time_now_us() == 19U);
    for (unsigned cycle = 0; cycle < 2; ++cycle) {
        nx_platform_start_result_t start = nx_platform_start();
        CHECK(start.primary == NX_SUCCESS && start.cleanup == NX_SUCCESS &&
              start.remaining_effects == 0U);
        CHECK(nx_factory_rtc(0) == NULL && nx_factory_lptim(0) == NULL);
        CHECK(nx_time_now_us() == 19U);
        CHECK(nx_platform_stop() == NX_SUCCESS);
        CHECK(nx_platform_stop() == NX_SUCCESS);
    }
    CHECK(nx_native_clock_configure(false, 0) == NX_SUCCESS);
    return 0;
}
