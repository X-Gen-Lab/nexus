/**
 * \file            consumer.c
 * \brief           Public typed graphics factory and lifecycle link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/display.h"
#include "nexus/io/image.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/* The separate SDRAM reference selects an explicit graphics-owned endpoint.
 * This pointer grants no CPU lease and is used only as a hardware loan address.
 */
#ifdef NX_GRAPHICS_SDRAM_REFERENCE
#define framebuffer (*(uint16_t(*)[64U * 32U])(uintptr_t)0xc0000000U)
#else
/* Default linker storage is reviewed internal SRAM, never an SDRAM grant. */
_Alignas(4) static uint16_t framebuffer[64U * 32U];
#endif
static nx_image_request_t render;
#ifdef NX_DISPLAY_REF_PANEL
static nx_display_request_t scanout;
#endif

/** \brief A failed fixture retains state for diagnosis, never force-releases.
 */
static void halt(void) {
    for (;;) {
    }
}

/** \brief Exercise selected public symbols; ARM linking does not execute them.
 * \note Timings and routes are software references with no selected physical
 *       panel. The selected SRAM/SDRAM sequence is linked, not executed as HIL:
 *       DMA2D joins before LTDC borrows the same bytes, and LTDC joins before
 *       Platform close. No bus-bandwidth or physical output is qualified.
 */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        halt();
#ifdef NX_GRAPHICS_SDRAM_REFERENCE
    const nx_external_bus_port_t* bus =
        nx_factory_external_bus_ref(NX_EXTERNAL_BUS_REF_MEMORY);
    const nx_memory_window_port_t* window =
        nx_factory_memory_window_ref(NX_MEMORY_WINDOW_REF_RAM);
    nx_memory_window_geometry_t geometry;
    nx_memory_window_lease_t cpu = {0};
    if (nx_external_bus_port_state(bus) != NX_EXTERNAL_BUS_PREPARED ||
        nx_memory_window_port_geometry(window, &geometry) != NX_SUCCESS ||
        geometry.address != (uintptr_t)framebuffer ||
        geometry.bytes < sizeof(framebuffer) ||
        nx_memory_window_port_acquire(window, &cpu) != NX_ERROR_UNSUPPORTED)
        halt();
#endif
#ifdef NX_DISPLAY_REF_PANEL
    const nx_display_port_t* display =
        nx_factory_display_ref(NX_DISPLAY_REF_PANEL);
    nx_display_constraints_t limits;
    nx_display_snapshot_t snapshot;
    if (display == NULL ||
        nx_display_port_query(display, 0U, &limits) != NX_SUCCESS ||
        nx_display_port_snapshot(display, 0U, &snapshot) != NX_SUCCESS ||
        limits.width != 64U || limits.height != 32U)
        halt();
#endif
    const nx_image_port_t* image = nx_factory_image_ref(NX_IMAGE_REF_BLITTER);
    nx_image_constraints_t constraints;
    if (image == NULL ||
        nx_image_port_query(image, NX_IMAGE_BLEND, NX_IMAGE_RGB565,
                            NX_IMAGE_ARGB8888, NX_IMAGE_ARGB8888,
                            &constraints) != NX_SUCCESS ||
        constraints.address_alignment != 4U)
        halt();
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(nx_time_now_us(), 100000U, &deadline) !=
        NX_SUCCESS)
        halt();
    nx_request_initialize(&render.base);
    nx_image_operation_t fill = {.kind = NX_IMAGE_FILL,
                                 .width = 64U,
                                 .height = 32U,
                                 .destination = {framebuffer,
                                                 sizeof(framebuffer), 128U,
                                                 NX_IMAGE_RGB565},
                                 .fill_value = 0x6d5bU};
    if (nx_image_prepare(&render, &fill, deadline) != NX_SUCCESS ||
        nx_image_port_submit(image, &render) != NX_SUCCESS)
        halt();
    while (nx_request_state(&render.base) != NX_REQUEST_SETTLED) {
        nx_result_t progress = nx_image_port_service(image);
        if (progress != NX_SUCCESS && progress != NX_ERROR_BUSY)
            halt();
    }
    nx_result_t rendered;
    size_t bytes;
    if (nx_request_result(&render.base, &rendered, &bytes) != NX_SUCCESS ||
        rendered != NX_SUCCESS || bytes != sizeof(framebuffer))
        halt();
#ifdef NX_DISPLAY_REF_PANEL
    nx_request_initialize(&scanout.base);
    nx_display_operation_t frame = {.frame = {(const uint8_t*)framebuffer, 128U,
                                              128U, 32U, sizeof(framebuffer)},
                                    .format = NX_IMAGE_RGB565,
                                    .layer = 0U};
    if (nx_display_prepare(&scanout, &frame, NX_DEADLINE_NEVER) != NX_SUCCESS ||
        nx_display_port_submit(display, &scanout) != NX_SUCCESS)
        halt();
    do {
        nx_result_t progress = nx_display_port_service(display);
        if ((progress != NX_SUCCESS && progress != NX_ERROR_BUSY) ||
            nx_display_port_snapshot(display, 0U, &snapshot) != NX_SUCCESS ||
            snapshot.quarantined)
            halt();
    } while (!snapshot.visible);
    for (;;) {
        nx_result_t stopped = nx_display_port_stop(display);
        if (stopped == NX_SUCCESS)
            break;
        if (stopped != NX_ERROR_BUSY)
            halt();
        nx_result_t progress = nx_display_port_service(display);
        if (progress != NX_SUCCESS && progress != NX_ERROR_BUSY)
            halt();
    }
    if (nx_request_state(&scanout.base) != NX_REQUEST_SETTLED)
        halt();
#endif
    while (nx_platform_stop() != NX_SUCCESS) {
    }
    halt();
}
