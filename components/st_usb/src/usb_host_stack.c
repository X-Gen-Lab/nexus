/**
 * \file            usb_host_stack.c
 * \brief           Selected official Host middleware packet adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "usb_host_stack.h"
#include "nexus/arch/arch.h"
#include <string.h>

/** \brief Overflow-safe admission without touching a packet or transport. */
static bool overlaps(const void* a, size_t na, const void* b, size_t nb) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    if (na == 0U || nb == 0U)
        return false;
    return x > UINTPTR_MAX - na || y > UINTPTR_MAX - nb ||
           (x < y + nb && y < x + na);
}

/** \brief Only the official core's named wire buffers may alias its Handle. */
static bool inside(const void* bytes, size_t length, const void* storage,
                   size_t capacity) {
    uintptr_t first = (uintptr_t)bytes, base = (uintptr_t)storage;
    return first >= base && first - base <= capacity &&
           length <= capacity - (first - base);
}

/** \brief Official callbacks run only through the explicitly polled task. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief No registry: every callback resolves its own private Handle. */
static nx_st_usb_host_stack_t* owner(USBH_HandleTypeDef* host) {
    if (host == NULL || host->pData == NULL)
        return NULL;
    nx_st_usb_host_stack_t* state = host->pData;
    return &state->middleware == host && state->config != NULL ? state : NULL;
}

/** \brief Retain the first bridge error even when ST ignores an LL result. */
static USBH_StatusTypeDef status(nx_st_usb_host_stack_t* state,
                                 nx_result_t result) {
    if (state != NULL && result != NX_SUCCESS && result != NX_ERROR_BUSY &&
        state->failure == NX_SUCCESS)
        state->failure = result;
    return result == NX_SUCCESS      ? USBH_OK
           : result == NX_ERROR_BUSY ? USBH_BUSY
                                     : USBH_FAIL;
}

/** \brief Exact caller-selected pipe, never a process-wide channel registry. */
static nx_st_usb_host_slot_t* selected(USBH_HandleTypeDef* host, uint8_t pipe) {
    nx_st_usb_host_stack_t* state = owner(host);
    return state != NULL && pipe < state->config->slot_count
               ? &state->config->slots[pipe]
               : NULL;
}

/** \brief One packet copies only bounded caller bytes into owned DMA staging.
 */
static nx_result_t submit_packet(nx_st_usb_host_stack_t* state, uint8_t index) {
    nx_st_usb_host_slot_t* slot = &state->config->slots[index];
    size_t length = slot->length - slot->offset;
    if (length > slot->pipe.max_packet)
        length = slot->pipe.max_packet;
    const bool in = (slot->pipe.endpoint_address & 0x80U) != 0U;
    if (!in && length != 0U)
        memcpy(slot->storage, slot->bytes + slot->offset, length);
    /* HAL's last word read is owned even for an odd OUT payload or ZLP. */
    size_t padded = (length + 3U) & ~(size_t)3U;
    if (!in && padded > length)
        memset(slot->storage + length, 0, padded - length);
    nx_time_us_t deadline;
    nx_result_t result = nx_deadline_finite_after(
        nx_time_now_us(), state->config->transfer_timeout_us, &deadline);
    if (result != NX_SUCCESS)
        return result;
    result = nx_usb_host_prepare(&slot->request, slot->storage, slot->capacity,
                                 length, slot->setup, slot->toggle, deadline);
    if (result == NX_SUCCESS)
        result =
            nx_usb_host_submit(state->config->transport, index, &slot->request);
    slot->pending = result == NX_SUCCESS;
    return result;
}

/** \brief Join every packet before class reset, buffer reuse or restart. */
static bool retire_slots(nx_st_usb_host_stack_t* state) {
    for (size_t i = 0U; i < state->config->slot_count; ++i) {
        const nx_st_usb_host_slot_t* slot = &state->config->slots[i];
        if (slot->pending &&
            nx_request_state(&slot->request.base) != NX_REQUEST_SETTLED)
            return false;
    }
    for (size_t i = 0U; i < state->config->slot_count; ++i) {
        nx_st_usb_host_slot_t* slot = &state->config->slots[i];
        slot->pending = slot->busy = slot->configured = false;
        slot->bytes = NULL;
        slot->urb = USBH_URB_ERROR;
    }
    return true;
}

nx_result_t
nx_st_usb_host_stack_initialize(nx_st_usb_host_stack_t* state,
                                const nx_st_usb_host_stack_config_t* config,
                                USBH_ClassTypeDef* selected_class,
                                void (*user)(USBH_HandleTypeDef*, uint8_t)) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || config->transport == NULL ||
        config->slots == NULL || config->slot_count < 2U ||
        config->slot_count > 12U || config->vbus == NULL ||
        config->power_context == NULL || config->transfer_timeout_us == 0U ||
        config->reset_timeout_us == 0U || config->restart == NULL ||
        config->restart_context == NULL ||
        (selected_class != NULL &&
         (selected_class->Init == NULL || selected_class->DeInit == NULL ||
          selected_class->Requests == NULL ||
          selected_class->BgndProcess == NULL ||
          overlaps(state, sizeof(*state), selected_class,
                   sizeof(*selected_class)))) ||
        overlaps(state, sizeof(*state), config, sizeof(*config)) ||
        overlaps(state, sizeof(*state), config->slots,
                 config->slot_count * sizeof(*config->slots)))
        return NX_ERROR_INVALID;
    if (state->config != NULL)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->slot_count; ++i) {
        nx_st_usb_host_slot_t* slot = &config->slots[i];
        if (slot->storage == NULL || slot->capacity < 1024U ||
            ((uintptr_t)slot->storage & 3U) != 0U ||
            overlaps(slot->storage, slot->capacity, state, sizeof(*state)) ||
            overlaps(slot->storage, slot->capacity, config, sizeof(*config)) ||
            overlaps(slot->storage, slot->capacity, config->slots,
                     config->slot_count * sizeof(*slot)))
            return NX_ERROR_INVALID;
        if (slot->busy)
            return NX_ERROR_BUSY;
        for (size_t j = 0U; j < i; ++j) {
            if (overlaps(slot->storage, slot->capacity,
                         config->slots[j].storage, config->slots[j].capacity))
                return NX_ERROR_INVALID;
        }
    }
    nx_usb_host_state_t transport;
    nx_result_t result = nx_usb_host_state(config->transport, &transport);
    if (result != NX_SUCCESS)
        return result;
    if (!transport.started || transport.closing)
        return NX_ERROR_STATE;
    memset(state, 0, sizeof(*state));
    state->config = config;
    state->middleware.pData = state;
    for (size_t i = 0U; i < config->slot_count; ++i) {
        uint8_t* bytes = config->slots[i].storage;
        size_t capacity = config->slots[i].capacity;
        memset(&config->slots[i], 0, sizeof(config->slots[i]));
        config->slots[i].storage = bytes;
        config->slots[i].capacity = capacity;
        nx_request_initialize(&config->slots[i].request.base);
    }
    (void)USBH_Init(&state->middleware, user, 0U);
    if (selected_class != NULL) {
        state->selected_class = *selected_class;
        if (USBH_RegisterClass(&state->middleware, &state->selected_class) !=
            USBH_OK)
            state->failure = NX_ERROR_INVALID;
    }
    (void)USBH_Start(&state->middleware);
    state->milliseconds = (uint32_t)(nx_time_now_us() / 1000U);
    USBH_LL_SetTimer(&state->middleware, state->milliseconds);
    return state->failure;
}
nx_result_t nx_st_usb_host_stack_step(nx_st_usb_host_stack_t* state) {
    if (state == NULL || state->config == NULL)
        return NX_ERROR_INVALID;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state->closing || state->stopped)
        return NX_ERROR_STATE;
    nx_result_t result = nx_usb_host_service(state->config->transport);
    if (result != NX_SUCCESS && result != NX_ERROR_BUSY)
        return result;
    for (uint8_t i = 0U; i < state->config->slot_count; ++i) {
        nx_st_usb_host_slot_t* slot = &state->config->slots[i];
        if (!slot->busy)
            continue;
        if (slot->pending) {
            nx_result_t terminal;
            size_t count;
            if (nx_request_result(&slot->request.base, &terminal, &count) !=
                NX_SUCCESS)
                continue;
            slot->pending = false;
            bool in = (slot->pipe.endpoint_address & 0x80U) != 0U;
            if (count > slot->request.length ||
                count > slot->length - slot->offset)
                terminal = NX_ERROR_OVERFLOW;
            if (terminal != NX_SUCCESS) {
                slot->urb = terminal == NX_ERROR_NACK ? USBH_URB_NOTREADY
                            : terminal == NX_ERROR_PERMISSION ? USBH_URB_STALL
                                                              : USBH_URB_ERROR;
                slot->busy = false;
                slot->bytes = NULL;
                continue;
            }
            if (in && count != 0U)
                memcpy(slot->bytes + slot->offset, slot->storage, count);
            slot->offset += count;
            slot->transferred = (uint32_t)slot->offset;
            slot->toggle = slot->setup ? true : !slot->toggle;
            if (slot->setup) {
                for (size_t peer = 0U; peer < state->config->slot_count;
                     ++peer) {
                    nx_st_usb_host_slot_t* other = &state->config->slots[peer];
                    if (other->configured &&
                        other->pipe.transfer == NX_USB_HOST_CONTROL &&
                        other->pipe.device_address ==
                            slot->pipe.device_address &&
                        (other->pipe.endpoint_address & 0x7FU) ==
                            (slot->pipe.endpoint_address & 0x7FU))
                        other->toggle = true;
                }
            }
            if (slot->offset == slot->length ||
                (in && count < slot->pipe.max_packet)) {
                slot->urb = USBH_URB_DONE;
                slot->busy = false;
                slot->bytes = NULL;
                continue;
            }
            if (!in && count != slot->request.length) {
                slot->urb = USBH_URB_ERROR;
                slot->busy = false;
                slot->bytes = NULL;
                continue;
            }
        }
        result = submit_packet(state, i);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            slot->urb = USBH_URB_ERROR;
            slot->busy = false;
            slot->bytes = NULL;
        }
    }
    nx_usb_host_state_t transport;
    result = nx_usb_host_state(state->config->transport, &transport);
    if (result != NX_SUCCESS)
        return result;
    if (transport.connected != state->connected) {
        if (!transport.connected) {
            USBH_StatusTypeDef drained = USBH_LL_Stop(&state->middleware);
            if (drained != USBH_OK)
                return state->failure == NX_SUCCESS ? NX_ERROR_BUSY
                                                    : state->failure;
        }
        state->connected = transport.connected;
        if (transport.connected)
            (void)USBH_LL_Connect(&state->middleware);
        else
            (void)USBH_LL_Disconnect(&state->middleware);
    }
    if (transport.port_enabled != state->enabled) {
        state->enabled = transport.port_enabled;
        if (transport.port_enabled)
            USBH_LL_PortEnabled(&state->middleware);
        else
            USBH_LL_PortDisabled(&state->middleware);
    }
    uint32_t now = (uint32_t)(nx_time_now_us() / 1000U);
    uint32_t elapsed = now - state->milliseconds;
    if (elapsed != 0U) {
        state->milliseconds = now;
        state->middleware.Timer += elapsed - 1U;
        USBH_LL_IncTimer(&state->middleware);
    }
    (void)USBH_Process(&state->middleware);
    return state->failure;
}
nx_result_t nx_st_usb_host_stack_close(nx_st_usb_host_stack_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state->config == NULL)
        return NX_SUCCESS;
    state->closing = true;
    nx_result_t result = state->stopped
                             ? NX_SUCCESS
                             : nx_usb_host_stop(state->config->transport);
    if (result != NX_SUCCESS)
        return result;
    state->stopped = true;
    if (!retire_slots(state))
        return NX_ERROR_BUSY;
    USBH_StatusTypeDef power = USBH_LL_DriverVBUS(&state->middleware, 0U);
    if (power != USBH_OK)
        return state->failure == NX_SUCCESS ? NX_ERROR_BUSY : state->failure;
    for (size_t i = 0U; i < state->config->slot_count; ++i) {
        state->config->slots[i].busy = false;
        state->config->slots[i].bytes = NULL;
    }
    /* Class storage is withdrawn only after the transport's final access. */
    if (state->middleware.pActiveClass != NULL) {
        USBH_ClassTypeDef* selected = state->middleware.pActiveClass;
        if (selected->DeInit == NULL)
            return NX_ERROR_STATE;
        USBH_StatusTypeDef class_status = selected->DeInit(&state->middleware);
        if (class_status != USBH_OK)
            return class_status == USBH_BUSY ? NX_ERROR_BUSY : NX_ERROR_IO;
        state->middleware.pActiveClass = NULL;
    }
    (void)USBH_DeInit(&state->middleware);
    state->middleware.pData = NULL;
    state->config = NULL;
    return NX_SUCCESS;
}

/** \brief Core lifecycle does not install or replace a hardware transport. */
USBH_StatusTypeDef USBH_LL_Init(USBH_HandleTypeDef* host) {
    return owner(host) != NULL ? USBH_OK : USBH_FAIL;
}
USBH_StatusTypeDef USBH_LL_DeInit(USBH_HandleTypeDef* host) {
    return owner(host) != NULL ? USBH_OK : USBH_FAIL;
}
USBH_StatusTypeDef USBH_LL_Start(USBH_HandleTypeDef* host) {
    nx_st_usb_host_stack_t* state = owner(host);
    if (state == NULL || state->closing)
        return USBH_FAIL;
    if (!state->stopped)
        return USBH_OK;
    if (!retire_slots(state))
        return USBH_BUSY;
    nx_time_us_t deadline;
    nx_result_t result = nx_deadline_finite_after(
        nx_time_now_us(), state->config->reset_timeout_us, &deadline);
    if (result == NX_SUCCESS)
        result =
            state->config->restart(state->config->restart_context, deadline);
    if (result == NX_SUCCESS)
        state->stopped = false;
    return status(state, result);
}
USBH_StatusTypeDef USBH_LL_Stop(USBH_HandleTypeDef* host) {
    nx_st_usb_host_stack_t* state = owner(host);
    if (state == NULL)
        return USBH_FAIL;
    nx_result_t result = state->stopped
                             ? NX_SUCCESS
                             : nx_usb_host_stop(state->config->transport);
    if (result == NX_SUCCESS)
        state->stopped = true;
    if (result == NX_SUCCESS && !retire_slots(state))
        result = NX_ERROR_BUSY;
    return status(state, result);
}
USBH_StatusTypeDef USBH_LL_DriverVBUS(USBH_HandleTypeDef* host,
                                      uint8_t enabled) {
    nx_st_usb_host_stack_t* state = owner(host);
    if (state == NULL)
        return USBH_FAIL;
    if (enabled == 0U && USBH_LL_Stop(host) != USBH_OK)
        return USBH_BUSY;
    return status(state, state->config->vbus(state->config->power_context,
                                             enabled != 0U));
}
USBH_SpeedTypeDef USBH_LL_GetSpeed(USBH_HandleTypeDef* host) {
    nx_st_usb_host_stack_t* state = owner(host);
    nx_usb_host_state_t transport;
    if (state == NULL ||
        nx_usb_host_state(state->config->transport, &transport) != NX_SUCCESS)
        return USBH_SPEED_FULL;
    return transport.speed == NX_USB_HOST_HIGH_SPEED  ? USBH_SPEED_HIGH
           : transport.speed == NX_USB_HOST_LOW_SPEED ? USBH_SPEED_LOW
                                                      : USBH_SPEED_FULL;
}
USBH_StatusTypeDef USBH_LL_ResetPort(USBH_HandleTypeDef* host) {
    nx_st_usb_host_stack_t* state = owner(host);
    if (state == NULL)
        return USBH_FAIL;
    nx_time_us_t deadline;
    nx_result_t result = nx_deadline_finite_after(
        nx_time_now_us(), state->config->reset_timeout_us, &deadline);
    if (result == NX_SUCCESS)
        result = nx_usb_host_reset(state->config->transport, deadline);
    return status(state, result);
}
uint32_t USBH_LL_GetLastXferSize(USBH_HandleTypeDef* host, uint8_t pipe) {
    nx_st_usb_host_slot_t* slot = selected(host, pipe);
    return slot != NULL ? slot->transferred : 0U;
}
USBH_StatusTypeDef USBH_LL_OpenPipe(USBH_HandleTypeDef* host, uint8_t pipe,
                                    uint8_t endpoint, uint8_t address,
                                    uint8_t speed, uint8_t type, uint16_t mps) {
    nx_st_usb_host_stack_t* state = owner(host);
    nx_st_usb_host_slot_t* slot = selected(host, pipe);
    if (slot == NULL || speed > USBH_SPEED_LOW || type > USB_EP_TYPE_INTR ||
        mps == 0U || mps > slot->capacity)
        return status(state, NX_ERROR_INVALID);
    if (slot->busy)
        return USBH_BUSY;
    nx_usb_host_pipe_t facts = {
        mps, address, endpoint,
        speed == USBH_SPEED_HIGH  ? NX_USB_HOST_HIGH_SPEED
        : speed == USBH_SPEED_LOW ? NX_USB_HOST_LOW_SPEED
                                  : NX_USB_HOST_FULL_SPEED,
        (nx_usb_host_transfer_t)type};
    nx_result_t result =
        nx_usb_host_open(state->config->transport, pipe, &facts);
    if (result == NX_SUCCESS) {
        slot->pipe = facts;
        slot->configured = true;
        slot->toggle = false;
        slot->urb = USBH_URB_IDLE;
    }
    return status(state, result);
}
USBH_StatusTypeDef USBH_LL_ClosePipe(USBH_HandleTypeDef* host, uint8_t pipe) {
    nx_st_usb_host_stack_t* state = owner(host);
    nx_st_usb_host_slot_t* slot = selected(host, pipe);
    if (slot == NULL)
        return USBH_FAIL;
    if (slot->busy)
        return USBH_BUSY;
    nx_result_t result =
        state->stopped ? NX_SUCCESS
                       : nx_usb_host_close(state->config->transport, pipe);
    if (result == NX_SUCCESS)
        slot->configured = false;
    return status(state, result);
}
USBH_StatusTypeDef USBH_LL_ActivatePipe(USBH_HandleTypeDef* host,
                                        uint8_t pipe) {
    nx_st_usb_host_slot_t* slot = selected(host, pipe);
    return slot != NULL && slot->configured ? USBH_OK : USBH_FAIL;
}
USBH_StatusTypeDef USBH_LL_SubmitURB(USBH_HandleTypeDef* host, uint8_t pipe,
                                     uint8_t direction, uint8_t type,
                                     uint8_t token, uint8_t* bytes,
                                     uint16_t length, uint8_t ping) {
    nx_st_usb_host_stack_t* state = owner(host);
    nx_st_usb_host_slot_t* slot = selected(host, pipe);
    if (slot == NULL || !slot->configured || direction > 1U || token > 1U ||
        type != slot->pipe.transfer ||
        direction != ((slot->pipe.endpoint_address >> 7U) & 1U) ||
        (length != 0U && bytes == NULL) ||
        (token == 0U &&
         (type != USB_EP_TYPE_CTRL || direction != 0U || length != 8U)))
        return status(state, NX_ERROR_INVALID);
    (void)ping; /* PING/NYET retry remains the explicit official Core policy. */
    if (slot->busy)
        return USBH_BUSY;
    for (size_t i = 0U; i < state->config->slot_count; ++i) {
        const nx_st_usb_host_slot_t* other = &state->config->slots[i];
        if (overlaps(bytes, length, other->storage, other->capacity) ||
            (other->busy &&
             overlaps(bytes, length, other->bytes, other->length)))
            return status(state, NX_ERROR_INVALID);
    }
    bool core_buffer =
        inside(bytes, length, state->middleware.Control.setup.d8,
               sizeof(state->middleware.Control.setup.d8)) ||
        inside(bytes, length, state->middleware.device.Data,
               sizeof(state->middleware.device.Data)) ||
        inside(bytes, length, state->middleware.device.CfgDesc_Raw,
               sizeof(state->middleware.device.CfgDesc_Raw));
    if ((!core_buffer && overlaps(bytes, length, state, sizeof(*state))) ||
        overlaps(bytes, length, state->config, sizeof(*state->config)) ||
        overlaps(bytes, length, state->config->slots,
                 state->config->slot_count * sizeof(*slot)))
        return status(state, NX_ERROR_INVALID);
    slot->bytes = bytes;
    slot->length = length;
    slot->offset = slot->transferred = 0U;
    slot->setup = token == 0U;
    if (slot->setup)
        slot->toggle = false;
    slot->urb = USBH_URB_IDLE;
    nx_result_t result = submit_packet(state, pipe);
    slot->busy = result == NX_SUCCESS;
    if (!slot->busy) {
        slot->bytes = NULL;
        slot->urb =
            result == NX_ERROR_BUSY ? USBH_URB_NOTREADY : USBH_URB_ERROR;
    }
    return status(state, result);
}
USBH_URBStateTypeDef USBH_LL_GetURBState(USBH_HandleTypeDef* host,
                                         uint8_t pipe) {
    nx_st_usb_host_slot_t* slot = selected(host, pipe);
    return slot != NULL ? slot->urb : USBH_URB_ERROR;
}
USBH_StatusTypeDef USBH_LL_SetToggle(USBH_HandleTypeDef* host, uint8_t pipe,
                                     uint8_t toggle) {
    nx_st_usb_host_slot_t* slot = selected(host, pipe);
    if (slot == NULL || toggle > 1U)
        return USBH_FAIL;
    if (slot->busy)
        return USBH_BUSY;
    slot->toggle = toggle != 0U;
    return USBH_OK;
}
uint8_t USBH_LL_GetToggle(USBH_HandleTypeDef* host, uint8_t pipe) {
    nx_st_usb_host_slot_t* slot = selected(host, pipe);
    return slot != NULL && slot->toggle ? 1U : 0U;
}
#ifndef NX_ST_USB_DELAY_PARK
#define NX_ST_USB_DELAY_PARK() nx_arch_dsb()
#endif
/** \brief A void Core delay cannot return success after losing its clock. */
static _Noreturn void delay_failed(void) {
    for (;;) {
        NX_ST_USB_DELAY_PARK();
    }
}

/** \brief Official Core delay shares time, with a finite progress work budget.
 * \note One million unchanged observations is a work bound, not a new clock
 *       or physical timeout calibration. Invalid sums, backward time or
 *       persistent freezing park the serialized caller without releasing any
 *       packet, transport, class or VBUS responsibility.
 */
void USBH_Delay(uint32_t milliseconds) {
    if (milliseconds == 0U)
        return;
    nx_time_us_t previous = nx_time_now_us();
    nx_time_us_t deadline;
    if (nx_deadline_finite_after(previous, (uint64_t)milliseconds * 1000U,
                                 &deadline) != NX_SUCCESS)
        delay_failed();
    const uint32_t budget = UINT32_C(1000000);
    uint32_t remaining = budget;
    for (;;) {
        nx_time_us_t now = nx_time_now_us();
        if (now < previous)
            delay_failed();
        if (nx_deadline_expired(deadline, now))
            return;
        if (now == previous) {
            if (remaining == 0U)
                delay_failed();
            --remaining;
        } else {
            previous = now;
            remaining = budget;
        }
        nx_arch_dsb();
    }
}
