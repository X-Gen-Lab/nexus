/**
 * \file            canopen.h
 * \brief           Selected CANopenNode server profile over an existing CAN
 * owner \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_CANOPEN_H
#define NEXUS_COMPONENTS_CANOPEN_H
#include "nexus/io/can.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Product staging variable, subindex zero, exactly 1/2/4 wire bytes.
 * \note Entries are strictly increasing in 0x2000..0x5fff. Little-endian data
 *       is mutable by the same executor only. RPDO requires writable=true.
 *       Each selected RPDO/TPDO totals 1..8 bytes, mapped in object order.
 *       These are raw staging values, never permission to mutate active gains
 *       or execute an actuator command without product validation.
 */
typedef struct {
    uint16_t index;
    void* value;
    uint8_t bytes;
    bool writable;
    bool receive_pdo;
    bool transmit_pdo;
} nx_canopen_object_t;
/** \brief Copied outgoing frame and original finite queue deadline. */
typedef struct {
    nx_can_frame_t frame;
    nx_time_us_t deadline;
} nx_canopen_tx_slot_t;
/**
 * \brief Explicit node profile, storage and sole existing classic CAN owner.
 * \note One Thread executor serializes init/step/TPDO/stop and object access.
 *       Bind a started CAN port with routes/filters for NMT, selected peer HB,
 *       default SDO and default PDO IDs. No second controller owner is created.
 *       Stable clock/port/context, objects/value storage, tx slots and
 * workspace remain live until stop succeeds and all component users have
 * joined. No IRQ call, dynamic allocator, worker or global current node exists.
 */
typedef struct {
    const nx_can_port_t* port;
    nx_clock_t clock;
    const nx_canopen_object_t* objects;
    size_t object_count;
    nx_canopen_tx_slot_t* tx_slots;
    size_t tx_capacity;
    size_t receive_budget;
    uint32_t maximum_step_us;
    uint16_t producer_heartbeat_ms;
    uint16_t consumer_heartbeat_ms;
    uint16_t sdo_timeout_ms;
    uint8_t node_id;
    uint8_t peer_node_id;
} nx_canopen_config_t;
/** \brief Protocol states, independent of electrical output or motor state. */
typedef enum {
    NX_CANOPEN_INITIALIZING,
    NX_CANOPEN_PRE_OPERATIONAL,
    NX_CANOPEN_OPERATIONAL,
    NX_CANOPEN_STOPPED
} nx_canopen_nmt_t;
/** \brief A peer is unknown until an actual heartbeat has been observed. */
typedef enum {
    NX_CANOPEN_PEER_UNKNOWN,
    NX_CANOPEN_PEER_ACTIVE,
    NX_CANOPEN_PEER_TIMEOUT
} nx_canopen_peer_t;
/** \brief NMT reset requests are observations; no reset is executed. */
typedef enum {
    NX_CANOPEN_RESET_NONE,
    NX_CANOPEN_RESET_COMMUNICATION,
    NX_CANOPEN_RESET_APPLICATION
} nx_canopen_reset_t;
/** \brief Fresh copied protocol observation, with no retained pointers. */
typedef struct {
    nx_canopen_nmt_t nmt;
    nx_canopen_peer_t peer;
    nx_canopen_reset_t reset;
    nx_result_t error;
    size_t queued;
    bool borrowed;
    bool closing;
} nx_canopen_report_t;
/** \brief Caller identity; state points into the supplied private workspace. */
typedef struct {
    void* state;
} nx_canopen_t;
/**
 * \brief Query exact workspace bytes for the selected fixed node profile.
 * \param[in] object_count: Positive number of product VAR objects.
 * \return Required bytes, or zero for impossible count/arithmetic overflow.
 * \note No heap or source preparation. Static consumers provide an explicitly
 *       budgeted aligned arena and verify this query during initialization.
 */
size_t nx_canopen_workspace_bytes(size_t object_count);
/** \brief Alignment of private selected protocol state, with no allocation. */
size_t nx_canopen_workspace_alignment(void);
/**
 * \brief Initialize one selected NMT/HB/expedited-SDO/asynchronous-PDO node.
 * \param[out] node: Fresh zero identity, distinct from all retained storage.
 * \param[in] config: Valid explicit profile; its fields are copied.
 * \param[in,out] workspace: Zero/unpublished aligned exact-profile arena.
 * \param[in] bytes: Available arena bytes, at least the reported requirement.
 * \return SUCCESS, INVALID/STATE/UNSUPPORTED; no CAN transmission on rejection.
 * \note Init reads the explicit clock but does not start/configure hardware.
 *       Input spans must be valid and disjoint from node/workspace/tx slots.
 *       Object values may alias only by explicit product design. Public APIs
 *       expose Nexus types; upstream OD and Handles stay private.
 */
nx_result_t nx_canopen_init(nx_canopen_t* node,
                            const nx_canopen_config_t* config, void* workspace,
                            size_t bytes);
/**
 * \brief Progress bounded receive, real protocol objects and one TX request.
 * \param[in,out] node: Live same-executor identity and retained storage.
 * \param[in] deadline: Finite absolute same-clock budget for newly queued TX.
 * \param[out] report: Distinct copied observation, written after progress.
 * \return SUCCESS, BUSY for reentry, or error requiring explicit stop/recovery.
 * \note Up to receive_budget frames are processed. Queueing preserves its
 *       original deadline; later calls never renew it. ACCEPTED CAN request
 *       storage remains borrowed through acquire-observed SETTLED. RX loss,
 *       time discontinuity, queue exhaustion and bus-off close admission.
 *       Reset requests close admission and are reported, never executed.
 *       No protocol write authenticates or validates product command policy.
 */
nx_result_t nx_canopen_step(nx_canopen_t* node, nx_time_us_t deadline,
                            nx_canopen_report_t* report);
/**
 * \brief Request the selected asynchronous TPDO from current staging data.
 * \param[in,out] node: Live same-executor OPERATIONAL node, not closing.
 * \return SUCCESS, STATE or BUSY for reentry; does not call hardware.
 * \note Copies wire data during a later step; caller keeps its objects stable.
 */
nx_result_t nx_canopen_request_tpdo(nx_canopen_t* node);
/**
 * \brief Close admission, discard unsent copies and drain the accepted TX.
 * \param[in,out] node: Live same-executor identity and every retained buffer.
 * \return SUCCESS clears node.state; BUSY/IO keeps the original storage alive.
 * \note Cancellation/timeout never revokes a borrow. Retry stop while provider
 *       service drains. This does not stop the outer CAN controller, reset a
 *       peer, free storage or join external component users. No auto recovery.
 */
nx_result_t nx_canopen_stop(nx_canopen_t* node);
#ifdef __cplusplus
}
#endif
#endif
