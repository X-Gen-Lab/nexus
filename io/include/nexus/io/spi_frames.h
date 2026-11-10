/**
 * \file            spi_frames.h
 * \brief           Explicit master/slave SPI frames and borrowed requests
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_SPI_FRAMES_H
#define NEXUS_IO_SPI_FRAMES_H

#include "nexus/core/request.h"
#include "nexus/io/wake.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exact clock owner, independent of an application protocol. */
typedef enum { NX_SPI_FRAMES_MASTER, NX_SPI_FRAMES_SLAVE } nx_spi_frames_role_t;
/** \brief Two data pads, a receive-only pad, or one bidirectional data pad. */
typedef enum {
    NX_SPI_FRAMES_TWO_LINE,
    NX_SPI_FRAMES_RECEIVE_ONLY,
    NX_SPI_FRAMES_ONE_WIRE
} nx_spi_frames_lines_t;
/** \brief Motorola whole-selection framing or TI frame-sync framing. */
typedef enum {
    NX_SPI_FRAMES_MOTOROLA,
    NX_SPI_FRAMES_TI
} nx_spi_frames_format_t;

/** \brief Immutable wire tuple selected before peripheral activation.
 * \note The caller owns peer clocks, wiring and shared-line arbitration.
 *       irq_latency_us bounds IRQ latency and all nonwaiting software through
 *       source withdrawal; it is a declared integration budget,
 *       never a measured hardware qualification. Master receive-only and
 *       one-wire reception require a nonzero bound and finite clock progress.
 *       CRC uses an odd polynomial and an extra hardware CRC frame, excluded
 *       from request counts. TI derives its required clock phase/polarity.
 *       Slave maximum_hz is the external peer's upper clock bound; BR does
 *       not impose the master's divide-by-256 minimum on slave operation.
 */
typedef struct {
    nx_spi_frames_role_t role;
    nx_spi_frames_lines_t lines;
    nx_spi_frames_format_t format;
    uint16_t crc_polynomial;
    uint16_t dummy;
    uint32_t irq_latency_us;
    uint8_t word_bits;
    uint8_t mode;
    bool lsb_first;
} nx_spi_frames_config_t;

/** \brief Caller-owned descriptor and word spans borrowed through SETTLED.
 * \note Word width belongs to the static port. Sixteen-bit spans contain
 *       aligned numeric uint16_t values. TWO_LINE permits NULL TX for dummy
 *       or NULL RX for discard, but not both. RECEIVE_ONLY requires RX only;
 *       ONE_WIRE requires exactly one span and selects its direction before
 *       activation. All spans, metadata and provider storage are disjoint.
 *       owner, generation and time_epoch are written only by port preparation;
 *       callers keep them unchanged until the descriptor is unborrowed.
 */
typedef struct {
    nx_request_t base;
    const void* owner;
    uint64_t generation;
    uint64_t time_epoch;
    const void* tx;
    void* rx;
    size_t words;
} nx_spi_frames_request_t;

/** \brief Readonly shared methods; one executor owns each instance. */
typedef struct {
    nx_result_t (*prepare)(void* context, nx_spi_frames_request_t* request,
                           const void* tx, void* rx, size_t words,
                           nx_time_us_t deadline);
    nx_result_t (*submit)(void* context, nx_spi_frames_request_t* request);
    nx_result_t (*cancel)(void* context, nx_spi_frames_request_t* request);
    void (*service)(void* context);
    nx_result_t (*stop)(void* context);
    nx_result_t (*recover)(void* context);
} nx_spi_frames_ops_t;
/** \brief Immutable controller face; lookup acquires no resources. */
typedef struct {
    const nx_spi_frames_ops_t* ops;
    void* context;
} nx_spi_frames_port_t;

/** \brief Bind unborrowed words and a deadline to one live port generation.
 * \param[in] port: Initialized static owner in the current Core clock epoch.
 * \param[in,out] request: Initialized fresh or consumed SETTLED descriptor.
 * \param[in] tx: Immutable words, or NULL for a supported dummy operation.
 * \param[out] rx: Writable words, or NULL for supported discard.
 * \param[in] words: Positive count, at most 65535; width belongs to the port.
 * \param[in] deadline: Absolute nx_time_now_us deadline, including wire tail.
 * \return Success or INVALID/STATE/CONTEXT; no provider borrow is created.
 * \note A cold restart or Core epoch change invalidates an old READY tuple.
 *       Prepare again against the live port with a new-domain deadline.
 */
nx_result_t nx_spi_frames_prepare(const nx_spi_frames_port_t* port,
                                  nx_spi_frames_request_t* request,
                                  const void* tx, void* rx, size_t words,
                                  nx_time_us_t deadline);
/** \brief Admit one complete frame sequence from unmasked Thread context.
 * \param[in] port: Initialized static controller, owned by one executor.
 * \param[in,out] request: READY descriptor and disjoint spans.
 * \return Success means ACCEPTED, including later start failure; every reject
 *         retains no new reference and leaves no hardware effect.
 * \note Slave peers must await successful arming before driving clocks/NSS.
 *       ISR moves bounded word data only; the owner calls service throughout
 *       completion, cancellation and stop. No worker or dynamic queue exists.
 */
nx_result_t nx_spi_frames_submit(const nx_spi_frames_port_t* port,
                                 nx_spi_frames_request_t* request);
/** \brief Stop future loading while retaining the active borrow until drain.
 * \param[in] port: The same single executor as submit and service.
 * \param[in,out] request: The active request, or its observed SETTLED identity.
 * \return Success requests cancellation; STATE means another request.
 * \note Cancellation never revokes a borrow or overwrites a latched terminal.
 */
nx_result_t nx_spi_frames_cancel(const nx_spi_frames_port_t* port,
                                 nx_spi_frames_request_t* request);
/** \brief Advance deadline/drain and publish only after source withdrawal.
 * \param[in] port: The unmasked Thread executor; bounded, no waiting callback.
 * \note Failed drain/disable retains QUARANTINED storage; continue service.
 */
void nx_spi_frames_service(const nx_spi_frames_port_t* port);
/** \brief Close admission and join requests before returning hardware.
 * \param[in] port: Executor after producers and observers have joined.
 * \return Success means stopped; BUSY retains state and requires service.
 */
nx_result_t nx_spi_frames_stop(const nx_spi_frames_port_t* port);
/** \brief Recover an idle fault without replaying a previous request.
 * \param[in] port: Executor with no active borrow and unchanged static config.
 * \return Success after reset/source proof; failures retain recovery duty.
 * \note A changed Core epoch requires complete cold close and restart.
 */
nx_result_t nx_spi_frames_recover(const nx_spi_frames_port_t* port);

#ifdef __cplusplus
}
#endif
#endif
