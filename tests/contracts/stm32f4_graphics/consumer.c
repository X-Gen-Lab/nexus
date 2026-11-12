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

/** \brief A failed fixture retains state for diagnosis, never force-releases.
 */
static void halt(void) {
    for (;;) {
    }
}

/** \brief Exercise selected public symbols; ARM linking does not execute them.
 * \note Timings and routes are software references with no selected physical
 *       panel. This consumer allocates no framebuffer and makes no scanout,
 *       image-content, bus-bandwidth or physical validation claim.
 */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        halt();
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
    while (nx_platform_stop() != NX_SUCCESS) {
    }
    halt();
}
