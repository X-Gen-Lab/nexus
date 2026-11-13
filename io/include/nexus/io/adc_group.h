/**
 * \file            adc_group.h
 * \brief           Optional finite whole-sequence integer ADC acquisition
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_ADC_GROUP_H
#define NEXUS_IO_ADC_GROUP_H
#include "nexus/core/request.h"
#include "nexus/io/adc_format.h"
#include "nexus/io/adc_observer.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief A request names raw frames or one explicit two-group result view. */
typedef enum {
    NX_ADC_GROUP_RESULTS_UNKNOWN,
    NX_ADC_GROUP_RAW_RESULTS,
    NX_ADC_GROUP_REGULAR_INJECTED_RESULTS
} nx_adc_group_results_kind_t;
/** \brief One exact result span; format belongs to its queried ADC group. */
typedef struct {
    void* data;
    size_t bytes;
} nx_adc_group_result_t;
/** \brief Both group spans and this descriptor have one request lifetime.
 * \note The producer borrows this immutable descriptor and both disjoint
 *       spans until one acquire-observed SETTLED. No independent subgroup
 *       success or partial valid frame is published.
 */
typedef struct {
    nx_adc_group_result_t regular;
    nx_adc_group_result_t injected;
} nx_adc_group_results_t;

/** \brief Complete raw frame span with an explicit finite request lifetime.
 * \note base.transferred counts complete frames, never bytes or individual
 *       ranks. SUCCESS means exactly frame_count; errors guarantee zero valid
 *       frames. Partial DMA/JEOC progress does not publish a valid prefix.
 */
typedef struct {
    nx_request_t base;
    union {
        void* frames; /**< Raw frame storage, including declared padding. */
        nx_adc_group_results_t* results; /**< Explicit paired descriptor. */
    } output;     /**< Explicit result view selected by results_kind. */
    size_t bytes; /**< Raw bytes, or sizeof(*output.results) when paired. */
    size_t frame_count; /**< Positive complete frames, within selected bound. */
    nx_adc_group_results_kind_t results_kind; /**< Explicit active view. */
} nx_adc_group_request_t;
/** \brief Cold selected rank order, actual integer layout and scheduling facts.
 * \note Arrays belong to immutable selected facts and outlive this port. A
 *       physical rank can be discarded padding required by silicon errata;
 *       format describes only valid logical results. No implicit data packing,
 *       engineering units, aperture timestamp or trigger counter is promised.
 *       format is the portable result layout. Channel/converter identifiers,
 *       sample-code tables and physical-rank details are explicit native
 *       diagnostics of this advanced acquisition profile, not portable IDs.
 */
typedef struct {
    nx_adc_format_t format;
    const uint8_t* channels;     /**< Logical rank order, including repeats. */
    const uint8_t* sample_times; /**< Actual per-channel sampling codes. */
    const uint16_t* sample_halfcycles; /**< Sampling-code lookup, immutable. */
    size_t sample_code_count;
    uint32_t adc_clock_hz;
    /** \brief Complete scan cost; zero for an unknown interleaved span. */
    uint32_t physical_scan_halfcycles;
    uint32_t timer_period_us; /**< Zero for software; not an aperture. */
    size_t max_frames;
    uint16_t physical_ranks;
    bool active;
    bool fault;
    bool quarantined;
    const uint8_t* converters; /**< Converter ID per logical result, or NULL. */
    uint8_t interleave_delay_cycles; /**< Selected minimum spacing. */
    uint8_t buffer_alignment;        /**< DMA access can exceed one sample. */
} nx_adc_group_info_t;
/** \brief Shared read-only selected methods; ordinary ADC tables do not grow.
 * \note A privileged task serializes every call. IRQs only record bounded
 *       facts in the same unique physical owner. No allocation, pool, implicit
 *       task, automatic rearm or runtime discovery is provided.
 */
typedef struct {
    nx_result_t (*submit)(void* context, nx_adc_group_request_t* request);
    nx_result_t (*cancel)(void* context, nx_adc_group_request_t* request);
    nx_result_t (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*info)(const void* context, nx_adc_group_info_t* info);
    nx_result_t (*start_blocks)(void* context, nx_stream_t* stream);
    nx_result_t (*attach_observer)(void* context, nx_adc_observer_t* observer);
    nx_result_t (*block_observation)(const void* context,
                                     const nx_stream_block_t* block,
                                     nx_adc_observation_t* observation);
    nx_result_t (*submit_injected)(void* context,
                                   nx_adc_group_request_t* request);
    nx_result_t (*injected_info)(const void* context,
                                 nx_adc_group_info_t* info);
} nx_adc_group_ops_t;
/** \brief Immutable selected face, sharing one const table across instances.
 * \note Face/context/configuration outlive every request, vector and retained
 *       close. Existing ordinary ADC or PWM/ADC faces cannot expose the same
 *       leased ADC. A factory only returns this existing object.
 */
struct nx_adc_group_port {
    const nx_adc_group_ops_t* ops;
    void* context;
};
/** \brief Prepare caller storage without hardware access or retained borrow.
 * \param[in,out] request: Initialized READY or fully consumed SETTLED storage.
 * \param[out] frames: Aligned nonempty raw output including explicit padding.
 * \param[in] bytes: Positive nonwrapping span; provider validates exact layout.
 * \param[in] frame_count: Positive whole-frame count, not a raw rank count.
 * \param[in] deadline: Finite absolute deadline in platform monotonic domain.
 * \return SUCCESS or INVALID/STATE, rejected request remains unchanged.
 * \note Request and frames are distinct. Preparation reads no sample bytes and
 *       does not prove a DMA domain or chip mode. All previous observers must
 *       be joined before reuse. NX_DEADLINE_NEVER is rejected.
 */
nx_result_t nx_adc_group_prepare(nx_adc_group_request_t* request, void* frames,
                                 size_t bytes, size_t frame_count,
                                 nx_time_us_t deadline);
/** \brief Prepare one explicit regular/injected result descriptor.
 * \param[in,out] request: Initialized READY or consumed SETTLED metadata.
 * \param[in,out] results: Stable descriptor naming two disjoint output spans.
 * \param[in] frame_count: Positive joint frames; provider checks its limit.
 * \param[in] deadline: Finite absolute deadline for both results and
 * withdrawal. \return SUCCESS or pure INVALID/STATE; no sample data is read or
 * written. \note Selection must expose paired acquisition. Admission borrows
 * descriptor, both spans and request jointly. Errors guarantee zero valid joint
 *       frames; cancel or failed drain cannot revoke either storage loan.
 */
nx_result_t nx_adc_group_prepare_results(nx_adc_group_request_t* request,
                                         nx_adc_group_results_t* results,
                                         size_t frame_count,
                                         nx_time_us_t deadline);
/** \brief Accept and start one selected finite acquisition under a sole owner.
 * \param[in] port: Initialized selected face with immutable retained facts.
 * \param[in,out] request: Exact READY descriptor and exclusive writable span.
 * \return SUCCESS means ACCEPTED, including immediate SETTLED failure. Other
 *         results reject without pointer retention or residual new effects.
 * \note Accepted descriptor is caller-immutable; caller cannot access frames
 *       until acquire-observed SETTLED. Priming may write this entire span but
 *       never publishes prime data as valid. Post-admission arm failure keeps
 *       the borrow until actual drain. Without trusted completion timestamps,
 *       completion first observed at/after deadline is TIMEOUT. One-frame
 *       injected profiles deliberately prevent a second trigger/JDR overwrite.
 */
nx_result_t nx_adc_group_submit(const nx_adc_group_port_t* port,
                                nx_adc_group_request_t* request);
/** \brief Start cancellation without revoking the accepted storage borrow.
 * \param[in] port: Same stable selected face and serialized privileged task.
 * \param[in,out] request: Exact current accepted request, still live.
 * \return SUCCESS starts/observes cancellation; it does not imply SETTLED.
 * \note The first terminal fault/cancel/timeout remains authoritative.
 */
nx_result_t nx_adc_group_cancel(const nx_adc_group_port_t* port,
                                nx_adc_group_request_t* request);
/** \brief Make bounded prime/acquisition/drain progress without hidden waits.
 * \param[in] port: Same stable selected face and serialized privileged task.
 * \return SUCCESS after progress/settlement, BUSY during outstanding drain,
 *         or an error with retained recovery responsibility.
 * \note No request or output is reclaimed while sources, ADC, DMA or IRQs may
 *       still reference it. QUARANTINED requests remain owned and retryable.
 */
nx_result_t nx_adc_group_service(const nx_adc_group_port_t* port);
/** \brief Stop the current request, prove drain and clear a recoverable fault.
 * \param[in] port: Same stable selected face and serialized privileged task.
 * \return SUCCESS only after final request detach; BUSY/IO keeps all leases.
 * \note Stop is not cold close: pads, common clock, internal-source enables
 *       and provider storage remain retained until platform joined close.
 *       A fully stopped face can accept another request with the same static
 *       selection. A pending platform close keeps admission sealed.
 */
nx_result_t nx_adc_group_stop(const nx_adc_group_port_t* port);
/** \brief Copy actual selected facts and current retained owner state.
 * \param[in] port: Same initialized selected face, unique task executor.
 * \param[out] info: Separate aligned output, unchanged on rejection.
 * \return SUCCESS or INVALID/STATE/CONTEXT/UNSUPPORTED.
 * \note Observing idle/fault does not release a request or any cold lease.
 */
nx_result_t nx_adc_group_info(const nx_adc_group_port_t* port,
                              nx_adc_group_info_t* info);
/** \brief Accept caller-owned continuous complete-scan blocks.
 * \param[in] port: Static blocks provider, one serialized privileged executor.
 * \param[in,out] stream: At least three equal complete-frame DMA slots.
 * \return SUCCESS accepts the session; it does not certify completed warmup.
 * \note Errors after acceptance latch until joined stop. Service is explicit;
 *       no heap, default buffers or worker exists. Budget loss never publishes
 *       an unproved bank. Consumer loans survive stop until released.
 */
nx_result_t nx_adc_group_start_blocks(const nx_adc_group_port_t* port,
                                      nx_stream_t* stream);
/** \brief Attach existing caller-owned ADC observation records while idle.
 * \param[in] port: Same selected continuous group port.
 * \param[in,out] observer: Initialized optional control, or NULL to detach.
 * \return SUCCESS or pure INVALID/BUSY/CONTEXT/UNSUPPORTED rejection.
 * \note Observation timestamps are software completion observations, never
 *       trigger counts or sample-aperture timestamps. Stop joins before reuse.
 */
nx_result_t nx_adc_group_attach_observer(const nx_adc_group_port_t* port,
                                         nx_adc_observer_t* observer);
/** \brief Read one acquired block's exact existing ADC observation.
 * \param[in] port: Same bound observation-capable group port.
 * \param[in] block: Current immutable consumer loan from that exact stream.
 * \param[out] observation: Separate copied record, unchanged on rejection.
 * \return SUCCESS or INVALID/STATE/UNSUPPORTED with no retained output.
 */
nx_result_t nx_adc_group_block_observation(const nx_adc_group_port_t* port,
                                           const nx_stream_block_t* block,
                                           nx_adc_observation_t* observation);
/** \brief Accept one explicit software injection alongside regular blocks.
 * \param[in] port: Sole selected combined regular/injected controller owner.
 * \param[in,out] request: One frame in the separately queried injected layout.
 * \return SUCCESS accepts and borrows; other results have no retained effects.
 * \note The selected minimum interval bounds regular conversion disruption.
 *       Only one injected request may be live. Cancellation waits for all JEOC
 *       or a complete controller withdrawal. No automatic injection is implied.
 */
nx_result_t nx_adc_group_submit_injected(const nx_adc_group_port_t* port,
                                         nx_adc_group_request_t* request);
/** \brief Query the separate selected injected integer rank layout.
 * \param[in] port: Same initialized combined controller owner.
 * \param[out] info: Distinct output; unchanged on refusal.
 * \return SUCCESS, UNSUPPORTED for finite/regular-only faces, or admission
 * error.
 */
nx_result_t nx_adc_group_injected_info(const nx_adc_group_port_t* port,
                                       nx_adc_group_info_t* info);
#ifdef __cplusplus
}
#endif
#endif
