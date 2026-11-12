/**
 * \file            usb_device_stack.c
 * \brief           Explicit caller-owned official Device Core DCD bridge
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "usb_device_stack.h"
#include "nexus/arch/arch.h"
#include <string.h>

/** \brief Reject wrapping or overlapping caller-owned objects before effects.
 */
static bool overlaps(const void* a, size_t na, const void* b, size_t nb) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    if (na == 0U || nb == 0U)
        return false;
    return x > UINTPTR_MAX - na || y > UINTPTR_MAX - nb ||
           (x < y + nb && y < x + na);
}
/** \brief Typed metadata has its natural C alignment and a finite extent. */
static bool storage(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t p = (uintptr_t)pointer;
    return pointer != NULL && p % alignment == 0U && bytes <= UINTPTR_MAX - p;
}
/** \brief Core and class callbacks have exactly one explicit Thread executor.
 */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Every official LL callback resolves its own private Handle. */
static nx_st_usb_device_stack_t* owner(USBD_HandleTypeDef* device) {
    if (device == NULL || device->pData == NULL)
        return NULL;
    nx_st_usb_device_stack_t* state = device->pData;
    return &state->middleware == device && state->config != NULL ? state : NULL;
}
/** \brief Retain errors even where the pinned Core ignores the LL result. */
static USBD_StatusTypeDef status(nx_st_usb_device_stack_t* state,
                                 nx_result_t result) {
    if (state != NULL && result != NX_SUCCESS && state->failure == NX_SUCCESS)
        state->failure = result;
    return result == NX_SUCCESS      ? USBD_OK
           : result == NX_ERROR_BUSY ? USBD_BUSY
                                     : USBD_FAIL;
}
/** \brief Endpoint identity comes only from the selected immutable profile. */
static nx_st_usb_device_slot_t* selected(nx_st_usb_device_stack_t* state,
                                         uint8_t number, bool in) {
    if (state == NULL || number >= 16U)
        return NULL;
    for (size_t i = 0U; i < state->config->slot_count; ++i) {
        nx_st_usb_device_slot_t* slot = &state->config->slots[i];
        if (slot->endpoint->number == number && slot->endpoint->in == in)
            return slot;
    }
    return NULL;
}
/** \brief All new LL operations reject a closed, failed or wrong executor. */
static nx_result_t admission(nx_st_usb_device_stack_t* state) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (state->failure != NX_SUCCESS)
        return state->failure;
    return state->closing || state->stopped ? NX_ERROR_STATE : NX_SUCCESS;
}
/** \brief Release private intent only after its accepted packet is joined. */
static void clear_intent(nx_st_usb_device_slot_t* slot) {
    slot->busy = slot->pending = slot->cancel_sent = false;
    slot->bytes = NULL;
    slot->length = slot->offset = slot->packet = 0U;
}
/** \brief Numeric containment for the Core's named owned wire words. */
static bool inside(const void* p, size_t n, const void* base, size_t bytes) {
    uintptr_t x = (uintptr_t)p, y = (uintptr_t)base;
    return x >= y && x - y <= bytes && n <= bytes - (x - y);
}
/** \brief Class buffers cannot alias live staging, slots or Core metadata. */
static bool payload_valid(const nx_st_usb_device_stack_t* state, void* bytes,
                          size_t length, bool in) {
    if (length == 0U)
        return true;
    if (!storage(bytes, length, 1U))
        return false;
    const USBD_HandleTypeDef* core = &state->middleware;
    bool core_word = in && (inside(bytes, length, &core->dev_config_status,
                                   sizeof(core->dev_config_status)) ||
                            inside(bytes, length, &core->dev_config,
                                   sizeof(core->dev_config)) ||
                            inside(bytes, length, &core->dev_default_config,
                                   sizeof(core->dev_default_config)));
    if ((!core_word && overlaps(bytes, length, state, sizeof(*state))) ||
        overlaps(bytes, length, state->config, sizeof(*state->config)) ||
        overlaps(bytes, length, state->config->slots,
                 state->config->slot_count * sizeof(*state->config->slots)))
        return false;
    for (size_t i = 0U; i < state->config->slot_count; ++i) {
        const nx_st_usb_device_slot_t* slot = &state->config->slots[i];
        if (overlaps(bytes, length, slot->storage, slot->capacity) ||
            (slot->busy && overlaps(bytes, length, slot->bytes, slot->length)))
            return false;
    }
    return true;
}
/** \brief Caller staging and mutable state cannot overwrite selected facts. */
static bool facts_overlap(const nx_st_usb_device_stack_config_t* config,
                          const void* bytes, size_t length) {
    const nx_usb_device_profile_t* profile = config->profile;
    if (overlaps(bytes, length, config, sizeof(*config)) ||
        overlaps(bytes, length, profile, sizeof(*profile)) ||
        overlaps(bytes, length, profile->endpoints,
                 profile->endpoint_count * sizeof(*profile->endpoints)) ||
        overlaps(bytes, length, profile->port, sizeof(*profile->port)) ||
        overlaps(bytes, length, profile->port->ops,
                 sizeof(*profile->port->ops)) ||
        overlaps(bytes, length, profile->setup_decision.ops,
                 sizeof(*profile->setup_decision.ops)) ||
        (config->activation != NULL &&
         (overlaps(bytes, length, config->activation,
                   sizeof(*config->activation)) ||
          overlaps(bytes, length, config->activation->ops,
                   sizeof(*config->activation->ops)))))
        return true;
    for (size_t i = 0U; i < profile->endpoint_count; ++i) {
        const nx_usb_endpoint_port_t* port = profile->endpoints[i].port;
        if (overlaps(bytes, length, port, sizeof(*port)) ||
            overlaps(bytes, length, port->ops, sizeof(*port->ops)))
            return true;
    }
    return false;
}
/** \brief DCD borrows only the exact packet and explicit aligned staging. */
static nx_result_t submit_packet(nx_st_usb_device_stack_t* state,
                                 nx_st_usb_device_slot_t* slot) {
    if (nx_time_now_us() >= slot->deadline) {
        clear_intent(slot);
        return NX_ERROR_TIMEOUT;
    }
    size_t bytes = slot->length - slot->offset;
    if (bytes > slot->endpoint->max_packet)
        bytes = slot->endpoint->max_packet;
    slot->packet = bytes;
    if (slot->endpoint->in && bytes != 0U)
        memcpy(slot->storage, slot->bytes + slot->offset, bytes);
    size_t padded = (bytes + 3U) & ~(size_t)3U;
    if (slot->endpoint->in && padded > bytes)
        memset(slot->storage + bytes, 0, padded - bytes);
    nx_usb_stage_t stage = NX_USB_DATA;
    if (slot->endpoint->number == 0U &&
        (state->middleware.ep0_state == USBD_EP0_STATUS_IN ||
         state->middleware.ep0_state == USBD_EP0_STATUS_OUT ||
         (!slot->endpoint->in && slot->length == 0U &&
          state->middleware.ep0_state == USBD_EP0_DATA_IN)))
        stage = NX_USB_STATUS;
    nx_result_t result =
        slot->endpoint->in
            ? nx_usb_in_prepare(&slot->request, slot->storage, bytes, stage,
                                slot->epoch, slot->deadline)
            : nx_usb_out_prepare(&slot->request, slot->storage, bytes, stage,
                                 slot->epoch, slot->deadline);
    if (result == NX_SUCCESS)
        result = nx_usb_endpoint_submit(slot->endpoint->port, &slot->request);
    slot->pending = result == NX_SUCCESS;
    if (result != NX_SUCCESS)
        clear_intent(slot);
    return result;
}
/** \brief An acquired STATUS fact must suppress every later DATA intent. */
static bool status_out_received(nx_st_usb_device_stack_t* state) {
    nx_st_usb_device_slot_t* slot = selected(state, 0U, false);
    if (slot == NULL || !slot->pending || slot->epoch != state->epoch ||
        slot->request.stage != NX_USB_STATUS)
        return false;
    /* The same-epoch DCD snapshot closes DATA admission at the wire fact.
     * Its packet may still borrow memory while a DMA address cannot join. */
    if (state->latest.epoch == state->epoch && !state->latest.control_pending)
        return true;
    return nx_request_state(&slot->request.base) == NX_REQUEST_SETTLED &&
           slot->request.base.result == NX_SUCCESS &&
           slot->request.base.transferred == 0U;
}
/** \brief Prearm OUT0 before the Core exposes its first DATA IN packet. */
static nx_result_t arm_status_out(nx_st_usb_device_stack_t* state) {
    nx_st_usb_device_slot_t* slot = selected(state, 0U, false);
    if (slot == NULL || !slot->opened)
        return NX_ERROR_STATE;
    if (slot->busy)
        return slot->pending && slot->epoch == state->epoch &&
                       slot->request.stage == NX_USB_STATUS
                   ? NX_SUCCESS
                   : NX_ERROR_BUSY;
    slot->bytes = NULL;
    slot->length = slot->offset = slot->received = 0U;
    slot->epoch = state->epoch;
    slot->deadline = state->control_deadline;
    slot->busy = true;
    slot->cancel_sent = false;
    return submit_packet(state, slot);
}
/** \brief One LL transfer never allocates, rounds length or refreshes deadline.
 */
static USBD_StatusTypeDef transfer(USBD_HandleTypeDef* device, uint8_t address,
                                   uint8_t* bytes, uint32_t length, bool in) {
    nx_st_usb_device_stack_t* state = owner(device);
    nx_result_t result = admission(state);
    if (result != NX_SUCCESS)
        return status(state, result);
    if ((address & 0x70U) != 0U || (!in && (address & 0x80U) != 0U) ||
        (in && address != 0U && (address & 0x80U) == 0U))
        return status(state, NX_ERROR_INVALID);
    nx_st_usb_device_slot_t* slot = selected(state, address & 15U, in);
    if (slot == NULL || !slot->opened)
        return status(state, NX_ERROR_INVALID);
    if (slot->busy) {
        /* Core repeats PrepareReceive during continued DATA and STATUS.
         * Keep the one accepted request and its original deadline. */
        if (!in && slot->endpoint->number == 0U && length == 0U &&
            slot->pending && slot->epoch == state->epoch &&
            slot->request.stage == NX_USB_STATUS)
            return USBD_OK;
        return USBD_BUSY;
    }
    if (in && slot->endpoint->number == 0U &&
        device->ep0_state == USBD_EP0_DATA_IN && status_out_received(state))
        return USBD_BUSY;
    size_t actual = length;
    if (slot->endpoint->number == 0U) {
        if (!in && device->ep0_state == USBD_EP0_DATA_OUT &&
            actual > device->ep_out[0].rem_length)
            actual = device->ep_out[0].rem_length;
        if (actual > slot->endpoint->max_packet)
            actual = slot->endpoint->max_packet;
    }
    /* The pinned Core passes MAX(remainder, MPS) for an OUT tail. Validate
     * only the exact extent the DCD can borrow, never adjacent metadata. */
    if (!payload_valid(state, bytes, actual, in))
        return status(state, NX_ERROR_INVALID);
    nx_time_us_t deadline = state->control_deadline;
    result = slot->endpoint->number == 0U
                 ? (deadline != 0U ? NX_SUCCESS : NX_ERROR_STATE)
                 : nx_deadline_finite_after(nx_time_now_us(),
                                            state->config->transfer_timeout_us,
                                            &deadline);
    if (result != NX_SUCCESS)
        return status(state, result);
    if (in && slot->endpoint->number == 0U &&
        device->ep0_state == USBD_EP0_DATA_IN) {
        result = arm_status_out(state);
        if (result != NX_SUCCESS)
            return result == NX_ERROR_BUSY ? USBD_BUSY : status(state, result);
        if (status_out_received(state))
            return USBD_BUSY;
    }
    slot->bytes = bytes;
    slot->length = actual;
    slot->offset = 0U;
    slot->received = 0U;
    slot->epoch = slot->endpoint->number == 0U ? state->epoch : 0U;
    slot->deadline = deadline;
    slot->busy = true;
    slot->cancel_sent = false;
    result = submit_packet(state, slot);
    if (result != NX_SUCCESS && in && slot->endpoint->number == 0U &&
        status_out_received(state))
        return USBD_BUSY;
    return status(state, result);
}
/** \brief Join selected old packet epochs before Core can alter class storage.
 */
static bool join_slots(nx_st_usb_device_stack_t* state, bool all) {
    bool joined = true;
    for (size_t i = 0U; i < state->config->slot_count; ++i) {
        nx_st_usb_device_slot_t* slot = &state->config->slots[i];
        if ((!all && slot->endpoint->number != 0U) || !slot->busy)
            continue;
        if (slot->pending &&
            nx_request_state(&slot->request.base) != NX_REQUEST_SETTLED) {
            if (!slot->cancel_sent) {
                nx_result_t result = nx_usb_endpoint_cancel(
                    slot->endpoint->port, &slot->request);
                if (result == NX_SUCCESS)
                    slot->cancel_sent = true;
                else if (nx_request_state(&slot->request.base) !=
                         NX_REQUEST_SETTLED)
                    (void)status(state, result);
            }
            if (nx_request_state(&slot->request.base) != NX_REQUEST_SETTLED) {
                joined = false;
                continue;
            }
        }
        clear_intent(slot);
    }
    return joined;
}
/** \brief Setup bytes are serialized explicitly, independent of struct padding.
 */
static void setup_bytes(const nx_usb_setup_t* setup, uint8_t bytes[8]) {
    bytes[0] = setup->request_type;
    bytes[1] = setup->request;
    bytes[2] = (uint8_t)setup->value;
    bytes[3] = (uint8_t)(setup->value >> 8U);
    bytes[4] = (uint8_t)setup->index;
    bytes[5] = (uint8_t)(setup->index >> 8U);
    bytes[6] = (uint8_t)setup->length;
    bytes[7] = (uint8_t)(setup->length >> 8U);
}
/** \brief Only protocol controls that can withdraw a class join every
 * direction. */
static bool reconfiguration(const nx_usb_setup_t* setup) {
    return (setup->request_type == 0U && setup->request == 9U) ||
           (setup->request_type == 1U && setup->request == 11U);
}
/** \brief One acquired packet becomes one exact Core completion. */
static void complete_packet(nx_st_usb_device_stack_t* state,
                            nx_st_usb_device_slot_t* slot) {
    if (!slot->pending ||
        nx_request_state(&slot->request.base) != NX_REQUEST_SETTLED)
        return;
    slot->pending = false;
    nx_result_t result = slot->request.base.result;
    size_t bytes = slot->request.base.transferred;
    if (result != NX_SUCCESS || bytes > slot->packet ||
        (slot->endpoint->in && bytes != slot->packet)) {
        clear_intent(slot);
        (void)status(state, result != NX_SUCCESS ? result : NX_ERROR_IO);
        return;
    }
    if (!slot->endpoint->in && bytes != 0U)
        memcpy(slot->bytes + slot->offset, slot->storage, bytes);
    slot->offset += bytes;
    slot->received = (uint32_t)slot->offset;
    if (slot->endpoint->number != 0U && slot->offset < slot->length &&
        bytes == slot->packet) {
        (void)status(state, submit_packet(state, slot));
        return;
    }
    bool in = slot->endpoint->in;
    uint8_t number = slot->endpoint->number;
    clear_intent(slot);
    USBD_StatusTypeDef core =
        in ? USBD_LL_DataInStage(&state->middleware, number, NULL)
           : USBD_LL_DataOutStage(&state->middleware, number, NULL);
    if (core != USBD_OK && state->failure == NX_SUCCESS)
        state->failure = NX_ERROR_IO;
}
/** \brief STATUS owns its wire result, never another direction's loan. */
static bool complete_status_out(nx_st_usb_device_stack_t* state) {
    nx_st_usb_device_slot_t* output = selected(state, 0U, false);
    if (output == NULL || !output->pending || output->epoch != state->epoch ||
        output->request.stage != NX_USB_STATUS)
        return false;
    if (nx_request_state(&output->request.base) != NX_REQUEST_SETTLED)
        return status_out_received(state);
    if (output->request.base.result != NX_SUCCESS ||
        output->request.base.transferred != 0U) {
        (void)status(state, output->request.base.result != NX_SUCCESS
                                ? output->request.base.result
                                : NX_ERROR_IO);
        return true;
    }
    nx_st_usb_device_slot_t* input = selected(state, 0U, true);
    if (input != NULL && input->pending) {
        if (nx_request_state(&input->request.base) != NX_REQUEST_SETTLED) {
            if (!input->cancel_sent) {
                nx_result_t result = nx_usb_endpoint_cancel(
                    input->endpoint->port, &input->request);
                if (result == NX_SUCCESS)
                    input->cancel_sent = true;
                else
                    (void)status(state, result);
            }
            return true;
        }
        if (input->request.base.result == NX_ERROR_CANCELLED) {
            clear_intent(input);
        } else {
            /* Preserve a real last ACK. Any further Core DATA intent sees
             * the still-published STATUS fact and cannot launch a packet. */
            complete_packet(state, input);
            if (state->failure != NX_SUCCESS)
                return true;
        }
    }
    if (input != NULL && input->busy)
        clear_intent(input);
    state->middleware.ep0_state = USBD_EP0_STATUS_OUT;
    clear_intent(output);
    USBD_StatusTypeDef core =
        USBD_LL_DataOutStage(&state->middleware, 0U, NULL);
    if (core != USBD_OK)
        (void)status(state, NX_ERROR_IO);
    else
        state->middleware.ep0_state = USBD_EP0_IDLE;
    return true;
}
/** \brief Capture EP0 STATUS before any slot-ordered Core callbacks. */
static void complete_packets(nx_st_usb_device_stack_t* state) {
    bool status_seen = complete_status_out(state);
    for (size_t i = 0U; i < state->config->slot_count; ++i) {
        if (state->failure != NX_SUCCESS)
            return;
        nx_st_usb_device_slot_t* slot = &state->config->slots[i];
        if (!status_seen || slot->endpoint->number != 0U)
            complete_packet(state, slot);
    }
}

nx_result_t
nx_st_usb_device_stack_initialize(nx_st_usb_device_stack_t* state,
                                  const nx_st_usb_device_stack_config_t* config,
                                  const USBD_DescriptorsTypeDef* descriptors,
                                  const USBD_ClassTypeDef* selected_class) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!storage(state, sizeof(*state), _Alignof(nx_st_usb_device_stack_t)) ||
        !storage(config, sizeof(*config),
                 _Alignof(nx_st_usb_device_stack_config_t)) ||
        !storage(descriptors, sizeof(*descriptors),
                 _Alignof(USBD_DescriptorsTypeDef)) ||
        !storage(selected_class, sizeof(*selected_class),
                 _Alignof(USBD_ClassTypeDef)) ||
        config->transfer_timeout_us == 0U ||
        nx_usb_profile_validate(config->profile) != NX_SUCCESS ||
        (config->profile->endpoint_count > 2U && config->activation == NULL) ||
        (config->activation != NULL &&
         (!storage(config->activation, sizeof(*config->activation),
                   _Alignof(nx_usb_activation_port_t)) ||
          !storage(config->activation->ops, sizeof(*config->activation->ops),
                   _Alignof(nx_usb_activation_ops_t)) ||
          config->activation->ops->set == NULL ||
          config->activation->context != config->profile->port->context)) ||
        config->slot_count != config->profile->endpoint_count ||
        !storage(config->slots, config->slot_count * sizeof(*config->slots),
                 _Alignof(nx_st_usb_device_slot_t)) ||
        selected_class->Init == NULL || selected_class->DeInit == NULL ||
        selected_class->Setup == NULL ||
        selected_class->GetFSConfigDescriptor == NULL ||
        (config->profile->speed == NX_USB_PROFILE_HIGH_SPEED &&
         (selected_class->GetHSConfigDescriptor == NULL ||
          selected_class->GetDeviceQualifierDescriptor == NULL ||
          selected_class->GetOtherSpeedConfigDescriptor == NULL)) ||
        descriptors->GetDeviceDescriptor == NULL ||
        overlaps(state, sizeof(*state), config, sizeof(*config)) ||
        overlaps(state, sizeof(*state), descriptors, sizeof(*descriptors)) ||
        overlaps(state, sizeof(*state), selected_class,
                 sizeof(*selected_class)) ||
        overlaps(state, sizeof(*state), config->slots,
                 config->slot_count * sizeof(*config->slots)) ||
        facts_overlap(config, state, sizeof(*state)) ||
        facts_overlap(config, config->slots,
                      config->slot_count * sizeof(*config->slots)))
        return NX_ERROR_INVALID;
    if (state->config != NULL)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < config->slot_count; ++i) {
        const nx_st_usb_device_slot_t* slot = &config->slots[i];
        size_t required =
            (config->profile->endpoints[i].max_packet + 3U) & ~(size_t)3U;
        if (!storage(slot->storage, slot->capacity, 4U) ||
            slot->capacity < required || slot->busy || slot->pending ||
            overlaps(slot->storage, slot->capacity, state, sizeof(*state)) ||
            overlaps(slot->storage, slot->capacity, config, sizeof(*config)) ||
            facts_overlap(config, slot->storage, slot->capacity) ||
            overlaps(slot->storage, slot->capacity, descriptors,
                     sizeof(*descriptors)) ||
            overlaps(slot->storage, slot->capacity, selected_class,
                     sizeof(*selected_class)) ||
            overlaps(slot->storage, slot->capacity, config->slots,
                     config->slot_count * sizeof(*slot)))
            return NX_ERROR_INVALID;
        for (size_t j = 0U; j < i; ++j)
            if (overlaps(slot->storage, slot->capacity,
                         config->slots[j].storage, config->slots[j].capacity))
                return NX_ERROR_INVALID;
    }
    nx_usb_device_state_t latest;
    nx_result_t result = nx_usb_device_state(config->profile->port, &latest);
    if (result != NX_SUCCESS)
        return result;
    nx_usb_device_speed_t expected =
        config->profile->speed == NX_USB_PROFILE_HIGH_SPEED
            ? NX_USB_DEVICE_HIGH_SPEED
            : NX_USB_DEVICE_FULL_SPEED;
    if (!latest.started || latest.closing || latest.address != 0U ||
        (latest.speed != NX_USB_DEVICE_SPEED_UNKNOWN &&
         latest.speed != expected))
        return NX_ERROR_STATE;
    *state = (nx_st_usb_device_stack_t){0};
    state->config = config;
    state->latest = latest;
    state->epoch = latest.epoch;
    state->lost_events = latest.lost_events;
    state->descriptors = *descriptors;
    state->selected_class = *selected_class;
    state->middleware.pData = state;
    for (size_t i = 0U; i < config->slot_count; ++i) {
        uint8_t* bytes = config->slots[i].storage;
        size_t capacity = config->slots[i].capacity;
        config->slots[i] = (nx_st_usb_device_slot_t){
            .storage = bytes,
            .capacity = capacity,
            .endpoint = &config->profile->endpoints[i]};
        nx_request_initialize(&config->slots[i].request.base);
    }
    state->executing = true;
    state->core_initialized = true;
    USBD_StatusTypeDef core =
        USBD_Init(&state->middleware, &state->descriptors, 0U);
    state->middleware.pUserData[0] = config->class_context;
    if (core == USBD_OK)
        core =
            USBD_LL_SetSpeed(&state->middleware,
                             config->profile->speed == NX_USB_PROFILE_HIGH_SPEED
                                 ? USBD_SPEED_HIGH
                                 : USBD_SPEED_FULL);
    if (core == USBD_OK)
        core = USBD_LL_Reset(&state->middleware);
    if (core == USBD_OK)
        core = USBD_RegisterClass(&state->middleware, &state->selected_class);
    if (core == USBD_OK &&
        config->profile->speed == NX_USB_PROFILE_HIGH_SPEED) {
        uint16_t bytes = 0U;
        state->middleware.pConfDesc =
            state->selected_class.GetHSConfigDescriptor(&bytes);
        if (state->middleware.pConfDesc == NULL || bytes < 9U)
            (void)status(state, NX_ERROR_INVALID);
    }
    if (core == USBD_OK)
        core = USBD_Start(&state->middleware);
    if (core != USBD_OK && state->failure == NX_SUCCESS)
        state->failure = NX_ERROR_IO;
    state->executing = false;
    return state->failure;
}

nx_result_t nx_st_usb_device_stack_step(nx_st_usb_device_stack_t* state) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || state->config == NULL)
        return NX_ERROR_INVALID;
    if (state->executing)
        return NX_ERROR_BUSY;
    if (state->closing || state->stopped)
        return NX_ERROR_STATE;
    state->executing = true;
    nx_result_t result = nx_usb_device_service(state->config->profile->port);
    if (result != NX_SUCCESS && result != NX_ERROR_BUSY)
        (void)status(state, result);
    result = nx_usb_device_state(state->config->profile->port, &state->latest);
    if (result != NX_SUCCESS)
        (void)status(state, result);
    if (!state->latest.started || state->latest.closing)
        (void)status(state, NX_ERROR_STATE);
    if (state->latest.lost_events != state->lost_events)
        (void)status(state, NX_ERROR_OVERFLOW);
    if (!state->event_pending && state->failure == NX_SUCCESS) {
        size_t count = 0U;
        result = nx_usb_device_read_events(state->config->profile->port,
                                           &state->event, 1U, &count);
        if (result == NX_ERROR_EMPTY && count == 0U)
            result = NX_SUCCESS;
        if (result != NX_SUCCESS || count > 1U)
            (void)status(state, result != NX_SUCCESS ? result : NX_ERROR_IO);
        else
            state->event_pending = count != 0U;
    }
    if (state->latest.epoch != state->epoch)
        state->joining = true;
    if (state->event_pending) {
        if (state->event.kind == NX_USB_EVENT_LOSS)
            (void)status(state, NX_ERROR_OVERFLOW);
        if (state->event.kind == NX_USB_EVENT_RESET ||
            (state->event.kind == NX_USB_EVENT_SETUP &&
             reconfiguration(&state->event.setup)))
            state->joining = state->join_all = true;
        if (state->event.kind == NX_USB_EVENT_SETUP)
            state->joining = true;
    }
    if (state->failure != NX_SUCCESS) {
        (void)join_slots(state, true);
    } else if (!state->joining || join_slots(state, state->join_all)) {
        if (state->event_pending) {
            state->joining = state->join_all = false;
            nx_usb_event_t event = state->event;
            state->event_pending = false;
            USBD_StatusTypeDef core = USBD_OK;
            if (event.kind == NX_USB_EVENT_SETUP &&
                event.setup.epoch == state->latest.epoch &&
                state->latest.control_pending) {
                uint8_t bytes[8];
                state->epoch = event.setup.epoch;
                result = nx_deadline_finite_after(
                    nx_time_now_us(), state->config->transfer_timeout_us,
                    &state->control_deadline);
                (void)status(state, result);
                for (size_t i = 0U; i < state->config->slot_count; ++i)
                    if (state->config->slots[i].endpoint->number == 0U)
                        state->config->slots[i].stalled = false;
                setup_bytes(&event.setup, bytes);
                if (result == NX_SUCCESS) {
                    /* A new SETUP may abort an earlier SET/CLEAR_FEATURE.
                     * Core's eager intent cannot replace the DCD's ACK. */
                    state->middleware.dev_remote_wakeup =
                        state->latest.remote_wakeup_enabled ? 1U : 0U;
                    core = USBD_LL_SetupStage(&state->middleware, bytes);
                }
            } else if (event.kind == NX_USB_EVENT_RESET) {
                state->epoch = state->latest.epoch;
                core = USBD_LL_Reset(&state->middleware);
                for (size_t i = 0U; i < state->config->slot_count; ++i)
                    if (state->config->slots[i].endpoint->number != 0U)
                        state->config->slots[i].opened = false;
            } else if (event.kind == NX_USB_EVENT_SUSPEND) {
                core = USBD_LL_Suspend(&state->middleware);
            } else if (event.kind == NX_USB_EVENT_RESUME) {
                core = USBD_LL_Resume(&state->middleware);
            } else if (event.kind == NX_USB_EVENT_ENUMERATED) {
                bool high =
                    state->config->profile->speed == NX_USB_PROFILE_HIGH_SPEED;
                if (state->latest.speed != (high ? NX_USB_DEVICE_HIGH_SPEED
                                                 : NX_USB_DEVICE_FULL_SPEED))
                    (void)status(state, NX_ERROR_STATE);
                else
                    core = USBD_LL_SetSpeed(&state->middleware,
                                            high ? USBD_SPEED_HIGH
                                                 : USBD_SPEED_FULL);
            }
            if (core != USBD_OK && state->failure == NX_SUCCESS)
                state->failure = NX_ERROR_IO;
        } else if (!state->joining) {
            complete_packets(state);
        }
    }
    state->executing = false;
    return state->failure;
}

nx_result_t nx_st_usb_device_stack_close(nx_st_usb_device_stack_t* state) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (state->config == NULL)
        return NX_SUCCESS;
    if (state->executing)
        return NX_ERROR_BUSY;
    state->executing = state->closing = true;
    nx_result_t result = state->stopped
                             ? NX_SUCCESS
                             : nx_usb_device_stop(state->config->profile->port);
    if (result == NX_SUCCESS)
        state->stopped = true;
    if (result != NX_SUCCESS || !join_slots(state, true)) {
        state->executing = false;
        return result != NX_SUCCESS ? result : NX_ERROR_BUSY;
    }
    if (state->core_initialized) {
        /* The stopped DCD owns no packet before class DeInit can mutate data.
         */
        state->failure = NX_SUCCESS;
        USBD_StatusTypeDef core = USBD_DeInit(&state->middleware);
        if (core != USBD_OK || state->failure != NX_SUCCESS) {
            state->executing = false;
            return state->failure != NX_SUCCESS ? state->failure : NX_ERROR_IO;
        }
        state->core_initialized = false;
    }
    state->middleware.pData = NULL;
    state->config = NULL;
    state->executing = false;
    return NX_SUCCESS;
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef* device) {
    return status(owner(device), task_context() && owner(device) != NULL
                                     ? NX_SUCCESS
                                     : NX_ERROR_CONTEXT);
}
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef* device) {
    nx_st_usb_device_stack_t* state = owner(device);
    return status(state,
                  state != NULL && state->stopped ? NX_SUCCESS : NX_ERROR_BUSY);
}
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef* device) {
    nx_st_usb_device_stack_t* state = owner(device);
    return status(state, admission(state));
}
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef* device) {
    nx_st_usb_device_stack_t* state = owner(device);
    return status(state, state != NULL && state->closing && state->stopped
                             ? NX_SUCCESS
                             : NX_ERROR_STATE);
}
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef* device, uint8_t address,
                                  uint8_t type, uint16_t maximum) {
    nx_st_usb_device_stack_t* state = owner(device);
    nx_result_t result = admission(state);
    if (result != NX_SUCCESS)
        return status(state, result);
    nx_st_usb_device_slot_t* slot =
        selected(state, address & 15U, (address & 0x80U) != 0U);
    if ((address & 0x70U) != 0U || slot == NULL ||
        maximum != slot->endpoint->max_packet ||
        type != (slot->endpoint->transfer == NX_USB_PROFILE_CONTROL
                     ? USBD_EP_TYPE_CTRL
                 : slot->endpoint->transfer == NX_USB_PROFILE_BULK
                     ? USBD_EP_TYPE_BULK
                 : slot->endpoint->transfer == NX_USB_PROFILE_ISOCHRONOUS
                     ? USBD_EP_TYPE_ISOC
                     : USBD_EP_TYPE_INTR))
        return status(state, NX_ERROR_INVALID);
    if (slot->busy)
        return status(state, NX_ERROR_BUSY);
    if (!slot->opened && slot->endpoint->number != 0U)
        result = nx_usb_activation_set(state->config->activation,
                                       state->latest.epoch, address, true);
    if (result == NX_SUCCESS) {
        slot->opened = true;
        slot->stalled = false;
        USBD_EndpointTypeDef* core =
            slot->endpoint->in ? &device->ep_in[slot->endpoint->number]
                               : &device->ep_out[slot->endpoint->number];
        core->maxpacket = maximum;
        core->is_used = 1U;
    }
    return status(state, result);
}
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef* device,
                                   uint8_t address) {
    nx_st_usb_device_stack_t* state = owner(device);
    if (!task_context() || state == NULL)
        return status(state, NX_ERROR_CONTEXT);
    nx_st_usb_device_slot_t* slot =
        selected(state, address & 15U, (address & 0x80U) != 0U);
    if ((address & 0x70U) != 0U || slot == NULL)
        return status(state, NX_ERROR_INVALID);
    if (slot->pending)
        return status(state, NX_ERROR_BUSY);
    nx_result_t result = NX_SUCCESS;
    if (slot->opened && slot->endpoint->number != 0U && !state->stopped)
        result = nx_usb_activation_set(state->config->activation,
                                       state->latest.epoch, address, false);
    if (result == NX_SUCCESS) {
        slot->opened = false;
        clear_intent(slot);
    }
    return status(state, result);
}
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef* device,
                                   uint8_t address) {
    nx_st_usb_device_stack_t* state = owner(device);
    nx_st_usb_device_slot_t* slot =
        selected(state, address & 15U, (address & 0x80U) != 0U);
    if (!task_context() || slot == NULL)
        return status(state, NX_ERROR_CONTEXT);
    if (slot->pending)
        return status(state, NX_ERROR_BUSY);
    clear_intent(slot);
    return USBD_OK;
}
/** \brief ST's EP0 guard STALL is not whole-SETUP rejection policy. */
static USBD_StatusTypeDef stall(USBD_HandleTypeDef* device, uint8_t address,
                                bool value) {
    nx_st_usb_device_stack_t* state = owner(device);
    if (!task_context() || state == NULL)
        return status(state, NX_ERROR_CONTEXT);
    nx_st_usb_device_slot_t* slot =
        selected(state, address & 15U, (address & 0x80U) != 0U);
    if ((address & 0x70U) != 0U || slot == NULL)
        return status(state, NX_ERROR_INVALID);
    nx_result_t result =
        state->stopped ? NX_SUCCESS
                       : nx_usb_endpoint_stall(slot->endpoint->port, value);
    if (result == NX_SUCCESS)
        slot->stalled = value;
    return status(state, result);
}
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef* device,
                                   uint8_t address) {
    return stall(device, address, true);
}
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef* device,
                                        uint8_t address) {
    return stall(device, address, false);
}
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef* device,
                                         uint8_t address) {
    nx_st_usb_device_stack_t* state = owner(device);
    nx_result_t result = admission(state);
    if (result == NX_SUCCESS)
        result = nx_usb_device_address(state->config->profile->port,
                                       state->epoch, address);
    return status(state, result);
}
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef* device, uint8_t address,
                                    uint8_t* bytes, uint32_t length) {
    return transfer(device, address, bytes, length, true);
}
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef* device,
                                          uint8_t address, uint8_t* bytes,
                                          uint32_t length) {
    return transfer(device, address, bytes, length, false);
}
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef* device, uint8_t address) {
    nx_st_usb_device_slot_t* slot =
        selected(owner(device), address & 15U, (address & 0x80U) != 0U);
    return slot != NULL && slot->stalled ? 1U : 0U;
}
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef* device, uint8_t address) {
    nx_st_usb_device_slot_t* slot =
        selected(owner(device), address & 15U, false);
    return slot != NULL ? slot->received : 0U;
}
