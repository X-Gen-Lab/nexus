/**
 * \file            tmp117.h
 * \brief           Explicit settled TMP117 one-shot sampling and provenance
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_TMP117_H
#define NEXUS_COMPONENTS_TMP117_H

#include "nexus/core/time.h"
#include "nexus/io/observation.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Synchronous register exchange, settled on every return
 * \note            Read writes one register byte then repeated-START reads two
 *                  bytes; write sends register plus two MSB-first bytes. The
 *                  serialized task owner keeps context and clock alive through
 *                  use, uncertain cleanup and joined software stop. Every
 *                  result releases TX/RX spans, including TIMEOUT/CANCELLED.
 *                  No asynchronous submit or retained stack span is permitted.
 */
typedef struct {
    void* context;
    nx_result_t (*exchange)(void* context, const uint8_t* tx, size_t tx_bytes,
                            uint8_t* rx, size_t rx_bytes,
                            nx_time_us_t deadline);
    nx_clock_t clock;
} nx_tmp117_transport_port_t;

/**
 * \brief           Explicit clock identity and expected existing calibration
 * \note            Nonzero domain/epoch follow nx_io_timestamp_t. The owner
 *                  changes epoch on discontinuity/reset/rate change. Offset is
 *                  already applied by the chip; this driver only verifies and
 *                  reports it, never programs calibration or EEPROM.
 */
typedef struct {
    uint32_t clock_domain;
    uint32_t clock_epoch;
    int16_t expected_offset_lsb;
} nx_tmp117_config_t;

/**
 * \brief           Independent copied sample, published only on SUCCESS
 * \note            micro_celsius rounds raw/128 Celsius to nearest microdegree,
 *                  ties away from zero. raw/offset use 1/128 Celsius per LSB;
 *                  offset is not added again. Reviewed output is -55..150C.
 *                  observed is software register-read completion with unknown
 *                  physical uncertainty, never hardware conversion time.
 *                  Conversion occurred within [conversion_started_at,
 *                  observed.ticks] in that same domain/epoch. A consumer can
 *                  use the lower bound for conservative logical-age checks;
 *                  recent read completion alone does not establish freshness.
 *                  All fields are values, with no retained device/payload
 *                  references. Copies survive stop; clock-domain assumptions
 *                  still require the caller's continuity/lifetime contract.
 */
typedef struct {
    int32_t micro_celsius;
    int16_t raw_lsb;
    int16_t offset_lsb;
    nx_time_us_t conversion_started_at;
    nx_io_timestamp_t observed;
} nx_tmp117_sample_t;

/**
 * \brief           Exact caller-owned instance, fresh zero before first init
 * \note            Task-only single executor, no concurrent/reentrant calls.
 *                  Uncertain physical state retains the port for explicit
 *                  cancel/resync; it creates no MCU buffer loan. Fields are
 *                  implementation state, never permission to reclaim a loan.
 */
typedef struct {
    nx_tmp117_transport_port_t transport;
    nx_tmp117_config_t config;
    nx_time_us_t deadline;
    nx_time_us_t conversion_started_at;
    nx_time_us_t last_clock;
    uint16_t base_configuration;
    bool initialized;
    bool measuring;
    bool uncertain;
    bool in_call;
} nx_tmp117_t;

/**
 * \brief           Validate ID 0117h, locked EEPROM, offset and shutdown
 * \param[in,out]   device: Fresh zero/stopped or same-binding uncertain storage
 * \param[in]       transport: Copied on success or uncertain shutdown write
 * \param[in]       config: Stable explicit facts, not overlapping device
 * \param[in]       deadline: Finite absolute deadline, shared across accesses
 * \return          SUCCESS, BUSY for power-up/EEPROM, or exact transport error;
 *                  unsupported identity, offset mismatch and unlocked EEPROM
 *                  reject without writes. Pre-write failure keeps no new
 *                  binding. Once shutdown write is issued, failure retains
 *                  the same port/config/clock as uncertain and uninitialized.
 * \note            No scan, reset, sleep, EEPROM write, worker or Board action.
 *                  Use a separate cleanup deadline to cancel, or retry init on
 *                  that same binding. A different binding requires joined
 *                  software stop; external uncertain-state recovery remains
 *                  the owner's duty. Failed write is not a device reset.
 */
nx_result_t nx_tmp117_init(nx_tmp117_t* device,
                           nx_tmp117_transport_port_t transport,
                           const nx_tmp117_config_t* config,
                           nx_time_us_t deadline);

/**
 * \brief           Begin one fresh conversion with averaging disabled
 * \param[in,out]   device: Initialized nonmeasuring exclusive device
 * \param[in]       deadline: Finite conversion/read deadline in the same clock
 * \return          SUCCESS, INVALID, BUSY, STATE, TIMEOUT or transport error
 * \note            Clears stale readiness before OS admission. BUSY retains
 *                  conversion metadata only, never a stack/caller buffer.
 *                  Failure after I/O closes sampling until explicit resync.
 */
nx_result_t nx_tmp117_begin(nx_tmp117_t* device, nx_time_us_t deadline);

/**
 * \brief           Observe readiness and publish one complete fresh sample
 * \param[in,out]   device: Previously begun conversion, same task executor
 * \param[out]      sample: Aligned separate output, unchanged on every error
 * \return          SUCCESS, INVALID, BUSY before ready, STATE or transport/IO
 * \note            Reads CONFIG clear readiness. A later temperature-read
 *                  failure is terminal and requires resync, never cached zero
 *                  or endless ready retry. Any transport error, including
 *                  BUSY, closes sampling until resync; only a successful
 *                  not-ready read retains the conversion. Repeated poll after
 *                  success is STATE. Output is disjoint from device, opaque
 *                  port/clock context storage and borrowed bus storage; only
 *                  known spans can be checked. POR/saturation and values
 *                  outside the reviewed range return IO.
 */
nx_result_t nx_tmp117_poll(nx_tmp117_t* device, nx_tmp117_sample_t* sample);

/**
 * \brief           Request and verify shutdown under a separate cleanup budget
 * \param[in,out]   device: Live or uncertain same binding, serialized owner
 * \param[in]       deadline: Finite cleanup deadline, not an expired sample one
 * \return          SUCCESS only after shutdown observation; other results keep
 *                  recovery responsibility and may require init resync
 * \note            Cannot preempt a synchronous exchange. Cancel failure never
 *                  proves physical stop; no MCU span survives a port return.
 *                  Success does not close a bus owner or join its observers.
 */
nx_result_t nx_tmp117_cancel(nx_tmp117_t* device, nx_time_us_t deadline);

/**
 * \brief           Drop software binding after calls and outer owners join
 * \param[in,out]   device: Quiescent instance with no in-progress call
 * \note            No physical I/O. Not power-off, verified cancellation,
 *                  Board release or erasure of external uncertain-state duty.
 *                  A caller requiring physical shutdown first completes cancel
 *                  or external recovery. Callback-time calls do nothing.
 */
void nx_tmp117_stop(nx_tmp117_t* device);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_TMP117_H */
