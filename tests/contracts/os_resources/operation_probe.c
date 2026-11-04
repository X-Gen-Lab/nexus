/**
 * \file            operation_probe.c
 * \brief           Execute real wait and diagnostic code with counted ports
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-04
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/os/diagnostic.h"
#include "nexus/os/wait.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t predicates;
static uint32_t arms;
static uint32_t waits;
static uint32_t saves;
static uint32_t restores;

/** \brief Count the external mask boundary without changing production code. */
nx_arch_irq_state_t nx_arch_irq_save(void) {
    ++saves;
    return (nx_arch_irq_state_t){7};
}

/** \brief Reject any failure to restore the exact incoming fixture token. */
void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    if (saved.value != 7) {
        exit(1);
    }
    ++restores;
}

/** \brief The counted model supplies one allowed privileged Thread context. */
bool nx_arch_is_privileged(void) {
    return true;
}

/** \brief This host fixture observes no physical exception or interrupt. */
uint32_t nx_arch_exception_number(void) {
    return 0;
}

/** \brief The measured predicate body has one counter and one Boolean read. */
static bool ready(void* context) {
    ++predicates;
    return *(bool*)context;
}

/** \brief Count subscription-arm calls at the existing port boundary. */
static uint32_t arm(void* context) {
    (void)context;
    ++arms;
    return 1;
}

/** \brief Terminate one real wait iteration with the actual timeout status. */
static nx_result_t wait_once(void* context, uint32_t sequence,
                             uint64_t deadline) {
    (void)context;
    (void)sequence;
    (void)deadline;
    ++waits;
    return NX_ERROR_TIMEOUT;
}

/** \brief Measure complete scans and caller-maintained cyclic start order. */
static void select_case(size_t count, unsigned scenario) {
    bool states[16] = {false};
    nx_wait_source_t sources[16];
    for (size_t index = 0; index < count; ++index) {
        states[index] = scenario == 2 || (scenario == 1 && index + 1 == count);
        sources[index] = (nx_wait_source_t){ready, &states[index]};
    }
    nx_wait_port_t port = {NULL, arm, wait_once, NULL};
    predicates = arms = waits = 0;
    size_t cursor = 0;
    for (size_t iteration = 0; iteration < (scenario == 2 ? count : 1);
         ++iteration) {
        size_t selected = SIZE_MAX;
        nx_result_t result =
            nx_wait_select(&port, sources, count, cursor, 1, &selected);
        if (scenario == 0) {
            if (result != NX_ERROR_TIMEOUT || selected != SIZE_MAX) {
                exit(1);
            }
        } else {
            size_t expected = scenario == 1 ? count - 1 : iteration;
            if (result != NX_SUCCESS || selected != expected) {
                exit(1);
            }
            cursor = (selected + 1) % count;
        }
    }
    const char* names[] = {"timeout", "last_ready", "fair_cursor"};
    printf("{\"sources\":%zu,\"case\":\"%s\",\"predicate_calls\":%" PRIu32
           ",\"arm_calls\":%" PRIu32 ",\"wait_calls\":%" PRIu32 "}",
           count, names[scenario], predicates, arms, waits);
}

/** \brief Emit actual boundary-call deltas, never an interrupt latency. */
static void diagnostic_case(const char* name, size_t copied) {
    printf("{\"case\":\"%s\",\"save_calls\":%" PRIu32
           ",\"restore_calls\":%" PRIu32 ",\"copied_record_bytes\":%zu}",
           name, saves, restores, copied);
    saves = restores = 0;
}

/** \brief Run small opt-in host counters with the maintained real C bodies. */
int main(void) {
    printf("{\"select\":[");
    const size_t counts[] = {1, 4, 16};
    for (size_t index = 0; index < 3; ++index) {
        for (unsigned scenario = 0; scenario < 3; ++scenario) {
            if (index != 0 || scenario != 0) {
                putchar(',');
            }
            select_case(counts[index], scenario);
        }
    }
    printf("],\"diagnostic\":[");
    nx_diagnostic_ring_t ring;
    nx_diagnostic_event_t storage[1];
    nx_diagnostic_event_t event = {42, 17, 23, 19};
    nx_diagnostic_event_t output = {0};
    if (nx_diagnostic_ring_init(&ring, storage, 1) != NX_SUCCESS ||
        nx_diagnostic_ring_write(&ring, &event) != NX_SUCCESS ||
        memcmp(storage, &event, sizeof(event)) != 0) {
        return 1;
    }
    diagnostic_case("write", sizeof(event));
    putchar(',');
    if (nx_diagnostic_ring_write(&ring, &event) != NX_ERROR_BUSY ||
        ring.dropped != 1) {
        return 1;
    }
    diagnostic_case("full_drop", 0);
    putchar(',');
    if (nx_diagnostic_ring_read(&ring, &output) != NX_SUCCESS ||
        memcmp(&output, &event, sizeof(event)) != 0) {
        return 1;
    }
    diagnostic_case("read", sizeof(event));
    putchar(',');
    if (nx_diagnostic_ring_read(&ring, &output) != NX_ERROR_BUSY) {
        return 1;
    }
    diagnostic_case("empty", 0);
    putchar(',');
    uint32_t dropped = 0;
    if (nx_diagnostic_ring_dropped(&ring, &dropped) != NX_SUCCESS ||
        dropped != 1) {
        return 1;
    }
    diagnostic_case("dropped_snapshot", 0);
    puts("]}");
    return 0;
}
