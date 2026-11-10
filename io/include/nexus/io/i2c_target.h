/**
 * \file            i2c_target.h
 * \brief           Finite I2C target exchanges and explicit SMBus responses
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_I2C_TARGET_H
#define NEXUS_IO_I2C_TARGET_H
#include "nexus/core/request.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif

#define NX_I2C_TARGET_MAX_BYTES 256U
/** \brief One addressed receive, transmit, or receive/repeated-START transmit.
 * \note ALERT_RESPONSE explicitly requests the SMBus ARA transaction and its
 *       provider-owned address byte. Its descriptor remains borrowed while
 *       expected ARA arbitration retries until success or a finite deadline.
 */
typedef enum nx_i2c_target_direction {
    NX_I2C_TARGET_RECEIVE,
    NX_I2C_TARGET_TRANSMIT,
    NX_I2C_TARGET_WRITE_READ,
    NX_I2C_TARGET_ALERT_RESPONSE
} nx_i2c_target_direction_t;
/** \brief Observed hardware/protocol diagnostics, independent of byte counts.
 * \note ARA arbitration is an expected retry and can accompany SUCCESS.
 */
typedef enum nx_i2c_target_fault {
    NX_I2C_TARGET_BUS_ERROR = 1U,
    NX_I2C_TARGET_ARBITRATION = 2U,
    NX_I2C_TARGET_OVERRUN = 4U,
    NX_I2C_TARGET_PEC_ERROR = 8U,
    NX_I2C_TARGET_CLOCK_TIMEOUT = 16U,
    NX_I2C_TARGET_PROTOCOL_ERROR = 32U
} nx_i2c_target_fault_t;

/** \brief Complete caller-owned exchange borrowed through acquire-SETTLED.
 * \note All spans are disjoint from metadata, provider state and one another.
 *       RX capacity is 1..256 bytes; RECEIVE with PEC requires exactly this
 *       many payload bytes and a separate PEC. TX has 1..256 immutable bytes.
 *       WRITE_READ supplies a precomputed response after one repeated START;
 *       PEC spans both address directions and payloads, ending in TX PEC.
 *       Application register/protocol policy is external. ARA has no payload
 *       span. Counts exclude addresses/PEC; ARA completion counts one byte.
 *       Owner/generation/epoch are provider-written preparation identities.
 *       Result fields may be read only after acquire-observed SETTLED.
 */
typedef struct {
    nx_request_t base;
    const void* owner;
    uint64_t generation;
    uint64_t time_epoch;
    uint8_t* rx;
    const uint8_t* tx;
    size_t rx_capacity;
    size_t tx_length;
    nx_i2c_target_direction_t direction;
    size_t rx_received;
    size_t tx_completed;
    uint32_t faults;
    uint16_t matched_address;
    uint8_t address_phases;
} nx_i2c_target_request_t;

/** \brief Readonly methods shared by static single-executor controllers. */
typedef struct {
    nx_result_t (*prepare)(void*, nx_i2c_target_request_t*, uint8_t*, size_t,
                           const uint8_t*, size_t, nx_i2c_target_direction_t,
                           nx_time_us_t);
    nx_result_t (*submit)(void*, nx_i2c_target_request_t*);
    nx_result_t (*cancel)(void*, nx_i2c_target_request_t*);
    void (*service)(void*);
    nx_result_t (*stop)(void*);
    nx_result_t (*recover)(void*);
} nx_i2c_target_ops_t;
/** \brief Immutable existing controller face; lookup has no hardware effects.
 */
struct nx_i2c_target_port {
    const nx_i2c_target_ops_t* ops;
    void* context;
};

/** \brief Bind unborrowed spans and a finite deadline to one live generation.
 * \param[in] port: Prepared static controller, privileged unmasked executor.
 * \param[in,out] request: Initialized or consumed SETTLED descriptor.
 * \param[out] rx: RX capacity bytes or NULL when the direction has no RX.
 * \param[in] rx_capacity: Positive bounded capacity, or zero without RX.
 * \param[in] tx: Immutable response bytes, or NULL without caller TX.
 * \param[in] tx_length: Positive bounded response length, or zero without TX.
 * \param[in] direction: Explicit finite wire sequence.
 * \param[in] deadline: Absolute Core deadline, including source withdrawal,
 *       in this live controller epoch. Expiry never revokes a retained borrow.
 * \return SUCCESS or rejection with no new borrow or controller/pad writes.
 * \note Observing the existing System clock can maintain its shared counter;
 *       it never arms I2C acknowledgement or acquires another clock owner.
 */
nx_result_t nx_i2c_target_prepare(const nx_i2c_target_port_t* port,
                                  nx_i2c_target_request_t* request, uint8_t* rx,
                                  size_t rx_capacity, const uint8_t* tx,
                                  size_t tx_length,
                                  nx_i2c_target_direction_t direction,
                                  nx_time_us_t deadline);
/** \brief Accept one whole descriptor before enabling address acknowledgement.
 * \note Peer traffic begins after successful arming. SUCCESS transfers all
 *       spans until SETTLED, including start failures, cancellation and
 * timeout. One privileged unmasked Thread executor calls service until settled.
 */
nx_result_t nx_i2c_target_submit(const nx_i2c_target_port_t* port,
                                 nx_i2c_target_request_t* request);
/** \brief Begin cancellation without revoking a borrow or replacing a result.
 */
nx_result_t nx_i2c_target_cancel(const nx_i2c_target_port_t* port,
                                 nx_i2c_target_request_t* request);
/** \brief Bounded Thread progress; no worker, allocation or implicit replay. */
void nx_i2c_target_service(const nx_i2c_target_port_t* port);
/** \brief Close admission and drain every source before controller release.
 * \return SUCCESS after withdrawal, BUSY/IO with retained cleanup duty.
 * \note Reset-based abort releases this controller's outputs/references; it
 *       does not certify shared-bus recovery or generate GPIO clock pulses.
 */
nx_result_t nx_i2c_target_stop(const nx_i2c_target_port_t* port);
/** \brief Reinitialize an idle faulted owner without replaying old storage.
 * \note Once cold close starts, only close may release its retained receipt;
 *       recovery cannot rearm partially restored pads, clocks or IRQs.
 */
nx_result_t nx_i2c_target_recover(const nx_i2c_target_port_t* port);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_IO_I2C_TARGET_H */
