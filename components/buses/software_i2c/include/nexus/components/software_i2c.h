/**
 * \file            software_i2c.h
 * \brief           Bounded open-drain GPIO I2C with explicit bus withdrawal
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SOFTWARE_I2C_H
#define NEXUS_COMPONENTS_SOFTWARE_I2C_H

#include "nexus/io/gpio.h"
#include "nexus/io/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exactly one authorized open-drain bit with actual input readback. */
typedef struct {
    const nx_gpio_port_t* port;
    uint32_t mask;
} nx_software_i2c_pin_t;

/**
 * \brief           Immutable single-controller two-wire electrical facts
 * \note            Both pins must already be open-drain outputs with external
 *                  pull-ups and actual IDR readback. Writing one releases a
 *                  line; it never drives a push-pull high. Caller owns mode,
 *                  wiring, voltage and GPIO lifetime. half_period_us is a
 *                  minimum 5..1000000 us phase, not measured bus frequency.
 *                  poll_limit is 1..1000000 observations per timed wait.
 *                  No concurrent foreign controller, IRQ writer or clock/pin
 *                  reconfiguration is permitted. Arbitration detection avoids
 *                  forcing STOP after a collision; multi-controller operation
 *                  and bus-clear pulses are not provided.
 */
typedef struct {
    nx_software_i2c_pin_t scl;
    nx_software_i2c_pin_t sda;
    nx_clock_t clock;
    uint32_t half_period_us;
    uint32_t poll_limit;
} nx_software_i2c_config_t;

/**
 * \brief           Caller-owned physical wire owner, zero before first init
 * \note            One serialized task executor, no concurrent/reentrant call.
 *                  No descriptors or payloads survive a synchronous return.
 *                  An unproved STOP retains config, fault and pin ownership;
 *                  no buffer loan is invented. Fields are implementation state.
 *                  SCCB uses this same wire storage with an explicit distinct
 *                  personality; it is never exposed as an I2C endpoint.
 */
typedef struct {
    const nx_software_i2c_config_t* config;
    nx_time_us_t last_clock;
    nx_result_t cleanup_result;
    uint8_t personality;
    bool initialized;
    bool in_call;
    bool faulted;
    bool bus_active;
    bool stop_proved;
    bool clock_failed;
} nx_software_i2c_t;

/** \brief Immutable exact addressed child; not another bus owner. */
typedef struct {
    nx_software_i2c_t* owner;
    uint8_t address;
} nx_software_i2c_device_t;

/**
 * \brief           Bind strict I2C and observe released idle under a budget
 * \param[in,out]   owner: Fresh zero/stopped disjoint owner
 * \param[in]       config: Immutable wiring, alive through successful stop
 * \param[in]       deadline: Finite absolute time in config clock domain
 * \return          SUCCESS, INVALID, BUSY, TIMEOUT or GPIO/clock error
 * \note            Invalid input or expired preflight touches no pin. Once a
 *                  pin write is attempted, failure retains the same binding
 *                  for explicit recovery/stop. Init is not physical bus proof.
 */
nx_result_t nx_software_i2c_init(nx_software_i2c_t* owner,
                                 const nx_software_i2c_config_t* config,
                                 nx_time_us_t deadline);

/**
 * \brief           Project the synchronous strict-I2C face for one child
 * \param[in]       device: Immutable child, exact 7-bit address 08h..77h
 * \return          Shared-method value face, or zero for invalid/SCCB child
 * \note            No discovery, allocation or hardware start. Face, child,
 *                  owner, config and contexts outlive all calls. Transactions
 *                  use 1..8 messages of 1..256 bytes, repeated START between
 *                  messages and one final STOP. transferred counts completed
 *                  payload bytes, excluding address phases; partial reads may
 *                  remain on error. All spans release even when STOP fails,
 *                  while the physical bus owner remains faulted and retained.
 *                  Async submit/start are unsupported, never a stack fallback.
 */
nx_i2c_endpoint_t
nx_software_i2c_endpoint(const nx_software_i2c_device_t* device);

/**
 * \brief           Project exact controller identity and nonwaiting stop
 * \param[in,out]   owner: Stable physical owner
 * \return          Shared-method controller face; no hardware access
 * \note            Port recover has no deadline and is UNSUPPORTED. Its stop
 *                  only releases/observes an already proved idle bus; otherwise
 *                  BUSY retains ownership. Use the direct budgeted APIs below.
 */
nx_i2c_port_t nx_software_i2c_port(nx_software_i2c_t* owner);

/**
 * \brief           Explicitly attempt one protocol STOP under a new budget
 * \param[in,out]   owner: Same retained binding, with no active call
 * \param[in]       deadline: Finite cleanup time in the same live clock domain
 * \return          SUCCESS only after STOP/idle observation; other errors
 * retain the fault, immutable binding and external pin-owner duty \note Caller
 * first reestablishes exclusive bus ownership, including after arbitration, and
 * joins foreign writers/controllers. This is one explicit STOP, never
 * nine-clock bus clear, reset, transaction replay or clock-domain replacement.
 */
nx_result_t nx_software_i2c_recover(nx_software_i2c_t* owner,
                                    nx_time_us_t deadline);

/**
 * \brief           Finish one explicit STOP and drop only a proved binding
 * \param[in,out]   owner: Same serialized wire owner
 * \param[in]       deadline: Finite cleanup budget, not an expired transfer one
 * \return          SUCCESS, INVALID, BUSY or retained cleanup failure
 * \note            Same exclusive-bus precondition as recover. No outer GPIO
 *                  lease or Board modes are released here. Clock discontinuity
 *                  cannot be repaired by a new numerical deadline.
 */
nx_result_t nx_software_i2c_stop(nx_software_i2c_t* owner,
                                 nx_time_us_t deadline);

/**
 * \brief           Observe a retained bus fault without touching pins
 * \param[in]       owner: Stable owner or NULL
 * \return          True only for a non-NULL faulted owner
 */
bool nx_software_i2c_faulted(const nx_software_i2c_t* owner);

/**
 * \brief           Observe cleanup independently of the primary transfer error
 * \param[in]       owner: Stable owner
 * \return          Last cleanup outcome, STATE when arbitration forbade STOP,
 *                  or INVALID for NULL
 */
nx_result_t nx_software_i2c_cleanup_result(const nx_software_i2c_t* owner);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_SOFTWARE_I2C_H */
