/**
 * \file            main.c
 *
 * \brief           Retained CPU runtime software reference, never Board
 *                  startup or executable hardware acceptance.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/arch/atomic.h"
#include "nexus/arch/cache.h"
#include "nexus/arch/mpu.h"
#include "nexus/arch/security.h"
#include "nexus/arch/sleep.h"
#include "nexus/core/request.h"
#include "nexus/os/baremetal.h"
#include "nexus_config.h"
#if NEXUS_CPU_HAS_MVE
#include <arm_mve.h>
#endif
#if defined(NEXUS_BACKEND_FREERTOS)
#include "nexus/os/freertos.h"
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
static void (*const exception_references[])(void)
    __attribute__((used, section(".runtime.references"))) = {
        SVC_Handler, PendSV_Handler, SysTick_Handler};
#endif

void _start(void);
#if NEXUS_CPU_HAS_FPU
float nexus_runtime_float_probe(float first, float second);
static volatile float floating_sink;
__attribute__((noinline)) float nexus_runtime_float_probe(float first,
                                                          float second) {
    return first + second;
}
#endif
#if NEXUS_CPU_HAS_MVE
void nexus_runtime_vector_probe(uint32_t* output, const uint32_t* first,
                                const uint32_t* second);
static uint32_t vector_first[4] = {1u, 2u, 3u, 4u};
static uint32_t vector_second[4] = {5u, 6u, 7u, 8u};
static uint32_t vector_output[4];
__attribute__((noinline)) void
nexus_runtime_vector_probe(uint32_t* output, const uint32_t* first,
                           const uint32_t* second) {
    vst1q_u32(output, vaddq_u32(vld1q_u32(first), vld1q_u32(second)));
}
#if (__ARM_FEATURE_MVE & 2) != 0
void nexus_runtime_vector_float_probe(float* output, const float* first,
                                      const float* second);
static float vector_float_first[4] = {1.0f, 2.0f, 3.0f, 4.0f};
static float vector_float_second[4] = {5.0f, 6.0f, 7.0f, 8.0f};
static float vector_float_output[4];
__attribute__((noinline)) void
nexus_runtime_vector_float_probe(float* output, const float* first,
                                 const float* second) {
    vst1q_f32(output, vaddq_f32(vld1q_f32(first), vld1q_f32(second)));
}
#endif
#endif

static volatile uint32_t result_sink;
static uint32_t atomic_word;

/** \brief Synthetic software clock, not a physical monotonic-time provider. */
nx_time_us_t nx_time_now_us(void) {
    static nx_time_us_t ticks;
    return ++ticks;
}

static uint64_t fixture_clock(void* context) {
    (void)context;
    return nx_time_now_us();
}
static bool fixture_ready(void* context) {
    (void)context;
    return true;
}
#if defined(NEXUS_BACKEND_FREERTOS)
static void fixture_task(void* context) {
    (void)context;
}
#endif

/** \brief Retain real API bodies without claiming CPU/hardware execution. */
void _start(void) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    result_sink = masks.primask + masks.basepri + masks.faultmask;
    result_sink += (uint32_t)nx_arch_irq_is_masked();
    result_sink += nx_arch_exception_number();
    result_sink += (uint32_t)nx_arch_is_privileged();
    result_sink += (uint32_t)nx_arch_in_isr();
    nx_arch_irq_restore(saved);
    nx_arch_dmb();
    nx_arch_dsb();
    nx_arch_isb();
    bool slept = false;
    result_sink += (uint32_t)nx_arch_wait_for_interrupt();
    result_sink +=
        (uint32_t)nx_arch_idle_if_unchanged(&atomic_word, 0u, &slept);
    uint32_t cycles = 0;
    result_sink += (uint32_t)nx_arch_cycle_snapshot(&cycles);
    nx_arch_features_t features = nx_arch_features();
    result_sink += features.dcache_line_bytes;
    result_sink += (uint32_t)nx_arch_security_state();
    result_sink += (uint32_t)nx_arch_dcache_clean(0x20000000u, 32u);
    result_sink += (uint32_t)nx_arch_dcache_invalidate(0x20000000u, 32u);
    result_sink += (uint32_t)nx_arch_dcache_clean_invalidate(0x20000000u, 32u);
    result_sink += (uint32_t)nx_arch_instruction_sync(0x20000000u, 32u);
    nx_arch_mpu_v7_region_t v7 = {0};
    nx_arch_mpu_v7_encoding_t v7_words;
    nx_arch_mpu_v8_region_t v8 = {0};
    nx_arch_mpu_v8_encoding_t v8_words;
    result_sink += (uint32_t)nx_arch_mpu_v7_encode(&v7, &v7_words);
    result_sink += (uint32_t)nx_arch_mpu_v8_encode(&v8, &v8_words);
    result_sink += (uint32_t)nx_arch_mpu_v7_program(0, &v7);
    result_sink += (uint32_t)nx_arch_mpu_v8_program(0, &v8);
    result_sink += (uint32_t)nx_arch_mpu_v8_attribute_set(0, 0);
    nx_arch_sau_region_t sau = {0};
    nx_arch_sau_words_t sau_words;
    result_sink += (uint32_t)nx_arch_sau_encode(&sau, &sau_words);
    result_sink += (uint32_t)nx_arch_sau_write(0, &sau);
    result_sink += (uint32_t)nx_arch_sau_clear(0);
    nx_request_t request;
    nx_request_slot_t slot = {0};
    uint64_t epoch = 0;
    nx_result_t result = NX_SUCCESS;
    size_t transferred = 0;
    nx_request_initialize(&request);
    result_sink += (uint32_t)nx_request_prepare(&request, NX_DEADLINE_NEVER);
    result_sink += (uint32_t)nx_request_admit(&request, NX_REQUEST_ACTIVE);
    result_sink +=
        (uint32_t)nx_request_transition(&request, NX_REQUEST_DRAINING);
    result_sink += (uint32_t)nx_request_slot_bind(&slot, &request, &epoch);
    result_sink += (uint32_t)(nx_request_slot_lookup(&slot, epoch) != NULL);
    nx_request_settle(&request, NX_SUCCESS, 0);
    result_sink += (uint32_t)nx_request_state(&request);
    result_sink += (uint32_t)nx_request_result(&request, &result, &transferred);
    result_sink += (uint32_t)nx_request_slot_release(&slot, epoch);
    nx_clock32_t clock;
    nx_time_us_t now = 0;
    result_sink += (uint32_t)nx_clock32_initialize(&clock, 0, 48000000u);
    result_sink += (uint32_t)nx_clock32_observe(&clock, 1u, &now);
    result_sink +=
        (uint32_t)nx_deadline_expired(nx_deadline_after(now, 1u), now);
    nx_atomic_u32_store_release(&atomic_word, 1u);
    result_sink += nx_atomic_u32_load_acquire(&atomic_word);
    result_sink += nx_atomic_u32_fetch_add_acq_rel(&atomic_word, 1u);
    uint32_t expected = 2u;
    result_sink += (uint32_t)nx_atomic_u32_compare_exchange_acq_rel(
        &atomic_word, &expected, 3u);
#if defined(NEXUS_BACKEND_FREERTOS)
    static nx_freertos_notify_t notification;
    static nx_freertos_task_t task;
    _Alignas(portBYTE_ALIGNMENT) static StackType_t stack[128];
    static nx_freertos_permanent_task_t permanent;
    _Alignas(portBYTE_ALIGNMENT) static StackType_t permanent_stack[128];
    static nx_freertos_direct_notify_t direct;
    static nx_freertos_queue_t queue;
    static nx_freertos_closable_queue_t closable;
    static nx_freertos_queue_waiter_t waiter;
    static uint8_t items[8];
    static uint8_t closable_items[8];
    result_sink += (uint32_t)nx_freertos_notify_init(&notification);
    nx_wait_port_t port = nx_freertos_notify_port(&notification);
    result_sink += (uint32_t)nx_wait_until(&port, fixture_ready, NULL, 0);
    result_sink += (uint32_t)nx_freertos_isr_allowed();
    result_sink += (uint32_t)nx_freertos_task_start(
        &task, "reference", stack, 128u, 1u, fixture_task, NULL);
    result_sink += (uint32_t)nx_freertos_task_join(&task, 0);
    result_sink +=
        (uint32_t)nx_freertos_direct_notify_init(&direct, task.handle, 0u);
    nx_wait_port_t direct_port = nx_freertos_direct_notify_port(&direct);
    result_sink +=
        (uint32_t)nx_wait_until(&direct_port, fixture_ready, NULL, 0);
    result_sink += (uint32_t)nx_freertos_direct_notify_destroy(&direct);
    result_sink += (uint32_t)nx_freertos_permanent_task_start(
        &permanent, "permanent", permanent_stack, 128u, 1u, fixture_task, NULL);
    result_sink += (uint32_t)nx_freertos_queue_init(&queue, items, 8u, 2u, 4u);
    result_sink += (uint32_t)nx_freertos_queue_send(&queue, items, 0);
    result_sink += (uint32_t)nx_freertos_queue_receive(&queue, items, 0);
    result_sink += (uint32_t)nx_freertos_queue_send_until(&queue, items, 0);
    result_sink += (uint32_t)nx_freertos_queue_receive_until(&queue, items, 0);
    result_sink += (uint32_t)nx_freertos_queue_destroy(&queue);
    result_sink += (uint32_t)nx_freertos_notify_destroy(&notification);
    result_sink += (uint32_t)nx_freertos_queue_waiter_init(&waiter);
    result_sink += (uint32_t)nx_freertos_closable_queue_init(
        &closable, closable_items, 8u, 2u, 4u, 1u);
    result_sink += (uint32_t)nx_freertos_closable_queue_send_until(
        &closable, closable_items, 0, &waiter);
    result_sink += (uint32_t)nx_freertos_closable_queue_receive_until(
        &closable, closable_items, 0, &waiter);
    result_sink += (uint32_t)nx_freertos_closable_queue_close(&closable);
    result_sink += (uint32_t)nx_freertos_closable_queue_destroy(&closable);
    result_sink += (uint32_t)nx_freertos_queue_waiter_destroy(&waiter);
    vTaskStartScheduler();
#else
    nx_baremetal_notify_t notification;
    result_sink +=
        (uint32_t)nx_baremetal_notify_init(&notification, fixture_clock, NULL);
    nx_wait_port_t port = nx_baremetal_notify_port(&notification);
    result_sink += (uint32_t)nx_wait_until(&port, fixture_ready, NULL, 0);
    result_sink += (uint32_t)port.wake(port.context);
#endif
#if NEXUS_CPU_HAS_FPU
    floating_sink = nexus_runtime_float_probe(1.0f, 2.0f);
#endif
#if NEXUS_CPU_HAS_MVE
    nexus_runtime_vector_probe(vector_output, vector_first, vector_second);
#if (__ARM_FEATURE_MVE & 2) != 0
    nexus_runtime_vector_float_probe(vector_float_output, vector_float_first,
                                     vector_float_second);
#endif
#endif
    for (;;) {
    }
}
