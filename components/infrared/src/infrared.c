/**
 * \file            infrared.c
 * \brief           Bounded NEC waveform decoding and explicit edge admission
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/infrared.h"

#define WAIT_START   0U
#define LEADER_MARK  1U
#define LEADER_SPACE 2U
#define BIT_MARK     3U
#define BIT_SPACE    4U
#define TRAILER_MARK 5U
#define REPEAT_MARK  6U

/** \brief Numeric checks precede access to each known typed control span. */
static bool span(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t start = (uintptr_t)pointer;
    return pointer != NULL && start % alignment == 0U &&
           bytes <= UINTPTR_MAX - start;
}

/** \brief Known control objects cannot be reinterpreted as copied edge data. */
static bool overlaps(const void* a, size_t a_bytes, const void* b,
                     size_t b_bytes) {
    uintptr_t first = (uintptr_t)a;
    uintptr_t second = (uintptr_t)b;
    return a_bytes > UINTPTR_MAX - first || b_bytes > UINTPTR_MAX - second ||
           (first < second + b_bytes && second < first + a_bytes);
}

/** \brief Every authored duration is finite and nonempty. */
static bool window_valid(nx_infrared_window_t window) {
    return window.minimum_us != 0U && window.minimum_us <= window.maximum_us &&
           window.maximum_us <= 20000U;
}

/** \brief Ambiguous timing cannot be repaired by a preferred decode branch. */
static bool windows_separate(nx_infrared_window_t a, nx_infrared_window_t b) {
    return a.maximum_us < b.minimum_us || b.maximum_us < a.minimum_us;
}

/** \brief Complete-frame cost uses bounded integer durations, never rounding.
 */
static bool configuration_valid(const nx_infrared_nec_config_t* config) {
    if (config->line > 15U ||
        (config->protocol != NX_INFRARED_NEC_STANDARD &&
         config->protocol != NX_INFRARED_NEC_EXTENDED) ||
        (config->source_policy != NX_INFRARED_SOURCE_STRICT &&
         config->source_policy != NX_INFRARED_SOURCE_IRQ_OBSERVATION) ||
        !window_valid(config->leader_mark) ||
        !window_valid(config->leader_space) ||
        !window_valid(config->repeat_space) ||
        !window_valid(config->bit_mark) || !window_valid(config->zero_space) ||
        !window_valid(config->one_space) ||
        !windows_separate(config->zero_space, config->one_space) ||
        !windows_separate(config->leader_space, config->repeat_space) ||
        config->frame_max_us > 1000000U || config->repeat_max_us > 5000000U)
        return false;
    uint32_t largest_space = config->one_space.maximum_us;
    if (config->zero_space.maximum_us > largest_space)
        largest_space = config->zero_space.maximum_us;
    uint32_t frame = (uint32_t)config->leader_mark.maximum_us +
                     config->leader_space.maximum_us +
                     33U * config->bit_mark.maximum_us + 32U * largest_space;
    uint32_t repeat = (uint32_t)config->leader_mark.maximum_us +
                      config->repeat_space.maximum_us +
                      config->bit_mark.maximum_us;
    return config->frame_max_us >= frame && config->repeat_max_us >= repeat;
}

/** \brief One single executor protects state across optional port callbacks. */
static nx_result_t admission(const nx_infrared_nec_t* owner) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_infrared_nec_t)))
        return NX_ERROR_INVALID;
    if (owner->in_call)
        return NX_ERROR_BUSY;
    if (!owner->initialized || owner->config == NULL)
        return NX_ERROR_STATE;
    return owner->clock_fault ? NX_ERROR_IO : NX_SUCCESS;
}

/** \brief An invalid history cannot authorize a repeat of an older command. */
static void discard(nx_infrared_nec_t* owner) {
    owner->stage = WAIT_START;
    owner->word = 0U;
    owner->bit_count = 0U;
    owner->last_valid = false;
    owner->source_flags = 0U;
    owner->valid_flags = 0U;
}

/** \brief A real falling edge may start a new frame after rejected history. */
static void restart(nx_infrared_nec_t* owner, const nx_exti_event_t* event) {
    discard(owner);
    if (event->edge == NX_EXTI_FALLING) {
        owner->stage = LEADER_MARK;
        owner->frame_started = event->timestamp_us;
        owner->source_flags = (uint8_t)event->flags;
    }
}

/** \brief Inclusive windows never imply an observed receiver tolerance. */
static bool within(nx_time_us_t width, nx_infrared_window_t window) {
    return width >= window.minimum_us && width <= window.maximum_us;
}

/** \brief Results are copied values, with no retained writable destination. */
static nx_infrared_nec_result_t outcome(nx_result_t result) {
    return (nx_infrared_nec_result_t){.result = result};
}

/** \brief Publish a full checked word or a separately authorized repeat. */
static nx_infrared_nec_result_t publish(nx_infrared_nec_t* owner,
                                        nx_time_us_t timestamp, bool repeat) {
    uint32_t flags = owner->source_flags;
    if (repeat)
        flags |= owner->valid_flags;
    if (!repeat) {
        uint8_t address = (uint8_t)owner->word;
        uint8_t address_high = (uint8_t)(owner->word >> 8U);
        uint8_t command = (uint8_t)(owner->word >> 16U);
        uint8_t inverse = (uint8_t)(owner->word >> 24U);
        if ((uint8_t)(command ^ inverse) != UINT8_MAX ||
            (owner->config->protocol == NX_INFRARED_NEC_STANDARD &&
             (uint8_t)(address ^ address_high) != UINT8_MAX)) {
            discard(owner);
            return outcome(NX_ERROR_IO);
        }
        owner->address = owner->config->protocol == NX_INFRARED_NEC_STANDARD
                             ? address
                             : (uint16_t)(owner->word & UINT32_C(0xffff));
        owner->command = command;
    }
    owner->stage = WAIT_START;
    owner->word = 0U;
    owner->bit_count = 0U;
    owner->valid_at = timestamp;
    owner->last_valid = true;
    owner->valid_flags = (uint8_t)flags;
    owner->source_flags = 0U;
    return (nx_infrared_nec_result_t){
        .result = NX_SUCCESS,
        .frame = {.timestamp_us = timestamp,
                  .address = owner->address,
                  .command = owner->command,
                  .repeat = repeat,
                  .quality = owner->config->source_policy ==
                                     NX_INFRARED_SOURCE_IRQ_OBSERVATION
                                 ? NX_INFRARED_QUALITY_SAMPLED_UNCERTAIN
                                 : NX_INFRARED_QUALITY_FLAG_FREE_SOURCE,
                  .source_flags = flags}};
}

/** \brief Edge history is copied and consumed once, never inferred from level.
 */
static nx_infrared_nec_result_t consume(nx_infrared_nec_t* owner,
                                        const nx_exti_event_t* event) {
    if (event->line != owner->config->line ||
        (event->flags & ~(NX_EXTI_EVENT_LOSS | NX_EXTI_EVENT_COALESCED)) != 0U)
        return outcome(NX_ERROR_INVALID);
    if ((event->flags & NX_EXTI_EVENT_LOSS) != 0U ||
        (event->flags != 0U &&
         owner->config->source_policy == NX_INFRARED_SOURCE_STRICT)) {
        /* LOSS detection/report time is not the timestamp of a missing edge. */
        discard(owner);
        return outcome(NX_ERROR_OVERFLOW);
    }
    if ((event->edge != NX_EXTI_RISING && event->edge != NX_EXTI_FALLING) ||
        event->timestamp_us == NX_DEADLINE_NEVER)
        return outcome(NX_ERROR_INVALID);
    if (owner->timestamp_seen && event->timestamp_us <= owner->last_timestamp) {
        discard(owner);
        owner->clock_fault = true;
        return outcome(NX_ERROR_IO);
    }
    nx_time_us_t width = event->timestamp_us - owner->last_timestamp;
    owner->last_timestamp = event->timestamp_us;
    owner->timestamp_seen = true;
    if (owner->stage != WAIT_START &&
        event->timestamp_us - owner->frame_started >
            owner->config->frame_max_us) {
        restart(owner, event);
        return outcome(NX_ERROR_TIMEOUT);
    }
    /* Explicit IRQ observation accepts possible coalescing without erasing
     * provenance or pretending a valid NEC word proves capture integrity. */
    if (owner->stage == WAIT_START)
        owner->source_flags = 0U;
    owner->source_flags |= (uint8_t)event->flags;
    switch (owner->stage) {
        case WAIT_START:
            if (event->edge == NX_EXTI_FALLING) {
                owner->frame_started = event->timestamp_us;
                owner->stage = LEADER_MARK;
                return outcome(NX_ERROR_EMPTY);
            }
            break;
        case LEADER_MARK:
            if (event->edge == NX_EXTI_RISING &&
                within(width, owner->config->leader_mark)) {
                owner->stage = LEADER_SPACE;
                return outcome(NX_ERROR_EMPTY);
            }
            break;
        case LEADER_SPACE:
            if (event->edge != NX_EXTI_FALLING)
                break;
            if (within(width, owner->config->leader_space)) {
                owner->word = 0U;
                owner->bit_count = 0U;
                owner->stage = BIT_MARK;
                return outcome(NX_ERROR_EMPTY);
            }
            if (within(width, owner->config->repeat_space)) {
                if (!owner->last_valid) {
                    discard(owner);
                    return outcome(NX_ERROR_IO);
                }
                if (event->timestamp_us - owner->valid_at >
                    owner->config->repeat_max_us) {
                    discard(owner);
                    return outcome(NX_ERROR_TIMEOUT);
                }
                owner->stage = REPEAT_MARK;
                return outcome(NX_ERROR_EMPTY);
            }
            break;
        case BIT_MARK:
            if (event->edge == NX_EXTI_RISING &&
                within(width, owner->config->bit_mark)) {
                owner->stage = BIT_SPACE;
                return outcome(NX_ERROR_EMPTY);
            }
            break;
        case BIT_SPACE:
            if (event->edge == NX_EXTI_FALLING) {
                bool one = within(width, owner->config->one_space);
                if (!one && !within(width, owner->config->zero_space))
                    break;
                if (one)
                    owner->word |= UINT32_C(1) << owner->bit_count;
                ++owner->bit_count;
                owner->stage =
                    owner->bit_count == 32U ? TRAILER_MARK : BIT_MARK;
                return outcome(NX_ERROR_EMPTY);
            }
            break;
        case TRAILER_MARK:
            if (event->edge == NX_EXTI_RISING &&
                within(width, owner->config->bit_mark))
                return publish(owner, event->timestamp_us, false);
            break;
        case REPEAT_MARK:
            if (event->edge == NX_EXTI_RISING &&
                within(width, owner->config->bit_mark)) {
                if (event->timestamp_us - owner->valid_at >
                    owner->config->repeat_max_us) {
                    discard(owner);
                    return outcome(NX_ERROR_TIMEOUT);
                }
                return publish(owner, event->timestamp_us, true);
            }
            break;
        default:
            break;
    }
    restart(owner, event);
    return outcome(NX_ERROR_IO);
}

nx_result_t nx_infrared_nec_init(nx_infrared_nec_t* owner,
                                 const nx_infrared_nec_config_t* config) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_infrared_nec_t)) ||
        !span(config, sizeof(*config), _Alignof(nx_infrared_nec_config_t)) ||
        overlaps(owner, sizeof(*owner), config, sizeof(*config)))
        return NX_ERROR_INVALID;
    if (owner->in_call || owner->initialized)
        return NX_ERROR_BUSY;
    if (!configuration_valid(config))
        return NX_ERROR_INVALID;
    *owner = (nx_infrared_nec_t){.config = config, .initialized = true};
    return NX_SUCCESS;
}

nx_infrared_nec_result_t nx_infrared_nec_feed(nx_infrared_nec_t* owner,
                                              const nx_exti_event_t* event) {
    nx_result_t result = admission(owner);
    if (result != NX_SUCCESS)
        return outcome(result);
    if (!span(event, sizeof(*event), _Alignof(nx_exti_event_t)) ||
        overlaps(event, sizeof(*event), owner, sizeof(*owner)) ||
        overlaps(event, sizeof(*event), owner->config, sizeof(*owner->config)))
        return outcome(NX_ERROR_INVALID);
    owner->in_call = true;
    nx_infrared_nec_result_t decoded = consume(owner, event);
    owner->in_call = false;
    return decoded;
}

nx_infrared_nec_result_t nx_infrared_nec_poll(nx_infrared_nec_t* owner,
                                              const nx_exti_port_t* port) {
    nx_result_t result = admission(owner);
    if (result != NX_SUCCESS)
        return outcome(result);
    if (!span(port, sizeof(*port), _Alignof(nx_exti_port_t)) ||
        overlaps(owner, sizeof(*owner), port, sizeof(*port)) ||
        overlaps(owner->config, sizeof(*owner->config), port, sizeof(*port)) ||
        !span(port->ops, sizeof(*port->ops), _Alignof(nx_exti_ops_t)) ||
        overlaps(owner, sizeof(*owner), port->ops, sizeof(*port->ops)) ||
        overlaps(owner->config, sizeof(*owner->config), port->ops,
                 sizeof(*port->ops)) ||
        port->context == NULL ||
        overlaps(owner, sizeof(*owner), port->context, 1U) ||
        overlaps(owner->config, sizeof(*owner->config), port->context, 1U))
        return outcome(NX_ERROR_INVALID);
    if (port->ops->read == NULL)
        return outcome(NX_ERROR_UNSUPPORTED);
    owner->in_call = true;
    nx_exti_event_t event = {0};
    size_t count = 0U;
    result = nx_exti_port_read(port, &event, 1U, &count);
    nx_infrared_nec_result_t decoded;
    if (result == NX_SUCCESS && count == 1U) {
        decoded = consume(owner, &event);
        /* Direct feed rejects bad arguments unchanged. Here a source fact
         * has already been consumed, so its missing history cannot preserve
         * a prior command's repeat authority. Rejected time is not rebased. */
        if (decoded.result == NX_ERROR_INVALID)
            discard(owner);
    } else if (result == NX_ERROR_EMPTY && count == 0U)
        decoded = outcome(NX_ERROR_EMPTY);
    else {
        discard(owner);
        decoded = outcome(result == NX_SUCCESS || result == NX_ERROR_EMPTY
                              ? NX_ERROR_IO
                              : result);
    }
    owner->in_call = false;
    return decoded;
}

nx_result_t nx_infrared_nec_stop(nx_infrared_nec_t* owner) {
    if (!span(owner, sizeof(*owner), _Alignof(nx_infrared_nec_t)))
        return NX_ERROR_INVALID;
    if (owner->in_call)
        return NX_ERROR_BUSY;
    *owner = (nx_infrared_nec_t){0};
    return NX_SUCCESS;
}
