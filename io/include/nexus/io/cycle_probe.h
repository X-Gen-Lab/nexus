/**
 * \file            cycle_probe.h
 * \brief           Opt-in raw cycle observations without elapsed-time claims.
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_CYCLE_PROBE_H
#define NEXUS_IO_CYCLE_PROBE_H

#include "nexus/io/spi_dma_transaction.h"
#include "nexus/io/uart_word.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Copied raw counter facts; readable does not mean elapsed is known.
 * \note A pair is a modulo-2^32 residue, including preemption and probe cost.
 *       Neither wrap count, reset, rate nor epoch continuity is established.
 *       elapsed_known is always false. No field establishes a passed budget,
 *       IRQ arrival latency, exclusive CPU time or a physical WCET bound.
 *       attempts includes a currently active attempt; other totals describe
 *       completed attempts. Totals saturate rather than wrap.
 */
typedef struct {
    uint32_t attempts;
    uint32_t raw_pairs;
    uint32_t unavailable_pairs;
    uint32_t reentries;
    uint32_t last_begin;
    uint32_t last_end;
    uint32_t last_modulo_cycles;
    uint32_t maximum_modulo_cycles;
    bool last_available;
    bool elapsed_known;
    bool active;
} nx_io_cycle_observation_t;

/**
 * \brief Optional zero-initialized sidecar; no default provider state grows.
 * \note One physical vector or one unique task executor writes each sidecar.
 *       Different nested vectors require distinct sidecars. NMI, HardFault,
 *       other cores and other security states must not access this storage.
 *       Reset/reclaim only after all producers, tokens and readers have joined.
 */
typedef struct {
    nx_io_cycle_observation_t observation;
} nx_io_cycle_probe_t;

/** \brief Zero-initialized stack token; do not copy or rebind while active. */
typedef struct {
    nx_io_cycle_probe_t* owner;
    uint32_t before;
    bool available;
} nx_io_cycle_token_t;

/**
 * \brief Start one raw observation without enabling or resetting DWT.
 * \param[in,out] probe: Live, exclusive zero/previously completed sidecar.
 * \param[in,out] token: Zero/inactive token distinct from the complete probe.
 * \return SUCCESS even if the counter is unavailable; INVALID/STATE/BUSY
 *         rejects before counter access. Same-probe reentry is counted.
 * \note Configurable IRQ or unique task executor. Probe failure must never
 *       prevent real I/O dispatch, cancellation, service or retained drain.
 */
nx_result_t nx_io_cycle_probe_begin(nx_io_cycle_probe_t* probe,
                                    nx_io_cycle_token_t* token);

/**
 * \brief Finish the exact token, publish raw facts and remove its reference.
 * \param[in,out] probe: Same stable sidecar supplied to begin.
 * \param[in,out] token: Exact still-active token; not concurrently modified.
 * \return SUCCESS, INVALID or STATE with another/inactive token.
 * \note No elapsed or budget validity is inferred from two counter values.
 */
nx_result_t nx_io_cycle_probe_end(nx_io_cycle_probe_t* probe,
                                  nx_io_cycle_token_t* token);

/**
 * \brief Copy bounded IRQ statistics under saved local interrupt exclusion.
 * \param[in] probe: Stable same-CPU sidecar and live producer binding.
 * \param[out] out: Distinct aligned copied result, unchanged on rejection.
 * \return SUCCESS, INVALID or CONTEXT for ISR/unprivileged callers.
 * \note Privileged task only. Incoming masks are restored. For a task service
 *       producer, use its same executor or join it before sampling; IRQ masks
 *       do not join a different preempted task. This is not SMP exclusion.
 *       No provider, payload, request or counter is accessed under the mask.
 */
nx_result_t nx_io_cycle_probe_snapshot(const nx_io_cycle_probe_t* probe,
                                       nx_io_cycle_observation_t* out);

/**
 * \brief Dispatch exactly one word service step with optional raw observation.
 * \param[in] port: Original stable face, methods and unique task executor.
 * \param[in,out] probe: Optional live sidecar, or NULL to omit observation.
 * \return SUCCESS proves dispatch only; INVALID/UNSUPPORTED describes the face.
 * \note No additional ops table, worker, request or payload reference. Invalid
 *       observation never prevents dispatch. Caller proves complete private
 *       state, borrowed descriptors and buffers disjoint from observation
 *       storage; this API knows only face/method spans and context identity.
 */
nx_result_t nx_uart_word_port_service_observed(const nx_uart_word_port_t* port,
                                               nx_io_cycle_probe_t* probe);

/**
 * \brief Dispatch exactly one whole-CS DMA service step with raw observation.
 * \param[in] port: Original enhanced controller and its unique task executor.
 * \param[in,out] probe: Optional stable disjoint sidecar, or NULL.
 * \return SUCCESS after dispatch, INVALID or UNSUPPORTED for an invalid face.
 * \note Same disjointness/executor rules as the word wrapper. No diagnostic
 *       failure releases a QUARANTINED loan or changes settlement authority.
 */
nx_result_t nx_spi_dma_transaction_service_observed(
    const nx_spi_dma_transaction_port_t* port, nx_io_cycle_probe_t* probe);

#ifdef __cplusplus
}
#endif

#ifndef NX_IO_CYCLE_PROBES
#define NX_IO_CYCLE_PROBES 0
#endif
#if NX_IO_CYCLE_PROBES != 0 && NX_IO_CYCLE_PROBES != 1
#error "NX_IO_CYCLE_PROBES requires an exact zero or one selection"
#endif

/* DECLARE is a complete declaration when enabled: generated C must not append
 * a semicolon to an empty, disabled file-scope expansion. BEGIN/END belong in
 * one function block around the unchanged direct provider dispatch. */
#if NX_IO_CYCLE_PROBES
#define NX_IO_IRQ_PROBE_DECLARE(id) nx_io_cycle_probe_t nx_io_irq_probe_##id;
#define NX_IO_IRQ_PROBE_BEGIN(id)                                              \
    nx_io_cycle_token_t nx_io_irq_token_##id = {NULL, 0U, false};              \
    (void)nx_io_cycle_probe_begin(&nx_io_irq_probe_##id, &nx_io_irq_token_##id)
#define NX_IO_IRQ_PROBE_END(id)                                                \
    (void)nx_io_cycle_probe_end(&nx_io_irq_probe_##id, &nx_io_irq_token_##id)
#else
#define NX_IO_IRQ_PROBE_DECLARE(id)
#define NX_IO_IRQ_PROBE_BEGIN(id) ((void)0)
#define NX_IO_IRQ_PROBE_END(id)   ((void)0)
#endif

#endif
