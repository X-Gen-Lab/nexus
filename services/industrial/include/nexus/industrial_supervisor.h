#ifndef NEXUS_INDUSTRIAL_SUPERVISOR_H
#define NEXUS_INDUSTRIAL_SUPERVISOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXUS_INDUSTRIAL_JOBS_MAX 8u
#define NEXUS_INDUSTRIAL_EVENTS_MAX 16u
#define NEXUS_INDUSTRIAL_BUILD_ID_MAX 64u

typedef enum {
    NEXUS_INDUSTRIAL_STARTING = 0,
    NEXUS_INDUSTRIAL_RUNNING,
    NEXUS_INDUSTRIAL_SAFE_FAULT
} nexus_industrial_state_t;

typedef enum {
    NEXUS_DATA_VALID = 0,
    NEXUS_DATA_STALE,
    NEXUS_DATA_INVALID
} nexus_industrial_quality_t;

typedef enum {
    NEXUS_INDUSTRIAL_FAULT_NONE = 0,
    NEXUS_INDUSTRIAL_FAULT_DEADLINE,
    NEXUS_INDUSTRIAL_FAULT_DATA,
    NEXUS_INDUSTRIAL_FAULT_CLOCK,
    NEXUS_INDUSTRIAL_FAULT_WATCHDOG_PORT,
    NEXUS_INDUSTRIAL_FAULT_SAFE_OUTPUT,
    NEXUS_INDUSTRIAL_FAULT_EXTERNAL
} nexus_industrial_fault_t;

typedef enum {
    NEXUS_INDUSTRIAL_EVENT_START = 1,
    NEXUS_INDUSTRIAL_EVENT_HEALTHY,
    NEXUS_INDUSTRIAL_EVENT_FAULT,
    NEXUS_INDUSTRIAL_EVENT_RESET
} nexus_industrial_event_kind_t;

typedef struct {
    uint64_t time_us;
    uint64_t cycle;
    nexus_industrial_event_kind_t kind;
    nexus_industrial_fault_t fault;
    uint8_t job; /* UINT8_MAX for a fault outside a particular job. */
} nexus_industrial_event_t;

typedef struct {
    uint32_t deadline_us; /* finish strictly BEFORE this cycle-relative time. */
} nexus_industrial_job_t;

typedef struct {
    uint32_t cycle_us;
    uint8_t job_count;
    nexus_industrial_job_t jobs[NEXUS_INDUSTRIAL_JOBS_MAX];
    const char* build_id; /* copied, nonempty, max 63 bytes */
    uint32_t reset_reason; /* board-provided raw reason, retained for diagnosis */
    void* context;
    /* Both callbacks are bounded and nonblocking task-context operations.
     * enter_safe implements PRODUCT policy, including deenergizing outputs.
     * Only this supervisor may feed the physical watchdog. False is a fault.
     */
    bool (*enter_safe)(void*, nexus_industrial_fault_t);
    bool (*feed_watchdog)(void*);
} nexus_industrial_config_t;

typedef struct {
    nexus_industrial_config_t config;
    char build_id[NEXUS_INDUSTRIAL_BUILD_ID_MAX];
    uint64_t cycle;
    uint64_t cycle_start_us;
    uint64_t last_time_us;
    uint64_t watchdog_feeds;
    uint64_t events_dropped;
    nexus_industrial_state_t state;
    nexus_industrial_fault_t fault;
    uint8_t completed_mask;
    uint8_t event_head;
    uint8_t event_count;
    nexus_industrial_event_t events[NEXUS_INDUSTRIAL_EVENTS_MAX];
    bool initialized;
    bool cycle_fed;
} nexus_industrial_supervisor_t;

/* Task context, serialized by the control task. No heap, locks, sleeping or
 * telemetry formatting. Token identifies a released cycle: an old callback
 * cannot satisfy a new cycle. Independent communication jobs stay out of the
 * watchdog mask unless the product explicitly considers them safety-critical.
 */
bool nexus_industrial_init(nexus_industrial_supervisor_t*,
                          const nexus_industrial_config_t*, uint64_t now_us);
bool nexus_industrial_poll(nexus_industrial_supervisor_t*, uint64_t now_us);
uint64_t nexus_industrial_cycle(const nexus_industrial_supervisor_t*);
bool nexus_industrial_report(nexus_industrial_supervisor_t*, uint8_t job,
                            uint64_t cycle_token, uint64_t completed_us,
                            nexus_industrial_quality_t quality);
void nexus_industrial_trip(nexus_industrial_supervisor_t*,
                          nexus_industrial_fault_t, uint64_t now_us);
/* Explicit authorized rearm; never call automatically from a network write.
 * The caller must resolve interlocks/root cause before invoking this API.
 * Reset still invokes enter_safe and requires a fresh complete healthy cycle.
 */
bool nexus_industrial_rearm(nexus_industrial_supervisor_t*, uint64_t now_us);
bool nexus_industrial_event_pop(nexus_industrial_supervisor_t*,
                               nexus_industrial_event_t*);

#ifdef __cplusplus
}
#endif
#endif
