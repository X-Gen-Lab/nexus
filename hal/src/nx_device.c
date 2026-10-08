/** Constant device discovery and task-owned, generation checked instances. */
#include "hal/base/nx_device.h"
#include "nx_device_internal.h"

/* ELF evidence reads the production compiler's descriptor layout. */
#if defined(_MSC_VER)
#pragma section(".nx_abi", read)
#endif
NX_USED NX_SECTION(".nx_abi") const uint32_t nx_device_descriptor_bytes = sizeof(nx_device_t);
#include "hal/nx_factory.h"
#include "arch/nx_arch.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>
static bool shutdown_admission_closed;
static uint32_t metadata_enter(void) { return nx_arch_irq_save().value; }
static void metadata_exit(uint32_t saved) { nx_arch_irq_restore((nx_arch_irq_state_t){saved}); }

#if NX_DEVICE_MANUAL_REGISTRATION
#ifndef NX_DEVICE_REGISTRY_SIZE
#define NX_DEVICE_REGISTRY_SIZE 320
#endif
static const nx_device_t* registry[NX_DEVICE_REGISTRY_SIZE];
static size_t registry_count;
#elif defined(__CC_ARM)
extern const nx_device_t Image$$nx_device$$Base NX_WEAK;
extern const nx_device_t Image$$nx_device$$Limit NX_WEAK;
#define REGISTRY_START ((uintptr_t)&Image$$nx_device$$Base)
#define REGISTRY_END ((uintptr_t)&Image$$nx_device$$Limit)
#else
extern const nx_device_t __nx_device_start[] NX_WEAK;
extern const nx_device_t __nx_device_end[] NX_WEAK;
#define REGISTRY_START ((uintptr_t)__nx_device_start)
#define REGISTRY_END ((uintptr_t)__nx_device_end)
#endif

nx_status_t nx_device_validate_registry_region(uintptr_t start, uintptr_t end,
                                               size_t* count) {
    if (!count) return NX_ERR_NULL_PTR;
    *count = 0;
    if (start == 0 && end == 0) return NX_OK;
    if (start == 0 || end == 0 || end < start ||
        start % sizeof(void*) != 0 || end % sizeof(void*) != 0 ||
        (end - start) % sizeof(nx_device_t) != 0)
        return NX_ERR_INVALID_STATE;
    *count = (size_t)((end - start) / sizeof(nx_device_t));
    return NX_OK;
}

/* All callers hold the short CPU critical region while reading mutable tables.
 * Integer linker spans avoid relational comparisons between distinct symbols. */
static nx_status_t registry_size(size_t* count) {
#if NX_DEVICE_MANUAL_REGISTRATION
    *count = registry_count;
    return NX_OK;
#else
    return nx_device_validate_registry_region(REGISTRY_START, REGISTRY_END, count);
#endif
}
static const nx_device_t* registry_at(size_t i) {
#if NX_DEVICE_MANUAL_REGISTRATION
    return registry[i];
#else
    return (const nx_device_t*)(REGISTRY_START + i * sizeof(nx_device_t));
#endif
}
static bool registered(const nx_device_t* dev) {
    size_t count;
    if (!dev || registry_size(&count) != NX_OK) return false;
    for (size_t i = 0; i < count; ++i) if (registry_at(i) == dev) return true;
    return false;
}
static nx_status_t find_locked(const char* name, const nx_device_t** out) {
    size_t count;
    nx_status_t status = registry_size(&count);
    if (status != NX_OK) return status;
    for (size_t i = 0; i < count; ++i) {
        const nx_device_t* dev = registry_at(i);
        if (dev->name && strcmp(dev->name, name) == 0) {
            *out = dev;
            return NX_OK;
        }
    }
    return NX_ERR_NOT_FOUND;
}

const nx_device_t* nx_device_find(const char* name) {
    if (!name) return NULL;
    const nx_device_t* out = NULL;
    uint32_t saved = metadata_enter();
    (void)find_locked(name, &out);
    metadata_exit(saved);
    return out;
}

nx_status_t nx_device_discover(const char* name, nx_device_class_t expected,
                               const nx_device_t** out) {
    if (!out) return NX_ERR_NULL_PTR;
    *out = NULL;
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (!name || expected <= NX_DEVICE_CLASS_UNKNOWN || expected >= NX_DEVICE_CLASS_COUNT)
        return NX_ERR_INVALID_PARAM;
    uint32_t saved = metadata_enter();
    const nx_device_t* dev = NULL;
    nx_status_t status = find_locked(name, &dev);
    if (status == NX_OK && dev->device_class != expected) status = NX_ERR_TYPE_MISMATCH;
    if (status == NX_OK && (!dev->state || (!dev->construct && !dev->device_init)))
        status = NX_ERR_INVALID_STATE;
    if (status == NX_OK) *out = dev;
    metadata_exit(saved);
    return status;
}

/* Construction is distinct from hardware open. Cache only a successful result;
 * constructors execute outside CPU masks and may report a precise failure. */
static nx_status_t construct_api(const nx_device_t* dev, bool typed, void** out) {
    *out = NULL;
    if (!dev || !dev->state || (!dev->construct && !dev->device_init)) return NX_ERR_INVALID_PARAM;
    uint32_t saved = metadata_enter();
    nx_device_config_state_t* state = dev->state;
    if (shutdown_admission_closed) {
        metadata_exit(saved);
        return NX_ERR_BUSY;
    }
    if (!typed && state->phase != NX_DEVICE_CLOSED) {
        metadata_exit(saved);
        return NX_ERR_BUSY;
    }
    if (state->initializing) {
        metadata_exit(saved);
        return NX_ERR_BUSY;
    }
    if (state->initialized) {
        *out = state->api;
        metadata_exit(saved);
        return *out ? NX_OK : NX_ERR_INVALID_STATE;
    }
    state->initializing = true;
    metadata_exit(saved);
    void* api = NULL;
    nx_status_t status;
    if (dev->construct) status = dev->construct(dev, &api);
    else {
        api = dev->device_init(dev);
        status = api ? NX_OK : NX_ERR_GENERIC;
    }
    if (status == NX_OK && !api) status = NX_ERR_INVALID_STATE;
    saved = metadata_enter();
    if (status == NX_OK) {
        state->api = api;
        state->initialized = true;
        *out = api;
    }
    state->init_res = (uint8_t)status;
    state->last_status = status;
    state->initializing = false;
    metadata_exit(saved);
    return status;
}
void* nx_device_init(const nx_device_t* dev) {
    if (nx_arch_in_isr()) return NULL;
    void* api = NULL;
    return construct_api(dev, false, &api) == NX_OK ? api : NULL;
}
void* nx_device_get(const char* name) { return nx_device_init(nx_device_find(name)); }
void* nx_device_get_checked(const char* name, nx_device_class_t expected) {
    const nx_device_t* dev = NULL;
    return nx_device_discover(name, expected, &dev) == NX_OK ? nx_device_init(dev) : NULL;
}

/* Casts are confined to this class-checked driver bridge, never derived from a
 * name. Custom typed constructors may provide their own lifecycle getter. */
#define LIFECYCLE_CASE(cls, type) \
    case cls: { type* value = api; return value->get_lifecycle ? value->get_lifecycle(value) : NULL; }
static nx_lifecycle_t* lifecycle(const nx_device_t* dev, void* api) {
    if (!api) return NULL;
    if (dev->get_lifecycle) return dev->get_lifecycle(api);
    switch (dev->device_class) {
        case NX_DEVICE_CLASS_GPIO: {
            nx_gpio_read_write_t* gpio = api;
            return gpio->write.get_lifecycle ? gpio->write.get_lifecycle(&gpio->write) : NULL;
        }
        LIFECYCLE_CASE(NX_DEVICE_CLASS_GPIO_READ, nx_gpio_read_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_GPIO_WRITE, nx_gpio_write_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_UART, nx_uart_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_SPI, nx_spi_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_I2C, nx_i2c_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_TIMER, nx_timer_base_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_TIMER_PWM, nx_timer_pwm_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_TIMER_ENCODER, nx_timer_encoder_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_ADC, nx_adc_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_ADC_BUFFER, nx_adc_buffer_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_DAC, nx_dac_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_FLASH, nx_internal_flash_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_CAN, nx_can_bus_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_USB, nx_usb_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_RTC, nx_rtc_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_CRC, nx_crc_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_SDIO, nx_sdio_t)
        LIFECYCLE_CASE(NX_DEVICE_CLASS_WATCHDOG, nx_watchdog_t)
        default: return NULL;
    }
}
#undef LIFECYCLE_CASE

nx_status_t nx_device_open(const char* name, nx_device_class_t expected,
                           uintptr_t owner, nx_device_ref_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (!owner) return NX_ERR_INVALID_PARAM;
    const nx_device_t* dev = NULL;
    nx_status_t status = nx_device_discover(name, expected, &dev);
    if (status != NX_OK) return status;
    uint32_t saved = metadata_enter();
    if (!registered(dev)) {
        metadata_exit(saved);
        return NX_ERR_NOT_FOUND;
    }
    nx_device_config_state_t* state = dev->state;
    if (shutdown_admission_closed || state->phase != NX_DEVICE_CLOSED || state->initializing) {
        metadata_exit(saved);
        return NX_ERR_BUSY;
    }
    if (state->generation == UINT64_MAX) {
        metadata_exit(saved);
        return NX_ERR_NO_RESOURCE;
    }
    ++state->generation;
    state->owner = owner;
    state->last_ticket = 0;
    state->phase = NX_DEVICE_OPENING;
    metadata_exit(saved);
    void* api = NULL;
    status = construct_api(dev, true, &api);
    nx_lifecycle_t* life = status == NX_OK ? lifecycle(dev, api) : NULL;
    bool attempted_init = false;
    if (status == NX_OK && (!life || !life->init || !life->deinit || !life->get_state))
        status = NX_ERR_NOT_SUPPORTED;
    if (status == NX_OK && life->get_state(life) != NX_DEV_STATE_UNINITIALIZED) {
        status = NX_ERR_RESOURCE_BUSY;
    }
    if (status == NX_OK) {
        attempted_init = true;
        status = life->init(life);
        if (status == NX_OK && life->get_state(life) != NX_DEV_STATE_RUNNING)
            status = NX_ERR_NOT_READY;
    }
    nx_status_t cleanup = NX_OK;
    if (status != NX_OK && attempted_init) {
        cleanup = life->deinit(life);
        if (cleanup == NX_ERR_NOT_INIT) cleanup = NX_OK;
    }
    saved = metadata_enter();
    state->last_status = status;
    if (status == NX_OK) {
        state->phase = NX_DEVICE_OPEN;
        *out = (nx_device_ref_t){dev, owner, state->generation, expected};
    } else if (cleanup != NX_OK) {
        state->phase = NX_DEVICE_RECOVERY_REQUIRED;
        state->last_status = cleanup;
        status = cleanup;
    } else {
        state->phase = NX_DEVICE_CLOSED;
        state->owner = 0;
    }
    metadata_exit(saved);
    return status;
}

static nx_status_t validate_ref(nx_device_ref_t ref, nx_device_config_state_t** out) {
    if (!registered(ref.descriptor)) return NX_ERR_NOT_FOUND;
    nx_device_config_state_t* state = ref.descriptor->state;
    if (!state || !ref.owner || !ref.generation || state->owner != ref.owner ||
        state->generation != ref.generation || state->phase != NX_DEVICE_OPEN)
        return NX_ERR_INVALID_STATE;
    if (ref.device_class != ref.descriptor->device_class) return NX_ERR_TYPE_MISMATCH;
    *out = state;
    return NX_OK;
}
nx_status_t nx_device_query(nx_device_ref_t ref, nx_device_caps_t* out) {
    if (!out) return NX_ERR_NULL_PTR;
    memset(out, 0, sizeof(*out));
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    uint32_t saved = metadata_enter();
    nx_device_config_state_t* state = NULL;
    nx_status_t status = validate_ref(ref, &state);
    if (status == NX_OK) {
        out->device_class = ref.descriptor->device_class;
        out->flags = ref.descriptor->capabilities;
    }
    void* api = status == NX_OK ? state->api : NULL;
    if (status == NX_OK) {
        if (state->active_calls) status = NX_ERR_BUSY;
        else ++state->active_calls;
    }
    metadata_exit(saved);
    if (status == NX_OK) {
        if (ref.device_class == NX_DEVICE_CLASS_UART) {
            nx_uart_t* uart = api;
            nx_uart_operations_t* operations = uart->get_operations ? uart->get_operations(uart) : NULL;
            if (operations && operations->submit && operations->poll) out->flags |= NX_DEVICE_CAP_UART_OPERATIONS;
            if (operations && operations->cancel) out->flags |= NX_DEVICE_CAP_UART_CANCEL;
            if (operations && operations->receive_event) out->flags |= NX_DEVICE_CAP_UART_RX_EVENTS;
        } else if (ref.device_class == NX_DEVICE_CLASS_SPI) {
            nx_spi_bus_t* bus = api;
            if (bus->open_device && bus->close_device) out->flags |= NX_DEVICE_CAP_SPI_DEVICES;
        }
        saved = metadata_enter();
        --state->active_calls;
        metadata_exit(saved);
    }
    return status;
}
nx_status_t nx_device_close(nx_device_ref_t ref) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    uint32_t saved = metadata_enter();
    nx_device_config_state_t* state = NULL;
    nx_status_t status = validate_ref(ref, &state);
    if (status == NX_OK && (state->active_calls || state->active_ticket || state->child_refs)) status = NX_ERR_BUSY;
    if (status != NX_OK) {
        metadata_exit(saved);
        return status;
    }
    state->phase = NX_DEVICE_CLOSING;
    void* api = state->api;
    metadata_exit(saved);
    nx_lifecycle_t* life = lifecycle(ref.descriptor, api);
    status = life && life->deinit ? life->deinit(life) : NX_ERR_NOT_SUPPORTED;
    saved = metadata_enter();
    state->last_status = status;
    if (status == NX_OK) {
        state->phase = NX_DEVICE_CLOSED;
        state->owner = 0;
    } else {
        /* Driver still owns hardware: retain the reference for retry. */
        state->phase = NX_DEVICE_OPEN;
    }
    metadata_exit(saved);
    return status;
}
nx_status_t nx_device_recover(const char* name, uintptr_t owner) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (!name || !owner) return NX_ERR_INVALID_PARAM;
    uint32_t saved = metadata_enter();
    const nx_device_t* dev = NULL;
    nx_status_t status = find_locked(name, &dev);
    if (status != NX_OK) { metadata_exit(saved); return status; }
    nx_device_config_state_t* state = dev->state;
    if (!state || state->phase != NX_DEVICE_RECOVERY_REQUIRED || state->owner != owner) {
        metadata_exit(saved);
        return NX_ERR_INVALID_STATE;
    }
    state->phase = NX_DEVICE_CLOSING;
    metadata_exit(saved);
    nx_lifecycle_t* life = lifecycle(dev, state->api);
    status = life && life->deinit ? life->deinit(life) : NX_ERR_NOT_SUPPORTED;
    if (status == NX_ERR_NOT_INIT) status = NX_OK;
    saved = metadata_enter();
    state->last_status = status;
    state->phase = status == NX_OK ? NX_DEVICE_CLOSED : NX_DEVICE_RECOVERY_REQUIRED;
    if (status == NX_OK) state->owner = 0;
    metadata_exit(saved);
    return status;
}

/* Pin while driver code executes, without holding CPU masks. Close can never
 * deinitialize a pinned callback and a copied old reference cannot address a
 * new owner. This is an internal bridge, not a raw-pointer product API. */
nx_status_t nx_device_dispatch_pin(nx_device_ref_t ref, nx_device_class_t expected,
                                   bool serialize, void** api) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    uint32_t saved = metadata_enter();
    nx_device_config_state_t* state = NULL;
    nx_status_t status = validate_ref(ref, &state);
    if (status == NX_OK && ref.device_class != expected) status = NX_ERR_TYPE_MISMATCH;
    if (status == NX_OK && serialize && state->active_calls) status = NX_ERR_BUSY;
    if (status == NX_OK && state->active_calls == UINT32_MAX) status = NX_ERR_NO_RESOURCE;
    if (status == NX_OK) { ++state->active_calls; *api = state->api; }
    metadata_exit(saved);
    return status;
}
void nx_device_dispatch_unpin(nx_device_ref_t ref) {
    uint32_t saved = metadata_enter();
    --ref.descriptor->state->active_calls;
    metadata_exit(saved);
}
static nx_status_t pin(nx_device_ref_t ref, nx_device_class_t expected, void** api) {
    return nx_device_dispatch_pin(ref, expected, true, api);
}
static void unpin(nx_device_ref_t ref) { nx_device_dispatch_unpin(ref); }
static nx_status_t gpio_pin(nx_device_ref_t ref, void** api) {
    if (ref.device_class != NX_DEVICE_CLASS_GPIO && ref.device_class != NX_DEVICE_CLASS_GPIO_READ &&
        ref.device_class != NX_DEVICE_CLASS_GPIO_WRITE) return NX_ERR_TYPE_MISMATCH;
    nx_status_t status = pin(ref, ref.device_class, api);
    if (status != NX_OK) return status;
    nx_lifecycle_t* life = lifecycle(ref.descriptor, *api);
    nx_device_state_t hardware = life && life->get_state ? life->get_state(life) : NX_DEV_STATE_ERROR;
    if (hardware != NX_DEV_STATE_RUNNING) {
        status = hardware == NX_DEV_STATE_SUSPENDED ? NX_ERR_SUSPENDED :
            hardware == NX_DEV_STATE_UNINITIALIZED ? NX_ERR_NOT_INIT : NX_ERR_NOT_READY;
        unpin(ref);
    }
    return status;
}
nx_status_t nx_device_gpio_read(nx_device_ref_t ref, uint8_t* value) {
    if (!value) return NX_ERR_NULL_PTR;
    *value = 0;
    if (ref.device_class == NX_DEVICE_CLASS_GPIO_WRITE) return NX_ERR_NOT_SUPPORTED;
    void* api = NULL;
    nx_status_t status = gpio_pin(ref, &api);
    if (status != NX_OK) return status;
    nx_gpio_read_t* gpio = ref.device_class == NX_DEVICE_CLASS_GPIO ? &((nx_gpio_t*)api)->read : api;
    if (gpio->read) *value = gpio->read(gpio); else status = NX_ERR_NOT_SUPPORTED;
    unpin(ref);
    return status;
}
nx_status_t nx_device_gpio_write(nx_device_ref_t ref, uint8_t value) {
    if (value > 1) return NX_ERR_INVALID_PARAM;
    if (ref.device_class == NX_DEVICE_CLASS_GPIO_READ) return NX_ERR_NOT_SUPPORTED;
    void* api = NULL;
    nx_status_t status = gpio_pin(ref, &api);
    if (status != NX_OK) return status;
    nx_gpio_write_t* gpio = ref.device_class == NX_DEVICE_CLASS_GPIO ? &((nx_gpio_t*)api)->write : api;
    if (gpio->write) gpio->write(gpio, value); else status = NX_ERR_NOT_SUPPORTED;
    unpin(ref);
    return status;
}
nx_status_t nx_device_gpio_toggle(nx_device_ref_t ref) {
    if (ref.device_class == NX_DEVICE_CLASS_GPIO_READ) return NX_ERR_NOT_SUPPORTED;
    void* api = NULL;
    nx_status_t status = gpio_pin(ref, &api);
    if (status != NX_OK) return status;
    nx_gpio_write_t* gpio = ref.device_class == NX_DEVICE_CLASS_GPIO ? &((nx_gpio_t*)api)->write : api;
    if (gpio->toggle) gpio->toggle(gpio); else status = NX_ERR_NOT_SUPPORTED;
    unpin(ref);
    return status;
}

static nx_uart_operations_t* uart_operations(void* api) {
    nx_uart_t* uart = api;
    return uart->get_operations ? uart->get_operations(uart) : NULL;
}
nx_status_t nx_device_uart_submit(nx_device_ref_t ref, const uint8_t* data,
                                  size_t len, uint32_t timeout_ms,
                                  nx_uart_ticket_t* ticket) {
    if (!ticket) return NX_ERR_NULL_PTR;
    ticket->sequence = 0;
    if (!data || !len || timeout_ms > INT32_MAX) return NX_ERR_INVALID_PARAM;
    void* api = NULL;
    nx_status_t status = pin(ref, NX_DEVICE_CLASS_UART, &api);
    if (status != NX_OK) return status;
    uint32_t saved = metadata_enter();
    bool active = ref.descriptor->state->active_ticket != 0;
    metadata_exit(saved);
    nx_uart_operations_t* ops = uart_operations(api);
    if (active) status = NX_ERR_BUSY;
    else if (!ops || !ops->submit || !ops->poll) status = NX_ERR_NOT_SUPPORTED;
    else status = ops->submit(ops, data, len, timeout_ms, ticket);
    if (status == NX_OK) {
        /* A port must never admit a zero ticket. Quarantine rather than close
         * hardware which may have borrowed storage under a broken contract. */
        saved = metadata_enter();
        ref.descriptor->state->active_ticket = ticket->sequence ? ticket->sequence : UINT64_MAX;
        ref.descriptor->state->last_ticket = ticket->sequence;
        metadata_exit(saved);
        if (!ticket->sequence) status = NX_ERR_INVALID_STATE;
    } else ticket->sequence = 0;
    unpin(ref);
    return status;
}
nx_status_t nx_device_uart_poll(nx_device_ref_t ref, nx_uart_ticket_t ticket,
                                nx_uart_result_t* result) {
    if (!result) return NX_ERR_NULL_PTR;
    memset(result, 0, sizeof(*result));
    result->status = NX_ERR_INVALID_STATE;
    if (!ticket.sequence) return NX_ERR_INVALID_PARAM;
    void* api = NULL;
    nx_status_t status = pin(ref, NX_DEVICE_CLASS_UART, &api);
    if (status != NX_OK) return status;
    uint32_t saved = metadata_enter();
    uint64_t active = ref.descriptor->state->active_ticket;
    uint64_t last = ref.descriptor->state->last_ticket;
    metadata_exit(saved);
    nx_uart_operations_t* ops = uart_operations(api);
    if (last != ticket.sequence || (active && active != ticket.sequence)) status = NX_ERR_INVALID_STATE;
    else if (!ops || !ops->poll) status = NX_ERR_NOT_SUPPORTED;
    else status = ops->poll(ops, ticket, result);
    if (status == NX_OK && result->settled && active == ticket.sequence) {
        saved = metadata_enter();
        ref.descriptor->state->active_ticket = 0;
        metadata_exit(saved);
    }
    unpin(ref);
    return status;
}
nx_status_t nx_device_uart_cancel(nx_device_ref_t ref, nx_uart_ticket_t ticket) {
    if (!ticket.sequence) return NX_ERR_INVALID_PARAM;
    void* api = NULL;
    nx_status_t status = pin(ref, NX_DEVICE_CLASS_UART, &api);
    if (status != NX_OK) return status;
    uint32_t saved = metadata_enter();
    uint64_t active = ref.descriptor->state->active_ticket;
    uint64_t last = ref.descriptor->state->last_ticket;
    metadata_exit(saved);
    nx_uart_operations_t* ops = uart_operations(api);
    if (last != ticket.sequence || (active && active != ticket.sequence)) status = NX_ERR_INVALID_STATE;
    else if (!ops || !ops->cancel) status = NX_ERR_NOT_SUPPORTED;
    else status = ops->cancel(ops, ticket);
    if (status == NX_OK && active == ticket.sequence) {
        saved = metadata_enter();
        ref.descriptor->state->active_ticket = 0;
        metadata_exit(saved);
    }
    unpin(ref);
    return status;
}
nx_status_t nx_device_uart_receive_event(nx_device_ref_t ref, nx_uart_rx_event_t* event) {
    if (!event) return NX_ERR_NULL_PTR;
    memset(event, 0, sizeof(*event));
    void* api = NULL;
    nx_status_t status = pin(ref, NX_DEVICE_CLASS_UART, &api);
    if (status != NX_OK) return status;
    nx_uart_operations_t* ops = uart_operations(api);
    status = ops && ops->receive_event ? ops->receive_event(ops, event) : NX_ERR_NOT_SUPPORTED;
    unpin(ref);
    return status;
}
nx_status_t nx_device_shutdown_check(void) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    uint32_t saved = metadata_enter();
    size_t count;
    nx_status_t status = registry_size(&count);
    if (status == NX_OK) for (size_t i = 0; i < count; ++i) {
        nx_device_config_state_t* state = registry_at(i)->state;
        if (state && (state->phase != NX_DEVICE_CLOSED || state->initializing)) {
            status = NX_ERR_BUSY;
            break;
        }
    }
    metadata_exit(saved);
    return status;
}
nx_status_t nx_device_shutdown_begin(void) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    uint32_t saved = metadata_enter();
    nx_status_t status = shutdown_admission_closed ? NX_ERR_BUSY : nx_device_shutdown_check();
    if (status == NX_OK) shutdown_admission_closed = true;
    metadata_exit(saved);
    return status;
}
void nx_device_shutdown_end(void) {
    if (nx_arch_in_isr()) return;
    uint32_t saved = metadata_enter();
    shutdown_admission_closed = false;
    metadata_exit(saved);
}

#if NX_DEVICE_MANUAL_REGISTRATION
nx_status_t nx_device_register(const nx_device_t* dev) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    if (!dev) return NX_ERR_NULL_PTR;
    uint32_t saved = metadata_enter();
    if (shutdown_admission_closed) { metadata_exit(saved); return NX_ERR_BUSY; }
    for (size_t i = 0; i < registry_count; ++i) {
        if (registry[i] == dev) { metadata_exit(saved); return NX_OK; }
        if (dev->name && registry[i]->name && strcmp(dev->name, registry[i]->name) == 0) {
            metadata_exit(saved);
            return NX_ERR_ALREADY_INIT;
        }
    }
    if (registry_count == NX_DEVICE_REGISTRY_SIZE) { metadata_exit(saved); return NX_ERR_NO_MEMORY; }
    registry[registry_count++] = dev;
    metadata_exit(saved);
    return NX_OK;
}
nx_status_t nx_device_registry_reset(void) {
    if (nx_arch_in_isr()) return NX_ERR_CONTEXT;
    uint32_t saved = metadata_enter();
    if (shutdown_admission_closed) { metadata_exit(saved); return NX_ERR_BUSY; }
    for (size_t i = 0; i < registry_count; ++i) {
        nx_device_config_state_t* state = registry[i]->state;
        if (state && (state->phase != NX_DEVICE_CLOSED || state->initializing)) {
            metadata_exit(saved);
            return NX_ERR_BUSY;
        }
    }
    registry_count = 0;
    metadata_exit(saved);
    return NX_OK;
}
void nx_device_clear_all(void) { (void)nx_device_registry_reset(); }
#endif
