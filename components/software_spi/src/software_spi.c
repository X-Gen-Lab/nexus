/**
 * \file            software_spi.c
 * \brief           Bounded GPIO SPI with explicit failed-withdrawal ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/software_spi.h"

/** \brief Validate aligned finite spans before typed access or pointer sums. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t address = (uintptr_t)pointer;
    return pointer != NULL && address % alignment == 0U &&
           bytes <= UINTPTR_MAX - address;
}

/** \brief Compare only admitted finite extents; opaque GPIO storage is
 * external. */
static bool overlap(const void* a, size_t a_bytes, const void* b,
                    size_t b_bytes) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return first < second + b_bytes && second < first + a_bytes;
}

/** \brief A pin is exactly one bit on an immutable, usable GPIO face. */
static bool pin_valid(nx_software_spi_pin_t pin, bool output) {
    return span(pin.port, sizeof(*pin.port), _Alignof(nx_gpio_port_t)) &&
           pin.port->ops != NULL && pin.port->context != NULL &&
           pin.mask != 0U && (pin.mask & (pin.mask - 1U)) == 0U &&
           (output ? pin.port->ops->write != NULL
                   : pin.port->ops->read != NULL);
}

/** \brief Reject known control aliases and duplicate electrical pin identities.
 */
static bool configuration_valid(const nx_software_spi_t* owner,
                                const nx_software_spi_config_t* config) {
    if (!span(config, sizeof(*config), _Alignof(nx_software_spi_config_t)) ||
        overlap(owner, sizeof(*owner), config, sizeof(*config)) ||
        config->clock.read == NULL || config->half_period_us == 0U ||
        config->half_period_us > 1000000U || config->select_setup_us == 0U ||
        config->select_setup_us > 1000000U || config->select_hold_us == 0U ||
        config->select_hold_us > 1000000U || config->poll_limit == 0U ||
        config->poll_limit > 1000000U || config->max_bytes == 0U ||
        config->max_bytes > 256U) {
        return false;
    }
    nx_software_spi_pin_t pins[4] = {config->sck, config->mosi, config->miso,
                                     config->cs};
    for (size_t i = 0U; i < 4U; ++i) {
        if (!pin_valid(pins[i], i != 2U) ||
            overlap(pins[i].port, sizeof(*pins[i].port), owner,
                    sizeof(*owner)) ||
            overlap(pins[i].port, sizeof(*pins[i].port), config,
                    sizeof(*config))) {
            return false;
        }
        for (size_t j = 0U; j < i; ++j) {
            if (pins[i].port->context == pins[j].port->context &&
                (pins[i].mask & pins[j].mask) != 0U) {
                return false;
            }
        }
    }
    return true;
}

/** \brief A write never changes pad modes or acquires a second GPIO lease. */
static nx_result_t set_pin(nx_software_spi_pin_t pin, bool high) {
    return nx_gpio_port_write(pin.port, high ? pin.mask : 0U,
                              high ? 0U : pin.mask);
}

/** \brief Attempt every idle output even when an earlier withdrawal fails. */
static nx_result_t idle(nx_software_spi_t* owner) {
    const nx_software_spi_config_t* config = owner->config;
    nx_result_t cs = set_pin(config->cs, true);
    nx_result_t sck = set_pin(config->sck, false);
    nx_result_t mosi = set_pin(config->mosi, false);
    return cs != NX_SUCCESS ? cs : sck != NX_SUCCESS ? sck : mosi;
}

/** \brief Observe one live, nondecreasing finite clock in the admitted domain.
 */
static nx_result_t observe(nx_software_spi_t* owner, nx_time_us_t deadline) {
    nx_time_us_t now = owner->config->clock.read(owner->config->clock.context);
    if (now == NX_DEADLINE_NEVER || now < owner->last_clock) {
        return NX_ERROR_IO;
    }
    owner->last_clock = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}

/** \brief Both elapsed time and finite observation count bound each wait. */
static nx_result_t delay(nx_software_spi_t* owner, uint32_t duration,
                         nx_time_us_t deadline) {
    nx_result_t result = observe(owner, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
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

nx_result_t nx_software_spi_init(nx_software_spi_t* owner,
                                 const nx_software_spi_config_t* config) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_software_spi_t))) {
        return NX_ERROR_INVALID;
    }
    if (owner->in_call || owner->initialized || owner->config != NULL ||
        owner->faulted) {
        return NX_ERROR_BUSY;
    }
    if (!configuration_valid(owner, config)) {
        return NX_ERROR_INVALID;
    }
    /* Do not bind an already exhausted domain or touch any pin on rejection. */
    owner->in_call = true;
    nx_time_us_t now = config->clock.read(config->clock.context);
    if (now == NX_DEADLINE_NEVER) {
        owner->in_call = false;
        return NX_ERROR_IO;
    }
    owner->config = config;
    owner->last_clock = now;
    nx_result_t result = idle(owner);
    owner->initialized = result == NX_SUCCESS;
    owner->faulted = result != NX_SUCCESS;
    owner->in_call = false;
    return result;
}

/** \brief Known payload/output extents must not alias control or GPIO faces. */
static bool separated(const nx_software_spi_t* owner, const void* pointer,
                      size_t bytes) {
    const nx_software_spi_config_t* config = owner->config;
    const nx_gpio_port_t* faces[4] = {config->sck.port, config->mosi.port,
                                      config->miso.port, config->cs.port};
    if (overlap(pointer, bytes, owner, sizeof(*owner)) ||
        overlap(pointer, bytes, config, sizeof(*config))) {
        return false;
    }
    for (size_t i = 0U; i < 4U; ++i) {
        if (overlap(pointer, bytes, faces[i], sizeof(*faces[i]))) {
            return false;
        }
    }
    return true;
}

/** \brief Reject spans before the first output count store or physical edge. */
static bool buffers_valid(const nx_software_spi_t* owner, const uint8_t* tx,
                          uint8_t* rx, size_t length, size_t* transferred) {
    if (!span(transferred, sizeof(*transferred), _Alignof(size_t)) ||
        !separated(owner, transferred, sizeof(*transferred)) || length == 0U ||
        length > owner->config->max_bytes || (tx == NULL && rx == NULL)) {
        return false;
    }
    if (tx != NULL &&
        (!span(tx, length, 1U) || !separated(owner, tx, length) ||
         overlap(tx, length, transferred, sizeof(*transferred)))) {
        return false;
    }
    if (rx != NULL &&
        (!span(rx, length, 1U) || !separated(owner, rx, length) ||
         overlap(rx, length, transferred, sizeof(*transferred)))) {
        return false;
    }
    return tx == NULL || rx == NULL || tx == rx ||
           !overlap(tx, length, rx, length);
}

/** \brief One byte is published only after its final falling clock edge. */
static nx_result_t byte_transfer(nx_software_spi_t* owner, uint8_t tx,
                                 uint8_t* rx, nx_time_us_t deadline) {
    const nx_software_spi_config_t* config = owner->config;
    uint8_t received = 0U;
    for (unsigned bit = 0U; bit < 8U; ++bit) {
        nx_result_t result = set_pin(config->mosi, (tx & 0x80U) != 0U);
        if (result == NX_SUCCESS) {
            result = delay(owner, config->half_period_us, deadline);
        }
        if (result == NX_SUCCESS) {
            result = set_pin(config->sck, true);
        }
        if (result == NX_SUCCESS && rx != NULL) {
            uint32_t sample = 0U;
            result = nx_gpio_port_read(config->miso.port, &sample);
            received = (uint8_t)((uint32_t)received << 1U);
            if ((sample & config->miso.mask) != 0U) {
                received = (uint8_t)(received | 1U);
            }
        }
        if (result == NX_SUCCESS) {
            result = delay(owner, config->half_period_us, deadline);
        }
        if (result == NX_SUCCESS) {
            result = set_pin(config->sck, false);
        }
        if (result != NX_SUCCESS) {
            return result;
        }
        tx = (uint8_t)((uint32_t)tx << 1U);
    }
    if (rx != NULL) {
        *rx = received;
    }
    return NX_SUCCESS;
}

/** \brief Synchronous CPU access ends before either success or fault return. */
static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred) {
    nx_software_spi_t* owner = context;
    if (!span(owner, sizeof(*owner), _Alignof(nx_software_spi_t))) {
        return NX_ERROR_INVALID;
    }
    if (owner->in_call) {
        return NX_ERROR_BUSY;
    }
    if (!owner->initialized || owner->faulted || owner->config == NULL) {
        return NX_ERROR_STATE;
    }
    if (deadline == NX_DEADLINE_NEVER ||
        !buffers_valid(owner, tx, rx, length, transferred)) {
        return NX_ERROR_INVALID;
    }
    owner->in_call = true;
    nx_result_t result = observe(owner, deadline);
    if (result != NX_SUCCESS) {
        owner->faulted = result != NX_ERROR_TIMEOUT;
        owner->in_call = false;
        return result;
    }
    *transferred = 0U;
    result = set_pin(owner->config->cs, false);
    if (result == NX_SUCCESS) {
        result = delay(owner, owner->config->select_setup_us, deadline);
    }
    for (size_t i = 0U; result == NX_SUCCESS && i < length; ++i) {
        result = byte_transfer(owner, tx != NULL ? tx[i] : 0xffU,
                               rx != NULL ? &rx[i] : NULL, deadline);
        if (result == NX_SUCCESS) {
            *transferred = i + 1U;
        }
    }
    if (result == NX_SUCCESS) {
        result = delay(owner, owner->config->select_hold_us, deadline);
    }
    nx_result_t cleanup = idle(owner);
    owner->faulted = result != NX_SUCCESS || cleanup != NX_SUCCESS;
    owner->in_call = false;
    return result != NX_SUCCESS ? result : cleanup;
}

/** \brief Retrying pin withdrawal never clocks or replays a payload byte. */
static nx_result_t control(nx_software_spi_t* owner, bool stop) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_software_spi_t))) {
        return NX_ERROR_INVALID;
    }
    if (owner->in_call) {
        return NX_ERROR_BUSY;
    }
    if (owner->config == NULL) {
        return stop ? NX_SUCCESS : NX_ERROR_STATE;
    }
    owner->in_call = true;
    nx_result_t result = idle(owner);
    owner->faulted = result != NX_SUCCESS;
    if (result == NX_SUCCESS) {
        owner->initialized = !stop;
        if (stop) {
            owner->config = NULL;
            owner->last_clock = 0U;
        }
    }
    owner->in_call = false;
    return result;
}

/** \brief Retry idle GPIO writes on the same binding; no byte is replayed. */
static nx_result_t recover(void* context) {
    return control(context, false);
}

/** \brief Release this binding only after successful logical pin withdrawal. */
static nx_result_t stop(void* context) {
    return control(context, true);
}

static const nx_spi_ops_t port_ops = {.recover = recover, .stop = stop};

/** \brief Endpoint and port faces share one exact software owner identity. */
static bool on_port(const void* context, const nx_spi_port_t* port) {
    return port != NULL && port->ops == &port_ops && port->context == context;
}

static const nx_spi_endpoint_ops_t endpoint_ops = {.transfer = transfer,
                                                   .on_port = on_port};

nx_spi_endpoint_t nx_software_spi_endpoint(nx_software_spi_t* owner) {
    return (nx_spi_endpoint_t){.ops = &endpoint_ops, .context = owner};
}

nx_spi_port_t nx_software_spi_port(nx_software_spi_t* owner) {
    return (nx_spi_port_t){.ops = &port_ops, .context = owner};
}

bool nx_software_spi_faulted(const nx_software_spi_t* owner) {
    return owner != NULL && owner->faulted;
}
