/**
 * \file            watchdog.h
 *
 * \brief           Irreversible IWDG effects and neutral hardware reset
 *                  causes.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_WATCHDOG_H
#define NEXUS_WATCHDOG_H

#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_watchdog_port nx_watchdog_port_t;
#define NX_RESET_POWER_ON  1u
#define NX_RESET_PIN       2u
#define NX_RESET_SOFTWARE  4u
#define NX_RESET_IWDG      8u
#define NX_RESET_WWDG      16u
#define NX_RESET_LOW_POWER 32u
#define NX_RESET_BROWNOUT  64u
/**
 * \brief           Hardware-selected timeout bounds, including declared LSI
 *                  uncertainty.
 *
 * \note            A zero minimum and maximum mean the timeout bounds are
 *                  unknown, for example while hardware shadow updates remain
 *                  pending or no oscillator limits have been reviewed. The
 *                  enabled, irreversible and debug_freeze facts remain valid.
 */
typedef struct {
    uint32_t minimum_timeout_us;
    uint32_t maximum_timeout_us;
    bool enabled;
    bool irreversible;
    bool debug_freeze;
} nx_watchdog_state_t;

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*enable)(void* context, uint32_t timeout_us, bool debug_freeze,
                          nx_watchdog_state_t* state);
    nx_result_t (*feed)(void* context);
    nx_result_t (*state)(const void* context, nx_watchdog_state_t* state);
} nx_watchdog_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_watchdog_port {
    const nx_watchdog_ops_t* ops;
    void* context;
};

/**
 * \brief           Enable IWDG and report its effect and known timing bounds;
 *                  activation is irreversible.
 *
 * \param[in,out]   port: Fixed provider; product owns feed/recovery policy.
 *
 * \param[in]       timeout_us: Requested nominal timeout in maintained range.
 *
 * \param[in]       debug_freeze: Whether debug halting freezes counting.
 *
 * \param[out]      state: Actual timeout bounds and remaining effect,
 *                  including enabled=true when later setup reports failure
 *                  after enabling.
 *
 * \return          Success or INVALID/STATE/IO; never claims enabled hardware
 *                  was disabled during rollback. Existing enable is
 *                  identified.
 *
 * \note            Success establishes nominal hardware programming, not a
 *                  guaranteed deadline. Both timeout bounds remain zero when
 *                  oscillator limits are unknown. A product requiring bounded
 *                  timing must admit the oscillator profile before activation.
 *                  Enable runs in a privileged task and is serialized with
 *                  feed, state and platform stop. Install IRQ feeding only
 *                  after this call returns; activation does not support a
 *                  concurrent reload during shadow-register programming.
 */
nx_result_t nx_watchdog_port_enable(const nx_watchdog_port_t* port,
                                    uint32_t timeout_us, bool debug_freeze,
                                    nx_watchdog_state_t* state);
/**
 * \brief           Feed an enabled IWDG without changing product health
 *                  policy.
 *
 * \param[in,out]   port: Fixed enabled provider with one serialized feed
 *                  owner.
 *
 * \return          Success or INVALID/STATE; bounded task/IRQ, no allocation.
 */
nx_result_t nx_watchdog_port_feed(const nx_watchdog_port_t* port);
/**
 * \brief           Query current watchdog effect and timeout uncertainty.
 *
 * \param[in]       port: Fixed provider.
 *
 * \param[out]      state: Current hardware/remembered initialization state.
 *
 * \return          Success or INVALID; UNSUPPORTED retains observed hardware
 *                  effects with both timeout bounds zero when those bounds
 *                  cannot be established. Never infer a disabled watchdog
 *                  from a failed query or unknown timeout.
 */
nx_result_t nx_watchdog_port_state(const nx_watchdog_port_t* port,
                                   nx_watchdog_state_t* state);
/**
 * \brief           Read latched boot reset causes captured before clearing
 *                  flags.
 *
 * \return          Bitmask of NX_RESET_*; multiple causes may be reported.
 *
 * \note            No implicit acknowledgement, reboot or product
 *                  classification.
 */
uint32_t nx_reset_cause(void);
#ifdef __cplusplus
}
#endif

#endif
