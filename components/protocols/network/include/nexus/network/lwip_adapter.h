/**
 * \file            lwip_adapter.h
 * \brief           Explicit single-owner lwIP Ethernet copying adapter
 * \author          Nexus Team
 */
#ifndef NEXUS_NETWORK_LWIP_ADAPTER_H
#define NEXUS_NETWORK_LWIP_ADAPTER_H

#include "nexus/io/ethernet_frames.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Backend storage is defined only by the explicit storage header. */
typedef struct nx_lwip_adapter nx_lwip_adapter_t;

/**
 * \brief Explicit static IPv4 configuration and caller-owned frame storage.
 * \note The serialized stack owner initializes lwIP once before attachment.
 *       The port, clock, TX buffer and views outlive attachment and all loans.
 *       Clock and provider deadlines use the same live monotonic domain.
 *       No default worker, lock, PHY, address, DHCP, DNS or heap is provided.
 */
typedef struct {
    const nx_eth_frames_port_t* port;
    nx_clock_t clock;
    nx_time_us_t tx_timeout_us;
    uint8_t* tx_buffer;
    size_t tx_capacity;
    nx_eth_fragment_t* rx_views;
    size_t rx_capacity;
    uint8_t hardware_address[6];
    uint8_t address[4];
    uint8_t netmask[4];
    uint8_t gateway[4];
} nx_lwip_config_t;

/**
 * \brief Attach one caller-owned netif without initializing MAC or PHY.
 * \param[in,out] adapter: Zero-initialized backend storage, not already live.
 * \param[in] config: Explicit configuration; copied without retaining it.
 * \return Success, invalid inputs, or BUSY for an already attached netif.
 * \note Task-only, one serialized stack execution owner. Address configuration
 *       is static IPv4; the underlying stack is the pinned NO_SYS profile.
 *       Storage must be disjoint from configuration, TX bytes and RX views.
 *       Failure retains no new hardware borrow. Link starts down; only an
 *       explicit external PHY observation may bring it up.
 */
nx_result_t nx_lwip_adapter_start(nx_lwip_adapter_t* adapter,
                                  const nx_lwip_config_t* config);

/**
 * \brief Progress MAC, bounded complete RX frames and stack timer callbacks.
 * \param[in,out] adapter: Live storage, including any unfinished loans.
 * \param[in] receive_budget: Nonzero maximum acquired frames this invocation.
 * \return Success, one pending settled TX failure, or provider/release/state
 *         error. A settled TX failure is consumed once before further progress.
 * \note Same task owner as every lwIP raw API/callback. Timer callback cost is
 *       governed by the fixed stack profile and application callbacks; this is
 *       not a hard bound on the entire invocation. The copying profile copies
 *       RX bytes before exact release. The explicit loaned-RX profile retains
 *       immutable payload tails until its custom-pbuf references end. A failed
 *       release retains its token and closes progress; stop retries release
 *       without feeding duplicate bytes into the stack.
 *       Inline and deferred TX failure remain observable before request reuse;
 *       while pending, new link output returns ERR_IF without admitting bytes.
 *       Success inline completion remains immediately reusable. Consuming an
 *       already-settled failure makes the TX request READY, without automatic
 *       reset, replay, adapter close or forced restart. The product chooses
 *       provider recovery; an actual unfinished borrow is never revoked.
 */
nx_result_t nx_lwip_adapter_service(nx_lwip_adapter_t* adapter,
                                    size_t receive_budget);

/**
 * \brief Apply a proven external PHY fact to the MAC, then the netif.
 * \param[in,out] adapter: Live, nonclosing adapter storage.
 * \param[in] link: Actual PHY observation, not a software default.
 * \return Underlying MAC admission result; failure preserves netif flags.
 * \note One task owner. Existing provider quiescence rules still apply. This
 *       does not negotiate, reset, power or prove the physical link.
 */
nx_result_t nx_lwip_adapter_apply_link(nx_lwip_adapter_t* adapter,
                                       const nx_eth_link_t* link);

/**
 * \brief Close adapter admission and detach only after actual MAC drain.
 * \param[in,out] adapter: Live storage retained on any unfinished withdrawal.
 * \return Success after detachment; otherwise the actual withdrawal error.
 * \note The product first closes its PCBs, raw callbacks, queues and observers.
 *       This stops only the netif/MAC adapter, not the global lwIP stack or all
 *       application connections. A timeout or failed drain never revokes TX
 *       or RX loans; retry after explicit provider recovery. No automatic reset
 *       or replay occurs. Stop is idempotent after successful detachment.
 */
nx_result_t nx_lwip_adapter_stop(nx_lwip_adapter_t* adapter);

/**
 * \brief Product-supplied monotonic milliseconds for lwIP timers.
 * \return Wrapping milliseconds in the stack's unchanged live time domain.
 * \note Task owner only. Supply this symbol once per selected stack profile;
 *       its clock source and lifetime must cover every pending stack timer.
 */
uint32_t nx_lwip_port_now_ms(void);

/**
 * \brief Product-supplied random word; no libc or timestamp fallback.
 * \return A word from the product's admitted random source.
 * \note Called by the serialized stack owner. Source failure must enter the
 *       product's nonreturning fault path; returning a fixed value is allowed
 *       only in an explicitly unqualified host test fixture.
 */
uint32_t nx_lwip_port_random_u32(void);

/**
 * \brief Product-supplied nonreturning stack invariant failure observation.
 * \param[in] message: Static diagnostic text, not a retained ownership token.
 * \note Task owner. Never return or release quarantined hardware storage merely
 *       because the stack reported an assertion. No default abort is linked.
 */
void nx_lwip_port_assert(const char* message);

#ifdef __cplusplus
}
#endif
#endif
