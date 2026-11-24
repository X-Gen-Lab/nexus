/**
 * \file            cia402_position.h
 * \brief           Selected absolute single-point Profile Position handshake
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_CIA402_POSITION_H
#define NEXUS_COMPONENTS_CIA402_POSITION_H
#include "nexus/components/cia402.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Selected position profile constants, not a wire field. */
enum nx_cia402_position_constants {
    NX_CIA402_POSITION_OBJECT_COUNT = 15,
    NX_CIA402_PROFILE_POSITION = 1
};
/**
 * \brief Fixed user-unit position, ramp and measured-settling requirements.
 * \note Position and velocity scales are independently declared, on the same
 *       mechanical side. Only absolute immediate rest-to-rest points are
 *       selected; relative, queued and moving replanning are not advertised.
 *       epoch is a nonzero application session identity. Join all users and
 *       choose a new epoch before reinitializing; generations never wrap.
 */
typedef struct {
    nx_cia402_config_t axis;
    float position_unit_rad;
    int32_t minimum_position;
    int32_t maximum_position;
    uint32_t profile_velocity;
    uint32_t position_window;
    uint16_t position_window_ms;
    float velocity_window_rad_s;
    uint64_t epoch;
} nx_cia402_position_config_t;
/**
 * \brief Actual position samples and explicit axis admission acknowledgments.
 * \note observed_at uses the bound transport's monotonic microsecond domain;
 *       sequence is consecutive and nonzero, even at rest. accepted_generation
 *       names the action genuinely admitted by the axis; it is a local adapter
 *       fact paired with acknowledgement_epoch, not a standard wire field.
 *       Samples meet the stated cadence; behavior between them is unmeasured.
 */
typedef struct {
    nx_cia402_feedback_t axis;
    float position_rad;
    nx_time_us_t observed_at;
    uint32_t sequence;
    uint32_t accepted_generation;
    uint32_t rejected_generation;
    uint64_t acknowledgement_epoch;
    bool position_valid;
} nx_cia402_position_feedback_t;
/**
 * \brief Stable repeated admission request for one explicit position point.
 * \note The adapter deduplicates generation and acknowledges actual admission,
 *       not receipt. A repeated action never authorizes another move or lease
 *       renewal. Position is copied; only an admitted axis owns execution.
 */
typedef struct {
    nx_cia402_action_t axis;
    float position_rad;
    float maximum_velocity_rad_s;
    uint32_t generation;
    uint64_t epoch;
    bool new_setpoint;
} nx_cia402_position_action_t;
/**
 * \brief Independent PP state and exact selected OD; PV pays no PP storage.
 * \note Same-executor caller storage, never copied or moved while initialized.
 *       Keep alive until bound CANopen stop and every external user join.
 */
typedef struct {
    nx_cia402_core_t core;
    nx_canopen_object_t objects[NX_CIA402_POSITION_OBJECT_COUNT];
    nx_cia402_position_config_t config;
    int32_t target_position;
    int32_t actual_position;
    int32_t retained_position;
    nx_time_us_t last_observed;
    nx_time_us_t pending_since;
    nx_time_us_t dwell_since;
    uint32_t maximum_step_us;
    uint32_t last_sequence;
    uint32_t generation;
    bool previous_setpoint;
    bool pending;
    bool acknowledged;
    bool have_target;
    bool settling;
    bool reached;
} nx_cia402_position_t;
/**
 * \brief Initialize a fresh selected PP object without I/O.
 * \param[out] drive: Fresh zero storage disjoint from config.
 * \param[in] config: Explicit finite units, bounds, ramp and settling contract.
 * \return SUCCESS, INVALID or BUSY; rejection leaves state unchanged.
 * \note No mechanical reference is discovered or hardware enabled.
 */
nx_result_t
nx_cia402_position_initialize(nx_cia402_position_t* drive,
                              const nx_cia402_position_config_t* config);
/**
 * \brief Bind fixed PP standard objects to the real asynchronous CANopen node.
 * \param[in,out] drive: Initialized stable unbound profile.
 * \param[out] node: Fresh zero node, retained through joined stop.
 * \param[in] transport: Normal explicit settings with objects=NULL/count=0.
 * \param[in,out] workspace: Aligned arena for POSITION_OBJECT_COUNT objects.
 * \param[in] bytes: Available caller bytes.
 * \return SUCCESS or exact admission error; no CAN transmission on rejection.
 * \note RPDO: 6040h(2),6060h(1),607Ah(4). TPDO: 6041h(2),6061h(1),6064h(4).
 *       Both total seven bytes. Fixed window/ramp objects remain SDO-readable.
 */
nx_result_t nx_cia402_position_node_init(nx_cia402_position_t* drive,
                                         nx_canopen_t* node,
                                         const nx_canopen_config_t* transport,
                                         void* workspace, size_t bytes);
/**
 * \brief Advance the bit-4 handshake and evaluate actual measured settling.
 * \param[in,out] drive: Stable initialized bound same-executor PP state.
 * \param[in] protocol: Fresh report from the bound CANopen node.
 * \param[in] feedback: Fresh actual sample and matching axis admission fact.
 * \param[out] action: Distinct copied application requests, never retained.
 * \return SUCCESS including observed fault, or INVALID without mutation.
 * \note Bit 12 follows genuine matching admission; bit 10 follows measured
 *       position AND velocity within fixed windows for the declared dwell.
 *       Trajectory completion alone is ignored. An unaccepted request expires
 *       after transport.maximum_step_us; a held bit 4 never generates repeats.
 *       Faults close requests; reset never automatically resumes an old point.
 */
nx_result_t
nx_cia402_position_update(nx_cia402_position_t* drive,
                          const nx_canopen_report_t* protocol,
                          const nx_cia402_position_feedback_t* feedback,
                          nx_cia402_position_action_t* action);
/**
 * \brief Clear joined profile storage without stopping an actuator.
 * \param[in,out] drive: Same-executor state and joined external users.
 * \return SUCCESS, INVALID for NULL or BUSY while protocol remains bound/live.
 * \note The physical power and mechanism still follow their own owner
 * contracts.
 */
nx_result_t nx_cia402_position_release(nx_cia402_position_t* drive);
#ifdef __cplusplus
}
#endif
#endif
