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
#include "nexus/io/stream.h"
#include "nexus/io/wake.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_adc_port nx_adc_port_t;
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
 * \param[in]       syscall_ceiling: Unshifted minimum kernel-safe IRQ priority.
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
