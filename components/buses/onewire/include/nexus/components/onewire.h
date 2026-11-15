/**
 * \file            onewire.h
 * \brief           Bounded standard-speed open-drain one-wire transactions
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_ONEWIRE_H
#define NEXUS_COMPONENTS_ONEWIRE_H
#include "nexus/core/time.h"
#include "nexus/io/gpio.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Immutable electrical and monotonic-clock facts, borrowed until stop.
 * \note Exactly one preconfigured open-drain output, writing one releases it,
 *       actual input readback and an external pull-up are required. Only one
 *       serialized task executor controls this physical bus; no concurrent
 *       pin, clock, mode or configuration writer. poll_limit is1..1000000.
 *       Standard-speed timing uses fixed conservative slot targets and finite
 *       observed jitter windows. This detects overruns; it is not a WCET,
 *       electrical rise-time or physical qualification. No IRQ masks or worker.
 */
typedef struct {
    const nx_gpio_port_t* port;
    uint32_t mask;
    nx_clock_t clock;
    uint32_t poll_limit;
} nx_onewire_config_t;
/** \brief Stable caller-owned wire state, initially zero and never copied.
 * \note Failed release retains this binding/fault, never caller buffers.
 *       Opaque GPIO/clock contexts stay live and disjoint through stop/joins.
 *       Clock discontinuity is terminal for transactions in this clock epoch.
 */
typedef struct {
    const nx_onewire_config_t* config;
    nx_time_us_t last_clock;
    nx_result_t cleanup_result;
    bool initialized;
    bool in_call;
    bool faulted;
    bool clock_failed;
} nx_onewire_t;
/** \brief Bind exact wiring and observe a released idle line under a budget.
 * \param[in,out] owner: Stable zero/stopped disjoint state.
 * \param[in] config: Immutable wiring, preconfigured externally.
 * \param[in] deadline: Finite absolute monotonic deadline.
 * \return SUCCESS, INVALID, BUSY, TIMEOUT or GPIO/clock failure.
 * \note Input/expired preflight is effect-free. After a pin write, failed
 *       withdrawal retains the binding for stop/recovery; no CPU buffer loan.
 */
nx_result_t nx_onewire_init(nx_onewire_t* owner,
                            const nx_onewire_config_t* config,
                            nx_time_us_t deadline);
/** \brief Reset/presence, send LSB-first bytes, then receive bounded bits.
 * \param[in,out] owner: Initialized serialized wire owner.
 * \param[in] tx: Separate readable1..16 bytes, unchanged throughout call.
 * \param[in] tx_bytes: Exact transmitted length.
 * \param[out] rx: Separate writable ceil(rx_bits/8) bytes, or NULL if zero.
 * \param[in] rx_bits: Zero to128 bits; unused high bits are zero.
 * \param[in] deadline: Finite absolute time, not a relative timeout.
 * \return SUCCESS or bounded timing/context/state/GPIO error.
 * \note No buffers survive return; partial receive bytes may change on error.
 *       Failure releases the open-drain line and records cleanup separately.
 *       Failure to prove release retains physical ownership. No ROM discovery,
 *       parasite supply, strong pull-up, overdrive or multichannel registry.
 */
nx_result_t nx_onewire_exchange(nx_onewire_t* owner, const uint8_t* tx,
                                size_t tx_bytes, uint8_t* rx, size_t rx_bits,
                                nx_time_us_t deadline);
/** \brief Observe the same finite monotonic epoch without touching the wire.
 * \param[in,out] owner: Stable initialized state, no overlapping executor.
 * \param[out] now: Separate writable output, unchanged on failure.
 * \return SUCCESS or STATE/INVALID/IO; backward/NEVER readings fault epoch.
 */
nx_result_t nx_onewire_now(nx_onewire_t* owner, nx_time_us_t* now);
/** \brief Release/observe the line and close admission without a protocol
 * reset. \param[in,out] owner: Same state, no active or reentrant caller.
 * \param[in] deadline: Finite new withdrawal budget in the same live epoch.
 * \return SUCCESS when released; failure retains binding and wire duty.
 * \note No descriptors survive; a stopped sensor may continue autonomous work.
 *       Faulted withdrawal additionally holds the wire through 500us after
 *       physical high observation, then rechecks high and deadline. A short
 *       deadline or insufficient poll count retains the binding for recovery.
 */
nx_result_t nx_onewire_stop(nx_onewire_t* owner, nx_time_us_t deadline);
/** \brief Dallas/Maxim CRC8 (reflected polynomial8Ch), initial accumulator0.
 * \param[in] bytes: Readable bytes, NULL permitted only when length is zero.
 * \param[in] length: Exact finite byte count.
 * \return CRC8; zero for NULL. No mutation, allocation or timing dependence.
 */
uint8_t nx_onewire_crc8(const uint8_t* bytes, size_t length);
#ifdef __cplusplus
}
#endif
#endif
