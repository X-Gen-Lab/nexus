/**
 * \file            adc.h
 *
 * \brief           Reviewed ADC single-shot and low-rate polling scans.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ADC_H
#define NEXUS_ADC_H

#include "nexus/core/time.h"
#include "nexus/io/adc_observer.h"
#include "nexus/io/fwd.h"
#include "nexus/io/stream.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Raw count facts; nominal Vref is not a calibrated physical
 *                  measurement.
 */
typedef struct {
    uint8_t resolution_bits;
    uint32_t reference_mv;
    size_t channel_count;
} nx_adc_info_t;

/**
 * \brief           Immutable complete-scan timing facts for the selected mode.
 * \note            sample_times are codes into sample_cycles, in channel order.
 *                  scan_cycles includes each channel's sample and conversion.
 *                  Clock declarations and cycle counts describe scheduling;
 *                  they do not establish electrical accuracy or sample aperture
 *                  timestamps. Arrays remain valid through provider lifetime.
 */
typedef struct {
    const uint8_t* channels;
    const uint8_t* sample_times;
    const uint16_t* sample_cycles;
    size_t channel_count;
    size_t sample_code_count;
    uint32_t adc_clock_hz;
    uint32_t timer_clock_hz;
    uint32_t scan_cycles;
    uint16_t conversion_cycles;
} nx_adc_sequence_info_t;

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*info)(const void* context, nx_adc_info_t* info);
    nx_result_t (*sample)(void* context, uint16_t* samples, size_t capacity,
                          nx_time_us_t deadline, size_t* count);
    nx_result_t (*stream_start)(void* context, nx_stream_t* stream,
                                uint32_t trigger_hz);
    nx_result_t (*stream_stop)(void* context);
    nx_result_t (*stream_service)(void* context);
    nx_result_t (*attach_wake)(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling);
    nx_result_t (*sequence_info)(const void* context,
                                 nx_adc_sequence_info_t* info);
    nx_result_t (*attach_observer)(void* context, nx_adc_observer_t* observer);
    nx_result_t (*block_observation)(const void* context,
                                     const nx_stream_block_t* block,
                                     nx_adc_observation_t* observation);
} nx_adc_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_adc_port {
    const nx_adc_ops_t* ops;
    void* context;
};

/**
 * \brief           Query the fixed reviewed ADC sequence and raw-count format.
 *
 * \param[in]       port: Single-shot/low-rate scan provider.
 *
 * \param[out]      info: Resolution, declared reference and sequence length.
 *
 * \return          Success or INVALID.
 */
nx_result_t nx_adc_port_info(const nx_adc_port_t* port, nx_adc_info_t* info);
/**
 * \brief           Read fixed channel order and complete-scan timing facts.
 * \param[in]       port: Initialized timing-capable provider.
 * \param[out]      info: Cold read-only facts; no acquisition starts.
 * \return          Success or INVALID/STATE/UNSUPPORTED.
 */
nx_result_t nx_adc_port_sequence_info(const nx_adc_port_t* port,
                                      nx_adc_sequence_info_t* info);
/**
 * \brief           Attach optional observer storage before acquisition starts.
 * \param[in]       port: Initialized continuous provider, one task owner.
 * \param[in,out]   observer: Initialized stable caller storage, or NULL to
 *                  detach. Provider validates exact stream count at start.
 * \return          Success or BUSY/CONTEXT/UNSUPPORTED.
 * \note            Stop and join before detach/reclaim. No hidden allocation;
 *                  polling and finite modes need no observation storage.
 */
nx_result_t nx_adc_port_attach_observer(const nx_adc_port_t* port,
                                        nx_adc_observer_t* observer);
/**
 * \brief           Query one live consumer loan's software completion record.
 * \param[in]       port: Same stable observer-capable provider.
 * \param[in]       block: Same stream's currently acquired immutable block.
 * \param[out]      observation: Copied epoch, ordinal, observed time and gap
 *                  flag. This is never a hardware sample aperture timestamp.
 * \return          Success or INVALID/STATE/UNSUPPORTED, output unchanged on
 *                  rejection.
 */
nx_result_t nx_adc_port_block_observation(const nx_adc_port_t* port,
                                          const nx_stream_block_t* block,
                                          nx_adc_observation_t* observation);

/**
 * \brief           Sample the fixed single/scan sequence by bounded polling.
 *
 * \param[in,out]   port: Single execution owner; fixed channel/sample-time
 *                  setup.
 *
 * \param[out]      samples: Raw counts in configured channel order.
 *
 * \param[in]       capacity: Sample capacity at least channel_count.
 *
 * \param[in]       deadline: Absolute clock-domain deadline.
 *
 * \param[out]      count: Complete valid samples; remaining output is
 *                  untouched.
 *
 * \return          Success or INVALID/BUSY/TIMEOUT/IO; abort/drain completes
 *                  before return. No retained buffer, DMA/trigger/stream
 *                  promise.
 *
 * \note            CPU-active task-only acquisition. Reference voltage,
 *                  sampling error and maximum rate need physical qualification.
 */
nx_result_t nx_adc_port_sample(const nx_adc_port_t* port, uint16_t* samples,
                               size_t capacity, nx_time_us_t deadline,
                               size_t* count);
/**
 * \brief           Start selected trigger/block acquisition with caller
 *                  storage.
 *
 * \param[in]       port: Trigger/block-capable provider, one task executor.
 *
 * \param[in,out]   stream: Initialized immutable-consumer block mechanism.
 *
 * \param[in]       trigger_hz: Positive requested complete-scan rate; provider
 *                  rejects rates outside its declared timer/ADC capability.
 *
 * \return          Success or INVALID/BUSY/UNSUPPORTED before buffer admission.
 *
 * \note            Blocks contain native uint16_t raw counts in channel order.
 *                  Capacities and lengths are multiples of a complete scan; DMA
 *                  domains/alignment and physical rate require separate
 *                  evidence.
 */
nx_result_t nx_adc_port_stream_start(const nx_adc_port_t* port,
                                     nx_stream_t* stream, uint32_t trigger_hz);
/**
 * \brief           Stop trigger and memory sources without revoking consumer
 *                  loans.
 *
 * \param[in]       port: Same acquisition executor; retain storage on BUSY.
 *
 * \return          Success proves source detach, BUSY retains producer storage,
 *                  UNSUPPORTED for a polling-only provider.
 *
 * \note            Consumer READY/BORROWED blocks must still be
 *                  drained/released.
 */
nx_result_t nx_adc_port_stream_stop(const nx_adc_port_t* port);
/**
 * \brief           Explicitly resume discrete block acquisition after consumer
 *                  drain.
 *
 * \param[in]       port: Same single task executor as start and stop.
 *
 * \return          Success when an engine is armed, BUSY during source drain or
 *                  free-block backpressure, STATE after acquisition stops.
 *
 * \note            No task, retry or hidden rearm is created. Block modes state
 *                  their trigger gaps; continuous zero-gap capture requires a
 *                  separately declared hardware capability and qualification.
 */
nx_result_t nx_adc_port_stream_service(const nx_adc_port_t* port);
/**
 * \brief           Attach an optional hint sink after checking publisher IRQ
 *                  priority.
 *
 * \param[in]       port: Initialized block-capable provider, one task executor.
 *
 * \param[in]       wake: Immutable borrowed target or NULL to detach.
 *
 * \param[in]       syscall_ceiling: Optional additional unshifted kernel-safe
 *                  priority floor; cannot weaken generated policy. PRIMASK
 *                  kernel targets require zero. Ignored without kernel calls.
 *
 * \return          Success or CONTEXT/STATE/PERMISSION/UNSUPPORTED.
 *
 * \note            Block/error predicates latch before notifications. Stop and
 *                  join publishers and waiters before reclaiming old sink
 *                  storage.
 */
nx_result_t nx_adc_port_attach_wake(const nx_adc_port_t* port,
                                    const nx_irq_wake_t* wake,
                                    uint8_t syscall_ceiling);
#ifdef __cplusplus
}
#endif

#endif
