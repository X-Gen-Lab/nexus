/**
 * \file            core_arch_probe.c
 * \brief           Opt-in actual Core object and retained mechanism costs
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-04
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/arch/cache.h"
#include "nexus/arch/cache_control.h"
#include "nexus/core/request_observer.h"
#include "nexus/os/diagnostic.h"
#include "nexus/os/wait.h"

#define COST_STORAGE __attribute__((used, section(".bss.os.probes")))
_Static_assert(NX_COST_INSTANCES == 0 || NX_COST_INSTANCES == 1 ||
                   NX_COST_INSTANCES == 2 || NX_COST_INSTANCES == 4,
               "The opt-in cost matrix has zero/one/two/four instances");

#if NX_COST_INSTANCES
nx_request_t nx_core_request__bytes[NX_COST_INSTANCES] COST_STORAGE;
nx_request_slot_t nx_core_slot__bytes[NX_COST_INSTANCES] COST_STORAGE;
nx_request_observer_t nx_core_observer__bytes[NX_COST_INSTANCES] COST_STORAGE;
nx_clock_t nx_core_clock__bytes[NX_COST_INSTANCES] COST_STORAGE;
nx_clock32_t nx_core_clock32__bytes[NX_COST_INSTANCES] COST_STORAGE;
#endif

static volatile uint32_t result_sink;
static volatile nx_time_us_t fixture_time;

void _start(void);

/** \brief Link-only clock; the fixture is never executed as firmware. */
nx_time_us_t nx_time_now_us(void) {
    return ++fixture_time;
}

#if NX_COST_INSTANCES
/** \brief Retain one injected-clock call without a runtime clock registry. */
static nx_time_us_t clock_read(void* context) {
    (void)context;
    return nx_time_now_us();
}
#endif

/** \brief True sizeof values belong to this compiler and CPU ABI. */
static void size_symbols(void) {
    __asm__ volatile(
        ".global __nexus_core_size_request\n"
        ".equ __nexus_core_size_request,%c0\n"
        ".global __nexus_core_size_slot\n"
        ".equ __nexus_core_size_slot,%c1\n"
        ".global __nexus_core_size_observer\n"
        ".equ __nexus_core_size_observer,%c2\n"
        ".global __nexus_core_size_clock\n"
        ".equ __nexus_core_size_clock,%c3\n"
        ".global __nexus_core_size_clock32\n"
        ".equ __nexus_core_size_clock32,%c4\n"
        ".global __nexus_core_size_diagnostic_event\n"
        ".equ __nexus_core_size_diagnostic_event,%c5\n"
        ".global __nexus_core_size_diagnostic_ring\n"
        ".equ __nexus_core_size_diagnostic_ring,%c6\n"
        ".global __nexus_core_size_wait_source\n"
        ".equ __nexus_core_size_wait_source,%c7\n"
        :
        : "i"(sizeof(nx_request_t)), "i"(sizeof(nx_request_slot_t)),
          "i"(sizeof(nx_request_observer_t)), "i"(sizeof(nx_clock_t)),
          "i"(sizeof(nx_clock32_t)), "i"(sizeof(nx_diagnostic_event_t)),
          "i"(sizeof(nx_diagnostic_ring_t)), "i"(sizeof(nx_wait_source_t)));
}

/** \brief Compile and link the real selected helpers with explicit storage. */
void _start(void) {
    size_symbols();
    nx_time_us_t deadline = 0;
    result_sink = (uint32_t)nx_deadline_finite_after(1, 2, &deadline);
#if NX_COST_INSTANCES
    for (size_t index = 0; index < NX_COST_INSTANCES; ++index) {
        nx_request_t* request = &nx_core_request__bytes[index];
        nx_request_slot_t* slot = &nx_core_slot__bytes[index];
        nx_request_observer_t* observer = &nx_core_observer__bytes[index];
        uint64_t epoch = 0;
        nx_request_observation_t observation;
        nx_request_initialize(request);
        result_sink += (uint32_t)nx_request_prepare(request, deadline);
        result_sink += (uint32_t)nx_request_slot_bind(slot, request, &epoch);
        result_sink +=
            (uint32_t)nx_request_observer_bind(observer, slot, epoch);
        result_sink += (uint32_t)nx_request_admit(request, NX_REQUEST_ACTIVE);
        result_sink +=
            (uint32_t)nx_request_observer_sample(observer, 2, &observation);
        nx_request_settle(request, NX_SUCCESS, 0);
        nx_request_observer_detach(observer);
        result_sink += (uint32_t)nx_request_slot_release(slot, epoch);
        nx_clock32_t* clock = &nx_core_clock32__bytes[index];
        result_sink += (uint32_t)nx_clock32_initialize(clock, 0, 1000000);
        result_sink += (uint32_t)nx_clock32_observe(clock, 1, &deadline);
        nx_core_clock__bytes[index] = (nx_clock_t){clock_read, NULL};
        result_sink += (uint32_t)nx_core_clock__bytes[index].read(NULL);
    }
#endif
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_arch_irq_restore(saved);
    result_sink += (uint32_t)nx_arch_dcache_clean(0x20000000U, 32);
    nx_arch_cache_control_t control = {0};
    result_sink +=
        (uint32_t)nx_arch_cache_cold_enable(&control, NX_ARCH_CACHE_DATA);
    result_sink += (uint32_t)nx_arch_cache_cold_disable(&control);
    for (;;) {
        __asm__ volatile("nop");
    }
}
