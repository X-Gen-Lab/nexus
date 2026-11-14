/**
 * \file            sustained_sequence.h
 * \brief           Caller-owned two-buffer graphics composition link fixture
 * \author          Nexus Team
 */
#ifndef NX_GRAPHICS_REFERENCE_SEQUENCE_H
#define NX_GRAPHICS_REFERENCE_SEQUENCE_H
#include "nexus/io/display.h"
#include "nexus/io/image.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Fixture storage outlives both providers and every failed withdrawal.
 * \note This is test-consumer composition, not a platform service or queue.
 *       Only accepted requests borrow buffers. A bank lease never grants
 *       permission to render over a still-borrowed scanout frame.
 */
typedef struct {
    const nx_image_port_t* image;
    const nx_display_port_t* display;
    nx_image_destination_t buffers[2];
    nx_image_request_t render;
    nx_display_request_t scanout[2];
    size_t width;
    size_t height;
    unsigned target;
    unsigned rendered;
    unsigned presented;
    unsigned retired;
    nx_result_t first_failure;
    uint8_t rendering;
    bool render_loan;
    bool display_loan[2];
    bool ready[2];
    bool initialized;
    bool stopping;
    bool stopped;
} nx_graphics_reference_t;
/** \brief Connect prepared public ports and two disjoint RGB565 buffers.
 * \param[in,out] state: Zero fixture, or a successfully stopped old fixture.
 * \param[in] image: Prepared image port; remains live through successful stop.
 * \param[in] display: Prepared layer-zero display port under the same executor.
 * \param[in] buffers: Exactly two independent caller-owned destinations.
 * \param[in] frames: Positive finite count of image/display submissions.
 * \return SUCCESS or rejection without starting a transfer or retaining a loan.
 * \note Caller owns Platform start and restart; no scheduler is introduced.
 */
nx_result_t nx_graphics_reference_initialize(
    nx_graphics_reference_t* state, const nx_image_port_t* image,
    const nx_display_port_t* display, const nx_image_destination_t buffers[2],
    unsigned frames);
/** \brief Advance one bounded step and submit only currently unborrowed bytes.
 * \param[in,out] state: Same stable initialized fixture and request storage.
 * \param[in] deadline: Finite absolute deadline for any image work submitted.
 * \return BUSY while progressing, SUCCESS after the finite handover prefix,
 *         or an error requiring explicit stop without reclaiming storage.
 * \note Display lifetimes explicitly use NEVER; snapshot.visible never grants
 *       reuse. Only acquire-observed SETTLED returns each accepted frame.
 */
nx_result_t nx_graphics_reference_step(nx_graphics_reference_t* state,
                                       nx_time_us_t deadline);
/** \brief Seal new work, join both providers and observe every request.
 * \param[in,out] state: Same retained fixture, requests and frame buffers.
 * \return SUCCESS after complete provider stop and all original requests
 *         SETTLED; otherwise the whole fixture remains available for retry.
 * \note Successful stop does not erase first_failure. Reinitialize only after
 *       external Platform shutdown/startup has prepared both ports again.
 */
nx_result_t nx_graphics_reference_stop(nx_graphics_reference_t* state);
#ifdef __cplusplus
}
#endif
#endif
