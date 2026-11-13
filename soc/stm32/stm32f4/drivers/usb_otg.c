/**
 * \file            usb_otg.c
 * \brief           Composite F407 USB cold role and VBUS ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_usb_otg.h"

/** \brief Cold, power and role work has exactly one unmasked Thread owner. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Numeric extents reject wrapping before any caller object is read. */
static bool overlaps(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (an != 0U && bn != 0U && x < y + bn && y < x + an);
}

/** \brief Cold assembly never erases facts or arrays inside either owner. */
static bool separate(const nx_stm32_usb_otg_t* state, const void* p,
                     size_t size) {
    return p != NULL && !overlaps(p, size, state, sizeof(*state));
}

/** \brief Both active and inactive selected storage survive composite reset. */
static bool device_storage(const nx_stm32_usb_otg_t* state,
                           nx_usb_endpoint_t* endpoints, size_t count,
                           const nx_stm32_usb_endpoint_config_t* plan,
                           nx_usb_event_t* events, size_t capacity,
                           const nx_stm32_platform_pin_config_t* pins,
                           uint8_t pin_count, const void* clock,
                           size_t clock_bytes) {
    if (count < 2U || count > 12U || capacity == 0U || pin_count > 12U ||
        capacity > SIZE_MAX / sizeof(*events) ||
        !separate(state, endpoints, count * sizeof(*endpoints)) ||
        !separate(state, plan, count * sizeof(*plan)) ||
        !separate(state, events, capacity * sizeof(*events)) ||
        !separate(state, pins, (size_t)pin_count * sizeof(*pins)) ||
        !separate(state, clock, clock_bytes))
        return false;
    for (uint8_t i = 0U; i < pin_count; ++i)
        if (!separate(state, pins[i].snapshot, sizeof(*pins[i].snapshot)))
            return false;
    return true;
}

/** \brief The whole composite, including the inactive transport, is metadata.
 */
static bool outside(const nx_stm32_usb_otg_t* state, const void* p, size_t n) {
    if (p == NULL || overlaps(p, n, state, sizeof(*state)) ||
        overlaps(p, n, state->config, sizeof(*state->config)))
        return false;
    const nx_stm32_usb_otg_config_t* c = state->config;
    return !overlaps(p, n, c->host, sizeof(*c->host)) &&
           (c->device_fs == NULL ||
            !overlaps(p, n, c->device_fs, sizeof(*c->device_fs))) &&
           (c->device_hs == NULL ||
            !overlaps(p, n, c->device_hs, sizeof(*c->device_hs)));
}

/** \brief A power signal is one selected GPIO bit, not inferred bus policy. */
static bool signal_valid(const nx_gpio_port_t* port, uint32_t mask,
                         bool output) {
    return port != NULL && port->ops != NULL && port->context != NULL &&
           port->ops->read != NULL && (!output || port->ops->write != NULL) &&
           mask != 0U && (mask & (mask - 1U)) == 0U;
}

/** \brief Distinct faces may share a GPIO port only with disjoint bits. */
static bool independent(const nx_gpio_port_t* a, uint32_t am,
                        const nx_gpio_port_t* b, uint32_t bm) {
    return a != b && (a->context != b->context || (am & bm) == 0U);
}

/** \brief Optional pulse storage cannot alias the inactive physical owner. */
static bool remote_storage(const nx_stm32_usb_otg_t* state,
                           const nx_stm32_usb_otg_config_t* config,
                           const nx_usb_remote_runtime_t* remote) {
    if (!NEXUS_STM32F4_USB_REMOTE_WAKEUP)
        return remote == NULL;
    if (remote == NULL)
        return true;
    return separate(state, remote, sizeof(*remote)) &&
           nx_stm32_usb_remote_separate(remote, config, sizeof(*config)) &&
           nx_stm32_usb_remote_separate(remote, config->host,
                                        sizeof(*config->host)) &&
           nx_stm32_usb_remote_separate(
               remote, config->host->transport.channels,
               (size_t)config->host->transport.channel_count *
                   sizeof(*config->host->transport.channels)) &&
           nx_stm32_usb_remote_separate(remote, config->source,
                                        sizeof(*config->source)) &&
           nx_stm32_usb_remote_separate(remote, config->present,
                                        sizeof(*config->present)) &&
           nx_stm32_usb_remote_separate(remote, config->overcurrent,
                                        sizeof(*config->overcurrent)) &&
           nx_stm32_usb_remote_separate(remote, config->source->ops,
                                        sizeof(*config->source->ops)) &&
           nx_stm32_usb_remote_separate(remote, config->present->ops,
                                        sizeof(*config->present->ops)) &&
           nx_stm32_usb_remote_separate(remote, config->overcurrent->ops,
                                        sizeof(*config->overcurrent->ops)) &&
           remote->request == NULL;
}

/** \brief Host and Device use one exact pad/snapshot and interrupt plan. */
static bool config_valid(const nx_stm32_usb_otg_t* state,
                         const nx_stm32_usb_otg_config_t* config) {
    if (!separate(state, config, sizeof(*config)) ||
        !separate(state, config->host, sizeof(*config->host)) ||
        !separate(state, config->source, sizeof(*config->source)) ||
        !separate(state, config->present, sizeof(*config->present)) ||
        !separate(state, config->overcurrent, sizeof(*config->overcurrent)) ||
        !signal_valid(config->source, config->source_mask, true) ||
        !signal_valid(config->present, config->present_mask, false) ||
        !signal_valid(config->overcurrent, config->overcurrent_mask, false) ||
        !independent(config->source, config->source_mask, config->present,
                     config->present_mask) ||
        !independent(config->source, config->source_mask, config->overcurrent,
                     config->overcurrent_mask) ||
        !independent(config->present, config->present_mask, config->overcurrent,
                     config->overcurrent_mask) ||
        config->fault_timeout_us == 0U ||
        !nx_stm32_usb_host_cold_valid(config->host) ||
        config->host->transport.phy != config->phy ||
        nx_stm32_usb_host_validate(&state->host, &config->host->transport) !=
            NX_SUCCESS)
        return false;
    if (!separate(state, config->host->transport.channels,
                  (size_t)config->host->transport.channel_count *
                      sizeof(*config->host->transport.channels)))
        return false;
    const nx_stm32_platform_pin_config_t* pins;
    const nx_irq_policy_t* policy;
    uint8_t count, priority;
    if (config->phy == NX_STM32_USB_FS_INTERNAL) {
        if (!separate(state, config->device_fs, sizeof(*config->device_fs)) ||
            config->device_hs != NULL)
            return false;
        const nx_stm32_usb_cold_config_t* d = config->device_fs;
        if (!remote_storage(state, config, d->remote) ||
            !device_storage(state, d->endpoints, d->endpoint_count, d->plan,
                            d->events, d->capacity, d->pins, 2U, d->clock,
                            sizeof(*d->clock)))
            return false;
        pins = config->device_fs->pins;
        count = 2U;
        priority = config->device_fs->priority;
        policy = &config->device_fs->irq_policy;
    } else {
        if ((config->phy != NX_STM32_USB_HS_INTERNAL_FS &&
             config->phy != NX_STM32_USB_HS_ULPI) ||
            config->device_fs != NULL ||
            !separate(state, config->device_hs, sizeof(*config->device_hs)) ||
            config->device_hs->phy != config->phy ||
            config->device_hs->ulpi_clock_hz !=
                config->host->transport.ulpi_clock_hz)
            return false;
        const nx_stm32_usb_hs_cold_config_t* d = config->device_hs;
        if (!remote_storage(state, config, d->remote) ||
            !device_storage(state, d->endpoints, d->endpoint_count, d->plan,
                            d->events, d->capacity, d->pins, d->pin_count,
                            d->clock, sizeof(*d->clock)))
            return false;
        pins = config->device_hs->pins;
        count = config->device_hs->pin_count;
        priority = config->device_hs->priority;
        policy = &config->device_hs->irq_policy;
    }
    const nx_irq_policy_t* hp = &config->host->transport.irq_policy;
    return pins == config->host->pins && count == config->host->pin_count &&
           nx_stm32_usb_pins_valid(config->phy, pins, count) &&
           priority == config->host->transport.priority &&
           policy->priority_bits == hp->priority_bits &&
           policy->kernel == hp->kernel &&
           policy->external_irq_count == hp->external_irq_count &&
           policy->syscall_ceiling == hp->syscall_ceiling;
}

/** \brief GPIO readback never substitutes requested switch state for VBUS. */
static nx_result_t read_signal(const nx_gpio_port_t* port, uint32_t mask,
                               bool high, bool* active) {
    uint32_t level;
    nx_result_t result = nx_gpio_port_read(port, &level);
    if (result == NX_SUCCESS)
        *active = ((level & mask) != 0U) == high;
    return result;
}

/** \brief The selected external overcurrent input is independently observed. */
static nx_result_t overcurrent(const nx_stm32_usb_otg_t* state, bool* active) {
    const nx_stm32_usb_otg_config_t* c = state->config;
    return read_signal(c->overcurrent, c->overcurrent_mask,
                       c->overcurrent_active_high, active);
}

/** \brief A successful command includes switch readback, even when turning off.
 */
static nx_result_t source(void* context, bool enabled) {
    nx_stm32_usb_otg_t* state = context;
    const nx_stm32_usb_otg_config_t* c = state->config;
    if (enabled) {
        bool asserted;
        nx_result_t result = overcurrent(state, &asserted);
        if (result != NX_SUCCESS || asserted) {
            state->fault = result != NX_SUCCESS ? result : NX_ERROR_IO;
            return state->fault;
        }
    }
    bool high = enabled == c->source_active_high;
    nx_result_t result = nx_gpio_port_write(
        c->source, high ? c->source_mask : 0U, high ? 0U : c->source_mask);
    bool actual = !enabled;
    if (result == NX_SUCCESS)
        result = read_signal(c->source, c->source_mask, c->source_active_high,
                             &actual);
    if (result == NX_SUCCESS && actual != enabled)
        result = NX_ERROR_IO;
    if (result != NX_SUCCESS)
        state->fault = result;
    return result;
}

/** \brief This is the actual independent VBUS observation, not switch feedback.
 */
static nx_result_t present(void* context, bool* active) {
    nx_stm32_usb_otg_t* state = context;
    const nx_stm32_usb_otg_config_t* c = state->config;
    return read_signal(c->present, c->present_mask, c->present_active_high,
                       active);
}

/** \brief Publish the attempted cold owner before its driver may enable IRQ. */
static nx_result_t start(void* context, nx_usb_role_kind_t role,
                         nx_time_us_t deadline) {
    nx_stm32_usb_otg_t* state = context;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (state->transport != NX_USB_ROLE_OFF)
        return NX_ERROR_BUSY;
    if (role == NX_USB_ROLE_DEVICE) {
        if (state->device.snapshot.epoch == UINT64_MAX)
            return NX_ERROR_OVERFLOW;
        /* A new physical role cannot accept an old control epoch. */
        ++state->device.snapshot.epoch;
        state->transport = NX_USB_ROLE_DEVICE;
        nx_result_t result =
            state->config->device_fs != NULL
                ? nx_stm32_usb_cold_prepare(&state->device,
                                            state->config->device_fs,
                                            &state->device_fs_leases)
                : nx_stm32_usb_hs_cold_prepare(&state->device,
                                               state->config->device_hs,
                                               &state->device_hs_receipt);
        return result == NX_SUCCESS &&
                       nx_deadline_expired(deadline, nx_time_now_us())
                   ? NX_ERROR_TIMEOUT
                   : result;
    }
    if (role != NX_USB_ROLE_HOST)
        return NX_ERROR_INVALID;
    state->transport = NX_USB_ROLE_HOST;
    nx_result_t result = nx_stm32_usb_host_cold_prepare(
        &state->host, state->config->host, &state->host_receipt);
    return result == NX_SUCCESS &&
                   nx_deadline_expired(deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : result;
}

/** \brief Full cold close precedes supply withdrawal and the next owner. */
static nx_result_t stop(void* context, nx_time_us_t deadline) {
    nx_stm32_usb_otg_t* state = context;
    (void)deadline;
    nx_result_t result = NX_SUCCESS;
    if (state->transport == NX_USB_ROLE_DEVICE)
        result = state->config->device_fs != NULL
                     ? nx_stm32_usb_cold_close(&state->device,
                                               state->config->device_fs,
                                               &state->device_fs_leases)
                     : nx_stm32_usb_hs_cold_close(&state->device,
                                                  state->config->device_hs,
                                                  &state->device_hs_receipt);
    else if (state->transport == NX_USB_ROLE_HOST)
        result = nx_stm32_usb_host_cold_close(&state->host, state->config->host,
                                              &state->host_receipt);
    if (result == NX_SUCCESS) {
        nx_arch_dsb();
        state->transport = NX_USB_ROLE_OFF;
    }
    return result;
}

static const nx_usb_role_backend_ops_t role_ops = {start, stop, source,
                                                   present};

/** \brief Inactive and transitioning faces reject before any MMIO or GPIO. */
static bool active(const nx_stm32_usb_otg_t* state, nx_usb_role_kind_t role) {
    return state != NULL && state->initialized && !state->closing &&
           state->fault == NX_SUCCESS && state->role.phase == 0U &&
           state->role.active == role && state->transport == role;
}

/** \brief Current face lookup also requires a running packet admission gate. */
static bool ready(const nx_stm32_usb_otg_t* state, nx_usb_role_kind_t role) {
    if (!active(state, role))
        return false;
    return role == NX_USB_ROLE_DEVICE
               ? state->device.snapshot.started &&
                     !state->device.snapshot.closing
               : state->host.snapshot.started && !state->host.snapshot.closing;
}

/** \brief Accepted role changes invalidate previously observed generations. */
static nx_result_t request(void* context, uint64_t generation,
                           nx_usb_otg_role_t target, nx_time_us_t deadline) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || !state->initialized || state->closing ||
        state->generation != generation)
        return NX_ERROR_STATE;
    if (target != NX_USB_OTG_OFF && target != NX_USB_OTG_DEVICE &&
        target != NX_USB_OTG_HOST)
        return NX_ERROR_INVALID;
    if (state->fault != NX_SUCCESS) {
        if (target != NX_USB_OTG_OFF || state->role.phase != 0U ||
            state->role.active != NX_USB_ROLE_OFF)
            return state->fault;
        bool asserted;
        nx_result_t result = overcurrent(state, &asserted);
        if (result != NX_SUCCESS || asserted)
            return result != NX_SUCCESS ? result : NX_ERROR_IO;
    }
    if (state->generation == UINT64_MAX &&
        (nx_usb_role_kind_t)target != state->role.active)
        return NX_ERROR_OVERFLOW;
    nx_result_t result =
        nx_usb_role_request(&state->role, (nx_usb_role_kind_t)target, deadline);
    if (result == NX_SUCCESS) {
        if (state->role.phase != 0U)
            ++state->generation;
        else if (target == NX_USB_OTG_OFF) {
            state->fault = NX_SUCCESS;
        }
    }
    return result;
}

/** \brief Overcurrent closes admission and uses the same retained drain path.
 */
static nx_result_t service(void* context) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || !state->initialized)
        return NX_ERROR_STATE;
    if (!state->closing && state->fault == NX_SUCCESS &&
        (state->role.source_owned || state->transport == NX_USB_ROLE_HOST)) {
        bool asserted;
        nx_result_t result = overcurrent(state, &asserted);
        if (result != NX_SUCCESS || asserted) {
            state->fault = result != NX_SUCCESS ? result : NX_ERROR_IO;
            nx_time_us_t deadline;
            result = nx_deadline_finite_after(
                nx_time_now_us(), state->config->fault_timeout_us, &deadline);
            if (result == NX_SUCCESS)
                result = nx_usb_role_cancel(&state->role, deadline);
            if (result != NX_SUCCESS)
                return result;
            if (state->generation != UINT64_MAX)
                ++state->generation;
        }
    }
    nx_result_t result = nx_usb_role_service(&state->role);
    return state->role.phase == 0U && state->fault != NX_SUCCESS ? state->fault
                                                                 : result;
}

/** \brief Authoritative role facts never poll electrical or USB registers. */
static nx_result_t snapshot(const void* context, nx_usb_otg_state_t* output) {
    const nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || !state->initialized)
        return NX_ERROR_STATE;
    if (!outside(state, output, sizeof(*output)))
        return NX_ERROR_INVALID;
    *output =
        (nx_usb_otg_state_t){.generation = state->generation,
                             .active = (nx_usb_otg_role_t)state->role.active,
                             .target = (nx_usb_otg_role_t)state->role.target,
                             .result = state->role.result,
                             .fault = state->fault,
                             .busy = state->role.phase != 0U,
                             .ready = ready(state, state->role.active) &&
                                      state->role.active != NX_USB_ROLE_OFF,
                             .source_owned = state->role.source_owned};
    return NX_SUCCESS;
}

/** \brief The stable typed face is available only in its current generation. */
static const nx_usb_device_port_t* device_face(const void* context,
                                               uint64_t generation) {
    const nx_stm32_usb_otg_t* state = context;
    return task_context() && ready(state, NX_USB_ROLE_DEVICE) &&
                   state->generation == generation
               ? &state->device_face
               : NULL;
}

/** \brief Host lookup performs no enumeration, power or cold acquisition. */
static const nx_usb_host_port_t* host_face(const void* context,
                                           uint64_t generation) {
    const nx_stm32_usb_otg_t* state = context;
    return task_context() && ready(state, NX_USB_ROLE_HOST) &&
                   state->generation == generation
               ? &state->host_face
               : NULL;
}

const nx_usb_otg_ops_t nx_stm32_usb_otg_ops = {request, service, snapshot,
                                               device_face, host_face};

/** \brief Device service retains the existing authoritative DCD implementation.
 */
static nx_result_t device_service(void* context) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return active(state, NX_USB_ROLE_DEVICE)
               ? nx_usb_fs_device_ops.service(&state->device)
               : NX_ERROR_STATE;
}

/** \brief Protect the entire composite before the DCD copies event output. */
static nx_result_t device_events(void* context, nx_usb_event_t* events,
                                 size_t count, size_t* actual) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!ready(state, NX_USB_ROLE_DEVICE))
        return NX_ERROR_STATE;
    if (count > SIZE_MAX / sizeof(*events) ||
        !outside(state, actual, sizeof(*actual)) ||
        (count != 0U && !outside(state, events, count * sizeof(*events))))
        return NX_ERROR_INVALID;
    return nx_usb_fs_device_ops.read_events(&state->device, events, count,
                                            actual);
}

/** \brief Inactive state reads leave caller bytes completely unchanged. */
static nx_result_t device_state(void* context, nx_usb_device_state_t* output) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!active(state, NX_USB_ROLE_DEVICE))
        return NX_ERROR_STATE;
    return outside(state, output, sizeof(*output))
               ? nx_usb_fs_device_ops.state(&state->device, output)
               : NX_ERROR_INVALID;
}

/** \brief Control epochs are still validated atomically by the one DCD. */
static nx_result_t device_address(void* context, uint64_t epoch,
                                  uint8_t value) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return ready(state, NX_USB_ROLE_DEVICE)
               ? nx_usb_fs_device_ops.address(&state->device, epoch, value)
               : NX_ERROR_STATE;
}

/** \brief Observer attachment cannot address the inactive physical owner. */
static nx_result_t device_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t priority) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return ready(state, NX_USB_ROLE_DEVICE)
               ? nx_usb_fs_device_ops.attach_wake(&state->device, wake,
                                                  priority)
               : NX_ERROR_STATE;
}

/** \brief Protocol close may stop its DCD before the outer role closes pads. */
static nx_result_t device_stop(void* context) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || !state->initialized || state->role.phase != 0U ||
        state->role.active != NX_USB_ROLE_DEVICE ||
        state->transport != NX_USB_ROLE_DEVICE)
        return NX_ERROR_STATE;
    return nx_usb_fs_device_ops.stop(&state->device);
}

/** \brief A pulse cannot borrow cold metadata still needed by role exit. */
static bool
device_remote_storage(const nx_stm32_usb_otg_t* state,
                      const nx_usb_remote_wakeup_request_t* request) {
    const nx_stm32_usb_otg_config_t* config = state->config;
    const nx_stm32_platform_pin_config_t* pins;
    const nx_stm32_usb_endpoint_config_t* plan;
    size_t count;
    uint8_t pin_count;
    if (config->device_fs != NULL) {
        pins = config->device_fs->pins;
        plan = config->device_fs->plan;
        count = config->device_fs->endpoint_count;
        pin_count = 2U;
    } else {
        pins = config->device_hs->pins;
        plan = config->device_hs->plan;
        count = config->device_hs->endpoint_count;
        pin_count = config->device_hs->pin_count;
    }
    if (!outside(state, request, sizeof(*request)) ||
        overlaps(request, sizeof(*request), config->host->transport.channels,
                 (size_t)config->host->transport.channel_count *
                     sizeof(*config->host->transport.channels)) ||
        overlaps(request, sizeof(*request), config->source,
                 sizeof(*config->source)) ||
        overlaps(request, sizeof(*request), config->present,
                 sizeof(*config->present)) ||
        overlaps(request, sizeof(*request), config->overcurrent,
                 sizeof(*config->overcurrent)) ||
        overlaps(request, sizeof(*request), config->source->ops,
                 sizeof(*config->source->ops)) ||
        overlaps(request, sizeof(*request), config->present->ops,
                 sizeof(*config->present->ops)) ||
        overlaps(request, sizeof(*request), config->overcurrent->ops,
                 sizeof(*config->overcurrent->ops)) ||
        overlaps(request, sizeof(*request), pins,
                 (size_t)pin_count * sizeof(*pins)) ||
        overlaps(request, sizeof(*request), plan, count * sizeof(*plan)))
        return false;
    for (uint8_t i = 0U; i < pin_count; ++i)
        if (overlaps(request, sizeof(*request), pins[i].snapshot,
                     sizeof(*pins[i].snapshot)))
            return false;
    return true;
}

/** \brief A current Device face admits through its existing physical DCD. */
static nx_result_t
device_remote_wakeup(void* context, nx_usb_remote_wakeup_request_t* request) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!ready(state, NX_USB_ROLE_DEVICE))
        return NX_ERROR_STATE;
    if (!NEXUS_STM32F4_USB_REMOTE_WAKEUP)
        return NX_ERROR_UNSUPPORTED;
    if (!device_remote_storage(state, request))
        return NX_ERROR_INVALID;
    return nx_usb_fs_device_ops.remote_wakeup(&state->device, request);
}

/** \brief The same retained DCD keeps cancellation and pulse withdrawal. */
static nx_result_t
device_remote_cancel(void* context, nx_usb_remote_wakeup_request_t* request) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!active(state, NX_USB_ROLE_DEVICE))
        return NX_ERROR_STATE;
    if (!NEXUS_STM32F4_USB_REMOTE_WAKEUP)
        return NX_ERROR_UNSUPPORTED;
    if (!device_remote_storage(state, request))
        return NX_ERROR_INVALID;
    return nx_usb_fs_device_ops.remote_cancel(&state->device, request);
}

const nx_usb_device_ops_t nx_stm32_usb_otg_device_ops = {
    device_service, device_events, device_state,         device_address,
    device_wake,    device_stop,   device_remote_wakeup, device_remote_cancel};

/** \brief Endpoint wrappers select only raw directions of this composite. */
static bool endpoint_ready(const nx_stm32_usb_otg_endpoint_t* endpoint) {
    if (endpoint == NULL || !ready(endpoint->owner, NX_USB_ROLE_DEVICE) ||
        endpoint->endpoint == NULL)
        return false;
    const nx_usb_controller_t* c = &endpoint->owner->device;
    for (size_t i = 0U; i < c->endpoint_count; ++i)
        if (endpoint->endpoint == &c->endpoints[i])
            return endpoint->endpoint->controller == c;
    return false;
}

/** \brief Packet rejection cannot overwrite the inactive transport metadata. */
static nx_result_t endpoint_submit(void* context, nx_usb_request_t* packet) {
    nx_stm32_usb_otg_endpoint_t* endpoint = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!endpoint_ready(endpoint))
        return NX_ERROR_STATE;
    if (!outside(endpoint->owner, packet, sizeof(*packet)) ||
        (packet->length != 0U &&
         !outside(endpoint->owner,
                  endpoint->endpoint->in ? (const void*)packet->in
                                         : packet->out,
                  packet->length)))
        return NX_ERROR_INVALID;
    return nx_usb_fs_endpoint_ops.submit(endpoint->endpoint, packet);
}

/** \brief Cancellation retains the exact original packet until real SETTLED. */
static nx_result_t endpoint_cancel(void* context, nx_usb_request_t* packet) {
    nx_stm32_usb_otg_endpoint_t* endpoint = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!endpoint_ready(endpoint))
        return NX_ERROR_STATE;
    return nx_usb_fs_endpoint_ops.cancel(endpoint->endpoint, packet);
}

/** \brief STALL is valid only for the active raw endpoint owner. */
static nx_result_t endpoint_stall(void* context, bool stalled) {
    nx_stm32_usb_otg_endpoint_t* endpoint = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return endpoint_ready(endpoint)
               ? nx_usb_fs_endpoint_ops.stall(endpoint->endpoint, stalled)
               : NX_ERROR_STATE;
}

const nx_usb_endpoint_ops_t nx_stm32_usb_otg_endpoint_ops = {
    endpoint_submit, endpoint_cancel, endpoint_stall};

/** \brief Activation keeps the mature selected-direction and epoch checks. */
static nx_result_t activation(void* context, uint64_t epoch, uint8_t address,
                              bool enabled) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return ready(state, NX_USB_ROLE_DEVICE)
               ? nx_usb_fs_activation_ops.set(&state->device, epoch, address,
                                              enabled)
               : NX_ERROR_STATE;
}

/** \brief Setup rejection cannot stall a different physical role. */
static nx_result_t reject_setup(void* context, uint64_t epoch) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return ready(state, NX_USB_ROLE_DEVICE)
               ? nx_usb_fs_setup_decision_ops.reject(&state->device, epoch)
               : NX_ERROR_STATE;
}

const nx_usb_activation_ops_t nx_stm32_usb_otg_activation_ops = {activation};
const nx_usb_setup_decision_ops_t nx_stm32_usb_otg_setup_decision_ops = {
    reject_setup};

/** \brief Host target configuration still belongs to its selected HCD owner. */
static nx_result_t host_open(void* context, uint8_t channel,
                             const nx_usb_host_pipe_t* pipe) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return ready(state, NX_USB_ROLE_HOST)
               ? nx_stm32_usb_host_ops.open(&state->host, channel, pipe)
               : NX_ERROR_STATE;
}

/** \brief A closing channel cannot cross a role handover. */
static nx_result_t host_close(void* context, uint8_t channel) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return ready(state, NX_USB_ROLE_HOST)
               ? nx_stm32_usb_host_ops.close(&state->host, channel)
               : NX_ERROR_STATE;
}

/** \brief Host DMA and PIO requests cannot alias composite metadata. */
static nx_result_t host_submit(void* context, uint8_t channel,
                               nx_usb_host_request_t* packet) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!ready(state, NX_USB_ROLE_HOST))
        return NX_ERROR_STATE;
    if (!outside(state, packet, sizeof(*packet)) ||
        (packet->capacity != 0U &&
         !outside(state, packet->bytes, packet->capacity)))
        return NX_ERROR_INVALID;
    return nx_stm32_usb_host_ops.submit(&state->host, channel, packet);
}

/** \brief The original channel/request identity remains authoritative. */
static nx_result_t host_cancel(void* context, uint8_t channel,
                               nx_usb_host_request_t* packet) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return ready(state, NX_USB_ROLE_HOST)
               ? nx_stm32_usb_host_ops.cancel(&state->host, channel, packet)
               : NX_ERROR_STATE;
}

/** \brief Packet service is distinct from explicit outer role service. */
static nx_result_t host_service(void* context) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!active(state, NX_USB_ROLE_HOST))
        return NX_ERROR_STATE;
    return state->host.config == NULL
               ? NX_SUCCESS
               : nx_stm32_usb_host_ops.service(&state->host);
}

/** \brief A current Host snapshot cannot overwrite either transport owner. */
static nx_result_t host_state(void* context, nx_usb_host_state_t* output) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!active(state, NX_USB_ROLE_HOST))
        return NX_ERROR_STATE;
    return outside(state, output, sizeof(*output))
               ? nx_stm32_usb_host_ops.state(&state->host, output)
               : NX_ERROR_INVALID;
}

/** \brief Query output also preserves resources held for the inactive DCD. */
static bool host_query_external(const nx_stm32_usb_otg_t* state,
                                const nx_usb_host_storage_requirements_t* out) {
    const nx_stm32_usb_otg_config_t* config = state->config;
    const nx_stm32_usb_cold_config_t* fs = config->device_fs;
    const nx_stm32_usb_hs_cold_config_t* hs = config->device_hs;
    const nx_usb_endpoint_t* endpoints =
        fs != NULL ? fs->endpoints : hs->endpoints;
    const nx_stm32_usb_endpoint_config_t* plan =
        fs != NULL ? fs->plan : hs->plan;
    const nx_usb_event_t* events = fs != NULL ? fs->events : hs->events;
    const size_t count = fs != NULL ? fs->endpoint_count : hs->endpoint_count;
    const size_t capacity = fs != NULL ? fs->capacity : hs->capacity;
    const void* clock =
        fs != NULL ? (const void*)fs->clock : (const void*)hs->clock;
    const size_t clock_bytes =
        fs != NULL ? sizeof(*fs->clock) : sizeof(*hs->clock);
    const nx_stm32_platform_pin_config_t* pins =
        fs != NULL ? fs->pins : hs->pins;
    const uint8_t pin_count = fs != NULL ? 2U : hs->pin_count;
    if (!outside(state, out, sizeof(*out)) ||
        overlaps(out, sizeof(*out), endpoints, count * sizeof(*endpoints)) ||
        overlaps(out, sizeof(*out), plan, count * sizeof(*plan)) ||
        overlaps(out, sizeof(*out), events, capacity * sizeof(*events)) ||
        overlaps(out, sizeof(*out), clock, clock_bytes) ||
        overlaps(out, sizeof(*out), config->source, sizeof(*config->source)) ||
        overlaps(out, sizeof(*out), config->present,
                 sizeof(*config->present)) ||
        overlaps(out, sizeof(*out), config->overcurrent,
                 sizeof(*config->overcurrent)) ||
        overlaps(out, sizeof(*out), config->source->ops,
                 sizeof(*config->source->ops)) ||
        overlaps(out, sizeof(*out), config->present->ops,
                 sizeof(*config->present->ops)) ||
        overlaps(out, sizeof(*out), config->overcurrent->ops,
                 sizeof(*config->overcurrent->ops)) ||
        overlaps(out, sizeof(*out), pins, (size_t)pin_count * sizeof(*pins)))
        return false;
    for (uint8_t i = 0U; i < pin_count; ++i)
        if (overlaps(out, sizeof(*out), pins[i].snapshot,
                     sizeof(*pins[i].snapshot)))
            return false;
    return true;
}

/** \brief Pure Host geometry follows the same current-role admission gate. */
static nx_result_t
host_requirements(const void* context, nx_usb_host_pipe_id_t pipe,
                  size_t length, nx_usb_host_storage_requirements_t* output) {
    const nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!ready(state, NX_USB_ROLE_HOST))
        return NX_ERROR_STATE;
    return host_query_external(state, output)
               ? nx_stm32_usb_host_ops.requirements(&state->host, pipe, length,
                                                    output)
               : NX_ERROR_INVALID;
}

/** \brief Port reset preserves the existing idle and finite deadline checks. */
static nx_result_t host_reset(void* context, nx_time_us_t deadline) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return ready(state, NX_USB_ROLE_HOST)
               ? nx_stm32_usb_host_ops.reset(&state->host, deadline)
               : NX_ERROR_STATE;
}

/** \brief Suspend never changes the selected physical owner or Core clock. */
static nx_result_t host_suspend(void* context, bool suspended,
                                nx_time_us_t deadline) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    return ready(state, NX_USB_ROLE_HOST)
               ? nx_stm32_usb_host_ops.suspend(&state->host, suspended,
                                               deadline)
               : NX_ERROR_STATE;
}

/** \brief Protocol shutdown drains HCD while role retains VBUS and pin leases.
 */
static nx_result_t host_stop(void* context) {
    nx_stm32_usb_otg_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || !state->initialized || state->role.phase != 0U ||
        state->role.active != NX_USB_ROLE_HOST ||
        state->transport != NX_USB_ROLE_HOST)
        return NX_ERROR_STATE;
    return nx_stm32_usb_host_ops.stop(&state->host);
}

const nx_usb_host_ops_t nx_stm32_usb_otg_host_ops = {
    host_open,  host_close, host_submit,  host_cancel, host_service,
    host_state, host_reset, host_suspend, host_stop,   host_requirements};

/** \brief Bind OFF after checking the exact common plan and inactive source. */
nx_result_t nx_stm32_usb_otg_prepare(nx_stm32_usb_otg_t* state,
                                     const nx_stm32_usb_otg_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (state->initialized || state->transport != NX_USB_ROLE_OFF)
        return NX_ERROR_BUSY;
    if (state->generation == UINT64_MAX || !config_valid(state, config))
        return NX_ERROR_INVALID;
    bool enabled;
    nx_result_t result = read_signal(config->source, config->source_mask,
                                     config->source_active_high, &enabled);
    if (result != NX_SUCCESS || enabled)
        return result != NX_SUCCESS ? result : NX_ERROR_STATE;
    uint64_t generation = state->generation + 1U;
    uint64_t epoch = state->device.snapshot.epoch;
    *state = (nx_stm32_usb_otg_t){0};
    state->generation = generation;
    state->device.snapshot.epoch = epoch;
    state->config = config;
    state->device_face =
        (nx_usb_device_port_t){&nx_stm32_usb_otg_device_ops, state};
    state->host_face = (nx_usb_host_port_t){&nx_stm32_usb_otg_host_ops, state};
    state->role_config = (nx_usb_role_config_t){&role_ops, state, true, true};
    result = nx_usb_role_initialize(&state->role, &state->role_config);
    if (result == NX_SUCCESS)
        state->initialized = true;
    return result;
}

/** \brief Even a pending power wait is cancelled through the same OFF barrier.
 */
nx_result_t nx_stm32_usb_otg_close(nx_stm32_usb_otg_t* state) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!state->initialized)
        return NX_SUCCESS;
    if (!state->closing) {
        nx_time_us_t deadline;
        nx_result_t result = nx_deadline_finite_after(
            nx_time_now_us(), state->config->fault_timeout_us, &deadline);
        if (result == NX_SUCCESS)
            result = nx_usb_role_cancel(&state->role, deadline);
        if (result != NX_SUCCESS)
            return result;
        state->closing = true;
    }
    nx_result_t result = nx_usb_role_service(&state->role);
    if (state->role.phase != 0U || state->role.active != NX_USB_ROLE_OFF ||
        state->role.transport_owned || state->role.source_owned ||
        state->role.discharge_required || state->transport != NX_USB_ROLE_OFF)
        return result == NX_SUCCESS ? NX_ERROR_BUSY : result;
    state->initialized = false;
    state->config = NULL;
    /* Final reclamation leaves no old deadline or pending role control. */
    state->role = (nx_usb_role_t){0};
    return NX_SUCCESS;
}

/** \brief The selected driver owns IRQ publication until its cold close joins.
 */
void nx_stm32_usb_otg_irq(nx_stm32_usb_otg_t* state) {
    if (state == NULL || !state->initialized)
        return;
    if (state->transport == NX_USB_ROLE_DEVICE)
        nx_usb_controller_irq(&state->device);
    else if (state->transport == NX_USB_ROLE_HOST)
        nx_stm32_usb_host_irq(&state->host);
}
