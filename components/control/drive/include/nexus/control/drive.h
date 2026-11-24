/**
 * \file            drive.h
 * \brief           Explicit current references and continuous control feedback
 * \author          Nexus Team
 */
#ifndef NEXUS_CONTROL_DRIVE_H
#define NEXUS_CONTROL_DRIVE_H

#include "nexus/control/axis.h"
#include "nexus/control/motor.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Complete Task-to-IRQ current reference, copied without borrowing.
 * \note received_at is the original local receipt, never refreshed by a queue.
 *       generation and parameters_revision are nonzero. The latter identifies
 *       the stopped Motor profile, not Axis's separately adopted revision.
 *       A finite current
 *       expiry must follow receipt; it remains independent from session life.
 */
typedef struct {
    nx_motor_reference_t current;
    uint32_t clock_domain;
    uint64_t epoch;
    uint32_t generation;
    uint32_t parameters_revision;
    uint32_t command_sequence;
    nx_time_us_t received_at;
} nx_drive_reference_t;

/** \brief Caller-owned SPSC reference storage and independent inhibit word.
 * \note One Task publishes, one fast executor consumes. Only atomic helpers
 *       access indices/inhibited concurrently. Other fields belong to their
 *       indicated executor. Capacity is a power of two; full rejects without
 *       overwriting. Join both executors before reinitializing any storage.
 */
typedef struct {
    nx_drive_reference_t* slots;
    uint32_t capacity;
    uint32_t clock_domain;
    uint64_t epoch;
    uint32_t produced;
    uint32_t consumed;
    uint32_t inhibited;
    uint32_t last_generation;
    bool initialized;
} nx_drive_references_t;

/** \brief Fast-executor adoption receipt, never a hardware completion. */
typedef struct {
    nx_motor_voltage_t voltage;
    nx_motor_ab_t stationary_voltage;
    uint32_t reference_generation;
    uint32_t command_sequence;
    uint64_t adopted_cycle;
    nx_time_us_t adopted_at;
    float accepted_id_a;
    float accepted_iq_a;
} nx_drive_output_t;

/** \brief One IRQ-owned algorithm instance; Task accesses only the queues.
 * \note No task, I/O, heap or hidden clock. The reference queue remains stable
 *       through hardware withdrawal and executor join. A parameter revision
 *       belongs to this stopped initialization, not mutable live fields.
 */
typedef struct {
    nx_motor_t motor;
    nx_drive_references_t* references;
    nx_drive_reference_t active;
    uint32_t parameters_revision;
    uint64_t adopted_cycle;
    nx_time_us_t adopted_at;
    nx_result_t fault;
    bool initialized;
    bool have_reference;
} nx_drive_t;

/** \brief Continuous outer-loop record, separate from droppable telemetry.
 * \note fast_first_cycle and fast_last_cycle include the shared interval
 *       endpoints. Adjacent records share one endpoint. accepted_iq_a belongs
 *       to reference_generation, not measured winding current. Match that
 *       generation's Axis output before downstream tracking; never track a
 *       newer Axis decision from this delayed receipt. command_sequence proves
 *       that command reached the fast owner by this record, not its earliest
 *       adoption time across reference generations. Mechanical units use
 *       the motor shaft unless the caller supplies a qualified side mapping.
 */
typedef struct {
    nx_axis_feedback_t mechanical;
    uint64_t fast_first_cycle;
    uint64_t fast_last_cycle;
    uint32_t reference_generation;
    uint32_t command_sequence;
    float accepted_iq_a;
} nx_drive_feedback_t;

/** \brief IRQ-producer/Task-consumer SPSC; any overflow latches lost coverage.
 * \note No automatic drop or renumbering. Once faulted, drain/reinitialize
 *       only after both executors and hardware publishers have joined.
 */
typedef struct {
    nx_drive_feedback_t* slots;
    uint32_t capacity;
    uint32_t clock_domain;
    uint64_t epoch;
    uint32_t produced;
    uint32_t consumed;
    uint32_t fault;
    uint32_t last_sequence;
    uint64_t last_fast_cycle;
    nx_time_us_t last_at;
    bool initialized;
} nx_drive_feedback_queue_t;

/** \brief Fixed decimator retaining actual interval and fast-cycle coverage.
 * \note One fast executor owns it. Positions are caller-qualified bounded
 *       motor-shaft radians; keep multi-turn integer counts upstream. It
 *       never treats Task scheduling time as a measurement interval.
 */
typedef struct {
    uint32_t clock_domain;
    uint64_t epoch;
    uint32_t division;
    uint32_t maximum_gap_us;
    uint32_t count;
    uint32_t sequence;
    uint64_t first_cycle;
    uint64_t last_cycle;
    nx_time_us_t interval_start;
    nx_time_us_t last_at;
    float first_position_rad;
    nx_result_t fault;
    bool initialized;
    bool anchored;
} nx_drive_feedback_sampler_t;

/** \brief Finite external common-bus power budget, not a protection circuit.
 * \note Positive requested power consumes the bus; negative regenerates.
 *       Budgets are nonnegative watts and are not netted against each other.
 *       The caller qualifies their source, domain and actual braking hardware.
 */
typedef struct {
    nx_time_us_t sampled_at;
    nx_time_us_t valid_until;
    float motoring_power_w;
    float regeneration_power_w;
} nx_drive_bus_budget_t;

/**
 * \brief Initialize empty caller-provided reference storage without hardware.
 * \param[out] queue: Joined, disjoint storage for this SPSC identity.
 * \param[in,out] slots: Stable aligned array for exactly capacity records.
 * \param[in] capacity: Nonzero power of two below 2^31.
 * \param[in] clock_domain: Nonzero Core domain for every reference.
 * \param[in] epoch: Nonzero epoch, immutable until all executors join.
 * \return SUCCESS or INVALID; rejection preserves queue and slots.
 * \note Serialized Task before publication. Storage is never allocated.
 */
nx_result_t nx_drive_references_initialize(nx_drive_references_t* queue,
                                           nx_drive_reference_t* slots,
                                           uint32_t capacity,
                                           uint32_t clock_domain,
                                           uint64_t epoch);

/**
 * \brief Publish a whole finite reference without changing its receipt.
 * \param[in,out] queue: Initialized queue; only its Task producer calls this.
 * \param[in] reference: Separate complete same-identity newer generation.
 * \return SUCCESS, BUSY, INVALID or STATE; rejection publishes nothing.
 * \note Task only. Publication is admission, never IRQ adoption. Full storage
 *       rejects; generations never wrap. No pointer is retained.
 */
nx_result_t nx_drive_reference_publish(nx_drive_references_t* queue,
                                       const nx_drive_reference_t* reference);

/**
 * \brief Latch the first independent inhibit without waiting for queue space.
 * \param[in,out] queue: Stable initialized identity shared with the fast owner.
 * \param[in] reason: Non-success result to retain until joined
 * reinitialization. \return SUCCESS or INVALID/STATE; no hardware operation is
 * performed. \note Task/IRQ atomic publication. The next fast step rejects;
 * hardware protection and provider withdrawal remain separately required.
 */
nx_result_t nx_drive_inhibit(nx_drive_references_t* queue, nx_result_t reason);

/**
 * \brief Initialize one stopped fast controller and its immutable revision.
 * \param[out] drive: Fresh or joined independent caller storage.
 * \param[in] motor: Complete same-domain numerical profile, copied on success.
 * \param[in,out] references: Stable initialized Task/IRQ exchange.
 * \param[in] parameters_revision: Nonzero revision required by every reference.
 * \return SUCCESS, INVALID or STATE; rejection preserves drive.
 * \note Task before the hardware session borrows drive. No actuation occurs.
 */
nx_result_t nx_drive_initialize(nx_drive_t* drive,
                                const nx_motor_config_t* motor,
                                nx_drive_references_t* references,
                                uint32_t parameters_revision);

/**
 * \brief Adopt at most one reference and compute one voltage decision.
 * \param[in,out] drive: Same sole fast executor, never accessed by Task live.
 * \param[in] sample: Qualified physical sample with exact cycle identity.
 * \param[out] out: Independent result, unchanged on every failure.
 * \return SUCCESS, EMPTY before a first reference, or exact latched fault.
 * \note Bounded IRQ/Task math. An empty queue reuses only unexpired authority.
 *       Stop, expiry, stale revision or bad measurement closes admission.
 *       Final modulation and hardware commit remain external and explicit.
 */
nx_result_t nx_drive_step(nx_drive_t* drive, const nx_motor_sample_t* sample,
                          nx_drive_output_t* out);

/**
 * \brief Track one final quantized plan through the current controller.
 * \param[in,out] drive: Same fast owner after a successful step.
 * \param[in] cycle: Exact most recent cycle, not tracked previously.
 * \param[in] voltage: Final planned dq volts in the output-angle frame.
 * \return SUCCESS or exact current-controller error.
 * \note No hardware receipt. Commit failure must latch/withdraw externally;
 *       never reuse advanced algorithm history after a failed commit.
 */
nx_result_t nx_drive_track_voltage(nx_drive_t* drive, uint64_t cycle,
                                   nx_motor_dq_t voltage);

/**
 * \brief Initialize a fixed fast-to-outer feedback decimator.
 * \param[out] sampler: Joined caller storage for one fast executor.
 * \param[in] clock_domain: Nonzero domain of the input observations.
 * \param[in] epoch: Nonzero immutable epoch.
 * \param[in] division: Positive number of fast intervals per publication.
 * \param[in] maximum_gap_us: Positive maximum between fast observations.
 * \return SUCCESS or INVALID; rejection preserves sampler.
 * \note Task before publication; no implicit clock, queue or sampling.
 */
nx_result_t nx_drive_feedback_sampler_initialize(
    nx_drive_feedback_sampler_t* sampler, uint32_t clock_domain, uint64_t epoch,
    uint32_t division, uint32_t maximum_gap_us);

/**
 * \brief Accumulate consecutive observations and produce one real interval.
 * \param[in,out] sampler: Same single fast executor.
 * \param[in] sample: Qualified same-domain current-cycle observation.
 * \param[in] position_rad: Qualified bounded position at sample_time_us.
 * \param[in] adopted: Same-cycle reference adoption receipt.
 * \param[out] out: Separate record, unchanged while BUSY or on failure.
 * \return SUCCESS at publication, BUSY while accumulating, or latched error.
 * \note IRQ/Task. Missing cycles, backward time and excessive gaps fault;
 *       observations are never renumbered to hide lost coverage.
 */
nx_result_t nx_drive_feedback_sample(nx_drive_feedback_sampler_t* sampler,
                                     const nx_motor_sample_t* sample,
                                     float position_rad,
                                     const nx_drive_output_t* adopted,
                                     nx_drive_feedback_t* out);

/**
 * \brief Initialize an empty continuous-feedback SPSC with external storage.
 * \param[out] queue: Joined independent queue metadata.
 * \param[in,out] slots: Stable aligned array of capacity records.
 * \param[in] capacity: Nonzero power of two below 2^31.
 * \param[in] clock_domain: Nonzero feedback domain.
 * \param[in] epoch: Nonzero immutable epoch.
 * \return SUCCESS or INVALID; rejection preserves all storage.
 * \note Task before publisher starts; IRQ publishes and Task takes copies.
 */
nx_result_t nx_drive_feedback_initialize(nx_drive_feedback_queue_t* queue,
                                         nx_drive_feedback_t* slots,
                                         uint32_t capacity,
                                         uint32_t clock_domain, uint64_t epoch);

/**
 * \brief Publish one whole consecutive control record, never droppable data.
 * \param[in,out] queue: Same single IRQ/fast producer.
 * \param[in] feedback: Complete disjoint same-identity observation.
 * \return SUCCESS, INVALID or exact latched STATE/OVERFLOW fault.
 * \note Full storage latches lost coverage independently of consumer progress.
 *       A telemetry channel must be a different object with its own contract.
 */
nx_result_t nx_drive_feedback_publish(nx_drive_feedback_queue_t* queue,
                                      const nx_drive_feedback_t* feedback);

/**
 * \brief Copy one control record and return its slot to the fast producer.
 * \param[in,out] queue: Same single Task consumer.
 * \param[out] feedback: Separate output, unchanged on EMPTY or failure.
 * \return SUCCESS, EMPTY or exact latched feedback fault.
 * \note Task only. A latched coverage fault cannot be repaired by draining
 *       old records or inventing a new sequence; stop/join before reset.
 */
nx_result_t nx_drive_feedback_take(nx_drive_feedback_queue_t* queue,
                                   nx_drive_feedback_t* feedback);

/**
 * \brief Compute conservative per-axis power scales for one common bus.
 * \param[in] budget: Qualified finite nonnegative consumption/regeneration.
 * \param[in] requested_power_w: Count complete signed per-axis watt requests.
 * \param[in] count: Actual positive axis count, no maximum array is allocated.
 * \param[in] now: Finite current time in the budget's qualified local domain.
 * \param[out] scales: Disjoint count results in [0,1], unchanged on failure.
 * \return SUCCESS, INVALID, TIMEOUT or EXHAUSTED; no retained references.
 * \note Task only. Positive/negative demands use separate totals; no assumed
 *       credit for opposing motors. Apply reference limits and track Axis
 *       downstream acceptance explicitly. This is not hardware protection.
 */
nx_result_t nx_drive_bus_limit(const nx_drive_bus_budget_t* budget,
                               const float* requested_power_w, size_t count,
                               nx_time_us_t now, float* scales);

#ifdef __cplusplus
}
#endif
#endif
