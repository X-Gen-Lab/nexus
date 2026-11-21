/**
 * \file            at_client.c
 * \brief           Serialized AT progress with explicit binary and resync state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/at_client.h"
#include <string.h>

/** \brief Reject overlapping retained storage and overflowed spans. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (an != 0 && bn != 0 && x < y + bn && y < x + an);
}
/** \brief Report a bounded callback-scoped view in the owning task. */
static void emit(nx_at_client_t* c, nx_at_event_kind_t kind,
                 const uint8_t* data, size_t size, nx_result_t result) {
    const nx_at_event_t event = {kind, data, size, result};
    c->event(c->context, &event);
}
/** \brief Latch lost framing instead of silently replaying a command. */
static nx_result_t fail(nx_at_client_t* c, nx_result_t result) {
    c->failed = true;
    c->result = result;
    if (c->command_active) {
        c->command_active = false;
        emit(c, NX_AT_COMPLETE, c->line, 0, result);
    }
    return result;
}
/** \brief Read one finite same-domain clock snapshot and reject rollback. */
static nx_result_t time_check(nx_at_client_t* c, nx_time_us_t deadline) {
    nx_time_us_t now = c->clock.read(c->clock.context);
    if (now == NX_DEADLINE_NEVER || now < c->last_time) {
        return fail(c, NX_ERROR_STATE);
    }
    c->last_time = now;
    if (now >= deadline || (c->command_active && now >= c->deadline)) {
        return fail(c, NX_ERROR_TIMEOUT);
    }
    return NX_SUCCESS;
}
/** \brief Bind stable caller-owned buffers and immutable event routing. */
nx_result_t nx_at_init(nx_at_client_t* c, const nx_at_config_t* config) {
    if (c == NULL || config == NULL || config->stream.ops == NULL ||
        config->stream.ops->read == NULL || config->stream.ops->write == NULL ||
        config->clock.read == NULL || config->event == NULL ||
        config->line == NULL || config->line_capacity == 0 ||
        config->transmit == NULL || config->transmit_capacity < 3 ||
        (config->urc_count != 0 && config->urc_prefixes == NULL) ||
        config->urc_count > SIZE_MAX / sizeof(*config->urc_prefixes)) {
        return NX_ERROR_INVALID;
    }
    const void* spans[] = {c,
                           config,
                           config->line,
                           config->transmit,
                           config->urc_prefixes,
                           config->stream.ops};
    const size_t sizes[] = {sizeof(*c),
                            sizeof(*config),
                            config->line_capacity,
                            config->transmit_capacity,
                            config->urc_count * sizeof(*config->urc_prefixes),
                            sizeof(*config->stream.ops)};
    for (size_t i = 0; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (overlap(spans[i], sizes[i], spans[j], sizes[j])) {
                return NX_ERROR_INVALID;
            }
        }
    }
    for (size_t i = 0; i < config->urc_count; ++i) {
        const char* prefix = config->urc_prefixes[i];
        if (prefix == NULL || prefix[0] == '\0') {
            return NX_ERROR_INVALID;
        }
        size_t length = 0;
        while (length < config->line_capacity && prefix[length] != '\0') {
            ++length;
        }
        if (length >= config->line_capacity ||
            overlap(c, sizeof(*c), prefix, length + 1) ||
            overlap(config->line, config->line_capacity, prefix, length + 1) ||
            overlap(config->transmit, config->transmit_capacity, prefix,
                    length + 1)) {
            return NX_ERROR_INVALID;
        }
    }
    nx_time_us_t now = config->clock.read(config->clock.context);
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    memset(c, 0, sizeof(*c));
    c->stream = config->stream;
    c->clock = config->clock;
    c->event = config->event;
    c->context = config->context;
    c->urc_prefixes = config->urc_prefixes;
    c->urc_count = config->urc_count;
    c->line = config->line;
    c->line_capacity = config->line_capacity;
    c->transmit = config->transmit;
    c->transmit_capacity = config->transmit_capacity;
    c->last_time = now;
    return NX_SUCCESS;
}
/** \brief Copy a whole command before any byte reaches the transport. */
nx_result_t nx_at_submit(nx_at_client_t* c, const uint8_t* command, size_t size,
                         nx_time_us_t deadline) {
    if (c == NULL || c->event == NULL || command == NULL || size == 0 ||
        deadline == NX_DEADLINE_NEVER ||
        overlap(c, sizeof(*c), command, size) ||
        overlap(c->line, c->line_capacity, command, size) ||
        overlap(c->transmit, c->transmit_capacity, command, size)) {
        return NX_ERROR_INVALID;
    }
    if (c->failed) {
        return NX_ERROR_STATE;
    }
    if (c->active || c->command_active || c->binary_remaining != 0 ||
        c->line_size != 0) {
        return NX_ERROR_BUSY;
    }
    if (size > c->transmit_capacity - 2) {
        return NX_ERROR_EXHAUSTED;
    }
    for (size_t i = 0; i < size; ++i) {
        if (command[i] == 0 || command[i] == '\r' || command[i] == '\n') {
            return NX_ERROR_INVALID;
        }
    }
    nx_time_us_t now = c->clock.read(c->clock.context);
    if (now == NX_DEADLINE_NEVER || now < c->last_time) {
        return NX_ERROR_STATE;
    }
    if (now >= deadline) {
        return NX_ERROR_TIMEOUT;
    }
    memcpy(c->transmit, command, size);
    c->transmit[size] = '\r';
    c->transmit[size + 1] = '\n';
    c->transmit_size = size + 2;
    c->transmit_offset = 0;
    c->deadline = deadline;
    c->last_time = now;
    c->command_active = true;
    c->result = NX_ERROR_BUSY;
    return NX_SUCCESS;
}
/** \brief Compare byte-counted lines without reading beyond their extent. */
static bool equal(const nx_at_client_t* c, const char* text) {
    size_t n = strlen(text);
    return c->line_size == n && memcmp(c->line, text, n) == 0;
}
/** \brief Match only explicit immutable URC prefixes selected by the product.
 */
static bool prefix(const nx_at_client_t* c, const char* text) {
    size_t n = strlen(text);
    return c->line_size >= n && memcmp(c->line, text, n) == 0;
}
/** \brief Terminals complete commands; configured URCs remain independent. */
static void line(nx_at_client_t* c) {
    bool terminal = false;
    nx_result_t result = NX_SUCCESS;
    if (c->command_active && equal(c, "OK")) {
        terminal = true;
    } else if (c->command_active &&
               (equal(c, "ERROR") || prefix(c, "+CME ERROR:") ||
                prefix(c, "+CMS ERROR:"))) {
        terminal = true;
        result = NX_ERROR_IO;
    }
    if (terminal) {
        c->command_active = false;
        c->result = result;
        emit(c, NX_AT_COMPLETE, c->line, c->line_size, result);
    } else {
        nx_at_event_kind_t kind = NX_AT_RESPONSE;
        for (size_t i = 0; i < c->urc_count; ++i) {
            if (prefix(c, c->urc_prefixes[i])) {
                kind = NX_AT_URC;
                break;
            }
        }
        emit(c, kind, c->line, c->line_size, NX_SUCCESS);
    }
    c->line_size = 0;
}
/** \brief Parse already-copied bytes, preserving arbitrary binary payload. */
static nx_result_t receive(nx_at_client_t* c, const uint8_t* bytes,
                           size_t size) {
    for (size_t i = 0; i < size;) {
        if (c->binary_remaining != 0) {
            size_t count =
                size - i < c->binary_remaining ? size - i : c->binary_remaining;
            c->binary_remaining -= count;
            emit(c, NX_AT_BINARY, bytes + i, count, NX_SUCCESS);
            i += count;
            continue;
        }
        uint8_t byte = bytes[i++];
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            if (c->line_size != 0) {
                line(c);
            }
            continue;
        }
        if (byte == 0) {
            return fail(c, NX_ERROR_INVALID);
        }
        if (c->line_size == c->line_capacity) {
            return fail(c, NX_ERROR_OVERFLOW);
        }
        c->line[c->line_size++] = byte;
    }
    return NX_SUCCESS;
}
/** \brief Advance bounded copying calls while retaining partial transmit data.
 */
nx_result_t nx_at_service(nx_at_client_t* c, nx_time_us_t deadline,
                          uint32_t io_budget) {
    if (c == NULL || c->event == NULL || deadline == NX_DEADLINE_NEVER ||
        io_budget == 0) {
        return NX_ERROR_INVALID;
    }
    if (c->active) {
        return NX_ERROR_BUSY;
    }
    if (c->failed) {
        return NX_ERROR_STATE;
    }
    c->active = true;
    bool progress = false;
    nx_result_t result = NX_SUCCESS;
    for (uint32_t i = 0; i < io_budget; ++i) {
        result = time_check(c, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        nx_time_us_t limit = c->command_active && c->deadline < deadline
                                 ? c->deadline
                                 : deadline;
        size_t count = 0;
        if (c->transmit_offset < c->transmit_size) {
            size_t remaining = c->transmit_size - c->transmit_offset;
            result = c->stream.ops->write(c->stream.context,
                                          c->transmit + c->transmit_offset,
                                          remaining, &count, limit);
            if (result == NX_SUCCESS && (count == 0 || count > remaining)) {
                result = NX_ERROR_IO;
            }
            if (result == NX_SUCCESS) {
                c->transmit_offset += count;
            }
        } else {
            uint8_t bytes[64];
            result = c->stream.ops->read(c->stream.context, bytes,
                                         sizeof(bytes), &count, limit);
            if (result == NX_SUCCESS && (count == 0 || count > sizeof(bytes))) {
                result = NX_ERROR_IO;
            }
            if (result == NX_SUCCESS) {
                result = receive(c, bytes, count);
            }
        }
        if (result == NX_ERROR_BUSY && count == 0) {
            result = NX_SUCCESS;
            break;
        }
        if (result != NX_SUCCESS || count == 0) {
            if (!c->failed) {
                (void)fail(c, result == NX_SUCCESS ? NX_ERROR_IO : result);
            }
            result = c->result;
            break;
        }
        progress = true;
        result = time_check(c, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
    }
    c->active = false;
    return result != NX_SUCCESS ? result
           : progress           ? NX_SUCCESS
                                : NX_ERROR_BUSY;
}
/** \brief Permit exactly the next announced number of bytes as binary data. */
nx_result_t nx_at_expect_binary(nx_at_client_t* c, size_t bytes) {
    if (c == NULL || c->event == NULL || bytes == 0) {
        return NX_ERROR_INVALID;
    }
    if (c->failed) {
        return NX_ERROR_STATE;
    }
    if (c->binary_remaining != 0) {
        return NX_ERROR_BUSY;
    }
    c->binary_remaining = bytes;
    return NX_SUCCESS;
}
/** \brief Reset parser only after product-proven transport synchronization. */
nx_result_t nx_at_recover(nx_at_client_t* c) {
    if (c == NULL || c->event == NULL) {
        return NX_ERROR_INVALID;
    }
    if (c->active) {
        return NX_ERROR_BUSY;
    }
    nx_time_us_t now = c->clock.read(c->clock.context);
    if (now == NX_DEADLINE_NEVER || now < c->last_time) {
        return NX_ERROR_STATE;
    }
    c->line_size = 0;
    c->transmit_size = 0;
    c->transmit_offset = 0;
    c->binary_remaining = 0;
    c->command_active = false;
    c->failed = false;
    c->result = NX_SUCCESS;
    c->last_time = now;
    return NX_SUCCESS;
}
