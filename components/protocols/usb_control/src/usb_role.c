/**
 * \file            usb_role.c
 * \brief           Bounded role handover with retained cleanup ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/usb_role.h"
#include "nexus/arch/arch.h"

enum {
    IDLE,
    STOP,
    SOURCE_OFF,
    DISCHARGE,
    WAIT_QUIET,
    SOURCE_ON,
    WAIT_POWER,
    START
};

/** \brief Role transactions use the same task context as the transport. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Numeric extents exclude overwrite of immutable configuration. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t av = (uintptr_t)a, bv = (uintptr_t)b;
    return an > UINTPTR_MAX - av || bn > UINTPTR_MAX - bv ||
           (an != 0U && bn != 0U && av < bv + bn && bv < av + an);
}

/** \brief Binding has no hardware effects and cannot grant electrical facts. */
nx_result_t nx_usb_role_initialize(nx_usb_role_t* role,
                                   const nx_usb_role_config_t* config) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (role == NULL || config == NULL || config->ops == NULL ||
        config->context == NULL || config->ops->start == NULL ||
        config->ops->stop == NULL || config->ops->source == NULL ||
        config->ops->present == NULL ||
        (!config->device_allowed && !config->host_allowed) ||
        overlap(role, sizeof(*role), config, sizeof(*config)) ||
        overlap(role, sizeof(*role), config->ops, sizeof(*config->ops))) {
        return NX_ERROR_INVALID;
    }
    *role = (nx_usb_role_t){.config = config, .result = NX_SUCCESS};
    return NX_SUCCESS;
}

/** \brief Admission records a target but performs no transport or VBUS work. */
nx_result_t nx_usb_role_request(nx_usb_role_t* role, nx_usb_role_kind_t target,
                                nx_time_us_t deadline) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (role == NULL || role->config == NULL ||
        (target != NX_USB_ROLE_OFF && target != NX_USB_ROLE_DEVICE &&
         target != NX_USB_ROLE_HOST) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (role->phase != IDLE) {
        return NX_ERROR_BUSY;
    }
    if ((target == NX_USB_ROLE_DEVICE && !role->config->device_allowed) ||
        (target == NX_USB_ROLE_HOST && !role->config->host_allowed)) {
        return NX_ERROR_PERMISSION;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (target == role->active) {
        role->result = NX_SUCCESS;
        return NX_SUCCESS;
    }
    role->target = target;
    role->deadline = deadline;
    role->result = NX_SUCCESS;
    role->phase = role->transport_owned ? STOP : SOURCE_OFF;
    return NX_SUCCESS;
}

/** \brief Publish an idle transaction only after retained effects are gone. */
static nx_result_t complete(nx_usb_role_t* role) {
    role->phase = IDLE;
    return role->result;
}

/** \brief Explicit cancellation retains its original transport and supply. */
nx_result_t nx_usb_role_cancel(nx_usb_role_t* role, nx_time_us_t deadline) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (role == NULL || role->config == NULL || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (role->phase == IDLE && role->active == NX_USB_ROLE_OFF) {
        return NX_SUCCESS;
    }
    role->deadline = deadline;
    role->target = NX_USB_ROLE_OFF;
    if (role->result == NX_SUCCESS) {
        role->result = NX_ERROR_CANCELLED;
    }
    if (role->transport_owned) {
        role->phase = STOP;
    } else if (role->phase != DISCHARGE) {
        role->phase = SOURCE_OFF;
    }
    return NX_SUCCESS;
}

/** \brief Each call issues at most one external command or observation. */
nx_result_t nx_usb_role_service(nx_usb_role_t* role) {
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (role == NULL || role->config == NULL) {
        return NX_ERROR_INVALID;
    }
    if (role->phase == IDLE) {
        return role->result;
    }
    const nx_usb_role_config_t* config = role->config;
    if (nx_deadline_expired(role->deadline, nx_time_now_us())) {
        if (role->result == NX_SUCCESS) {
            role->result = NX_ERROR_TIMEOUT;
        }
        role->target = NX_USB_ROLE_OFF;
        if (role->transport_owned) {
            role->phase = STOP;
        } else if (role->phase != DISCHARGE) {
            role->phase = SOURCE_OFF;
        }
    }
    nx_result_t status;
    bool present = false;
    switch (role->phase) {
        case STOP:
            status = config->ops->stop(config->context, role->deadline);
            if (status != NX_SUCCESS) {
                return status;
            }
            role->transport_owned = false;
            role->active = NX_USB_ROLE_OFF;
            role->phase = SOURCE_OFF;
            return NX_ERROR_BUSY;
        case SOURCE_OFF:
            status = config->ops->source(config->context, false);
            if (status != NX_SUCCESS) {
                return status;
            }
            role->source_owned = false;
            if (role->discharge_required) {
                role->phase = DISCHARGE;
            } else if (role->target == NX_USB_ROLE_OFF) {
                return complete(role);
            } else {
                role->phase =
                    role->target == NX_USB_ROLE_HOST ? WAIT_QUIET : WAIT_POWER;
            }
            return NX_ERROR_BUSY;
        case DISCHARGE:
        case WAIT_QUIET:
        case WAIT_POWER:
            status = config->ops->present(config->context, &present);
            if (status != NX_SUCCESS) {
                return status;
            }
            if (role->phase == WAIT_POWER) {
                if (present) {
                    role->phase = START;
                }
                return NX_ERROR_BUSY;
            }
            if (present) {
                return NX_ERROR_BUSY;
            }
            if (role->phase == DISCHARGE) {
                role->discharge_required = false;
                if (role->target == NX_USB_ROLE_OFF) {
                    return complete(role);
                }
                role->phase =
                    role->target == NX_USB_ROLE_HOST ? WAIT_QUIET : WAIT_POWER;
            } else {
                role->phase = SOURCE_ON;
            }
            return NX_ERROR_BUSY;
        case SOURCE_ON:
            /* A failed switch write may still have enabled the supply. */
            role->source_owned = true;
            role->discharge_required = true;
            status = config->ops->source(config->context, true);
            if (status != NX_SUCCESS) {
                role->result = status;
                role->target = NX_USB_ROLE_OFF;
                role->phase = SOURCE_OFF;
                return NX_ERROR_BUSY;
            }
            role->phase = WAIT_POWER;
            return NX_ERROR_BUSY;
        case START:
            /* Record cleanup before a start can create partial effects. */
            role->transport_owned = true;
            status = config->ops->start(config->context, role->target,
                                        role->deadline);
            if (status != NX_SUCCESS) {
                role->result = status;
                role->target = NX_USB_ROLE_OFF;
                role->phase = STOP;
                return NX_ERROR_BUSY;
            }
            role->active = role->target;
            return complete(role);
        default:
            return NX_ERROR_STATE;
    }
}
