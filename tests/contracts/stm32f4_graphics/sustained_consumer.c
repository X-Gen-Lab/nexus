/**
 * \file            sustained_consumer.c
 * \brief           Two static graphics buffers through replacement and restart
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include "sustained_sequence.h"

#define FRAME_BYTES    (64U * 32U * 2U)
#define EXECUTOR_POLLS 200000U
#ifndef NX_GRAPHICS_SDRAM_REFERENCE
static _Alignas(4) uint8_t pixels[2][FRAME_BYTES];
#else
/* A mistaken CPU-window selection must not lose a successfully granted loan. */
static nx_memory_window_lease_t unexpected_cpu;
static bool unexpected_cpu_live;
#endif
static nx_graphics_reference_t sequence;

/** \brief SDRAM grants engine regions, never CPU access or per-frame reuse.
 */
static nx_result_t destinations(nx_image_destination_t buffers[2]) {
#ifdef NX_GRAPHICS_SDRAM_REFERENCE
    const nx_external_bus_port_t* bus =
        nx_factory_external_bus_ref(NX_EXTERNAL_BUS_REF_MEMORY);
    const nx_memory_window_port_t* window =
        nx_factory_memory_window_ref(NX_MEMORY_WINDOW_REF_RAM);
    nx_memory_window_geometry_t geometry;
    if (nx_external_bus_port_state(bus) != NX_EXTERNAL_BUS_PREPARED ||
        nx_memory_window_port_geometry(window, &geometry) != NX_SUCCESS ||
        geometry.address != UINT32_C(0xc0000000) ||
        geometry.bytes < 2U * FRAME_BYTES)
        return NX_ERROR_PERMISSION;
    nx_result_t access = nx_memory_window_port_acquire(window, &unexpected_cpu);
    if (access == NX_SUCCESS) {
        unexpected_cpu_live = true;
        if (nx_memory_window_port_release(window, &unexpected_cpu,
                                          unexpected_cpu.epoch) == NX_SUCCESS)
            unexpected_cpu_live = false;
        return NX_ERROR_PERMISSION;
    }
    if (access != NX_ERROR_UNSUPPORTED)
        return access;
    for (unsigned i = 0U; i < 2U; ++i)
        buffers[i] = (nx_image_destination_t){
            (void*)(geometry.address + (uintptr_t)i * FRAME_BYTES), FRAME_BYTES,
            128U, NX_IMAGE_RGB565};
#else
    for (unsigned i = 0U; i < 2U; ++i)
        buffers[i] = (nx_image_destination_t){pixels[i], FRAME_BYTES, 128U,
                                              NX_IMAGE_RGB565};
#endif
    return NX_SUCCESS;
}
/** \brief Caller-owned polling demonstrates composition, not a hidden worker.
 */
static nx_result_t run(void) {
    nx_time_us_t deadline;
    nx_result_t result =
        nx_deadline_finite_after(nx_time_now_us(), 1000000U, &deadline);
    if (result != NX_SUCCESS)
        return result;
    for (unsigned i = 0U; i < EXECUTOR_POLLS; ++i) {
        result = nx_graphics_reference_step(&sequence, deadline);
        if (result != NX_ERROR_BUSY)
            return result;
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
    }
    return NX_ERROR_BUSY;
}
/** \brief Preserve every descriptor and framebuffer until actual cold join.
 */
static nx_result_t join(void) {
    nx_time_us_t deadline;
    nx_result_t budget =
        nx_deadline_finite_after(nx_time_now_us(), 100000U, &deadline);
    nx_result_t result = NX_ERROR_BUSY;
    for (unsigned i = 0U; i < EXECUTOR_POLLS; ++i) {
        result = nx_graphics_reference_stop(&sequence);
        if (result == NX_SUCCESS)
            return NX_SUCCESS;
        if (budget != NX_SUCCESS ||
            nx_deadline_expired(deadline, nx_time_now_us()))
            break;
    }
    return result;
}
/** \brief Link two finite handover sequences with complete Platform restart.
 * \note Software-reference panel timing and SDRAM geometry do not qualify
 *       hardware. DMA2D never writes an active/pending LTDC frame, even while
 *       both controllers hold the same graphics-owned bank. Errors preserve
 *       static controls and pixels; no failed proof authorizes forced reuse.
 */
int main(void) {
    for (unsigned cycle = 0U; cycle < 2U; ++cycle) {
        nx_platform_start_result_t started = nx_platform_start();
        if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
            return 1;
        const nx_image_port_t* image =
            nx_factory_image_ref(NX_IMAGE_REF_BLITTER);
        const nx_display_port_t* display =
            nx_factory_display_ref(NX_DISPLAY_REF_PANEL);
        nx_image_destination_t buffers[2];
        nx_result_t result = destinations(buffers);
        if (result == NX_SUCCESS)
            result = nx_graphics_reference_initialize(&sequence, image, display,
                                                      buffers, 4U);
        if (result != NX_SUCCESS) {
#ifdef NX_GRAPHICS_SDRAM_REFERENCE
            if (unexpected_cpu_live)
                return (int)result;
#endif
            (void)nx_platform_stop();
            return (int)result;
        }
        result = run();
        nx_result_t stopped = join();
        if (stopped != NX_SUCCESS)
            return (int)stopped;
        nx_result_t closed = nx_platform_stop();
        if (result != NX_SUCCESS)
            return (int)result;
        if (sequence.first_failure != NX_SUCCESS)
            return (int)sequence.first_failure;
        if (closed != NX_SUCCESS)
            return (int)closed;
    }
    return 0;
}
