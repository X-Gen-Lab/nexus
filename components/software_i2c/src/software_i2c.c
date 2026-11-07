/**
 * \file            software_i2c.c
 * \brief           Bounded GPIO two-wire clocking with explicit personalities
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/software_i2c.h"
#include "nexus/components/software_sccb.h"

#define PERSONALITY_I2C  1U
#define PERSONALITY_SCCB 2U

static const nx_i2c_ops_t port_ops;

/** \brief Admit finite aligned extents before typed storage is accessed. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Finite known control and payload intervals cannot alias. */
static bool overlap(const void* a, size_t a_bytes, const void* b,
                    size_t b_bytes) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return a_bytes > UINTPTR_MAX - first || b_bytes > UINTPTR_MAX - second ||
           (first < second + b_bytes && second < first + a_bytes);
}

/** \brief Both open-drain lines require release/write and actual input reads.
 */
static bool pin_valid(nx_software_i2c_pin_t pin) {
    return span(pin.port, sizeof(*pin.port), _Alignof(nx_gpio_port_t)) &&
           span(pin.port->ops, sizeof(*pin.port->ops),
                _Alignof(nx_gpio_ops_t)) &&
           pin.port->context != NULL && pin.port->ops->write != NULL &&
           pin.port->ops->read != NULL && pin.mask != 0U &&
           (pin.mask & (pin.mask - 1U)) == 0U;
}

/** \brief Configuration owns one immutable two-pin identity, not pad modes. */
static bool configuration_valid(const nx_software_i2c_t* owner,
                                const nx_software_i2c_config_t* config) {
    if (!span(config, sizeof(*config), _Alignof(nx_software_i2c_config_t)) ||
        overlap(owner, sizeof(*owner), config, sizeof(*config)) ||
        !pin_valid(config->scl) || !pin_valid(config->sda) ||
        config->clock.read == NULL || config->half_period_us < 5U ||
        config->half_period_us > 1000000U || config->poll_limit == 0U ||
        config->poll_limit > 1000000U) {
        return false;
    }
    const nx_gpio_port_t* pins[2] = {config->scl.port, config->sda.port};
    for (size_t i = 0U; i < 2U; ++i) {
        if (overlap(owner, sizeof(*owner), pins[i], sizeof(*pins[i])) ||
            overlap(owner, sizeof(*owner), pins[i]->ops,
                    sizeof(*pins[i]->ops)) ||
            overlap(config, sizeof(*config), pins[i], sizeof(*pins[i]))) {
            return false;
        }
    }
    return config->scl.port->context != config->sda.port->context ||
           (config->scl.mask & config->sda.mask) == 0U;
}

/** \brief A high bit releases an already admitted open-drain pad. */
static nx_result_t drive(nx_software_i2c_pin_t pin, bool released) {
    return nx_gpio_port_write(pin.port, released ? pin.mask : 0U,
                              released ? 0U : pin.mask);
}

/** \brief Sample physical input, never the driver's output latch. */
static nx_result_t sample(nx_software_i2c_pin_t pin, bool* high) {
    uint32_t value = 0U;
    nx_result_t result = nx_gpio_port_read(pin.port, &value);
    if (result == NX_SUCCESS) {
        *high = (value & pin.mask) != 0U;
    }
    return result;
}

/** \brief Clock discontinuity cannot be hidden by a later numerical recovery.
 */
static nx_result_t observe(nx_software_i2c_t* owner, nx_time_us_t deadline) {
    if (owner->clock_failed) {
        return NX_ERROR_IO;
    }
    nx_time_us_t now = owner->config->clock.read(owner->config->clock.context);
    if (now == NX_DEADLINE_NEVER || now < owner->last_clock) {
        owner->clock_failed = true;
        return NX_ERROR_IO;
    }
    owner->last_clock = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief Time and explicit observations independently bound every phase. */
static nx_result_t delay(nx_software_i2c_t* owner, nx_time_us_t deadline) {
    nx_result_t result = observe(owner, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    uint32_t duration = owner->config->half_period_us;
    if (duration >= NX_DEADLINE_NEVER - owner->last_clock) {
        return NX_ERROR_EXHAUSTED;
    }
    nx_time_us_t target = owner->last_clock + duration;
    for (uint32_t i = 0U; i < owner->config->poll_limit; ++i) {
        result = observe(owner, deadline);
        if (result != NX_SUCCESS || owner->last_clock >= target) {
            return result;
        }
    }
    return NX_ERROR_IO;
}

/** \brief A target may hold a released line low only within the caller budget.
 */
static nx_result_t wait_high(nx_software_i2c_t* owner,
                             nx_software_i2c_pin_t pin, nx_time_us_t deadline) {
    for (uint32_t i = 0U; i < owner->config->poll_limit; ++i) {
        nx_result_t result = observe(owner, deadline);
        bool high = false;
        if (result == NX_SUCCESS) {
            result = sample(pin, &high);
        }
        if (result != NX_SUCCESS || high) {
            return result;
        }
    }
    return NX_ERROR_BUSY;
}

/** \brief Releasing SCL is not observing high; clock stretching is explicit. */
static nx_result_t raise_scl(nx_software_i2c_t* owner, nx_time_us_t deadline) {
    nx_result_t result = drive(owner->config->scl, true);
    return result == NX_SUCCESS ? wait_high(owner, owner->config->scl, deadline)
                                : result;
}

/** \brief Attempt both output releases even if the first write failed. */
static nx_result_t release_lines(nx_software_i2c_t* owner) {
    nx_result_t data = drive(owner->config->sda, true);
    nx_result_t clock = drive(owner->config->scl, true);
    return data != NX_SUCCESS ? data : clock;
}

/** \brief Quiescent observation alone never proves a previously missing STOP.
 */
static nx_result_t released_idle(nx_software_i2c_t* owner) {
    nx_result_t result = release_lines(owner);
    bool clock = false;
    bool data = false;
    if (result == NX_SUCCESS) {
        result = sample(owner->config->scl, &clock);
    }
    if (result == NX_SUCCESS) {
        result = sample(owner->config->sda, &data);
    }
    return result != NX_SUCCESS ? result
           : clock && data      ? NX_SUCCESS
                                : NX_ERROR_BUSY;
}

/** \brief Observe the initial exclusive idle interval without scanning a
 * target. */
static nx_result_t wait_idle(nx_software_i2c_t* owner, nx_time_us_t deadline) {
    nx_result_t result = release_lines(owner);
    if (result == NX_SUCCESS) {
        result = wait_high(owner, owner->config->scl, deadline);
    }
    if (result == NX_SUCCESS) {
        result = wait_high(owner, owner->config->sda, deadline);
    }
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        bool clock = false;
        bool data = false;
        result = sample(owner->config->scl, &clock);
        if (result == NX_SUCCESS) {
            result = sample(owner->config->sda, &data);
        }
        if (result == NX_SUCCESS && (!clock || !data)) {
            result = NX_ERROR_BUSY;
        }
    }
    return result;
}

/** \brief START and repeated START share the same observed high-to-low edge. */
static nx_result_t start(nx_software_i2c_t* owner, nx_time_us_t deadline) {
    owner->stop_proved = false;
    nx_result_t result;
    if (!owner->bus_active) {
        result = wait_idle(owner, deadline);
    } else {
        result = drive(owner->config->sda, true);
        if (result == NX_SUCCESS) {
            result = delay(owner, deadline);
        }
        if (result == NX_SUCCESS) {
            result = raise_scl(owner, deadline);
        }
    }
    if (result == NX_SUCCESS) {
        bool data = false;
        result = sample(owner->config->sda, &data);
        if (result == NX_SUCCESS && !data) {
            result = NX_ERROR_ARBITRATION;
        }
    }
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        /* Even a reported GPIO failure may have created the START edge. */
        owner->bus_active = true;
        result = drive(owner->config->sda, false);
    }
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        result = drive(owner->config->scl, false);
    }
    return result;
}

/** \brief Sending a released one observes arbitration without forcing a STOP.
 */
static nx_result_t write_bit(nx_software_i2c_t* owner, bool bit,
                             bool check_arbitration, nx_time_us_t deadline) {
    nx_result_t result = drive(owner->config->sda, bit);
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        result = raise_scl(owner, deadline);
    }
    if (result == NX_SUCCESS && bit && check_arbitration) {
        bool data = false;
        result = sample(owner->config->sda, &data);
        if (result == NX_SUCCESS && !data) {
            result = NX_ERROR_ARBITRATION;
        }
    }
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        result = drive(owner->config->scl, false);
    }
    return result;
}

/** \brief Read one target-controlled bit with SCL high observed first. */
static nx_result_t read_bit(nx_software_i2c_t* owner, bool* bit,
                            nx_time_us_t deadline) {
    nx_result_t result = drive(owner->config->sda, true);
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        result = raise_scl(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        result = sample(owner->config->sda, bit);
    }
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        result = drive(owner->config->scl, false);
    }
    return result;
}

/** \brief Strict I2C observes ACK; SCCB masks its distinct Don't-Care bit. */
static nx_result_t write_byte(nx_software_i2c_t* owner, uint8_t byte,
                              bool require_ack, nx_time_us_t deadline) {
    for (unsigned i = 0U; i < 8U; ++i) {
        nx_result_t result =
            write_bit(owner, (byte & 0x80U) != 0U, true, deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        byte = (uint8_t)(byte << 1U);
    }
    if (!require_ack) {
        return write_bit(owner, true, false, deadline);
    }
    bool nack = false;
    nx_result_t result = read_bit(owner, &nack, deadline);
    return result == NX_SUCCESS && nack ? NX_ERROR_NACK : result;
}

/** \brief Publish only a complete byte and its controller ACK/NA phase. */
static nx_result_t read_byte(nx_software_i2c_t* owner, uint8_t* byte, bool last,
                             bool sccb, nx_time_us_t deadline) {
    uint8_t value = 0U;
    for (unsigned i = 0U; i < 8U; ++i) {
        bool bit = false;
        nx_result_t result = read_bit(owner, &bit, deadline);
        if (result != NX_SUCCESS) {
            return result;
        }
        value = (uint8_t)((uint32_t)value << 1U);
        if (bit) {
            value = (uint8_t)(value | 1U);
        }
    }
    nx_result_t result = write_bit(owner, last, !sccb && last, deadline);
    if (result == NX_SUCCESS) {
        *byte = value;
    }
    return result;
}

/** \brief One requested STOP, never an undisclosed bus-clear clock sequence. */
static nx_result_t stop_wire(nx_software_i2c_t* owner, nx_time_us_t deadline) {
    nx_result_t result = observe(owner, deadline);
    if (result == NX_SUCCESS) {
        result = drive(owner->config->scl, false);
    }
    if (result == NX_SUCCESS) {
        result = drive(owner->config->sda, false);
    }
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        result = raise_scl(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        result = drive(owner->config->sda, true);
    }
    if (result == NX_SUCCESS) {
        result = wait_high(owner, owner->config->sda, deadline);
    }
    if (result == NX_SUCCESS) {
        result = delay(owner, deadline);
    }
    if (result == NX_SUCCESS) {
        bool high = false;
        result = sample(owner->config->scl, &high);
        if (result == NX_SUCCESS && !high) {
            result = NX_ERROR_BUSY;
        }
    }
    if (result == NX_SUCCESS) {
        owner->bus_active = false;
        owner->stop_proved = true;
    }
    return result;
}

/** \brief Cleanup and primary outcomes remain distinct; all C spans release. */
static nx_result_t finish(nx_software_i2c_t* owner, nx_result_t primary,
                          nx_time_us_t deadline) {
    if (primary == NX_ERROR_ARBITRATION) {
        nx_result_t released = release_lines(owner);
        owner->cleanup_result =
            released == NX_SUCCESS ? NX_ERROR_STATE : released;
        owner->faulted = true;
        owner->stop_proved = false;
        return primary;
    }
    owner->cleanup_result = stop_wire(owner, deadline);
    if (owner->cleanup_result != NX_SUCCESS) {
        (void)release_lines(owner);
    }
    owner->faulted = owner->cleanup_result != NX_SUCCESS ||
                     (primary != NX_SUCCESS && primary != NX_ERROR_NACK);
    return primary == NX_SUCCESS ? owner->cleanup_result : primary;
}

/** \brief An admitted call has one live context and never an infinite wait. */
static nx_result_t admit(nx_software_i2c_t* owner, nx_time_us_t deadline,
                         uint8_t personality) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_software_i2c_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (owner->in_call) {
        return NX_ERROR_BUSY;
    }
    if (owner->personality != personality && owner->personality != 0U) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (!owner->initialized || owner->config == NULL || owner->faulted) {
        return NX_ERROR_STATE;
    }
    return NX_SUCCESS;
}

/** \brief Known bus/face/configuration extents cannot become payload storage.
 */
static bool separated(const nx_software_i2c_t* owner, const void* pointer,
                      size_t bytes) {
    const nx_software_i2c_config_t* config = owner->config;
    const nx_gpio_port_t* faces[2] = {config->scl.port, config->sda.port};
    if (overlap(pointer, bytes, owner, sizeof(*owner)) ||
        overlap(pointer, bytes, config, sizeof(*config))) {
        return false;
    }
    for (size_t i = 0U; i < 2U; ++i) {
        if (overlap(pointer, bytes, faces[i], sizeof(*faces[i])) ||
            overlap(pointer, bytes, faces[i]->ops, sizeof(*faces[i]->ops))) {
            return false;
        }
    }
    return true;
}

/** \brief Reject complete descriptor and mixed-direction overlaps before edges.
 */
static nx_result_t messages_valid(const nx_software_i2c_device_t* device,
                                  nx_i2c_message_t* messages, size_t count,
                                  size_t* transferred) {
    if (count == 0U ||
        !span(transferred, sizeof(*transferred), _Alignof(size_t))) {
        return NX_ERROR_INVALID;
    }
    if (count > NX_I2C_MAX_MESSAGES) {
        return NX_ERROR_UNSUPPORTED;
    }
    size_t descriptors = count * sizeof(*messages);
    const nx_software_i2c_t* owner = device->owner;
    if (!span(messages, descriptors, _Alignof(nx_i2c_message_t)) ||
        !separated(owner, messages, descriptors) ||
        !separated(owner, transferred, sizeof(*transferred)) ||
        overlap(messages, descriptors, device, sizeof(*device)) ||
        overlap(transferred, sizeof(*transferred), device, sizeof(*device)) ||
        overlap(messages, descriptors, transferred, sizeof(*transferred))) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < count; ++i) {
        const nx_i2c_message_t* item = &messages[i];
        if (item->length == 0U) {
            return NX_ERROR_INVALID;
        }
        if (item->length > NX_I2C_MAX_MESSAGE_BYTES) {
            return NX_ERROR_UNSUPPORTED;
        }
        if (!span(item->data, item->length, 1U) ||
            !separated(owner, item->data, item->length) ||
            overlap(item->data, item->length, device, sizeof(*device)) ||
            overlap(item->data, item->length, messages, descriptors) ||
            overlap(item->data, item->length, transferred,
                    sizeof(*transferred))) {
            return NX_ERROR_INVALID;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (item->read != messages[j].read &&
                overlap(item->data, item->length, messages[j].data,
                        messages[j].length)) {
                return NX_ERROR_INVALID;
            }
        }
    }
    return NX_SUCCESS;
}

/** \brief Exact seven-bit identity, never inferred from a bus name or scan. */
static bool device_valid(const nx_software_i2c_device_t* device) {
    return span(device, sizeof(*device), _Alignof(nx_software_i2c_device_t)) &&
           span(device->owner, sizeof(*device->owner),
                _Alignof(nx_software_i2c_t)) &&
           device->address >= 8U && device->address <= 119U &&
           !overlap(device, sizeof(*device), device->owner,
                    sizeof(*device->owner));
}

/** \brief Synchronous stack descriptors never enter an asynchronous backend. */
static nx_result_t transaction(void* context, nx_i2c_message_t* messages,
                               size_t count, nx_time_us_t deadline,
                               size_t* transferred) {
    const nx_software_i2c_device_t* device = context;
    if (!device_valid(device)) {
        return NX_ERROR_INVALID;
    }
    nx_software_i2c_t* owner = device->owner;
    nx_result_t result = admit(owner, deadline, PERSONALITY_I2C);
    if (result == NX_SUCCESS) {
        result = messages_valid(device, messages, count, transferred);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    *transferred = 0U;
    owner->in_call = true;
    result = observe(owner, deadline);
    if (result == NX_SUCCESS) {
        for (size_t i = 0U; i < count && result == NX_SUCCESS; ++i) {
            result = start(owner, deadline);
            if (result == NX_SUCCESS) {
                uint8_t address = (uint8_t)((uint32_t)device->address << 1U);
                result = write_byte(
                    owner, (uint8_t)(address | (messages[i].read ? 1U : 0U)),
                    true, deadline);
            }
            for (size_t j = 0U; j < messages[i].length && result == NX_SUCCESS;
                 ++j) {
                result = messages[i].read
                             ? read_byte(owner, &messages[i].data[j],
                                         j + 1U == messages[i].length, false,
                                         deadline)
                             : write_byte(owner, messages[i].data[j], true,
                                          deadline);
                if (result == NX_SUCCESS) {
                    ++*transferred;
                }
            }
        }
        result = finish(owner, result, deadline);
    } else if (owner->clock_failed) {
        owner->faulted = true;
    }
    owner->in_call = false;
    return result;
}

/** \brief Only the same physical owner may satisfy controller identity. */
static bool on_port(const void* context, const nx_i2c_port_t* port) {
    const nx_software_i2c_device_t* device = context;
    return device_valid(device) && port != NULL && port->ops == &port_ops &&
           port->context == device->owner &&
           device->owner->personality != PERSONALITY_SCCB;
}

static const nx_i2c_endpoint_ops_t endpoint_ops = {.transaction = transaction,
                                                   .on_port = on_port};

/** \brief Budget-free port stop cannot create or infer a missing protocol STOP.
 */
static nx_result_t port_stop(void* context) {
    nx_software_i2c_t* owner = context;
    if (!span(owner, sizeof(*owner), _Alignof(nx_software_i2c_t))) {
        return NX_ERROR_INVALID;
    }
    if (owner->in_call) {
        return NX_ERROR_BUSY;
    }
    if (owner->config == NULL) {
        return NX_SUCCESS;
    }
    if (owner->faulted || owner->bus_active || !owner->stop_proved) {
        return NX_ERROR_BUSY;
    }
    owner->in_call = true;
    nx_result_t result = released_idle(owner);
    owner->cleanup_result = result;
    if (result == NX_SUCCESS) {
        *owner = (nx_software_i2c_t){0};
    } else {
        owner->faulted = true;
        owner->in_call = false;
    }
    return result;
}

static const nx_i2c_ops_t port_ops = {.stop = port_stop};

/** \brief Common physical binding keeps strict and SCCB personalities distinct.
 */
static nx_result_t initialize(nx_software_i2c_t* owner,
                              const nx_software_i2c_config_t* config,
                              nx_time_us_t deadline, uint8_t personality) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_software_i2c_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (owner->in_call || owner->config != NULL || owner->initialized ||
        owner->faulted) {
        return NX_ERROR_BUSY;
    }
    if (!configuration_valid(owner, config)) {
        return NX_ERROR_INVALID;
    }
    owner->in_call = true;
    nx_time_us_t now = config->clock.read(config->clock.context);
    if (now == NX_DEADLINE_NEVER || now >= deadline) {
        owner->in_call = false;
        return now == NX_DEADLINE_NEVER ? NX_ERROR_IO : NX_ERROR_TIMEOUT;
    }
    owner->config = config;
    owner->last_clock = now;
    owner->personality = personality;
    nx_result_t result = wait_idle(owner, deadline);
    owner->initialized = result == NX_SUCCESS;
    owner->stop_proved = result == NX_SUCCESS;
    owner->faulted = result != NX_SUCCESS;
    owner->cleanup_result = result;
    owner->in_call = false;
    return result;
}

nx_result_t nx_software_i2c_init(nx_software_i2c_t* owner,
                                 const nx_software_i2c_config_t* config,
                                 nx_time_us_t deadline) {
    return initialize(owner, config, deadline, PERSONALITY_I2C);
}

nx_result_t nx_software_sccb_init(nx_software_i2c_t* owner,
                                  const nx_software_i2c_config_t* config,
                                  nx_time_us_t deadline) {
    return initialize(owner, config, deadline, PERSONALITY_SCCB);
}

nx_i2c_endpoint_t
nx_software_i2c_endpoint(const nx_software_i2c_device_t* device) {
    return device_valid(device) &&
                   device->owner->personality != PERSONALITY_SCCB
               ? (nx_i2c_endpoint_t){&endpoint_ops, (void*)device}
               : (nx_i2c_endpoint_t){0};
}

nx_i2c_port_t nx_software_i2c_port(nx_software_i2c_t* owner) {
    return (nx_i2c_port_t){&port_ops, owner};
}

nx_result_t nx_software_i2c_recover(nx_software_i2c_t* owner,
                                    nx_time_us_t deadline) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_software_i2c_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (owner->in_call) {
        return NX_ERROR_BUSY;
    }
    if (owner->config == NULL) {
        return NX_ERROR_STATE;
    }
    owner->in_call = true;
    owner->cleanup_result = stop_wire(owner, deadline);
    if (owner->cleanup_result == NX_SUCCESS) {
        owner->initialized = true;
        owner->faulted = false;
    } else {
        (void)release_lines(owner);
        owner->faulted = true;
    }
    owner->in_call = false;
    return owner->cleanup_result;
}

nx_result_t nx_software_i2c_stop(nx_software_i2c_t* owner,
                                 nx_time_us_t deadline) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_software_i2c_t)) ||
        deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (owner->in_call) {
        return NX_ERROR_BUSY;
    }
    if (owner->config == NULL) {
        return NX_SUCCESS;
    }
    if (owner->clock_failed && owner->stop_proved && !owner->bus_active) {
        owner->in_call = true;
        nx_result_t result = released_idle(owner);
        owner->in_call = false;
        if (result == NX_SUCCESS) {
            *owner = (nx_software_i2c_t){0};
        }
        return result;
    }
    nx_result_t result = nx_software_i2c_recover(owner, deadline);
    if (result == NX_SUCCESS) {
        *owner = (nx_software_i2c_t){0};
    }
    return result;
}

bool nx_software_i2c_faulted(const nx_software_i2c_t* owner) {
    return owner != NULL && owner->faulted;
}

nx_result_t nx_software_i2c_cleanup_result(const nx_software_i2c_t* owner) {
    return owner == NULL ? NX_ERROR_INVALID : owner->cleanup_result;
}

/** \brief An SCCB child has its own even eight-bit ID and physical personality.
 */
static nx_result_t sccb_admit(const nx_software_sccb_device_t* device,
                              nx_time_us_t deadline) {
    if (!span(device, sizeof(*device), _Alignof(nx_software_sccb_device_t)) ||
        !span(device->owner, sizeof(*device->owner),
              _Alignof(nx_software_i2c_t)) ||
        (device->write_id & 1U) != 0U ||
        overlap(device, sizeof(*device), device->owner,
                sizeof(*device->owner))) {
        return NX_ERROR_INVALID;
    }
    return admit(device->owner, deadline, PERSONALITY_SCCB);
}

/** \brief SCCB Don't-Care is selected explicitly, never inferred from NACK. */
static nx_result_t sccb_pointer(nx_software_i2c_t* owner, uint8_t id,
                                uint8_t reg, nx_time_us_t deadline) {
    nx_result_t result = start(owner, deadline);
    if (result == NX_SUCCESS) {
        result = write_byte(owner, id, false, deadline);
    }
    if (result == NX_SUCCESS) {
        result = write_byte(owner, reg, false, deadline);
    }
    return result;
}

nx_result_t
nx_software_sccb_write_register(const nx_software_sccb_device_t* device,
                                uint8_t reg, uint8_t value,
                                nx_time_us_t deadline) {
    nx_result_t result = sccb_admit(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_software_i2c_t* owner = device->owner;
    owner->in_call = true;
    result = observe(owner, deadline);
    if (result == NX_SUCCESS) {
        result = sccb_pointer(owner, device->write_id, reg, deadline);
        if (result == NX_SUCCESS) {
            result = write_byte(owner, value, false, deadline);
        }
        result = finish(owner, result, deadline);
    } else if (owner->clock_failed) {
        owner->faulted = true;
    }
    owner->in_call = false;
    return result;
}

nx_result_t
nx_software_sccb_read_register(const nx_software_sccb_device_t* device,
                               uint8_t reg, uint8_t* value,
                               nx_time_us_t deadline) {
    nx_result_t result = sccb_admit(device, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_software_i2c_t* owner = device->owner;
    if (!span(value, 1U, 1U) || !separated(owner, value, 1U) ||
        overlap(value, 1U, device, sizeof(*device))) {
        return NX_ERROR_INVALID;
    }
    owner->in_call = true;
    result = observe(owner, deadline);
    uint8_t received = 0U;
    if (result == NX_SUCCESS) {
        result = sccb_pointer(owner, device->write_id, reg, deadline);
        result = finish(owner, result, deadline);
        if (result == NX_SUCCESS) {
            result = start(owner, deadline);
            if (result == NX_SUCCESS) {
                result = write_byte(owner, (uint8_t)(device->write_id | 1U),
                                    false, deadline);
            }
            if (result == NX_SUCCESS) {
                result = read_byte(owner, &received, true, true, deadline);
            }
            result = finish(owner, result, deadline);
        }
    } else if (owner->clock_failed) {
        owner->faulted = true;
    }
    if (result == NX_SUCCESS) {
        *value = received;
    }
    owner->in_call = false;
    return result;
}
