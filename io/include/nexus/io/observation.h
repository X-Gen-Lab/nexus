/**
 * \file            observation.h
 * \brief           Explicit timestamp provenance and checked service dispatch
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_OBSERVATION_H
#define NEXUS_IO_OBSERVATION_H

#include "nexus/io/i2c.h"
#include "nexus/io/spi.h"
#include "nexus/io/uart.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Observation location; software time never implies hardware SOF. */
typedef enum {
    NX_IO_TIMESTAMP_UNKNOWN = 0,
    NX_IO_TIMESTAMP_HARDWARE_EVENT,
    NX_IO_TIMESTAMP_IRQ_OBSERVATION,
    NX_IO_TIMESTAMP_SERVICE_OBSERVATION,
    NX_IO_TIMESTAMP_ACQUIRE_OBSERVATION
} nx_io_timestamp_source_t;

/**
 * \brief           Explicit, caller-supplied time fact without clock access.
 * \note            ticks is extended, monotonically increasing within one
 *                  nonzero domain/epoch. Its owner extends counter wrap and
 *                  changes epoch on reset, discontinuity or rate change.
 *                  Domain and epoch are assembly-local identities, not clocks
 *                  synchronized across cores, security domains or devices.
 *                  UNKNOWN carries no usable measurement. Frequency is exact
 *                  stated ticks/second, not measured accuracy. A known zero
 *                  uncertainty states zero error; unknown uncertainty has a
 *                  separate flag. These facts do not establish physical
 *                  precision, causal pairing or event identity by themselves.
 */
typedef struct {
    uint64_t ticks;
    uint64_t ticks_per_second;
    uint32_t domain;
    uint32_t epoch;
    uint64_t uncertainty_ticks;
    nx_io_timestamp_source_t source;
    bool uncertainty_known;
} nx_io_timestamp_t;

/**
 * \brief           Delta in one common clock domain, with stated error bound.
 * \note            When uncertainty_known is false, uncertainty_ticks is zero
 *                  and describes no error bound. No conversion, rounding or
 *                  synchronization across clocks is implicit.
 */
typedef struct {
    uint64_t ticks;
    uint64_t ticks_per_second;
    uint64_t uncertainty_ticks;
    bool uncertainty_known;
} nx_io_timestamp_delta_t;

/**
 * \brief           Compare two explicitly related observed events.
 * \param[in]       first: Stable aligned first fact, not concurrently modified.
 * \param[in]       last: Stable last fact in the same domain, epoch and rate.
 * \param[out]      out: Distinct aligned result, unchanged on rejection.
 * \return          SUCCESS, INVALID for malformed/overlapping storage,
 *                  UNSUPPORTED for UNKNOWN source, STATE for incompatible
 *                  domains or reversed ticks, EXHAUSTED for bound overflow.
 * \note            Task/IRQ pure constant work; no clock, hardware, allocation,
 *                  lock or reference retention. Inputs may coincide. The
 *                  caller proves event pairing and timestamp provenance.
 *                  Different explicit source kinds may be compared to observe
 *                  delivery latency, without relabeling either observation.
 */
nx_result_t nx_io_timestamp_elapsed(const nx_io_timestamp_t* first,
                                    const nx_io_timestamp_t* last,
                                    nx_io_timestamp_delta_t* out);

/**
 * \brief           Dispatch the selected UART service or explain its absence.
 * \param[in]       port: Live immutable face and initialized provider state.
 * \return          SUCCESS after one dispatch, INVALID for malformed face,
 *                  UNSUPPORTED when no service method exists.
 * \note            Same context, serialization and lifetime rules as ordinary
 *                  service. SUCCESS proves dispatch only, not progress,
 *                  health, settlement or permission to reclaim storage. This
 *                  reads no request and adds no state, lock or hidden worker.
 */
nx_result_t nx_uart_port_service_checked(const nx_uart_port_t* port);

/**
 * \brief           Dispatch the selected SPI service or explain its absence.
 * \param[in]       port: Live immutable face and initialized provider state.
 * \return          SUCCESS after dispatch, INVALID or UNSUPPORTED.
 * \note            Same context and owner rules as nx_spi_port_service;
 *                  SUCCESS establishes dispatch only, never settlement.
 */
nx_result_t nx_spi_port_service_checked(const nx_spi_port_t* port);

/**
 * \brief           Dispatch the selected I2C service or explain its absence.
 * \param[in]       port: Live immutable face and initialized provider state.
 * \return          SUCCESS after dispatch, INVALID or UNSUPPORTED.
 * \note            Same context and owner rules as nx_i2c_port_service;
 *                  SUCCESS establishes dispatch only, never settlement.
 */
nx_result_t nx_i2c_port_service_checked(const nx_i2c_port_t* port);

#ifdef __cplusplus
}
#endif
#endif
