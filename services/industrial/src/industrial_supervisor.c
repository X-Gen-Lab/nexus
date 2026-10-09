#include "nexus/industrial_supervisor.h"

#include <limits.h>
#include <string.h>

static void event_push(nexus_industrial_supervisor_t* s,
                       nexus_industrial_event_kind_t kind,
                       nexus_industrial_fault_t fault, uint8_t job,
                       uint64_t now) {
    if (s->event_count == NEXUS_INDUSTRIAL_EVENTS_MAX) {
        s->event_head = (uint8_t)((s->event_head + 1u) % NEXUS_INDUSTRIAL_EVENTS_MAX);
        --s->event_count;
        ++s->events_dropped;
    }
    const uint8_t index = (uint8_t)((s->event_head + s->event_count) %
                                   NEXUS_INDUSTRIAL_EVENTS_MAX);
    s->events[index] = (nexus_industrial_event_t){now, s->cycle, kind, fault, job};
    ++s->event_count;
}

static void trip(nexus_industrial_supervisor_t* s,
                 nexus_industrial_fault_t fault, uint8_t job, uint64_t now) {
    if (s->state == NEXUS_INDUSTRIAL_SAFE_FAULT) {
        return;
    }
    s->state = NEXUS_INDUSTRIAL_SAFE_FAULT;
    s->fault = fault;
    if (now > s->last_time_us) {
        s->last_time_us = now;
    }
    event_push(s, NEXUS_INDUSTRIAL_EVENT_FAULT, fault, job, now);
    if (!s->config.enter_safe(s->config.context, fault)) {
        s->fault = NEXUS_INDUSTRIAL_FAULT_SAFE_OUTPUT;
        event_push(s, NEXUS_INDUSTRIAL_EVENT_FAULT, s->fault, UINT8_MAX, now);
    }
}

bool nexus_industrial_init(nexus_industrial_supervisor_t* s,
                          const nexus_industrial_config_t* c, uint64_t now) {
    if (s == NULL || c == NULL || c->cycle_us == 0 || c->job_count == 0 ||
        c->job_count > NEXUS_INDUSTRIAL_JOBS_MAX || c->build_id == NULL ||
        c->enter_safe == NULL || c->feed_watchdog == NULL ||
        UINT64_MAX - now < c->cycle_us) {
        return false;
    }
    /* Bound the scan as well as the destination. */
    size_t build_length = 0;
    while (build_length < NEXUS_INDUSTRIAL_BUILD_ID_MAX &&
           c->build_id[build_length] != '\0') {
        ++build_length;
    }
    if (build_length == 0 || build_length == NEXUS_INDUSTRIAL_BUILD_ID_MAX) {
        return false;
    }
    for (uint8_t i = 0; i < c->job_count; ++i) {
        if (c->jobs[i].deadline_us == 0 || c->jobs[i].deadline_us > c->cycle_us) {
            return false;
        }
    }
    memset(s, 0, sizeof(*s));
    s->config = *c;
    memcpy(s->build_id, c->build_id, build_length + 1);
    s->config.build_id = s->build_id;
    s->cycle = 1;
    s->cycle_start_us = now;
    s->last_time_us = now;
    s->initialized = true;
    event_push(s, NEXUS_INDUSTRIAL_EVENT_START, NEXUS_INDUSTRIAL_FAULT_NONE,
               UINT8_MAX, now);
    if (!c->enter_safe(c->context, NEXUS_INDUSTRIAL_FAULT_NONE)) {
        trip(s, NEXUS_INDUSTRIAL_FAULT_SAFE_OUTPUT, UINT8_MAX, now);
        return false;
    }
    return true;
}

bool nexus_industrial_poll(nexus_industrial_supervisor_t* s, uint64_t now) {
    if (s == NULL || !s->initialized || s->state == NEXUS_INDUSTRIAL_SAFE_FAULT) {
        return false;
    }
    if (now < s->last_time_us) {
        trip(s, NEXUS_INDUSTRIAL_FAULT_CLOCK, UINT8_MAX, now);
        return false;
    }
    s->last_time_us = now;
    const uint64_t elapsed = now - s->cycle_start_us;
    /* Validate the prior cycle before advancing. No missed cycles are silently
     * caught up: stale progress cannot keep feeding a watchdog. */
    for (uint8_t i = 0; i < s->config.job_count; ++i) {
        if ((s->completed_mask & (1u << i)) == 0 &&
            elapsed >= s->config.jobs[i].deadline_us) {
            trip(s, NEXUS_INDUSTRIAL_FAULT_DEADLINE, i, now);
            return false;
        }
    }
    if (elapsed >= s->config.cycle_us) {
        if (!s->cycle_fed || elapsed >= (uint64_t)s->config.cycle_us * 2u ||
            s->cycle == UINT64_MAX ||
            UINT64_MAX - s->cycle_start_us < s->config.cycle_us * UINT64_C(2)) {
            trip(s, NEXUS_INDUSTRIAL_FAULT_DEADLINE, UINT8_MAX, now);
            return false;
        }
        ++s->cycle;
        s->cycle_start_us += s->config.cycle_us;
        s->completed_mask = 0;
        s->cycle_fed = false;
        /* The new cycle may already have missed an earlier job deadline. */
        return nexus_industrial_poll(s, now);
    }
    return true;
}

uint64_t nexus_industrial_cycle(const nexus_industrial_supervisor_t* s) {
    return s != NULL && s->initialized ? s->cycle : 0;
}

bool nexus_industrial_report(nexus_industrial_supervisor_t* s, uint8_t job,
                            uint64_t token, uint64_t now,
                            nexus_industrial_quality_t quality) {
    if (s == NULL || !s->initialized || job >= s->config.job_count) {
        return false;
    }
    /* Reject stale/duplicate reports before reading their old timestamps. */
    if (token != s->cycle || (s->completed_mask & (1u << job)) != 0) {
        return false;
    }
    if (!nexus_industrial_poll(s, now) || token != s->cycle) {
        return false;
    }
    if (quality != NEXUS_DATA_VALID) {
        trip(s, NEXUS_INDUSTRIAL_FAULT_DATA, job, now);
        return false;
    }
    s->completed_mask |= (uint8_t)(1u << job);
    const uint8_t required = (uint8_t)((1u << s->config.job_count) - 1u);
    if (s->completed_mask == required && !s->cycle_fed) {
        if (!s->config.feed_watchdog(s->config.context)) {
            trip(s, NEXUS_INDUSTRIAL_FAULT_WATCHDOG_PORT, UINT8_MAX, now);
            return false;
        }
        ++s->watchdog_feeds;
        s->cycle_fed = true;
        if (s->state == NEXUS_INDUSTRIAL_STARTING) {
            s->state = NEXUS_INDUSTRIAL_RUNNING;
            event_push(s, NEXUS_INDUSTRIAL_EVENT_HEALTHY,
                       NEXUS_INDUSTRIAL_FAULT_NONE, UINT8_MAX, now);
        }
    }
    return true;
}

void nexus_industrial_trip(nexus_industrial_supervisor_t* s,
                          nexus_industrial_fault_t fault, uint64_t now) {
    if (s != NULL && s->initialized) {
        trip(s, fault == NEXUS_INDUSTRIAL_FAULT_NONE ?
                    NEXUS_INDUSTRIAL_FAULT_EXTERNAL : fault, UINT8_MAX, now);
    }
}

bool nexus_industrial_rearm(nexus_industrial_supervisor_t* s, uint64_t now) {
    if (s == NULL || !s->initialized || now < s->last_time_us ||
        s->cycle == UINT64_MAX || UINT64_MAX - now < s->config.cycle_us) {
        return false;
    }
    if (!s->config.enter_safe(s->config.context, NEXUS_INDUSTRIAL_FAULT_NONE)) {
        s->state = NEXUS_INDUSTRIAL_SAFE_FAULT;
        s->fault = NEXUS_INDUSTRIAL_FAULT_SAFE_OUTPUT;
        event_push(s, NEXUS_INDUSTRIAL_EVENT_FAULT, s->fault, UINT8_MAX, now);
        return false;
    }
    ++s->cycle; /* Old completion tokens stay invalid after maintenance rearm. */
    s->cycle_start_us = now;
    s->last_time_us = now;
    s->state = NEXUS_INDUSTRIAL_STARTING;
    s->fault = NEXUS_INDUSTRIAL_FAULT_NONE;
    s->completed_mask = 0;
    s->cycle_fed = false;
    event_push(s, NEXUS_INDUSTRIAL_EVENT_RESET, NEXUS_INDUSTRIAL_FAULT_NONE,
               UINT8_MAX, now);
    return true;
}

bool nexus_industrial_event_pop(nexus_industrial_supervisor_t* s,
                               nexus_industrial_event_t* event) {
    if (s == NULL || !s->initialized || event == NULL || s->event_count == 0) {
        return false;
    }
    *event = s->events[s->event_head];
    s->event_head = (uint8_t)((s->event_head + 1u) % NEXUS_INDUSTRIAL_EVENTS_MAX);
    --s->event_count;
    return true;
}
