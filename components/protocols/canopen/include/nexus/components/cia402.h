/**
 * \file            cia402.h
 * \brief           Explicit CiA 402 Profile Velocity objects and axis requests
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_CIA402_H
#define NEXUS_COMPONENTS_CIA402_H
#include "nexus/components/canopen.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Selected velocity profile constants, not a wire field. */
enum nx_cia402_constants {
    NX_CIA402_OBJECT_COUNT = 12,
    NX_CIA402_PROFILE_VELOCITY = 3
};
/**
 * \brief Immutable user-unit scaling and fixed Profile Velocity limits.
 * \note Velocity objects use signed units; acceleration objects use the same
 *       units per second. The mechanical side and direction are product facts.
 *       No CiA 402 universal radian scaling is implied. Limits and ramps are
 *       read-only SDO objects in this selected profile, not live parameters.
 */
typedef struct {
    float velocity_unit_rad_s;
    uint32_t maximum_velocity;
    uint32_t acceleration;
    uint32_t deceleration;
    uint32_t quick_stop_deceleration;
} nx_cia402_config_t;
/**
 * \brief Causally current axis facts supplied by the same Thread executor.
 * \note The application proves freshness, units and mechanical-side conversion.
 *       ready, voltage_enabled and operation_enabled describe actual observed
 *       conditions, never the preceding action request. target_reached requires
 *       the application's velocity/quality/dwell criterion. The profile neither
 *       controls hardware nor interprets trajectory completion as arrival.
 */
typedef struct {
    float velocity_rad_s;
    int8_t mode;
    bool valid;
    bool ready;
    bool voltage_enabled;
    bool operation_enabled;
    bool quick_stopping;
    bool target_reached;
    bool limited;
    bool fault;
    bool reset_permitted;
} nx_cia402_feedback_t;
/**
 * \brief Copied requests for the application's actuator/axis admission layer.
 * \note These are requests, not applied states or physical safety guarantees.
 *       Same-value repeats are not new-command evidence. The application owns
 *       an explicit authorization lease; NMT or these snapshots do not renew
 * it. halt and quick_stop require valid local feedback and energy handling.
 *       Deactivation still requires the real owner to withdraw and join.
 */
typedef struct {
    float velocity_rad_s;
    float acceleration_rad_s2;
    float deceleration_rad_s2;
    bool enable_voltage;
    bool enable_operation;
    bool halt;
    bool quick_stop;
    bool reset_fault;
} nx_cia402_action_t;
/** \brief Selected drive states, independently confirmed by axis facts. */
typedef enum {
    NX_CIA402_SWITCH_ON_DISABLED,
    NX_CIA402_READY_TO_SWITCH_ON,
    NX_CIA402_SWITCHED_ON,
    NX_CIA402_OPERATION_ENABLED,
    NX_CIA402_QUICK_STOP_ACTIVE,
    NX_CIA402_FAULT_REACTION_ACTIVE,
    NX_CIA402_FAULT
} nx_cia402_state_t;
/**
 * \brief Caller-owned selected profile and stable private-node OD storage.
 * \note One Thread executor serializes every API and CANopen step. Fields are
 *       not task/IRQ controls. Keep the object and its bound node alive until
 *       CANopen stop succeeds and external users join, then release. There is
 *       no allocator, worker, controller owner, automatic motion or reset.
 */
typedef struct {
    nx_cia402_config_t config;
    nx_canopen_t* node;
    uint16_t error_code;
    uint16_t controlword;
    uint16_t statusword;
    int8_t requested_mode;
    int8_t selected_mode;
    int8_t actual_mode;
    int32_t actual_velocity;
    int32_t target_velocity;
    uint32_t supported_modes;
    nx_cia402_state_t state;
    uint16_t previous_controlword;
    bool initialized;
    bool fault_latched;
    bool reset_pending;
} nx_cia402_core_t;
/** \brief PV owns exactly its selected OD, without position-mode storage. */
typedef struct {
    nx_cia402_core_t core;
    nx_canopen_object_t objects[NX_CIA402_OBJECT_COUNT];
} nx_cia402_t;
/**
 * \brief Initialize a fresh profile without I/O or retained input pointers.
 * \param[out] drive: Fresh zeroed state, disjoint from config.
 * \param[in] config: Explicit finite scaling and positive representable limits.
 * \return SUCCESS, INVALID or BUSY; rejection leaves state unchanged.
 * \note No mode is advertised until actual axis feedback confirms mode 3.
 */
nx_result_t nx_cia402_initialize(nx_cia402_t* drive,
                                 const nx_cia402_config_t* config);
/**
 * \brief Bind the profile's real standard OD to one existing CANopen transport.
 * \param[in,out] drive: Initialized stable unbound profile.
 * \param[out] node: Fresh zero identity, retained through joined stop.
 * \param[in] transport: Explicit CANopen settings, objects=NULL/count=0.
 * \param[in,out] workspace: Aligned caller arena for OBJECT_COUNT objects.
 * \param[in] bytes: Available bytes, checked by the existing CANopen query.
 * \return SUCCESS or exact admission error; no CAN transmission on rejection.
 * \note RPDO: controlword(2), mode(1), target velocity(4). TPDO: statusword(2),
 *       actual mode(1), velocity(4). Each is seven bytes, asynchronously
 * mapped. SDO exposes fixed limits; no SYNC, CSP/CSV, homing or PP is selected.
 */
nx_result_t nx_cia402_node_init(nx_cia402_t* drive, nx_canopen_t* node,
                                const nx_canopen_config_t* transport,
                                void* workspace, size_t bytes);
/**
 * \brief Consume real protocol and actual-axis facts, then copy one request.
 * \param[in,out] drive: Same-executor initialized live profile.
 * \param[in] protocol: Fresh report from the bound node's completed step.
 * \param[in] feedback: Fresh validated actual-axis facts, not previous
 * requests. \param[out] action: Distinct complete copy, never retained. \return
 * SUCCESS also reports latched faults; INVALID leaves output unchanged. \note
 * Invalid feedback, target bounds, live network loss and protocol reset close
 * admission. A rising fault-reset only requests recovery; observed reset
 * permission and removed fault are required before DISABLED. It never enables
 * motion. Caller still owns command freshness and hardware shutdown. A
 * protocol/axis observation is not an electrical guarantee.
 */
nx_result_t nx_cia402_update(nx_cia402_t* drive,
                             const nx_canopen_report_t* protocol,
                             const nx_cia402_feedback_t* feedback,
                             nx_cia402_action_t* action);
/**
 * \brief Clear profile storage only after the bound protocol has stopped.
 * \param[in,out] drive: Same-executor state; all external users already joined.
 * \return SUCCESS or BUSY while node state is still live.
 * \note Does no I/O and does not prove that the motor has stopped or
 * deenergized.
 */
nx_result_t nx_cia402_release(nx_cia402_t* drive);
#ifdef __cplusplus
}
#endif
#endif
