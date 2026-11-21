/**
 * \file            fsm.c
 * \brief           Synchronous table-driven finite state machines
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/fsm.h"

static bool valid_config(const nx_fsm_config_t* config, size_t initial) {
    if (config->states == NULL || initial >= config->state_count ||
        (config->transition_count != 0 && config->transitions == NULL)) {
        return false;
    }
    for (size_t i = 0; i < config->transition_count; ++i) {
        const nx_fsm_transition_t* row = &config->transitions[i];
        if (row->source >= config->state_count ||
            row->target >= config->state_count ||
            (row->kind != NX_FSM_EXTERNAL && row->kind != NX_FSM_INTERNAL) ||
            (row->kind == NX_FSM_INTERNAL && row->source != row->target)) {
            return false;
        }
    }
    return true;
}

nx_result_t nx_fsm_initialize(nx_fsm_t* machine, const nx_fsm_config_t* config,
                              void* context, size_t initial) {
    if (machine == NULL || config == NULL) {
        return NX_ERROR_INVALID;
    }
    if (machine->busy) {
        return NX_ERROR_BUSY;
    }
    if (!valid_config(config, initial)) {
        return NX_ERROR_INVALID;
    }

    machine->config = config;
    machine->context = context;
    machine->current = initial;
    machine->busy = true;
    if (config->states[initial].entry != NULL) {
        config->states[initial].entry(context, NULL);
    }
    machine->busy = false;
    return NX_SUCCESS;
}

nx_result_t nx_fsm_dispatch(nx_fsm_t* machine, const nx_fsm_event_t* event) {
    if (machine == NULL || event == NULL ||
        (event->size != 0 && event->payload == NULL)) {
        return NX_ERROR_INVALID;
    }
    if (machine->busy) {
        return NX_ERROR_BUSY;
    }
    const nx_fsm_config_t* config = machine->config;
    if (config == NULL || machine->current >= config->state_count) {
        return NX_ERROR_STATE;
    }

    /* Hold the synchronous reentry barrier throughout guards and actions. */
    machine->busy = true;
    for (size_t i = 0; i < config->transition_count; ++i) {
        const nx_fsm_transition_t* row = &config->transitions[i];
        if (row->source != machine->current || row->event_id != event->id ||
            (row->guard != NULL && !row->guard(machine->context, event))) {
            continue;
        }
        const nx_fsm_state_t* source = &config->states[machine->current];
        const nx_fsm_state_t* target = &config->states[row->target];
        if (row->kind == NX_FSM_EXTERNAL && source->exit != NULL) {
            source->exit(machine->context, event);
        }
        if (row->action != NULL) {
            row->action(machine->context, event);
        }
        machine->current = row->target;
        if (row->kind == NX_FSM_EXTERNAL && target->entry != NULL) {
            target->entry(machine->context, event);
        }
        machine->busy = false;
        return NX_SUCCESS;
    }
    machine->busy = false;
    return NX_ERROR_EMPTY;
}
