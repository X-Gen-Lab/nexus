/**
 * \file            canopen_main.c
 * \brief           Static CANopenNode application over one selected CAN owner.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/canopen.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

enum {
    ARENA_BYTES = 1536U,
    TX_SLOTS = 4U,
    TX_BUDGET_US = 20000U,
    RUN_BUDGET_US = 250000U,
    STEP_LIMIT = 200000U,
    STOP_LIMIT = 4096U
};

static nx_canopen_t s_nx_node;
static nx_canopen_tx_slot_t s_nx_tx[TX_SLOTS];
_Alignas(max_align_t) static uint8_t s_nx_arena[ARENA_BYTES];
static uint16_t s_nx_received;
static uint16_t s_nx_reported;
static const nx_canopen_object_t s_nx_objects[] = {
    {.index = 0x2000U,
     .value = &s_nx_received,
     .bytes = 2U,
     .writable = true,
     .receive_pdo = true},
    {.index = 0x2001U,
     .value = &s_nx_reported,
     .bytes = 2U,
     .transmit_pdo = true},
};

/** \brief Read the same monotonic domain used by the concrete CAN provider. */
static nx_time_us_t read_time(void* context) {
    (void)context;
    return nx_time_now_us();
}

/** \brief Verify the exact selected upstream profile fits static storage. */
static nx_result_t start_node(const nx_can_port_t* bus) {
    const size_t object_count = sizeof(s_nx_objects) / sizeof(s_nx_objects[0]);
    const size_t required = nx_canopen_workspace_bytes(object_count);
    const size_t alignment = nx_canopen_workspace_alignment();
    if (bus == NULL || required == 0U || required > sizeof(s_nx_arena) ||
        alignment == 0U || (uintptr_t)s_nx_arena % alignment != 0U) {
        return NX_ERROR_INVALID;
    }

    const nx_canopen_config_t config = {
        .port = bus,
        .clock = {read_time, NULL},
        .objects = s_nx_objects,
        .object_count = object_count,
        .tx_slots = s_nx_tx,
        .tx_capacity = TX_SLOTS,
        .receive_budget = 4U,
        .maximum_step_us = 2000U,
        .producer_heartbeat_ms = 100U,
        .consumer_heartbeat_ms = 500U,
        .sdo_timeout_ms = 100U,
        .node_id = 10U,
        .peer_node_id = 11U,
    };
    return nx_canopen_init(&s_nx_node, &config, s_nx_arena, sizeof(s_nx_arena));
}

/** \brief Execute a bounded software witness with a non-actuating PDO echo. */
static nx_result_t progress_node(void) {
    nx_time_us_t end;
    nx_result_t result =
        nx_deadline_finite_after(nx_time_now_us(), RUN_BUDGET_US, &end);
    if (result != NX_SUCCESS) {
        return result;
    }

    bool reported = false;
    for (unsigned step = 0U; step < STEP_LIMIT; ++step) {
        const nx_time_us_t now = nx_time_now_us();
        nx_time_us_t tx_deadline;
        result = nx_deadline_finite_after(now, TX_BUDGET_US, &tx_deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        if (nx_deadline_expired(end, now)) {
            return NX_SUCCESS;
        }

        nx_canopen_report_t report;
        result = nx_canopen_step(&s_nx_node, tx_deadline, &report);
        if (result != NX_SUCCESS || report.closing) {
            return result != NX_SUCCESS ? result : NX_ERROR_STATE;
        }
        if (!reported && report.nmt == NX_CANOPEN_OPERATIONAL) {
            s_nx_reported = s_nx_received;
            result = nx_canopen_request_tpdo(&s_nx_node);
            if (result != NX_SUCCESS) {
                return result;
            }
            reported = true;
        }
    }
    return NX_ERROR_TIMEOUT;
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }

    nx_result_t result = start_node(nx_factory_can_ref(NX_CAN_REF_BUS));
    if (result == NX_SUCCESS) {
        result = progress_node();
        nx_result_t stopped = NX_ERROR_BUSY;
        for (unsigned i = 0U; i < STOP_LIMIT && stopped == NX_ERROR_BUSY; ++i) {
            stopped = nx_canopen_stop(&s_nx_node);
        }
        if (stopped != NX_SUCCESS) {
            return (int)stopped;
        }
    }

    const nx_result_t closed = nx_platform_stop();
    return (int)(closed == NX_SUCCESS ? result : closed);
}
