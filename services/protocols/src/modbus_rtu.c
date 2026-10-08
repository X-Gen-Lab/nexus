#include "nexus/modbus_rtu.h"

#include <limits.h>
#include <string.h>

static uint64_t deadline_add(uint64_t now, uint32_t duration) {
    return UINT64_MAX - now < duration ? UINT64_MAX : now + duration;
}

static uint16_t get_u16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void put_u16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

uint16_t nexus_modbus_crc16(const uint8_t* bytes, size_t length) {
    uint16_t crc = UINT16_C(0xffff);
    if (bytes == NULL && length != 0) {
        return 0;
    }
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (uint16_t)((crc >> 1) ^ ((crc & 1u) ? 0xa001u : 0u));
        }
    }
    return crc;
}

bool nexus_modbus_rtu_init(nexus_modbus_rtu_t* s,
                            const nexus_modbus_config_t* c) {
    if (s == NULL || c == NULL || c->address == 0 || c->address > 247 ||
        c->baudrate < 300 || c->baudrate > 4000000 ||
        c->bits_per_character < 10 || c->bits_per_character > 12 ||
        c->tx_timeout_us == 0 || c->abort_timeout_us == 0 ||
        c->registers.read == NULL || c->registers.authorize_write == NULL ||
        c->registers.write_atomic == NULL || c->port.now_us == NULL ||
        c->port.direction == NULL || c->port.write == NULL ||
        c->port.drain == NULL || c->port.abort_and_settle == NULL) {
        return false;
    }
    memset(s, 0, sizeof(*s));
    s->config = *c;
    if (c->baudrate > 19200) {
        s->t1_5_us = 750;
        s->t3_5_us = 1750;
    } else {
        const uint32_t numerator = (uint32_t)c->bits_per_character * 500000u;
        s->t1_5_us = (numerator * 3u + c->baudrate - 1u) / c->baudrate;
        s->t3_5_us = (numerator * 7u + c->baudrate - 1u) / c->baudrate;
    }
    /* The initial electrical direction is part of initialization. */
    if (!s->config.port.direction(s->config.port.context, false)) {
        s->state = NEXUS_MODBUS_LINK_FAULTED;
        return false;
    }
    s->last_event_us = s->config.port.now_us(s->config.port.context);
    s->last_byte_us = s->last_event_us;
    s->clock_started = true;
    s->state = NEXUS_MODBUS_RX_DISCARD; /* Require initial bus silence. */
    s->initialized = true;
    return true;
}

static bool clock_ok(nexus_modbus_rtu_t* s, uint64_t now) {
    if (s->clock_started && now < s->last_event_us) {
        ++s->counters.clock_errors;
        s->rx_length = 0;
        s->state = NEXUS_MODBUS_RX_DISCARD;
        /* Retain the previous high-water mark: recovery requires real time. */
        return false;
    }
    s->clock_started = true;
    s->last_event_us = now;
    return true;
}

static uint8_t exception_code(nexus_modbus_register_result_t result) {
    switch (result) {
        case NEXUS_MODBUS_ILLEGAL_ADDRESS:
        case NEXUS_MODBUS_ILLEGAL_VALUE:
        case NEXUS_MODBUS_DEVICE_FAILURE:
        case NEXUS_MODBUS_DEVICE_BUSY:
            return (uint8_t)result;
        default:
            return NEXUS_MODBUS_DEVICE_FAILURE;
    }
}

static void exception_response(nexus_modbus_rtu_t* s, uint8_t function,
                               uint8_t code) {
    s->tx[0] = s->config.address;
    s->tx[1] = (uint8_t)(function | 0x80u);
    s->tx[2] = code;
    s->tx_length = 3;
    ++s->counters.exceptions;
}

static bool range_valid(uint16_t start, uint16_t count) {
    return count != 0 && (uint32_t)start + count <= 65536u;
}

static void decode_frame(nexus_modbus_rtu_t* s) {
    ++s->counters.frames;
    s->tx_length = 0;
    if (s->rx_length < 4) {
        return;
    }
    const uint16_t payload = (uint16_t)(s->rx_length - 2u);
    const uint16_t expected = nexus_modbus_crc16(s->rx, payload);
    const uint16_t received = (uint16_t)(s->rx[payload] |
                                      ((uint16_t)s->rx[payload + 1] << 8));
    if (expected != received) {
        ++s->counters.crc_errors;
        return;
    }
    const bool broadcast = s->rx[0] == 0;
    if (!broadcast && s->rx[0] != s->config.address) {
        return;
    }
    const uint8_t function = s->rx[1];
    if (broadcast && function != 6 && function != 16) {
        return;
    }
    ++s->counters.accepted;
    if (broadcast) {
        ++s->counters.broadcasts;
    }
    nexus_modbus_register_result_t result = NEXUS_MODBUS_OK;
    uint16_t start = 0;
    uint16_t count = 0;
    if (function == 3 || function == 6) {
        if (s->rx_length != 8) {
            exception_response(s, function, NEXUS_MODBUS_ILLEGAL_VALUE);
            goto done;
        }
        start = get_u16(&s->rx[2]);
        count = function == 3 ? get_u16(&s->rx[4]) : 1;
        if (!range_valid(start, count) || count > NEXUS_MODBUS_REGISTER_MAX) {
            exception_response(s, function, NEXUS_MODBUS_ILLEGAL_VALUE);
            goto done;
        }
        if (function == 3) {
            result = s->config.registers.read(s->config.registers.context,
                                              start, count, s->values);
            if (result == NEXUS_MODBUS_OK) {
                s->tx[0] = s->config.address;
                s->tx[1] = function;
                s->tx[2] = (uint8_t)(count * 2u);
                for (uint16_t i = 0; i < count; ++i) {
                    put_u16(&s->tx[3u + (size_t)i * 2u], s->values[i]);
                }
                s->tx_length = (uint16_t)(3u + count * 2u);
            }
        } else {
            s->values[0] = get_u16(&s->rx[4]);
        }
    } else if (function == 16) {
        if (s->rx_length < 9) {
            exception_response(s, function, NEXUS_MODBUS_ILLEGAL_VALUE);
            goto done;
        }
        start = get_u16(&s->rx[2]);
        count = get_u16(&s->rx[4]);
        if (!range_valid(start, count) || count > 123 ||
            (uint16_t)s->rx[6] != count * 2u ||
            s->rx_length != (uint16_t)(9u + count * 2u)) {
            exception_response(s, function, NEXUS_MODBUS_ILLEGAL_VALUE);
            goto done;
        }
        for (uint16_t i = 0; i < count; ++i) {
            s->values[i] = get_u16(&s->rx[7u + (size_t)i * 2u]);
        }
    } else {
        exception_response(s, function, 1); /* illegal function */
        goto done;
    }
    if (function == 6 || function == 16) {
        result = s->config.registers.authorize_write(
            s->config.registers.context, start, count, s->values);
        if (result == NEXUS_MODBUS_OK) {
            result = s->config.registers.write_atomic(
                s->config.registers.context, start, count, s->values);
        }
        if (result == NEXUS_MODBUS_OK) {
            memcpy(s->tx, s->rx, 6);
            s->tx_length = 6;
        }
    }
    if (result != NEXUS_MODBUS_OK) {
        exception_response(s, function, exception_code(result));
    }
done:
    if (broadcast) {
        s->tx_length = 0; /* All broadcast failures remain silent. */
    }
    if (s->tx_length != 0) {
        const uint16_t crc = nexus_modbus_crc16(s->tx, s->tx_length);
        s->tx[s->tx_length++] = (uint8_t)crc;
        s->tx[s->tx_length++] = (uint8_t)(crc >> 8);
    }
}

static bool transmit(nexus_modbus_rtu_t* s) {
    nexus_modbus_rs485_port_t* p = &s->config.port;
    const uint64_t started = p->now_us(p->context);
    if (started < s->last_event_us) {
        ++s->counters.clock_errors;
        s->state = NEXUS_MODBUS_LINK_FAULTED;
        return false;
    }
    const uint64_t deadline = deadline_add(started,
                                          s->config.tx_timeout_us);
    bool active = p->direction(p->context, true);
    bool success = active && p->write(p->context, s->tx, s->tx_length, deadline);
    if (success && p->now_us(p->context) >= deadline) {
        success = false;
    }
    if (success) {
        success = p->drain(p->context, deadline);
        if (success && p->now_us(p->context) >= deadline) {
            success = false;
        }
    }
    if (!success) {
        ++s->counters.tx_errors;
        const uint64_t abort_deadline = deadline_add(p->now_us(p->context),
                                                    s->config.abort_timeout_us);
        if (!p->abort_and_settle(p->context, abort_deadline)) {
            s->state = NEXUS_MODBUS_LINK_FAULTED;
            return false;
        }
    }
    if (!p->direction(p->context, false)) {
        ++s->counters.tx_errors;
        s->state = NEXUS_MODBUS_LINK_FAULTED;
        return false;
    }
    s->tx_length = 0;
    /* Receiver silence is measured from actual wire completion, not request. */
    const uint64_t finished = p->now_us(p->context);
    if (finished < started) {
        ++s->counters.clock_errors;
        s->state = NEXUS_MODBUS_LINK_FAULTED;
        return false;
    }
    if (finished > s->last_event_us) {
        s->last_event_us = finished;
    }
    s->last_byte_us = s->last_event_us;
    s->state = NEXUS_MODBUS_RX_DISCARD;
    return success;
}

static bool finish_frame(nexus_modbus_rtu_t* s) {
    decode_frame(s);
    s->rx_length = 0;
    s->state = NEXUS_MODBUS_RX_IDLE;
    return s->tx_length == 0 || transmit(s);
}

bool nexus_modbus_rtu_feed(nexus_modbus_rtu_t* s, uint8_t byte,
                          uint64_t arrival) {
    if (s == NULL || !s->initialized || s->state == NEXUS_MODBUS_LINK_FAULTED) {
        return false;
    }
    if (!clock_ok(s, arrival)) {
        return false;
    }
    if (s->state != NEXUS_MODBUS_RX_IDLE) {
        const uint64_t gap = arrival - s->last_byte_us;
        if (gap >= s->t3_5_us) {
            if (s->state == NEXUS_MODBUS_RX_FRAME) {
                /* Silence must be serviced by poll before the next request.
                 * Starting a reply after receiving a new byte would collide
                 * on a half-duplex wire. Drop the unserviced old frame. */
                ++s->counters.timing_errors;
                s->rx_length = 0;
            }
            s->state = NEXUS_MODBUS_RX_IDLE;
        } else if (s->state == NEXUS_MODBUS_RX_DISCARD || gap > s->t1_5_us) {
            if (s->state != NEXUS_MODBUS_RX_DISCARD) {
                ++s->counters.timing_errors;
            }
            s->state = NEXUS_MODBUS_RX_DISCARD;
            s->rx_length = 0;
            s->last_byte_us = arrival;
            return false;
        }
    }
    s->last_byte_us = arrival;
    if (s->rx_length == NEXUS_MODBUS_ADU_MAX) {
        ++s->counters.overflows;
        s->state = NEXUS_MODBUS_RX_DISCARD;
        s->rx_length = 0;
        return false;
    }
    s->rx[s->rx_length++] = byte;
    s->state = NEXUS_MODBUS_RX_FRAME;
    return true;
}

bool nexus_modbus_rtu_poll(nexus_modbus_rtu_t* s, uint64_t now) {
    if (s == NULL || !s->initialized || s->state == NEXUS_MODBUS_LINK_FAULTED ||
        !clock_ok(s, now)) {
        return false;
    }
    if (s->state != NEXUS_MODBUS_RX_IDLE && now - s->last_byte_us >= s->t3_5_us) {
        if (s->state == NEXUS_MODBUS_RX_FRAME) {
            return finish_frame(s);
        }
        s->state = NEXUS_MODBUS_RX_IDLE;
    }
    return true;
}

void nexus_modbus_rtu_rx_error(nexus_modbus_rtu_t* s, uint64_t now) {
    if (s == NULL || !s->initialized || s->state == NEXUS_MODBUS_LINK_FAULTED) {
        return;
    }
    ++s->counters.overflows;
    if (clock_ok(s, now)) {
        s->last_byte_us = now;
    }
    s->rx_length = 0;
    s->state = NEXUS_MODBUS_RX_DISCARD;
}

bool nexus_modbus_rtu_recover(nexus_modbus_rtu_t* s) {
    if (s == NULL || !s->initialized) {
        return false;
    }
    nexus_modbus_rs485_port_t* p = &s->config.port;
    const uint64_t now = p->now_us(p->context);
    if (!p->abort_and_settle(p->context,
                              deadline_add(now, s->config.abort_timeout_us)) ||
        !p->direction(p->context, false)) {
        s->state = NEXUS_MODBUS_LINK_FAULTED;
        return false;
    }
    s->tx_length = 0;
    s->rx_length = 0;
    s->clock_started = true;
    s->last_event_us = p->now_us(p->context);
    s->last_byte_us = s->last_event_us;
    s->state = NEXUS_MODBUS_RX_DISCARD;
    return true;
}
