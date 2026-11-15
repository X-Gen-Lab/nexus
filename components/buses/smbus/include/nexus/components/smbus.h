/**
 * \file            smbus.h
 * \brief           Bounded packet PEC over one reviewed asynchronous I2C
 * binding \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SMBUS_H
#define NEXUS_COMPONENTS_SMBUS_H
#include "nexus/io/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_SMBUS_MAX_PAYLOAD_BYTES 32U

/** \brief One immutable transport and its exact seven-bit PEC address.
 * \note Generated bindings derive all three fields from one I2C endpoint.
 *       Manual integration must supply the same wire address as the endpoint;
 *       an opaque endpoint does not provide runtime address authentication.
 *       Endpoint/controller identity is checked without opening hardware.
 *       Binding, faces, operations and opaque contexts outlive adapter stop.
 *       This is packet framing, not a hardware SMBTYPE/timeout/ALERT owner.
 */
typedef struct {
    const nx_i2c_endpoint_t* endpoint;
    const nx_i2c_port_t* controller;
    uint8_t address;
} nx_smbus_binding_t;

/** \brief Immutable payload and an explicitly writable terminal PEC byte.
 * \note Empty is {NULL,0,NULL}. Payload is1..32 bytes. A write-only packet
 *       requires pec == data + payload_bytes in the same actual contiguous
 *       wire allocation, with that tail writable. No const cast or payload
 *       copy is made. Write/read uses pec=NULL; its only PEC ends the RX.
 */
typedef struct {
    const uint8_t* data;
    size_t payload_bytes;
    uint8_t* pec;
} nx_smbus_tx_t;

/** \brief Caller-owned payload plus one byte reserved for received PEC.
 * \note Empty is {NULL,0,0}. Payload is1..32 bytes and capacity covers at
 *       least payload_bytes+1. The whole capacity is disjoint from TX and
 *       all adapter, binding and transport metadata. RX bytes are untrusted
 *       until acquire-observed SETTLED reports SUCCESS and PEC_VALID.
 */
typedef struct {
    uint8_t* data;
    size_t payload_bytes;
    size_t capacity;
} nx_smbus_rx_t;

/** \brief Received-packet integrity, readable only after acquire-SETTLED.
 * \note Writes remain NOT_CHECKED: an ACK does not prove peer PEC policy.
 *       CRC8 detects transmission errors; it provides no authentication.
 */
typedef enum {
    NX_SMBUS_PEC_NOT_CHECKED,
    NX_SMBUS_PEC_VALID,
    NX_SMBUS_PEC_ERROR
} nx_smbus_pec_state_t;

typedef struct nx_smbus nx_smbus_t;

/** \brief One finite packet, borrowed through acquire-observed SETTLED.
 * \note Initialize base before prepare. TX payload stays immutable; only the
 *       explicit PEC tail and RX wire storage may be written after acceptance.
 *       Successful transferred counts TX+RX payload bytes, excluding PEC and
 *       addresses. Errors report zero, with possible remote effects and partial
 *       RX writes. Never replay automatically or consume failed RX as valid.
 *       Fields are private after prepare; READY is bound to one owner epoch.
 */
typedef struct {
    nx_request_t base;
    nx_smbus_tx_t tx;
    nx_smbus_rx_t rx;
    const nx_smbus_t* owner;
    uint64_t generation;
    nx_smbus_pec_state_t pec;
} nx_smbus_request_t;

/** \brief Optional single-executor adapter; zero before its first initialize.
 * \note Private control after initialization, never copied or zeroed to revoke
 *       a borrow. Exactly two message headers support the one repeated START;
 *       there is no payload scratch, queue, allocation, task or hidden retry.
 *       One unmasked task serializes calls and I2C execution. No reentry or
 *       direct competing submission through this binding while active.
 *       The Core clock domain and underlying I2C owner stay live until stop.
 */
struct nx_smbus {
    const nx_smbus_binding_t* binding;
    nx_smbus_request_t* active;
    nx_i2c_request_t transfer;
    nx_i2c_message_t messages[2];
    uint64_t generation;
    bool initialized;
    bool closing;
    bool in_call;
};

/** \brief Bind a preconfigured endpoint without opening or changing hardware.
 * \param[in,out] owner: Fresh zero or successfully stopped independent control.
 * \param[in] binding: Immutable matching transport/address,08h..77h.
 * \return SUCCESS or INVALID/UNSUPPORTED/CONTEXT/BUSY/EXHAUSTED.
 * \note Task-only. Opaque provider control extents are externally disjoint
 *       from this adapter and every request/payload; public extents are
 * checked.
 */
nx_result_t nx_smbus_initialize(nx_smbus_t* owner,
                                const nx_smbus_binding_t* binding);

/** \brief Prepare write, read, or write/read with one terminal packet PEC.
 * \param[in] owner: Initialized adapter and unchanged transport generation.
 * \param[in,out] request: Initialized/consumed descriptor, disjoint from data.
 * \param[in] tx: Immutable payload and optional explicit writable PEC tail.
 * \param[in] rx: Writable payload and reserved PEC capacity.
 * \param[in] deadline: Finite absolute Core deadline, unchanged clock domain.
 * \return SUCCESS or INVALID/STATE/CONTEXT/BUSY; failure changes no packet.
 * \note Task-only, no callback or payload access, hardware or PEC-tail writes.
 *       No SMBus command, dynamic block-count or product register policy.
 */
nx_result_t nx_smbus_prepare(nx_smbus_t* owner, nx_smbus_request_t* request,
                             nx_smbus_tx_t tx, nx_smbus_rx_t rx,
                             nx_time_us_t deadline);

/** \brief Accept one packet before writing its PEC tail or submitting I2C.
 * \return SUCCESS is ACCEPTED; inspect acquire-SETTLED for final status.
 *       Rejection has no retained reference, PEC write or transport effect.
 * \note Task-only, bounded. An inner start rejection after outer acceptance
 *       settles the outer request with that error; it never leaks a borrow.
 */
nx_result_t nx_smbus_submit(nx_smbus_t* owner, nx_smbus_request_t* request);

/** \brief Advance the existing I2C owner and observe its authoritative loan.
 * \return BUSY while borrowed, terminal packet result on settlement, or
 *       SUCCESS while idle; CONTEXT/STATE leaves existing ownership intact.
 * \note Task-only. QUARANTINED storage remains borrowed. Inner SETTLED must
 *       prove complete hardware/IRQ withdrawal before PEC verification and
 *       final outer publication. Only final packet SUCCESS validates RX.
 */
nx_result_t nx_smbus_service(nx_smbus_t* owner);

/** \brief Request withdrawal without revoking wire-storage ownership.
 * \return SUCCESS for requested/already completed cancellation, otherwise
 *       the exact context, identity or underlying cancellation error.
 * \note Task-only. Already settled I2C wins a later cancel. Continue service
 *       until outer SETTLED, including after timeout or cancellation failure.
 */
nx_result_t nx_smbus_cancel(nx_smbus_t* owner, nx_smbus_request_t* request);

/** \brief Close packet admission and join this adapter's current request.
 * \return SUCCESS after settlement, BUSY while withdrawal remains borrowed,
 *       otherwise CONTEXT/STATE. Failure retains the same binding/control.
 * \note Task-only. This does not stop/reset the shared I2C controller or return
 *       its resources. Caller stops that independent owner after all consumers.
 *       Repeated successful stop is a no-op and does not call the transport.
 */
nx_result_t nx_smbus_stop(nx_smbus_t* owner);

#ifdef __cplusplus
}
#endif
#endif
