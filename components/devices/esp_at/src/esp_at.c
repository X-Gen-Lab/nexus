/**
 * \file            esp_at.c
 * \brief           Bounded ESP32-C3 AT command and single socket lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/esp_at.h"
#include <string.h>

/** \brief Shared selected firmware commands; no per-instance command table. */
static const char* const setup_commands[] = {
    "AT+GMR",      "ATE0",         "AT+SYSSTORE=0", "AT+CWMODE=1,0",
    "AT+CIPMUX=0", "AT+CIPMODE=0", "AT+CIPDINFO=0", "AT+CIPRECVTYPE=0"};
static const nx_at_framing_t framing = {"+IPD,", ':', '>'};
static const char* const urcs[] = {"WIFI ", "CONNECT", "CLOSED", "ready"};

/** \brief Reject overlapping retained storage and overflowing spans. */
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    uintptr_t x = (uintptr_t)a;
    uintptr_t y = (uintptr_t)b;
    return an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y ||
           (an != 0U && bn != 0U && x < y + bn && y < x + an);
}
/** \brief Keep public data/count outputs outside private retained storage. */
static bool outside(const nx_esp_at_t* m, const void* data, size_t bytes) {
    return !overlap(m, sizeof(*m), data, bytes) &&
           !overlap(m->config.line, m->config.line_capacity, data, bytes) &&
           !overlap(m->config.command, m->config.command_capacity, data,
                    bytes) &&
           !overlap(m->config.transmit, m->config.transmit_capacity, data,
                    bytes) &&
           !overlap(m->config.receive, m->config.receive_capacity, data, bytes);
}
/** \brief A latched failure never automatically replays an admitted command. */
static nx_result_t fail(nx_esp_at_t* m, nx_result_t result) {
    if (m->phase != NX_ESP_AT_FAILED) {
        m->failure = result;
    }
    m->phase = NX_ESP_AT_FAILED;
    m->connected = false;
    return m->failure;
}
/** \brief Check a finite operation while preserving the original clock history.
 */
static nx_result_t clock_check(nx_esp_at_t* m, nx_time_us_t deadline) {
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    nx_time_us_t now = m->config.clock.read(m->config.clock.context);
    if (now == NX_DEADLINE_NEVER || now < m->last_time) {
        return fail(m, NX_ERROR_STATE);
    }
    m->last_time = now;
    return now >= deadline ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}
/** \brief Compare callback-scoped text without assuming NUL termination. */
static bool equal(const nx_at_event_t* event, const char* text) {
    size_t bytes = strlen(text);
    return event->size == bytes && memcmp(event->data, text, bytes) == 0;
}
/** \brief Match only the selected module's explicit text prefix. */
static bool starts(const nx_at_event_t* event, const char* text) {
    size_t bytes = strlen(text);
    return event->size >= bytes && memcmp(event->data, text, bytes) == 0;
}
/** \brief Core version differs from project release; verify its chip marker. */
static bool firmware_target(const nx_at_event_t* event) {
    static const char target[] = "ESP32C3";
    for (size_t i = 0U; i + sizeof(target) - 1U <= event->size; ++i) {
        size_t end = i + sizeof(target) - 1U;
        if (memcmp(event->data + i, target, sizeof(target) - 1U) == 0 &&
            (i == 0U || event->data[i - 1U] == ' ' ||
             event->data[i - 1U] == '(') &&
            (end == event->size || event->data[end] == ' ' ||
             event->data[end] == ')')) {
            return true;
        }
    }
    return false;
}
/** \brief Validate length before generic parsing hands over same-read bytes. */
static void header(nx_esp_at_t* m, const nx_at_event_t* event) {
    if (!m->connected || event->size <= 5U || !starts(event, "+IPD,")) {
        (void)fail(m, NX_ERROR_STATE);
        return;
    }
    size_t bytes = 0U;
    for (size_t i = 5U; i < event->size; ++i) {
        uint8_t byte = event->data[i];
        if (byte < '0' || byte > '9') {
            (void)fail(m, NX_ERROR_INVALID);
            return;
        }
        size_t digit = (size_t)(byte - '0');
        if (bytes > (SIZE_MAX - digit) / 10U) {
            (void)fail(m, NX_ERROR_OVERFLOW);
            return;
        }
        bytes = bytes * 10U + digit;
    }
    if (bytes == 0U || bytes > 2920U ||
        bytes > m->config.receive_capacity - m->received_bytes) {
        (void)fail(m, bytes == 0U ? NX_ERROR_INVALID : NX_ERROR_EXHAUSTED);
        return;
    }
    nx_result_t result = nx_at_expect_binary(&m->client, bytes);
    if (result != NX_SUCCESS) {
        (void)fail(m, result);
    }
}
/** \brief Consume callback views only; all new commands wait until return. */
static void event(void* context, const nx_at_event_t* item) {
    nx_esp_at_t* m = context;
    if (m->phase == NX_ESP_AT_FAILED) {
        return;
    }
    if (item->kind == NX_AT_HEADER) {
        header(m, item);
    } else if (item->kind == NX_AT_BINARY) {
        if (!m->connected ||
            item->size > m->config.receive_capacity - m->received_bytes) {
            (void)fail(m, NX_ERROR_EXHAUSTED);
            return;
        }
        size_t tail =
            (m->receive_head + m->received_bytes) % m->config.receive_capacity;
        size_t first = m->config.receive_capacity - tail;
        if (first > item->size) {
            first = item->size;
        }
        memcpy(m->config.receive + tail, item->data, first);
        memcpy(m->config.receive, item->data + first, item->size - first);
        m->received_bytes += item->size;
    } else if (item->kind == NX_AT_PROMPT) {
        if (m->phase != NX_ESP_AT_SENDING ||
            (!m->command_completed && !m->send_accepted) || m->prompt_seen) {
            (void)fail(m, NX_ERROR_STATE);
        } else {
            m->prompt_seen = true;
        }
    } else if (item->kind == NX_AT_COMPLETE) {
        if (item->result != NX_SUCCESS) {
            (void)fail(m, item->result);
        } else {
            m->command_completed = true;
        }
    } else if (starts(item, "+IPD,")) {
        (void)fail(m, NX_ERROR_INVALID);
    } else if (equal(item, "ready")) {
        (void)fail(m, NX_ERROR_STATE);
    } else if (equal(item, "WIFI GOT IP")) {
        m->wifi = true;
    } else if (equal(item, "WIFI DISCONNECT")) {
        m->wifi = false;
        if (m->connected || m->phase == NX_ESP_AT_CONNECTING) {
            (void)fail(m, NX_ERROR_IO);
        }
    } else if (equal(item, "CONNECT")) {
        if (m->phase != NX_ESP_AT_CONNECTING || m->connect_seen) {
            (void)fail(m, NX_ERROR_STATE);
        } else {
            m->connect_seen = true;
        }
    } else if (equal(item, "CLOSED")) {
        if (m->phase == NX_ESP_AT_CLOSING) {
            m->close_seen = true;
            m->connected = false;
        } else if (m->phase == NX_ESP_AT_CONNECTED) {
            m->connected = false;
            m->phase = NX_ESP_AT_CLOSED;
        } else {
            (void)fail(m, NX_ERROR_IO);
        }
    } else if (equal(item, "SEND OK")) {
        if (m->phase != NX_ESP_AT_SENDING || !m->prompt_seen ||
            m->transmitted_bytes != m->pending_bytes) {
            (void)fail(m, NX_ERROR_STATE);
        } else {
            m->pending_bytes = 0U;
            m->transmitted_bytes = 0U;
            m->prompt_seen = false;
            m->send_accepted = false;
            m->phase = NX_ESP_AT_CONNECTED;
        }
    } else if (equal(item, "SEND FAIL")) {
        (void)fail(m, NX_ERROR_IO);
    } else if (m->phase == NX_ESP_AT_CONFIGURING && m->setup == 0U &&
               starts(item, "AT version:")) {
        if (!firmware_target(item)) {
            (void)fail(m, NX_ERROR_UNSUPPORTED);
        } else {
            m->firmware_stage = 1U;
        }
    } else if (m->phase == NX_ESP_AT_CONFIGURING && m->setup == 0U &&
               starts(item, "Bin version:")) {
        if (m->firmware_stage != 1U ||
            !equal(item, "Bin version:4.1.1.0(MINI-1)")) {
            (void)fail(m, NX_ERROR_UNSUPPORTED);
        } else {
            m->firmware_stage = 2U;
        }
    }
    /* Publish completed connection metadata before the next same-read URC. */
    if (m->phase == NX_ESP_AT_CONNECTING && m->command_completed &&
        m->connect_seen) {
        m->phase = NX_ESP_AT_CONNECTED;
        m->connected = true;
        m->command_completed = false;
    }
}
/** \brief Copy a bounded decimal field without stdio or allocation. */
static size_t decimal(char* output, uint32_t value) {
    char reversed[10];
    size_t bytes = 0U;
    do {
        reversed[bytes++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    for (size_t i = 0U; i < bytes; ++i) {
        output[i] = reversed[bytes - i - 1U];
    }
    return bytes;
}
/** \brief Submit only after the generic client's callback has returned. */
static nx_result_t submit(nx_esp_at_t* m, const char* command, size_t bytes,
                          nx_time_us_t deadline, nx_esp_at_phase_t phase) {
    nx_result_t result = clock_check(m, deadline);
    if (result == NX_SUCCESS) {
        result =
            nx_at_submit(&m->client, (const uint8_t*)command, bytes, deadline);
    }
    if (result == NX_SUCCESS) {
        m->deadline = deadline;
        m->phase = phase;
        m->command_completed = false;
    }
    return result;
}
/** \brief Advance completed protocol phases without performing transport I/O.
 */
static nx_result_t advance(nx_esp_at_t* m) {
    if (!m->command_completed) {
        return NX_SUCCESS;
    }
    if (m->phase == NX_ESP_AT_CONFIGURING) {
        if (m->setup == 0U && m->firmware_stage != 2U) {
            return fail(m, NX_ERROR_UNSUPPORTED);
        }
        ++m->setup;
        if (m->setup == sizeof(setup_commands) / sizeof(setup_commands[0])) {
            m->phase = NX_ESP_AT_READY;
            m->command_completed = false;
        } else {
            const char* command = setup_commands[m->setup];
            nx_result_t result = submit(m, command, strlen(command),
                                        m->deadline, NX_ESP_AT_CONFIGURING);
            if (result != NX_SUCCESS) {
                return fail(m, result);
            }
        }
    } else if (m->phase == NX_ESP_AT_JOINING && m->wifi) {
        m->phase = NX_ESP_AT_READY;
        m->command_completed = false;
        memset(m->config.command, 0, m->config.command_capacity);
    } else if (m->phase == NX_ESP_AT_CLOSING && m->close_seen) {
        m->phase = NX_ESP_AT_CLOSED;
        m->connected = false;
        m->command_completed = false;
    } else if (m->phase == NX_ESP_AT_SENDING) {
        m->send_accepted = true;
        m->command_completed = false;
    }
    return NX_SUCCESS;
}
/** \brief Validate byte-face outputs before changing the copied socket state.
 */
static bool transfer_valid(const nx_esp_at_t* m, const void* data, size_t bytes,
                           size_t* count, nx_time_us_t deadline) {
    return m != NULL && m->initialized && count != NULL &&
           deadline != NX_DEADLINE_NEVER && outside(m, count, sizeof(*count)) &&
           (bytes == 0U || (data != NULL && outside(m, data, bytes) &&
                            !overlap(data, bytes, count, sizeof(*count))));
}
/** \brief Copy TX before returning; module SEND OK is progressed separately. */
static nx_result_t write(void* context, const uint8_t* data, size_t bytes,
                         size_t* accepted, nx_time_us_t deadline) {
    nx_esp_at_t* m = context;
    if (!transfer_valid(m, data, bytes, accepted, deadline)) {
        return NX_ERROR_INVALID;
    }
    *accepted = 0U;
    if (bytes == 0U) {
        return NX_SUCCESS;
    }
    if (m->active) {
        return NX_ERROR_BUSY;
    }
    if (m->phase == NX_ESP_AT_SENDING) {
        return NX_ERROR_BUSY;
    }
    if (m->phase != NX_ESP_AT_CONNECTED) {
        return NX_ERROR_STATE;
    }
    size_t count = bytes < m->config.transmit_capacity
                       ? bytes
                       : m->config.transmit_capacity;
    char command[24] = "AT+CIPSEND=";
    size_t length = 11U + decimal(command + 11U, (uint32_t)count);
    nx_result_t result =
        submit(m, command, length, deadline, NX_ESP_AT_SENDING);
    if (result != NX_SUCCESS) {
        return result;
    }
    memcpy(m->config.transmit, data, count);
    m->pending_bytes = count;
    m->transmitted_bytes = 0U;
    m->prompt_seen = false;
    m->send_accepted = false;
    *accepted = count;
    return NX_SUCCESS;
}
/** \brief Consume copied bytes; orderly remote close produces EOF afterwards.
 */
static nx_result_t read(void* context, uint8_t* data, size_t bytes,
                        size_t* received, nx_time_us_t deadline) {
    nx_esp_at_t* m = context;
    if (!transfer_valid(m, data, bytes, received, deadline)) {
        return NX_ERROR_INVALID;
    }
    *received = 0U;
    if (bytes == 0U) {
        return NX_SUCCESS;
    }
    if (m->active) {
        return NX_ERROR_BUSY;
    }
    if (m->phase == NX_ESP_AT_FAILED) {
        return m->failure;
    }
    if (!m->connected && m->phase != NX_ESP_AT_CLOSED) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = clock_check(m, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (m->received_bytes == 0U) {
        return m->phase == NX_ESP_AT_CLOSED ? NX_SUCCESS : NX_ERROR_BUSY;
    }
    size_t count = bytes < m->received_bytes ? bytes : m->received_bytes;
    size_t first = m->config.receive_capacity - m->receive_head;
    if (first > count) {
        first = count;
    }
    memcpy(data, m->config.receive + m->receive_head, first);
    memcpy(data + first, m->config.receive, count - first);
    m->receive_head = (m->receive_head + count) % m->config.receive_capacity;
    m->received_bytes -= count;
    *received = count;
    return NX_SUCCESS;
}
static const nx_byte_stream_ops_t stream_ops = {write, read};

nx_result_t nx_esp_at_init(nx_esp_at_t* m, const nx_esp_at_config_t* config) {
    if (m == NULL || config == NULL || config->uart.ops == NULL ||
        config->line == NULL || config->line_capacity < 128U ||
        config->command == NULL || config->command_capacity < 224U ||
        config->transmit == NULL || config->transmit_capacity == 0U ||
        config->transmit_capacity > 8192U || config->receive == NULL ||
        config->receive_capacity == 0U) {
        return NX_ERROR_INVALID;
    }
    const void* spans[] = {m,
                           config,
                           config->line,
                           config->command,
                           config->transmit,
                           config->receive,
                           config->uart.ops};
    const size_t sizes[] = {sizeof(*m),
                            sizeof(*config),
                            config->line_capacity,
                            config->command_capacity,
                            config->transmit_capacity,
                            config->receive_capacity,
                            sizeof(*config->uart.ops)};
    for (size_t i = 0U; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        for (size_t j = 0U; j < i; ++j) {
            if (overlap(spans[i], sizes[i], spans[j], sizes[j])) {
                return NX_ERROR_INVALID;
            }
        }
    }
    nx_at_client_t client;
    const nx_at_config_t selected = {config->uart,
                                     config->clock,
                                     event,
                                     m,
                                     urcs,
                                     sizeof(urcs) / sizeof(urcs[0]),
                                     config->line,
                                     config->line_capacity,
                                     config->command,
                                     config->command_capacity};
    nx_result_t result = nx_at_init(&client, &selected);
    if (result == NX_SUCCESS) {
        result = nx_at_set_framing(&client, &framing);
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    *m = (nx_esp_at_t){.client = client,
                       .config = *config,
                       .stream = {&stream_ops, m},
                       .last_time = client.last_time,
                       .initialized = true};
    return NX_SUCCESS;
}

nx_result_t nx_esp_at_configure(nx_esp_at_t* m, nx_time_us_t deadline) {
    if (m == NULL || !m->initialized) {
        return NX_ERROR_INVALID;
    }
    if (m->active) {
        return NX_ERROR_BUSY;
    }
    if (m->phase != NX_ESP_AT_COLD) {
        return NX_ERROR_STATE;
    }
    return submit(m, setup_commands[0], strlen(setup_commands[0]), deadline,
                  NX_ESP_AT_CONFIGURING);
}
/** \brief Append one escaped quoted argument in a bounded local command. */
static bool quoted(char* command, size_t* position, const char* text,
                   size_t bytes) {
    command[(*position)++] = '"';
    for (size_t i = 0U; i < bytes; ++i) {
        unsigned char byte = (unsigned char)text[i];
        if (byte == 0U || byte == '\r' || byte == '\n') {
            return false;
        }
        if (byte == '"' || byte == '\\' || byte == ',') {
            command[(*position)++] = '\\';
        }
        command[(*position)++] = (char)byte;
    }
    command[(*position)++] = '"';
    return true;
}

nx_result_t nx_esp_at_join(nx_esp_at_t* m, const char* ssid, size_t ssid_bytes,
                           const char* password, size_t password_bytes,
                           nx_time_us_t deadline) {
    if (m == NULL || !m->initialized || ssid == NULL || ssid_bytes == 0U ||
        ssid_bytes > 32U || password_bytes > 63U ||
        (password_bytes != 0U && (password == NULL || password_bytes < 8U)) ||
        !outside(m, ssid, ssid_bytes) ||
        (password_bytes != 0U && !outside(m, password, password_bytes))) {
        return NX_ERROR_INVALID;
    }
    if (m->active) {
        return NX_ERROR_BUSY;
    }
    if (m->phase != NX_ESP_AT_READY) {
        return NX_ERROR_STATE;
    }
    char command[224] = "AT+CWJAP=";
    size_t bytes = 9U;
    if (!quoted(command, &bytes, ssid, ssid_bytes)) {
        return NX_ERROR_INVALID;
    }
    command[bytes++] = ',';
    if (!quoted(command, &bytes, password, password_bytes)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = submit(m, command, bytes, deadline, NX_ESP_AT_JOINING);
    if (result == NX_SUCCESS) {
        m->wifi = false;
    }
    return result;
}

nx_result_t nx_esp_at_connect(nx_esp_at_t* m, const char* host,
                              size_t host_bytes, uint16_t port,
                              nx_time_us_t deadline) {
    if (m == NULL || !m->initialized || host == NULL || host_bytes == 0U ||
        host_bytes > 64U || port == 0U || !outside(m, host, host_bytes)) {
        return NX_ERROR_INVALID;
    }
    if (m->active) {
        return NX_ERROR_BUSY;
    }
    if (m->phase != NX_ESP_AT_READY || !m->wifi || m->received_bytes != 0U) {
        return NX_ERROR_STATE;
    }
    for (size_t i = 0U; i < host_bytes; ++i) {
        unsigned char byte = (unsigned char)host[i];
        if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
              (byte >= '0' && byte <= '9') || byte == '.' || byte == '-')) {
            return NX_ERROR_INVALID;
        }
    }
    char command[96] = "AT+CIPSTART=\"TCP\",\"";
    size_t bytes = 19U;
    memcpy(command + bytes, host, host_bytes);
    bytes += host_bytes;
    command[bytes++] = '"';
    command[bytes++] = ',';
    bytes += decimal(command + bytes, port);
    nx_result_t result =
        submit(m, command, bytes, deadline, NX_ESP_AT_CONNECTING);
    if (result == NX_SUCCESS) {
        m->connect_seen = false;
    }
    return result;
}

nx_result_t nx_esp_at_service(nx_esp_at_t* m, nx_time_us_t deadline,
                              uint32_t io_budget) {
    if (m == NULL || !m->initialized || deadline == NX_DEADLINE_NEVER ||
        io_budget == 0U) {
        return NX_ERROR_INVALID;
    }
    if (m->active) {
        return NX_ERROR_BUSY;
    }
    if (m->phase == NX_ESP_AT_FAILED) {
        return m->failure;
    }
    if (m->phase == NX_ESP_AT_COLD) {
        return NX_ERROR_STATE;
    }
    m->active = true;
    bool progress = false;
    nx_result_t result = NX_SUCCESS;
    for (uint32_t i = 0U; i < io_budget; ++i) {
        bool operating = m->phase != NX_ESP_AT_READY &&
                         m->phase != NX_ESP_AT_CONNECTED &&
                         m->phase != NX_ESP_AT_CLOSED;
        nx_time_us_t limit =
            operating && m->deadline < deadline ? m->deadline : deadline;
        result = clock_check(m, limit);
        if (result != NX_SUCCESS) {
            result = fail(m, result);
            break;
        }
        if (m->phase == NX_ESP_AT_SENDING && m->prompt_seen &&
            m->send_accepted && m->transmitted_bytes < m->pending_bytes) {
            size_t count = 0U;
            size_t remaining = m->pending_bytes - m->transmitted_bytes;
            result = m->config.uart.ops->write(m->config.uart.context,
                                               m->config.transmit +
                                                   m->transmitted_bytes,
                                               remaining, &count, limit);
            if (result == NX_SUCCESS) {
                if (count == 0U || count > remaining) {
                    result = NX_ERROR_IO;
                } else {
                    m->transmitted_bytes += count;
                }
            } else if (count != 0U) {
                result = NX_ERROR_IO;
            }
        } else {
            result = nx_at_service(&m->client, limit, 1U);
        }
        if (m->phase == NX_ESP_AT_FAILED) {
            result = m->failure;
            break;
        }
        if (result == NX_ERROR_BUSY) {
            break;
        }
        if (result != NX_SUCCESS) {
            result = fail(m, result);
            break;
        }
        progress = true;
        result = advance(m);
        if (result != NX_SUCCESS) {
            break;
        }
    }
    m->active = false;
    return result != NX_SUCCESS && result != NX_ERROR_BUSY ? result
           : progress                                      ? NX_SUCCESS
                                                           : NX_ERROR_BUSY;
}

const nx_byte_stream_t* nx_esp_at_stream(const nx_esp_at_t* m) {
    return m != NULL && m->initialized ? &m->stream : NULL;
}

nx_result_t nx_esp_at_close(nx_esp_at_t* m, nx_time_us_t deadline) {
    if (m == NULL || !m->initialized || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (m->active) {
        return NX_ERROR_BUSY;
    }
    if (m->phase == NX_ESP_AT_CLOSED) {
        return NX_SUCCESS;
    }
    if (m->phase == NX_ESP_AT_SENDING) {
        return NX_ERROR_BUSY;
    }
    if (m->phase != NX_ESP_AT_CONNECTED) {
        return NX_ERROR_STATE;
    }
    nx_result_t result =
        submit(m, "AT+CIPCLOSE", 11U, deadline, NX_ESP_AT_CLOSING);
    if (result == NX_SUCCESS) {
        m->close_seen = false;
    }
    return result;
}

nx_result_t nx_esp_at_recover(nx_esp_at_t* m) {
    if (m == NULL || !m->initialized) {
        return NX_ERROR_INVALID;
    }
    if (m->active) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_at_recover(&m->client);
    if (result != NX_SUCCESS) {
        return result;
    }
    m->phase = NX_ESP_AT_COLD;
    m->failure = NX_SUCCESS;
    m->received_bytes = 0U;
    m->receive_head = 0U;
    m->pending_bytes = 0U;
    m->transmitted_bytes = 0U;
    m->firmware_stage = 0U;
    m->wifi = false;
    m->connected = false;
    m->command_completed = false;
    m->connect_seen = false;
    m->close_seen = false;
    m->prompt_seen = false;
    m->send_accepted = false;
    m->setup = 0U;
    memset(m->config.command, 0, m->config.command_capacity);
    return NX_SUCCESS;
}

nx_result_t nx_esp_at_status(const nx_esp_at_t* m, nx_esp_at_status_t* status) {
    if (m == NULL || !m->initialized || status == NULL ||
        !outside(m, status, sizeof(*status))) {
        return NX_ERROR_INVALID;
    }
    if (m->active) {
        return NX_ERROR_BUSY;
    }
    *status = (nx_esp_at_status_t){.phase = m->phase,
                                   .failure = m->failure,
                                   .received_bytes = m->received_bytes,
                                   .pending_bytes = m->pending_bytes,
                                   .firmware_verified = m->firmware_stage == 2U,
                                   .wifi = m->wifi,
                                   .connected = m->connected};
    return NX_SUCCESS;
}
