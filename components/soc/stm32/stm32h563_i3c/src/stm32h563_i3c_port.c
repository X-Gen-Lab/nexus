/**
 * \file            stm32h563_i3c_port.c
 * \brief           Bounded H563 I3C polling hardware transport
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h563_i3c_port.h"
#include "stm32h5xx_ll_i3c.h"
#include <string.h>

/* CubeH5 v1.7.0: CMSIS 884b8dc and HAL/LL 56e28db. HAL IT retains
 * descriptors/spans, while HAL polling blocks on HAL_GetTick. Neither can
 * implement a no-retain bounded pump. Use unchanged official LL operations
 * with one transport owner. Raw reads only inspect aggregate admission and
 * proof bits for which the SDK has no complete atomic profile helper.
 */
_Static_assert(I3C1_EV_IRQn == 123 && I3C1_ER_IRQn == 124,
               "Exact H563 I3C1 IRQ identity changed");
_Static_assert(I3C_CR_DCNT == UINT16_MAX,
               "Finite SDR count must remain sixteen bits");
_Static_assert(sizeof(((I3C_TypeDef*)0)->DEVRX) == 4U * sizeof(uint32_t),
               "H563 has four IBI characteristic slots");

/** \brief Check the prepared exclusive polling profile without writes. */
static nx_result_t prepared(const nx_h563_i3c_port_t* port) {
    const I3C_TypeDef* r = port->config.registers;
    const uint32_t forbidden =
        I3C_CFGR_NOARBH | I3C_CFGR_RXDMAEN | I3C_CFGR_TXDMAEN |
        I3C_CFGR_CDMAEN | I3C_CFGR_SDMAEN | I3C_CFGR_TMODE | I3C_CFGR_SMODE |
        I3C_CFGR_RXTHRES | I3C_CFGR_TXTHRES | I3C_CFGR_RSTPTRN |
        I3C_CFGR_EXITPTRN;
    uint32_t cfgr = READ_REG(r->CFGR);
    if ((cfgr & (I3C_CFGR_EN | I3C_CFGR_CRINIT)) !=
            (I3C_CFGR_EN | I3C_CFGR_CRINIT) ||
        (cfgr & forbidden) != 0 || READ_REG(r->IER) != 0 ||
        !LL_I3C_IsEnabledOwnDynAddress(r)) {
        return NX_ERROR_UNSUPPORTED;
    }
    return NX_SUCCESS;
}

/** \brief FIFO flush requests are not evidence of completed flushes. */
static bool fifos_empty(const nx_h563_i3c_port_t* port) {
    const I3C_TypeDef* r = port->config.registers;
    uint32_t flags = READ_REG(r->EVR);
    const uint32_t flushing =
        I3C_CFGR_RXFLUSH | I3C_CFGR_TXFLUSH | I3C_CFGR_CFLUSH | I3C_CFGR_SFLUSH;
    return (flags & (I3C_EVR_CFEF | I3C_EVR_TXFEF)) ==
               (I3C_EVR_CFEF | I3C_EVR_TXFEF) &&
           (flags & (I3C_EVR_RXFNEF | I3C_EVR_SFNEF)) == 0 &&
           (READ_REG(r->CFGR) & flushing) == 0;
}

/** \brief Clear characteristic slots only with an independently idle bus. */
static void clear_targets(nx_h563_i3c_port_t* port) {
    I3C_TypeDef* r = port->config.registers;
    for (uint32_t i = 1; i <= 4; ++i) {
        LL_I3C_DisableTargetIBIAck(r, i);
        LL_I3C_DisableTargetCRAck(r, i);
        LL_I3C_DisableIBIAddData(r, i);
        LL_I3C_SetTargetDynamicAddress(r, i, 0);
    }
    port->ibi_slots = 0;
}

/** \brief Close new accepted target events before stable withdrawal proof. */
static void pause_events(nx_h563_i3c_port_t* port) {
    I3C_TypeDef* r = port->config.registers;
    LL_I3C_DisableHJAck(r);
    for (uint32_t i = 1; i <= port->ibi_slots; ++i) {
        LL_I3C_DisableTargetIBIAck(r, i);
    }
    port->events_paused = (READ_REG(r->CFGR) & I3C_CFGR_HJACK) == 0;
    for (uint32_t i = 0; i < port->ibi_slots; ++i) {
        if ((READ_REG(r->DEVRX[i]) & (I3C_DEVRX_IBIACK | I3C_DEVRX_CRACK)) !=
            0) {
            port->events_paused = false;
        }
    }
    port->quiescence_proven = false;
}

/** \brief Resume copied events only on an explicit next operation or pump. */
static void resume_events(nx_h563_i3c_port_t* port) {
    if (port->events_paused) {
        for (uint32_t i = 1; i <= port->ibi_slots; ++i) {
            LL_I3C_EnableTargetIBIAck(port->config.registers, i);
        }
        LL_I3C_EnableHJAck(port->config.registers);
        port->events_paused = false;
    }
    port->quiescence_proven = false;
}

/** \brief Rejection retains no resources and performs no peripheral writes. */
static nx_result_t claim(void* context, const void* owner) {
    nx_h563_i3c_port_t* port = context;
    if (owner == NULL) {
        return NX_ERROR_INVALID;
    }
    if (port->owner != NULL) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = prepared(port);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!fifos_empty(port) ||
        (READ_REG(port->config.registers->EVR) &
         (I3C_EVR_FCF | I3C_EVR_ERRF | I3C_EVR_IBIF | I3C_EVR_HJF)) != 0) {
        return NX_ERROR_BUSY;
    }
    result =
        port->config.resources->acquire(port->config.resource_context, owner);
    if (result != NX_SUCCESS) {
        return result;
    }
    port->owner = owner;
    port->error = NX_SUCCESS;
    port->active = false;
    port->withdrawing = false;
    clear_targets(port);
    port->events_paused = true;
    resume_events(port);
    return NX_SUCCESS;
}

/** \brief Admission is read-only; payload shape policy stays in the core. */
static nx_result_t ready(void* context) {
    const nx_h563_i3c_port_t* port = context;
    if (port->owner == NULL || port->error != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    if (port->active || port->withdrawing) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = prepared(port);
    return result != NX_SUCCESS
               ? result
               : (fifos_empty(port) ? NX_SUCCESS : NX_ERROR_BUSY);
}

/** \brief Copy the command and start one finite frame without retaining spans.
 */
static void begin(void* context, const nx_i3c_command_t* command) {
    nx_h563_i3c_port_t* port = context;
    I3C_TypeDef* r = port->config.registers;
    port->command = *command;
    port->moved = 0;
    port->identity_bytes = 0;
    port->identity_waiting = false;
    port->assignment_pending = false;
    port->frame_complete = false;
    port->direct_pending = false;
    port->withdrawing = false;
    port->active = true;
    resume_events(port);
    if (command->kind == NX_I3C_DISCOVER) {
        /* Never hide RSTDAA or treat it as whole-target reset proof. */
        LL_I3C_ControllerHandleCCC(r, 0x07, 0, LL_I3C_GENERATE_STOP);
    } else if (command->kind == NX_I3C_CCC) {
        port->direct_pending = !command->broadcast;
        LL_I3C_ControllerHandleCCC(
            r, command->ccc, command->broadcast ? command->bytes : 0,
            command->broadcast ? LL_I3C_GENERATE_STOP
                               : LL_I3C_GENERATE_RESTART);
    } else {
        LL_I3C_ControllerHandleMessage(
            r, command->address, command->bytes,
            command->read ? LL_I3C_DIRECTION_READ : LL_I3C_DIRECTION_WRITE,
            LL_I3C_CONTROLLER_MTYPE_PRIVATE, LL_I3C_GENERATE_STOP);
    }
}

/** \brief Copy the single hardware IBI and hot-join latches before clearing. */
static void events(nx_h563_i3c_port_t* port,
                   nx_i3c_observation_t* observation) {
    I3C_TypeDef* r = port->config.registers;
    if (LL_I3C_IsActiveFlag_IBI(r)) {
        uint32_t bytes = LL_I3C_GetNbIBIAddData(r);
        uint32_t payload = LL_I3C_GetIBIPayload(r);
        observation->ibi_address = (uint8_t)LL_I3C_GetIBITargetAddr(r);
        if (bytes > 4) {
            observation->error = NX_ERROR_OVERFLOW;
        } else {
            observation->ibi = true;
            observation->ibi_bytes = (uint8_t)bytes;
            for (uint32_t i = 0; i < bytes; ++i) {
                observation->ibi_payload[i] = (uint8_t)(payload >> (i * 8U));
            }
        }
        LL_I3C_ClearFlag_IBI(r);
    }
    if (LL_I3C_IsActiveFlag_HJ(r)) {
        observation->hot_join = true;
        LL_I3C_ClearFlag_HJ(r);
    }
}

/** \brief Configure an IBI slot only after hardware acknowledged assignment. */
static void acknowledge(nx_h563_i3c_port_t* port,
                        nx_i3c_observation_t* observation) {
    if ((port->pending_bcr & 2U) != 0) {
        uint32_t slot = (uint32_t)port->ibi_slots + 1U;
        LL_I3C_SetTargetDynamicAddress(port->config.registers, slot,
                                       port->pending_address);
        if ((port->pending_bcr & 4U) != 0) {
            LL_I3C_EnableIBIAddData(port->config.registers, slot);
        }
        LL_I3C_EnableTargetIBIAck(port->config.registers, slot);
        ++port->ibi_slots;
    }
    port->assignment_pending = false;
    observation->assigned = true;
}

/** \brief Consume at most one fixed eight-byte DAA identity, never payload. */
static void discovery(nx_h563_i3c_port_t* port, uint32_t flags,
                      nx_i3c_observation_t* observation) {
    I3C_TypeDef* r = port->config.registers;
    bool next = (flags & (I3C_EVR_TXFNFF | I3C_EVR_RXFNEF)) ==
                (I3C_EVR_TXFNFF | I3C_EVR_RXFNEF);
    if (port->assignment_pending && (port->frame_complete || next)) {
        acknowledge(port, observation);
    }
    if (!port->identity_waiting && !port->assignment_pending &&
        (next || port->identity_bytes != 0)) {
        while (port->identity_bytes < 8 && LL_I3C_IsActiveFlag_RXFNE(r)) {
            port->identity[port->identity_bytes++] = LL_I3C_ReceiveData8(r);
        }
        if (port->identity_bytes == 8) {
            port->pending_bcr = port->identity[6];
            if ((port->pending_bcr & 2U) != 0 && port->ibi_slots == 4) {
                observation->error = NX_ERROR_OVERFLOW;
                return;
            }
            for (size_t i = 0; i < 6; ++i) {
                observation->pid = (observation->pid << 8U) | port->identity[i];
            }
            observation->bcr = port->pending_bcr;
            observation->dcr = port->identity[7];
            observation->identity = true;
            port->identity_bytes = 0;
            port->identity_waiting = true;
        }
    }
    if (port->frame_complete) {
        if (port->identity_waiting || port->identity_bytes != 0 ||
            port->assignment_pending) {
            observation->error = NX_ERROR_IO;
        } else {
            observation->complete = true;
        }
    }
}

/** \brief Service only this call's byte span; never store a payload pointer. */
static void payload(nx_h563_i3c_port_t* port, const uint8_t* tx,
                    size_t tx_capacity, uint8_t* rx, size_t rx_capacity,
                    nx_i3c_observation_t* observation) {
    I3C_TypeDef* r = port->config.registers;
    size_t remaining = port->command.bytes - port->moved;
    size_t capacity = port->command.read ? rx_capacity : tx_capacity;
    if (capacity > 8) {
        capacity = 8;
    }
    if (capacity > remaining) {
        capacity = remaining;
    }
    size_t done = 0;
    while (done < capacity) {
        if (port->command.read) {
            if (!LL_I3C_IsActiveFlag_RXFNE(r)) {
                break;
            }
            rx[done] = LL_I3C_ReceiveData8(r);
        } else {
            if (!LL_I3C_IsActiveFlag_TXFNF(r)) {
                break;
            }
            LL_I3C_TransmitData8(r, tx[done]);
        }
        ++done;
    }
    port->moved += (uint32_t)done;
    if (port->command.read) {
        observation->received = done;
    } else {
        observation->transmitted = done;
    }
    if (port->frame_complete) {
        if (LL_I3C_GetXferDataCount(r) != port->command.bytes ||
            (!LL_I3C_IsActiveFlag_RXFNE(r) &&
             port->moved != port->command.bytes) ||
            (port->moved == port->command.bytes &&
             LL_I3C_IsActiveFlag_RXFNE(r))) {
            observation->error = NX_ERROR_IO;
        } else if (port->moved == port->command.bytes) {
            observation->complete = true;
        }
    }
}

/** \brief ERR wins over FC from the same coherent event observation. */
static void error_status(nx_h563_i3c_port_t* port, uint32_t flags,
                         nx_i3c_observation_t* observation) {
    if ((flags & I3C_EVR_ERRF) != 0) {
        uint32_t error = READ_REG(port->config.registers->SER);
        observation->error = (error & (I3C_SER_ANACK | I3C_SER_DNACK)) != 0
                                 ? NX_ERROR_NACK
                                 : NX_ERROR_IO;
        LL_I3C_ClearFlag_ERR(port->config.registers);
    }
    if (observation->error != NX_SUCCESS) {
        port->error = observation->error;
        observation->complete = false;
    }
}

/** \brief Bound data, DAA metadata and event work without waits or callbacks.
 */
static void pump(void* context, const uint8_t* tx, size_t tx_capacity,
                 uint8_t* rx, size_t rx_capacity,
                 nx_i3c_observation_t* observation) {
    nx_h563_i3c_port_t* port = context;
    I3C_TypeDef* r = port->config.registers;
    memset(observation, 0, sizeof(*observation));
    if (port->owner != NULL && !port->withdrawing &&
        port->error == NX_SUCCESS) {
        resume_events(port);
    }
    events(port, observation);
    uint32_t flags = READ_REG(r->EVR);
    error_status(port, flags, observation);
    if (port->error != NX_SUCCESS || port->owner == NULL || port->withdrawing) {
        observation->error = port->error;
        return;
    }
    if (!port->active) {
        return;
    }
    nx_result_t profile = prepared(port);
    if (profile != NX_SUCCESS || (tx_capacity != 0 && tx == NULL) ||
        (rx_capacity != 0 && rx == NULL)) {
        port->error = profile != NX_SUCCESS ? profile : NX_ERROR_INVALID;
        observation->error = port->error;
        return;
    }
    if ((flags & I3C_EVR_FCF) != 0) {
        port->frame_complete = true;
        LL_I3C_ClearFlag_FC(r);
    }
    if (port->direct_pending && LL_I3C_IsActiveFlag_CFNF(r)) {
        LL_I3C_ControllerHandleMessage(
            r, port->command.address, port->command.bytes,
            port->command.read ? LL_I3C_DIRECTION_READ : LL_I3C_DIRECTION_WRITE,
            LL_I3C_CONTROLLER_MTYPE_DIRECT, LL_I3C_GENERATE_STOP);
        port->direct_pending = false;
    }
    if (port->command.kind == NX_I3C_DISCOVER) {
        discovery(port, flags, observation);
    } else {
        payload(port, tx, tx_capacity, rx, rx_capacity, observation);
    }
    /* The last FIFO read can latch an error after the entry snapshot. */
    error_status(port, READ_REG(r->EVR), observation);
}

/** \brief Enqueue an assignment; enqueue is never its wire acknowledgement. */
static void assign(void* context, uint8_t address) {
    nx_h563_i3c_port_t* port = context;
    if (address == LL_I3C_GetOwnDynamicAddress(port->config.registers)) {
        port->error = NX_ERROR_INVALID;
        return;
    }
    if (!port->identity_waiting || port->assignment_pending ||
        !LL_I3C_IsActiveFlag_TXFNF(port->config.registers)) {
        port->error = NX_ERROR_IO;
        return;
    }
    port->pending_address = address;
    LL_I3C_TransmitData8(port->config.registers, address);
    port->identity_waiting = false;
    port->assignment_pending = true;
}

/** \brief Request the vendor's bounded FIFO-flush abort, without settling. */
static void withdraw(void* context) {
    nx_h563_i3c_port_t* port = context;
    I3C_TypeDef* r = port->config.registers;
    if (port->owner == NULL || port->withdrawing) {
        return;
    }
    if (LL_I3C_IsActiveFlag_FC(r)) {
        port->frame_complete = true;
        LL_I3C_ClearFlag_FC(r);
    }
    port->withdrawing = true;
    pause_events(port);
    /* HAL_I3C_Abort_IT uses these flushes to obtain automatic STOP. Actual
     * frame/FIFO/wire/IRQ completion still requires subsequent proof. */
    LL_I3C_RequestTxFIFOFlush(r);
    LL_I3C_RequestRxFIFOFlush(r);
    LL_I3C_RequestControlFIFOFlush(r);
    LL_I3C_RequestStatusFIFOFlush(r);
}

/** \brief Observe complete withdrawal; failed proof keeps all ownership. */
static nx_result_t quiescent(void* context) {
    nx_h563_i3c_port_t* port = context;
    if (port->owner == NULL) {
        return NX_ERROR_STATE;
    }
    pause_events(port);
    if (!port->events_paused) {
        return NX_ERROR_BUSY;
    }
    if (LL_I3C_IsActiveFlag_FC(port->config.registers)) {
        port->frame_complete = true;
        LL_I3C_ClearFlag_FC(port->config.registers);
    }
    if ((port->active && !port->frame_complete) || !fifos_empty(port)) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = prepared(port);
    if (result == NX_SUCCESS) {
        result =
            port->config.resources->quiescent(port->config.resource_context);
    }
    if (result == NX_SUCCESS) {
        port->active = false;
        port->withdrawing = false;
        port->quiescence_proven = true;
    }
    return result;
}

/** \brief External proof can recover a stopped wire that never raised FCF. */
static nx_result_t reset(void* context) {
    nx_h563_i3c_port_t* port = context;
    if (port->owner == NULL) {
        return NX_ERROR_STATE;
    }
    withdraw(port);
    nx_result_t result =
        port->config.resources->reset_targets(port->config.resource_context);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = prepared(port);
    pause_events(port);
    if (result != NX_SUCCESS || !port->events_paused || !fifos_empty(port)) {
        return result != NX_SUCCESS ? result : NX_ERROR_BUSY;
    }
    result = port->config.resources->quiescent(port->config.resource_context);
    if (result != NX_SUCCESS) {
        return result;
    }
    clear_targets(port);
    LL_I3C_ClearFlag_FC(port->config.registers);
    LL_I3C_ClearFlag_ERR(port->config.registers);
    LL_I3C_ClearFlag_IBI(port->config.registers);
    LL_I3C_ClearFlag_HJ(port->config.registers);
    port->active = false;
    port->withdrawing = false;
    port->frame_complete = false;
    port->identity_bytes = 0;
    port->identity_waiting = false;
    port->assignment_pending = false;
    port->direct_pending = false;
    port->error = NX_SUCCESS;
    port->quiescence_proven = true;
    return NX_SUCCESS;
}

/** \brief Release unique resources only after matching final proof. */
static void release(void* context, const void* owner) {
    nx_h563_i3c_port_t* port = context;
    if (port->owner != owner || owner == NULL || !port->events_paused ||
        !port->quiescence_proven || port->active || port->withdrawing) {
        return;
    }
    /* Event admission was closed before final proof. Never repeat a
     * fallible proof after core has committed to its void release. */
    clear_targets(port);
    port->config.resources->release(port->config.resource_context, owner);
    port->owner = NULL;
}

const nx_i3c_port_ops_t nx_h563_i3c_port_ops = {
    claim, release, ready, begin, pump, assign, withdraw, quiescent, reset};

nx_result_t nx_h563_i3c_port_bind(nx_h563_i3c_port_t* port,
                                  const nx_h563_i3c_port_config_t* config) {
    if (port == NULL || config == NULL ||
        (uintptr_t)port % _Alignof(nx_h563_i3c_port_t) != 0) {
        return NX_ERROR_INVALID;
    }
    if (port->config.registers != NULL) {
        return NX_ERROR_STATE;
    }
    if (config->registers == NULL || config->resources == NULL ||
        config->resources->acquire == NULL ||
        config->resources->release == NULL ||
        config->resources->quiescent == NULL ||
        config->resources->reset_targets == NULL) {
        return NX_ERROR_INVALID;
    }
#ifndef NEXUS_CMSIS_HOST_MODEL
    if (config->registers != I3C1) {
        return NX_ERROR_UNSUPPORTED;
    }
#endif
    port->config = *config;
    return NX_SUCCESS;
}
