/**
 * \file            image.h
 * \brief           Explicit single-shot image-memory operations and loans
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_IMAGE_H
#define NEXUS_IO_IMAGE_H

#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#include "nexus/io/wake.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Memory values, independent of RGB wiring or panel color policy.
 * \note RGB565 stores native uint16_t values; ARGB8888 stores native uint32_t
 *       0xAARRGGBB values. A provider explicitly admits each operation/format.
 *       No byte-order conversion, gamma or premultiplication is implicit.
 */
typedef enum { NX_IMAGE_RGB565, NX_IMAGE_ARGB8888 } nx_image_format_t;

/** \brief One selected hardware task, not a software fallback pipeline. */
typedef enum {
    NX_IMAGE_FILL,
    NX_IMAGE_COPY,
    NX_IMAGE_CONVERT,
    NX_IMAGE_BLEND
} nx_image_kind_t;

/** \brief Explicit blend alpha source; no implicit premultiplication. */
typedef enum {
    NX_IMAGE_ALPHA_MULTIPLY,
    NX_IMAGE_ALPHA_REPLACE
} nx_image_alpha_t;

/** \brief Immutable source storage and complete row stride in bytes.
 * \note Capacity is caller-owned readable extent, including any declared
 *       padding. No access outside capacity or hidden readable padding is
 *       permitted. The request borrows this complete declared span.
 */
typedef struct {
    const void* data;
    size_t capacity;
    size_t stride_bytes;
    nx_image_format_t format;
} nx_image_source_t;

/** \brief Writable destination, distinct from the const source type. */
typedef struct {
    void* data;
    size_t capacity;
    size_t stride_bytes;
    nx_image_format_t format;
} nx_image_destination_t;

/** \brief Value tuple; referenced byte spans are not copied.
 * \note Width/height are pixels/rows. Every stride and extent is checked;
 *       no clipping, rotation, scaling or framebuffer allocation is implicit.
 *       FILL has zero source spans; fill_value is encoded in destination
 *       format (RGB565 uses the low16 bits). COPY/CONVERT use foreground only;
 *       BLEND uses both sources with straight per-pixel alpha multiplied by
 *       the explicit constant for MULTIPLY, or replaced by it for REPLACE.
 *       Unused source metadata, constants and alpha modes are zero.
 *       Direct565-to8888 conversion replicates channel MSBs into added low
 *       bits and adds opaque alpha; conversion to565 keeps channel MSBs.
 *       For ARGB output, RGB bits at resulting alpha0 have no defined value.
 *       Blend to565 may require a parameter-proven positive input alpha;
 *       no provider scans source pixels to guess whether output can be0.
 *       Unsupported combinations reject.
 */
typedef struct {
    nx_image_kind_t kind;
    nx_image_source_t foreground;
    nx_image_source_t background;
    nx_image_destination_t destination;
    size_t width;
    size_t height;
    uint32_t fill_value;
    uint8_t foreground_alpha;
    uint8_t background_alpha;
    nx_image_alpha_t foreground_alpha_mode;
    nx_image_alpha_t background_alpha_mode;
} nx_image_operation_t;

/** \brief Selected limits; operation/format query is side-effect-free. */
typedef struct {
    size_t address_alignment;
    size_t stride_alignment;
    size_t maximum_width;
    size_t maximum_height;
    size_t maximum_offset_pixels;
} nx_image_constraints_t;

/** \brief One operation retained through actual final memory access.
 * \note Initialize base before first preparation. ACCEPTED retains request
 *       and all source/destination capacities through acquire-observed
 *       SETTLED. SUCCESS reports width*height*destination-bytes-per-pixel,
 *       excluding stride padding. Error reports zero valid bytes, even when
 *       a partial destination was modified; no rollback copy is supplied.
 *       Cancel/timeout only start withdrawal. Refused engine/IRQ/address or
 *       bus withdrawal keeps QUARANTINED and every borrowed span.
 */
typedef struct {
    nx_request_t base;
    nx_image_operation_t operation;
} nx_image_request_t;

/** \brief Shared immutable operations, one serialized privileged owner. */
typedef struct {
    nx_result_t (*query)(void*, nx_image_kind_t, nx_image_format_t,
                         nx_image_format_t, nx_image_format_t,
                         nx_image_constraints_t*);
    nx_result_t (*submit)(void*, nx_image_request_t*);
    nx_result_t (*service)(void*);
    nx_result_t (*cancel)(void*, nx_image_request_t*);
    nx_result_t (*stop)(void*);
    nx_result_t (*attach_wake)(void*, const nx_irq_wake_t*, uint8_t);
} nx_image_ops_t;

/** \brief Stable controller identity; factory lookup has no hardware effect. */
struct nx_image_port {
    const nx_image_ops_t* ops;
    void* context;
};

/** \brief Query one exact operation/format combination without a borrow.
 * \param[in] port: Stable face and live owner storage, serialized Task use.
 * \param[in] kind: Selected fill/copy/convert/blend operation.
 * \param[in] foreground: Foreground memory format; ignored for FILL.
 * \param[in] background: Background format; used only for BLEND.
 * \param[in] destination: Output memory format.
 * \param[out] constraints: Independent output, unchanged on rejection.
 * \return SUCCESS, INVALID, UNSUPPORTED or provider error.
 * \note No fallback, worker or allocation. A known enum is not support.
 *       Geometry limits do not authorize every scalar alpha combination;
 *       admission separately checks a provider's parameter-proven alpha rule.
 */
nx_result_t nx_image_port_query(const nx_image_port_t* port,
                                nx_image_kind_t kind,
                                nx_image_format_t foreground,
                                nx_image_format_t background,
                                nx_image_format_t destination,
                                nx_image_constraints_t* constraints);

/** \brief Prepare a one-shot value tuple without reading any image bytes.
 * \param[in,out] request: Initialized READY or consumed SETTLED descriptor.
 * \param[in] operation: Copied value; its object is not retained.
 * \param[in] deadline: Finite absolute deadline in the live Core domain.
 * \return SUCCESS or INVALID/STATE/EXHAUSTED, unchanged on rejection.
 * \note Every referenced capacity is disjoint from the whole request.
 *       Source/destination overlap is rejected; this is not memmove. Provider
 *       admission also proves separation from full private state/config,
 *       resource/region facts and optional wake storage. No hidden padding.
 */
nx_result_t nx_image_prepare(nx_image_request_t* request,
                             const nx_image_operation_t* operation,
                             nx_time_us_t deadline);

/** \brief Admit one image operation without a queue or hidden task.
 * \param[in] port: Initialized privileged, IRQ-unmasked serialized owner.
 * \param[in,out] request: READY tuple with stable referenced capacities.
 * \return SUCCESS means ACCEPTED, including a later settled start failure.
 * \note Rejection has no retained references or residual hardware effects.
 */
nx_result_t nx_image_port_submit(const nx_image_port_t* port,
                                 nx_image_request_t* request);

/** \brief Make bounded progress and preserve first terminal cause.
 * \param[in] port: Same privileged unmasked executor, no callback reentry.
 * \return SUCCESS after progress; BUSY/IO may retain quarantined storage.
 * \note IRQ callbacks only latch facts and signal an optional bounded hint.
 *       Transfer complete or HAL_READY alone is not a withdrawal proof.
 */
nx_result_t nx_image_port_service(const nx_image_port_t* port);

/** \brief Request cancellation without revoking image loans.
 * \param[in] port: Same serialized owner.
 * \param[in,out] request: Exact accepted descriptor, still kept alive.
 * \return SUCCESS requests cancel; STATE means another request.
 * \note Continue service through engine/IRQ/address/bus withdrawal.
 */
nx_result_t nx_image_port_cancel(const nx_image_port_t* port,
                                 nx_image_request_t* request);

/** \brief Seal admission and join the active transfer before resource return.
 * \param[in] port: Same owner with external producers/observers quiesced.
 * \return SUCCESS only after final withdrawal; BUSY/IO retains responsibility.
 * \note Late IRQ cannot access request/config after successful stop. Repeat
 *       stop is idempotent and cannot dereference returned configuration.
 */
nx_result_t nx_image_port_stop(const nx_image_port_t* port);

/** \brief Attach an optional hint under existing IRQ-ceiling policy.
 * \param[in] port: Live serialized owner without active work.
 * \param[in] wake: Stable hint or NULL to detach; no request-state authority.
 * \param[in] syscall_ceiling: Must match the selected backend policy.
 * \return SUCCESS or INVALID/CONTEXT/STATE/BUSY/UNSUPPORTED.
 * \note Join source and publishers before reclaiming target/context.
 */
nx_result_t nx_image_port_attach_wake(const nx_image_port_t* port,
                                      const nx_irq_wake_t* wake,
                                      uint8_t syscall_ceiling);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_IMAGE_H */
