/**
 * \file            onewire.c
 * \brief           Bounded GPIO one-wire reset, presence and data slots
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/onewire.h"

static bool span(const void* p, size_t bytes, size_t alignment) {
    uintptr_t first = (uintptr_t)p;
    return p != NULL && first % alignment == 0U && bytes <= UINTPTR_MAX - first;
}
static bool overlap(const void* a, size_t first_bytes, const void* b,
                    size_t second_bytes) {
    return (uintptr_t)a < (uintptr_t)b + second_bytes &&
           (uintptr_t)b < (uintptr_t)a + first_bytes;
}
static bool owner_valid(const nx_onewire_t* owner) {
    return span(owner, sizeof(*owner), _Alignof(nx_onewire_t));
}
static bool configuration_valid(const nx_onewire_t* owner,
                                const nx_onewire_config_t* c) {
    return span(c, sizeof(*c), _Alignof(nx_onewire_config_t)) &&
           !overlap(owner, sizeof(*owner), c, sizeof(*c)) &&
           span(c->port, sizeof(*c->port), _Alignof(nx_gpio_port_t)) &&
           span(c->port->ops, sizeof(*c->port->ops), _Alignof(nx_gpio_ops_t)) &&
           !overlap(owner, sizeof(*owner), c->port, sizeof(*c->port)) &&
           !overlap(owner, sizeof(*owner), c->port->ops,
                    sizeof(*c->port->ops)) &&
           !overlap(c, sizeof(*c), c->port, sizeof(*c->port)) &&
           !overlap(c, sizeof(*c), c->port->ops, sizeof(*c->port->ops)) &&
           c->port->context != NULL && c->port->ops->write != NULL &&
           c->port->ops->read != NULL && c->mask != 0U &&
           (c->mask & (c->mask - 1U)) == 0U && c->clock.read != NULL &&
           c->poll_limit != 0U && c->poll_limit <= 1000000U;
}
static bool external(const nx_onewire_t* owner, const void* p, size_t bytes,
                     size_t alignment) {
    const nx_onewire_config_t* c = owner->config;
    return span(p, bytes, alignment) &&
           !overlap(p, bytes, owner, sizeof(*owner)) &&
           !overlap(p, bytes, c, sizeof(*c)) &&
           !overlap(p, bytes, c->port, sizeof(*c->port)) &&
           !overlap(p, bytes, c->port->ops, sizeof(*c->port->ops));
}
/** \brief A set bit releases the preconfigured open-drain output. */
static nx_result_t drive(nx_onewire_t* o, bool released) {
    uint32_t mask = o->config->mask;
    return nx_gpio_port_write(o->config->port, released ? mask : 0U,
                              released ? 0U : mask);
}
static nx_result_t sample(nx_onewire_t* o, bool* high) {
    uint32_t value = 0U;
    nx_result_t r = nx_gpio_port_read(o->config->port, &value);
    if (r == NX_SUCCESS)
        *high = (value & o->config->mask) != 0U;
    return r;
}
/** \brief The original clock epoch cannot recover by numerical retiming. */
static nx_result_t observe(nx_onewire_t* o, nx_time_us_t deadline) {
    if (o->clock_failed)
        return NX_ERROR_IO;
    nx_time_us_t now = o->config->clock.read(o->config->clock.context);
    if (now == NX_DEADLINE_NEVER || now < o->last_clock) {
        o->clock_failed = o->faulted = true;
        return NX_ERROR_IO;
    }
    o->last_clock = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}
/** \brief Both the absolute deadline and finite observations bound each wait.
 */
static nx_result_t wait_until(nx_onewire_t* o, nx_time_us_t target,
                              nx_time_us_t maximum, nx_time_us_t deadline) {
    if (target > maximum || maximum == NX_DEADLINE_NEVER)
        return NX_ERROR_IO;
    for (uint32_t i = 0U; i < o->config->poll_limit; ++i) {
        nx_result_t r = observe(o, deadline);
        if (r != NX_SUCCESS)
            return r;
        if (o->last_clock > maximum)
            return NX_ERROR_IO;
        if (o->last_clock >= target)
            return NX_SUCCESS;
    }
    return NX_ERROR_IO;
}
/** \brief Bracket physical sampling; delayed callbacks cannot publish success.
 */
static nx_result_t timed_sample(nx_onewire_t* o, nx_time_us_t start,
                                uint32_t maximum, nx_time_us_t deadline,
                                bool* high) {
    nx_result_t r = observe(o, deadline);
    if (r != NX_SUCCESS)
        return r;
    if (maximum >= NX_DEADLINE_NEVER - start)
        return NX_ERROR_EXHAUSTED;
    if (o->last_clock > start + maximum)
        return NX_ERROR_IO;
    bool value = false;
    r = sample(o, &value);
    if (r != NX_SUCCESS)
        return r;
    r = observe(o, deadline);
    if (r != NX_SUCCESS)
        return r;
    if (o->last_clock > start + maximum)
        return NX_ERROR_IO;
    *high = value;
    return NX_SUCCESS;
}
/** \brief One immediate release/read, independent of an expired transfer wait.
 * \note No pointer to caller payload survives this final cleanup attempt.
 */
static nx_result_t cleanup(nx_onewire_t* o, nx_result_t primary,
                           nx_time_us_t deadline) {
    nx_result_t r = drive(o, true);
    bool high = false;
    if (r == NX_SUCCESS)
        r = sample(o, &high);
    if (r == NX_SUCCESS && !high)
        r = NX_ERROR_BUSY;
    o->cleanup_result = r;
    if (primary == NX_SUCCESS && r == NX_SUCCESS)
        primary = observe(o, deadline);
    o->in_call = false;
    if (primary != NX_SUCCESS || r != NX_SUCCESS)
        o->faulted = true;
    return primary != NX_SUCCESS ? primary : r;
}
/** \brief Reset high includes presence and at least480us before the command. */
static nx_result_t reset_presence(nx_onewire_t* o, nx_time_us_t deadline) {
    nx_result_t r = drive(o, true);
    bool high = false;
    if (r == NX_SUCCESS)
        r = sample(o, &high);
    if (r != NX_SUCCESS)
        return r;
    if (!high)
        return NX_ERROR_BUSY;
    r = observe(o, deadline);
    if (r != NX_SUCCESS)
        return r;
    nx_time_us_t start = o->last_clock;
    if (2000U >= NX_DEADLINE_NEVER - start)
        return NX_ERROR_EXHAUSTED;
    r = drive(o, false);
    if (r == NX_SUCCESS)
        r = observe(o, deadline);
    if (r != NX_SUCCESS)
        return r;
    /* Falling edge lies between the pre-write and post-write observations.
     * MIN uses the latest possible edge; MAX uses the earliest possible one. */
    nx_time_us_t post_fall = o->last_clock;
    if (post_fall - start > 460U)
        return NX_ERROR_IO;
    r = wait_until(o, post_fall + 500U, start + 960U, deadline);
    if (r != NX_SUCCESS)
        return r;
    nx_time_us_t release = o->last_clock;
    r = drive(o, true);
    if (r == NX_SUCCESS)
        r = observe(o, deadline);
    if (r != NX_SUCCESS)
        return r;
    if (o->last_clock > start + 960U)
        return NX_ERROR_IO;
    /* Rise must be observed before the earliest valid presence pulse. The
     * fixed960us reset-low ceiling is this conservative software profile,
     * not the external-VDD data sheet's maximum (which is unspecified). */
    high = false;
    for (uint32_t i = 0U; i < o->config->poll_limit && !high; ++i) {
        r = timed_sample(o, release, 14U, deadline, &high);
        if (r != NX_SUCCESS)
            return r;
    }
    if (!high)
        return NX_ERROR_IO;
    nx_time_us_t first_high = o->last_clock;
    bool found = false;
    for (uint32_t i = 0U; i < o->config->poll_limit; ++i) {
        r = timed_sample(o, release, 75U, deadline, &high);
        if (r == NX_ERROR_IO && o->last_clock > release + 75U)
            return NX_ERROR_NACK;
        if (r != NX_SUCCESS)
            return r;
        if (!high) {
            found = true;
            break;
        }
    }
    if (!found)
        return NX_ERROR_NACK;
    high = false;
    for (uint32_t i = 0U; i < o->config->poll_limit && !high; ++i) {
        r = timed_sample(o, release, 315U, deadline, &high);
        if (r != NX_SUCCESS)
            return r;
    }
    if (!high)
        return NX_ERROR_IO;
    nx_time_us_t ready = release + 500U;
    if (ready < first_high + 480U)
        ready = first_high + 480U;
    return wait_until(o, ready, release + 620U, deadline);
}
/** \brief No global interrupt masking: observe each actual slot's bounds. */
static nx_result_t bit_slot(nx_onewire_t* o, bool read, bool bit,
                            nx_time_us_t deadline, bool* value) {
    nx_result_t r = observe(o, deadline);
    if (r != NX_SUCCESS)
        return r;
    nx_time_us_t start = o->last_clock;
    if (200U >= NX_DEADLINE_NEVER - start)
        return NX_ERROR_EXHAUSTED;
    uint32_t low = read || bit ? 3U : 65U;
    uint32_t low_maximum = read ? 12U : bit ? 15U : 120U;
    r = drive(o, false);
    if (r == NX_SUCCESS)
        r = observe(o, deadline);
    if (r != NX_SUCCESS)
        return r;
    nx_time_us_t post_fall = o->last_clock;
    if (post_fall - start > low_maximum - low)
        return NX_ERROR_IO;
    r = wait_until(o, post_fall + low, start + low_maximum, deadline);
    if (r != NX_SUCCESS)
        return r;
    r = drive(o, true);
    if (r == NX_SUCCESS)
        r = observe(o, deadline);
    if (r != NX_SUCCESS)
        return r;
    if (o->last_clock > start + low_maximum)
        return NX_ERROR_IO;
    nx_time_us_t post_release = o->last_clock;
    bool sampled = false;
    if (read) {
        r = wait_until(o, post_fall + 10U, start + 14U, deadline);
        if (r == NX_SUCCESS)
            r = timed_sample(o, start, 14U, deadline, &sampled);
        if (r != NX_SUCCESS)
            return r;
    }
    nx_time_us_t end = start + 70U;
    if (end < post_fall + 60U)
        end = post_fall + 60U;
    if (end < post_release + 1U)
        end = post_release + 1U;
    r = wait_until(o, end, start + 120U, deadline);
    if (r != NX_SUCCESS)
        return r;
    bool high = false;
    r = timed_sample(o, start, 120U, deadline, &high);
    if (r != NX_SUCCESS)
        return r;
    if (!high)
        return NX_ERROR_IO;
    if (read)
        *value = sampled;
    return NX_SUCCESS;
}

nx_result_t nx_onewire_init(nx_onewire_t* o, const nx_onewire_config_t* c,
                            nx_time_us_t deadline) {
    if (!owner_valid(o))
        return NX_ERROR_INVALID;
    if (o->in_call || o->initialized || o->config != NULL)
        return NX_ERROR_BUSY;
    if (!configuration_valid(o, c) || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    o->in_call = true;
    nx_time_us_t now = c->clock.read(c->clock.context);
    if (now == NX_DEADLINE_NEVER || now >= deadline) {
        o->in_call = false;
        return now == NX_DEADLINE_NEVER ? NX_ERROR_IO : NX_ERROR_TIMEOUT;
    }
    o->config = c;
    o->last_clock = now;
    o->clock_failed = o->faulted = false;
    o->initialized = true;
    nx_result_t r = drive(o, true);
    bool high = false;
    if (r == NX_SUCCESS)
        r = sample(o, &high);
    if (r == NX_SUCCESS && !high)
        r = NX_ERROR_BUSY;
    if (r == NX_SUCCESS)
        r = observe(o, deadline);
    return cleanup(o, r, deadline);
}
nx_result_t nx_onewire_exchange(nx_onewire_t* o, const uint8_t* tx,
                                size_t bytes, uint8_t* rx, size_t bits,
                                nx_time_us_t deadline) {
    if (!owner_valid(o))
        return NX_ERROR_INVALID;
    if (o->in_call)
        return NX_ERROR_BUSY;
    if (!o->initialized || o->config == NULL || o->faulted)
        return NX_ERROR_STATE;
    size_t received = (bits + 7U) / 8U;
    if (bytes == 0U || bytes > 16U || bits > 128U ||
        deadline == NX_DEADLINE_NEVER ||
        !external(o, tx, bytes, _Alignof(uint8_t)) ||
        (bits != 0U && (!external(o, rx, received, _Alignof(uint8_t)) ||
                        overlap(tx, bytes, rx, received))))
        return NX_ERROR_INVALID;
    o->in_call = true;
    nx_result_t r = observe(o, deadline);
    if (r != NX_SUCCESS) {
        o->in_call = false;
        return r;
    }
    for (size_t i = 0U; i < received; ++i)
        rx[i] = 0U;
    r = reset_presence(o, deadline);
    for (size_t i = 0U; r == NX_SUCCESS && i < bytes * 8U; ++i)
        r = bit_slot(o, false, (((unsigned)tx[i / 8U] >> (i % 8U)) & 1U) != 0U,
                     deadline, NULL);
    for (size_t i = 0U; r == NX_SUCCESS && i < bits; ++i) {
        bool bit = false;
        r = bit_slot(o, true, false, deadline, &bit);
        if (r == NX_SUCCESS && bit)
            rx[i / 8U] |= (uint8_t)(1U << (i % 8U));
    }
    return cleanup(o, r, deadline);
}
nx_result_t nx_onewire_now(nx_onewire_t* o, nx_time_us_t* now) {
    if (!owner_valid(o))
        return NX_ERROR_INVALID;
    if (o->in_call)
        return NX_ERROR_BUSY;
    if (o->clock_failed)
        return NX_ERROR_IO;
    if (!o->initialized || o->config == NULL || o->faulted)
        return NX_ERROR_STATE;
    if (!external(o, now, sizeof(*now), _Alignof(nx_time_us_t)))
        return NX_ERROR_INVALID;
    o->in_call = true;
    nx_result_t r = observe(o, NX_DEADLINE_NEVER);
    if (r == NX_SUCCESS)
        *now = o->last_clock;
    o->in_call = false;
    return r;
}
nx_result_t nx_onewire_stop(nx_onewire_t* o, nx_time_us_t deadline) {
    if (!owner_valid(o) || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (o->in_call)
        return NX_ERROR_BUSY;
    if (o->config == NULL)
        return NX_SUCCESS;
    o->in_call = true;
    nx_result_t r = observe(o, deadline);
    if (r != NX_SUCCESS) {
        if (r == NX_ERROR_IO)
            return cleanup(o, r, deadline);
        o->in_call = false;
        return r;
    }
    r = drive(o, true);
    bool high = false;
    for (uint32_t i = 0U; r == NX_SUCCESS && i < o->config->poll_limit; ++i) {
        r = observe(o, deadline);
        if (r == NX_SUCCESS)
            r = sample(o, &high);
        if (r == NX_SUCCESS)
            r = observe(o, deadline);
        if (r != NX_SUCCESS || high)
            break;
    }
    if (r == NX_SUCCESS && high && o->faulted) {
        /* An aborted reset can read high before presence starts 15..60us
         * later. Keep the existing OD/clock lease through the full standard
         * reset-high envelope, anchored after actual high readback. */
        if (500U >= NX_DEADLINE_NEVER - o->last_clock)
            r = NX_ERROR_EXHAUSTED;
        else if (500U >= deadline - o->last_clock)
            r = NX_ERROR_TIMEOUT;
        else
            r = wait_until(o, o->last_clock + 500U, deadline - 1U, deadline);
        if (r == NX_SUCCESS)
            r = sample(o, &high);
        if (r == NX_SUCCESS)
            r = observe(o, deadline);
    }
    if (r == NX_SUCCESS && !high)
        r = NX_ERROR_BUSY;
    o->cleanup_result = r;
    o->in_call = false;
    if (r == NX_SUCCESS) {
        o->initialized = false;
        o->config = NULL;
        o->faulted = false;
    } else
        o->faulted = true;
    return r;
}
