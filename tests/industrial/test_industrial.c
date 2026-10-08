#include "nexus/industrial_supervisor.h"
#include "nexus/modbus_rtu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CHECK remains active in Release builds. */
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); } } while (0)

typedef struct {
    uint64_t now;
    uint16_t registers[128];
    uint8_t reply[256];
    size_t reply_length;
    unsigned sends;
    unsigned writes;
    unsigned aborts;
    bool transmit;
    bool owned;
    bool allow_critical;
    bool fail_write;
    bool fail_drain;
    bool fail_abort;
    bool fail_direction;
    bool fail_commit;
    bool unsafe_deassert;
} modbus_fixture_t;

static uint64_t port_now(void* context) {
    return ((modbus_fixture_t*)context)->now;
}

static bool port_direction(void* context, bool tx) {
    modbus_fixture_t* f = context;
    if (!tx && f->owned) {
        f->unsafe_deassert = true;
        return false;
    }
    if (f->fail_direction) {
        return false;
    }
    f->transmit = tx;
    return true;
}

static bool port_write(void* context, const uint8_t* bytes, size_t length,
                       uint64_t deadline) {
    modbus_fixture_t* f = context;
    CHECK(f->transmit && !f->owned);
    CHECK(length <= sizeof(f->reply));
    CHECK(deadline >= f->now);
    memcpy(f->reply, bytes, length);
    f->reply_length = length;
    ++f->sends;
    f->owned = true;
    return !f->fail_write;
}

static bool port_drain(void* context, uint64_t deadline) {
    modbus_fixture_t* f = context;
    CHECK(f->owned && deadline >= f->now);
    if (f->fail_drain) {
        return false;
    }
    f->now += f->reply_length * 100u;
    f->owned = false;
    return true;
}

static bool port_abort(void* context, uint64_t deadline) {
    modbus_fixture_t* f = context;
    CHECK(deadline >= f->now);
    ++f->aborts;
    if (f->fail_abort) {
        return false;
    }
    f->owned = false;
    return true;
}

static nexus_modbus_register_result_t read_registers(void* context,
    uint16_t start, uint16_t count, uint16_t* values) {
    modbus_fixture_t* f = context;
    if ((uint32_t)start + count > 128u) {
        return NEXUS_MODBUS_ILLEGAL_ADDRESS;
    }
    memcpy(values, &f->registers[start], (size_t)count * sizeof(uint16_t));
    return NEXUS_MODBUS_OK;
}

static nexus_modbus_register_result_t authorize_write(void* context,
    uint16_t start, uint16_t count, const uint16_t* values) {
    modbus_fixture_t* f = context;
    (void)values;
    if ((uint32_t)start + count > 128u) {
        return NEXUS_MODBUS_ILLEGAL_ADDRESS;
    }
    if ((uint32_t)start + count > 120u && !f->allow_critical) {
        return NEXUS_MODBUS_DEVICE_BUSY;
    }
    return NEXUS_MODBUS_OK;
}

static nexus_modbus_register_result_t write_registers(void* context,
    uint16_t start, uint16_t count, const uint16_t* values) {
    modbus_fixture_t* f = context;
    if (f->fail_commit) {
        return NEXUS_MODBUS_DEVICE_FAILURE;
    }
    memcpy(&f->registers[start], values, (size_t)count * sizeof(uint16_t));
    ++f->writes;
    return NEXUS_MODBUS_OK;
}

static nexus_modbus_config_t modbus_config(modbus_fixture_t* f) {
    nexus_modbus_config_t config = {
        .address = 17, .baudrate = 115200, .bits_per_character = 11,
        .tx_timeout_us = 100000, .abort_timeout_us = 1000,
        .registers = {f, read_registers, authorize_write, write_registers},
        .port = {f, port_now, port_direction, port_write, port_drain, port_abort}
    };
    return config;
}

static void initialize(modbus_fixture_t* f, nexus_modbus_rtu_t* s) {
    memset(f, 0, sizeof(*f));
    f->now = 10000;
    for (unsigned i = 0; i < 128; ++i) {
        f->registers[i] = (uint16_t)(i + 1000u);
    }
    const nexus_modbus_config_t config = modbus_config(f);
    CHECK(nexus_modbus_rtu_init(s, &config));
    f->now += s->t3_5_us;
    CHECK(nexus_modbus_rtu_poll(s, f->now));
}

static size_t append_crc(uint8_t* bytes, size_t length) {
    const uint16_t crc = nexus_modbus_crc16(bytes, length);
    bytes[length] = (uint8_t)crc;
    bytes[length + 1] = (uint8_t)(crc >> 8);
    return length + 2;
}

static bool request(modbus_fixture_t* f, nexus_modbus_rtu_t* s,
                    uint8_t* bytes, size_t length) {
    length = append_crc(bytes, length);
    f->now += s->t3_5_us + 100u;
    for (size_t i = 0; i < length; ++i) {
        CHECK(nexus_modbus_rtu_feed(s, bytes[i], f->now));
        f->now += 100;
    }
    f->now += s->t3_5_us;
    return nexus_modbus_rtu_poll(s, f->now);
}

static void check_exception(const modbus_fixture_t* f, uint8_t function,
                            uint8_t code) {
    CHECK(f->reply_length == 5);
    CHECK(f->reply[0] == 17 && f->reply[1] == (function | 0x80u));
    CHECK(f->reply[2] == code);
    CHECK(nexus_modbus_crc16(f->reply, f->reply_length) == 0);
}

static void test_crc_and_config(void) {
    const uint8_t vector[] = {1, 3, 0, 0, 0, 10};
    CHECK(nexus_modbus_crc16(vector, sizeof(vector)) == 0xcdc5);
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    CHECK(s.t1_5_us == 750 && s.t3_5_us == 1750);
    nexus_modbus_config_t c = modbus_config(&f);
    c.baudrate = 9600;
    CHECK(nexus_modbus_rtu_init(&s, &c));
    CHECK(s.t1_5_us == 1719 && s.t3_5_us == 4011);
    c.address = 0;
    CHECK(!nexus_modbus_rtu_init(&s, &c));
    c.address = 248;
    CHECK(!nexus_modbus_rtu_init(&s, &c));
    c.address = 17;
    c.registers.authorize_write = NULL;
    CHECK(!nexus_modbus_rtu_init(&s, &c));
    CHECK(!nexus_modbus_rtu_init(NULL, &c));
}

static void test_read_and_maximum(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    uint8_t packet[256] = {17, 3, 0, 1, 0, 2};
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.reply_length == 9 && f.reply[2] == 4);
    CHECK(f.reply[3] == 3 && f.reply[4] == 0xe9);
    CHECK(nexus_modbus_crc16(f.reply, f.reply_length) == 0);
    CHECK(!f.transmit && !f.owned);
    packet[3] = 0;
    packet[5] = 125;
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.reply_length == 255 && f.reply[2] == 250);
    packet[5] = 126;
    CHECK(request(&f, &s, packet, 6));
    check_exception(&f, 3, 3);
}

static void test_single_and_critical_write(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    uint8_t packet[16] = {17, 6, 0, 5, 0xab, 0xcd};
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.registers[5] == 0xabcd && f.writes == 1);
    CHECK(f.reply_length == 8 && memcmp(f.reply, packet, 8) == 0);
    packet[3] = 120;
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.registers[120] == 1120 && f.writes == 1);
    check_exception(&f, 6, 6);
    f.allow_critical = true;
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.registers[120] == 0xabcd && f.writes == 2);
}

static void test_multiple_write_and_atomic_failure(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    uint8_t packet[256] = {17, 16, 0, 10, 0, 2, 4, 0, 11, 0, 12};
    CHECK(request(&f, &s, packet, 11));
    CHECK(f.registers[10] == 11 && f.registers[11] == 12);
    CHECK(f.reply_length == 8 && memcmp(f.reply, packet, 6) == 0);
    f.fail_commit = true;
    packet[8] = 55;
    packet[10] = 66;
    CHECK(request(&f, &s, packet, 11));
    check_exception(&f, 16, 4);
    CHECK(f.registers[10] == 11 && f.registers[11] == 12);
    CHECK(f.writes == 1);
    packet[6] = 3;
    CHECK(request(&f, &s, packet, 11));
    check_exception(&f, 16, 3);
}

static void test_multiple_maximum(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    f.allow_critical = true;
    uint8_t packet[256] = {17, 16, 0, 0, 0, 123, 246};
    for (unsigned i = 0; i < 123; ++i) {
        packet[7 + i * 2] = 0;
        packet[8 + i * 2] = (uint8_t)i;
    }
    CHECK(request(&f, &s, packet, 253));
    CHECK(f.writes == 1 && f.registers[122] == 122);
    CHECK(f.reply_length == 8);
    packet[5] = 124;
    CHECK(request(&f, &s, packet, 253));
    check_exception(&f, 16, 3);
}

static void test_address_crc_and_broadcast(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    uint8_t packet[16] = {18, 3, 0, 0, 0, 1};
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.sends == 0);
    packet[0] = 0;
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.sends == 0 && f.writes == 0);
    packet[1] = 6;
    packet[3] = 7;
    packet[5] = 77;
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.sends == 0 && f.writes == 1 && f.registers[7] == 77);
    packet[3] = 120;
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.sends == 0 && f.writes == 1 && f.registers[120] == 1120);
    packet[0] = 17;
    size_t length = append_crc(packet, 6);
    packet[length - 1] ^= 1;
    f.now += s.t3_5_us + 100;
    for (size_t i = 0; i < length; ++i) {
        CHECK(nexus_modbus_rtu_feed(&s, packet[i], f.now++));
    }
    f.now += s.t3_5_us;
    CHECK(nexus_modbus_rtu_poll(&s, f.now));
    CHECK(s.counters.crc_errors == 1 && f.sends == 0);
}

static void test_functions_lengths_and_ranges(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    uint8_t packet[16] = {17, 99, 0, 0, 0, 1};
    CHECK(request(&f, &s, packet, 6));
    check_exception(&f, 99, 1);
    packet[1] = 3;
    CHECK(request(&f, &s, packet, 5));
    check_exception(&f, 3, 3);
    packet[2] = 0xff;
    packet[3] = 0xff;
    packet[4] = 0;
    packet[5] = 2;
    CHECK(request(&f, &s, packet, 6));
    check_exception(&f, 3, 3);
    packet[2] = 0;
    packet[3] = 127;
    CHECK(request(&f, &s, packet, 6));
    check_exception(&f, 3, 2);
    packet[5] = 0;
    CHECK(request(&f, &s, packet, 6));
    check_exception(&f, 3, 3);
}

static void test_intercharacter_and_overrun(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    CHECK(nexus_modbus_rtu_feed(&s, 17, f.now));
    f.now += s.t1_5_us + 1;
    CHECK(!nexus_modbus_rtu_feed(&s, 6, f.now));
    CHECK(s.state == NEXUS_MODBUS_RX_DISCARD);
    f.now += s.t3_5_us;
    CHECK(nexus_modbus_rtu_poll(&s, f.now));
    CHECK(s.state == NEXUS_MODBUS_RX_IDLE && f.writes == 0);
    for (unsigned i = 0; i < 256; ++i) {
        CHECK(nexus_modbus_rtu_feed(&s, 0x55, ++f.now));
    }
    CHECK(!nexus_modbus_rtu_feed(&s, 0x55, ++f.now));
    CHECK(s.counters.overflows == 1 && s.rx_length == 0);
    nexus_modbus_rtu_rx_error(&s, ++f.now);
    CHECK(s.counters.overflows == 2);
    f.now += s.t3_5_us;
    CHECK(nexus_modbus_rtu_poll(&s, f.now));
    uint8_t packet[8] = {17, 3, 0, 0, 0, 1};
    CHECK(request(&f, &s, packet, 6));
    CHECK(f.sends == 1 && s.counters.timing_errors == 1);
}

static void test_unserviced_silence_and_clock(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    uint8_t packet[8] = {17, 6, 0, 0, 0, 1};
    const size_t length = append_crc(packet, 6);
    for (size_t i = 0; i < length; ++i) {
        CHECK(nexus_modbus_rtu_feed(&s, packet[i], ++f.now));
    }
    f.now += s.t3_5_us;
    CHECK(nexus_modbus_rtu_feed(&s, 17, f.now));
    CHECK(f.sends == 0 && f.writes == 0 && s.counters.timing_errors == 1);
    CHECK(!nexus_modbus_rtu_poll(&s, f.now - 1));
    CHECK(s.counters.clock_errors == 1);
    f.now += s.t3_5_us;
    CHECK(nexus_modbus_rtu_poll(&s, f.now));
    CHECK(s.state == NEXUS_MODBUS_RX_IDLE);
}

static void test_tx_abort_and_retained_ownership(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    uint8_t packet[8] = {17, 3, 0, 0, 0, 1};
    f.fail_drain = true;
    CHECK(!request(&f, &s, packet, 6));
    CHECK(f.aborts == 1 && !f.owned && !f.transmit);
    CHECK(s.state != NEXUS_MODBUS_LINK_FAULTED);
    f.fail_abort = true;
    CHECK(!request(&f, &s, packet, 6));
    CHECK(f.owned && f.transmit && !f.unsafe_deassert);
    CHECK(s.state == NEXUS_MODBUS_LINK_FAULTED && s.tx_length != 0);
    const unsigned sends = f.sends;
    CHECK(!nexus_modbus_rtu_feed(&s, 17, ++f.now));
    CHECK(!nexus_modbus_rtu_poll(&s, ++f.now));
    CHECK(f.sends == sends);
    CHECK(!nexus_modbus_rtu_recover(&s));
    f.fail_abort = false;
    f.fail_drain = false;
    CHECK(nexus_modbus_rtu_recover(&s));
    CHECK(!f.owned && !f.transmit && s.tx_length == 0);
    CHECK(request(&f, &s, packet, 6));
    CHECK(!f.unsafe_deassert);
}

static void test_tx_write_and_direction_failure(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    uint8_t packet[8] = {17, 3, 0, 0, 0, 1};
    f.fail_write = true;
    CHECK(!request(&f, &s, packet, 6));
    CHECK(f.aborts == 1 && !f.owned && !f.transmit);
    f.fail_write = false;
    f.fail_direction = true;
    CHECK(!request(&f, &s, packet, 6));
    CHECK(s.state == NEXUS_MODBUS_LINK_FAULTED);
    f.fail_direction = false;
    CHECK(nexus_modbus_rtu_recover(&s));
    CHECK(request(&f, &s, packet, 6));
}

static void test_tx_absolute_deadline(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    /* A port that reports success after its absolute deadline is rejected. */
    s.config.tx_timeout_us = 10;
    uint8_t packet[8] = {17, 3, 0, 0, 0, 1};
    CHECK(!request(&f, &s, packet, 6));
    CHECK(s.counters.tx_errors == 1 && f.aborts == 1);
    CHECK(!f.owned && !f.transmit && !f.unsafe_deassert);
}

static void test_parser_fuzz(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    uint32_t seed = 0x541dc37u;
    for (unsigned i = 0; i < 50000; ++i) {
        /* Deterministic input generator is test data, never an entropy source. */
        seed = seed * 1664525u + 1013904223u;
        f.now += (seed & 4095u);
        (void)nexus_modbus_rtu_feed(&s, (uint8_t)(seed >> 24), f.now);
        if ((i & 31u) == 0) {
            f.now += s.t3_5_us;
            (void)nexus_modbus_rtu_poll(&s, f.now);
        }
        CHECK(s.rx_length <= NEXUS_MODBUS_ADU_MAX);
        CHECK(s.tx_length <= NEXUS_MODBUS_ADU_MAX);
        if (f.reply_length != 0) {
            CHECK(nexus_modbus_crc16(f.reply, f.reply_length) == 0);
        }
    }
    CHECK(!f.unsafe_deassert && !f.owned);
}

static void test_structured_valid_crc_fuzz(void) {
    modbus_fixture_t f;
    nexus_modbus_rtu_t s;
    initialize(&f, &s);
    f.allow_critical = true;
    uint32_t seed = 0x719de25u;
    uint8_t packet[256];
    for (unsigned iteration = 0; iteration < 10000; ++iteration) {
        for (size_t i = 0; i < sizeof(packet); ++i) {
            seed = seed * 1664525u + 1013904223u;
            packet[i] = (uint8_t)(seed >> 24);
        }
        packet[0] = (iteration % 7u == 0) ? 0 : 17;
        const uint8_t functions[] = {3, 6, 16, 4, 0x80};
        packet[1] = functions[iteration % 5u];
        size_t length = 2u + (seed % 252u);
        if (iteration % 3u == 0) {
            packet[2] = 0;
            packet[3] = (uint8_t)(iteration % 140u);
            packet[4] = 0;
            packet[5] = (uint8_t)(iteration % 126u);
            length = 6;
            if (packet[1] == 16) {
                /* Permit malformed count and byte-count combinations too. */
                const unsigned count = iteration % 125u;
                packet[5] = (uint8_t)count;
                packet[6] = (uint8_t)(count * 2u);
                length = 7u + count * 2u;
                if (length > 253u) {
                    length = 253;
                }
            }
        }
        const unsigned sends = f.sends;
        CHECK(request(&f, &s, packet, length));
        CHECK(!f.owned && !f.unsafe_deassert);
        CHECK(s.rx_length == 0 && s.tx_length == 0);
        if (packet[0] == 0) {
            CHECK(f.sends == sends);
        } else if (f.sends != sends) {
            CHECK(f.reply_length <= 256);
            CHECK(nexus_modbus_crc16(f.reply, f.reply_length) == 0);
        }
    }
}

typedef struct {
    unsigned safe_calls;
    unsigned feeds;
    bool fail_safe;
    bool fail_feed;
} supervisor_fixture_t;

static bool safe_output(void* context, nexus_industrial_fault_t fault) {
    supervisor_fixture_t* f = context;
    (void)fault;
    ++f->safe_calls;
    return !f->fail_safe;
}

static bool watchdog(void* context) {
    supervisor_fixture_t* f = context;
    ++f->feeds;
    return !f->fail_feed;
}

static nexus_industrial_config_t supervisor_config(supervisor_fixture_t* f) {
    nexus_industrial_config_t c = {
        .cycle_us = 1000, .job_count = 3,
        .jobs = {{200}, {500}, {900}},
        .build_id = "industrial-test-build", .reset_reason = 0x1234,
        .context = f, .enter_safe = safe_output, .feed_watchdog = watchdog
    };
    return c;
}

static void supervisor_initialize(supervisor_fixture_t* f,
                                  nexus_industrial_supervisor_t* s) {
    memset(f, 0, sizeof(*f));
    const nexus_industrial_config_t config = supervisor_config(f);
    CHECK(nexus_industrial_init(s, &config, 10000));
    CHECK(f->safe_calls == 1 && f->feeds == 0);
}

static void healthy_cycle(nexus_industrial_supervisor_t* s, uint64_t start) {
    const uint64_t token = nexus_industrial_cycle(s);
    CHECK(nexus_industrial_report(s, 0, token, start + 100, NEXUS_DATA_VALID));
    CHECK(nexus_industrial_report(s, 1, token, start + 300, NEXUS_DATA_VALID));
    CHECK(nexus_industrial_report(s, 2, token, start + 600, NEXUS_DATA_VALID));
}

static void test_supervisor_gating_and_cycles(void) {
    supervisor_fixture_t f;
    nexus_industrial_supervisor_t s;
    supervisor_initialize(&f, &s);
    CHECK(nexus_industrial_report(&s, 0, 1, 10100, NEXUS_DATA_VALID));
    CHECK(f.feeds == 0 && s.state == NEXUS_INDUSTRIAL_STARTING);
    CHECK(!nexus_industrial_report(&s, 0, 1, 10101, NEXUS_DATA_VALID));
    CHECK(nexus_industrial_report(&s, 1, 1, 10300, NEXUS_DATA_VALID));
    CHECK(f.feeds == 0);
    CHECK(nexus_industrial_report(&s, 2, 1, 10600, NEXUS_DATA_VALID));
    CHECK(f.feeds == 1 && s.state == NEXUS_INDUSTRIAL_RUNNING);
    CHECK(nexus_industrial_poll(&s, 11000));
    CHECK(nexus_industrial_cycle(&s) == 2);
    CHECK(!nexus_industrial_report(&s, 1, 1, 10300, NEXUS_DATA_VALID));
    healthy_cycle(&s, 11000);
    CHECK(f.feeds == 2 && s.watchdog_feeds == 2);
    CHECK(s.fault == NEXUS_INDUSTRIAL_FAULT_NONE);
}

static void test_supervisor_deadline_and_quality(void) {
    supervisor_fixture_t f;
    nexus_industrial_supervisor_t s;
    supervisor_initialize(&f, &s);
    CHECK(!nexus_industrial_report(&s, 0, 1, 10200, NEXUS_DATA_VALID));
    CHECK(s.state == NEXUS_INDUSTRIAL_SAFE_FAULT);
    CHECK(s.fault == NEXUS_INDUSTRIAL_FAULT_DEADLINE && f.feeds == 0);
    CHECK(f.safe_calls == 2);
    CHECK(!nexus_industrial_poll(&s, 10500));
    CHECK(f.safe_calls == 2);
    CHECK(nexus_industrial_rearm(&s, 11000));
    CHECK(nexus_industrial_cycle(&s) == 2);
    CHECK(!nexus_industrial_report(&s, 0, 1, 10100, NEXUS_DATA_VALID));
    CHECK(!nexus_industrial_report(&s, 0, 2, 11100, NEXUS_DATA_STALE));
    CHECK(s.fault == NEXUS_INDUSTRIAL_FAULT_DATA && f.feeds == 0);
}

static void test_supervisor_missing_job_and_skipped_cycles(void) {
    supervisor_fixture_t f;
    nexus_industrial_supervisor_t s;
    supervisor_initialize(&f, &s);
    CHECK(nexus_industrial_report(&s, 0, 1, 10100, NEXUS_DATA_VALID));
    CHECK(!nexus_industrial_poll(&s, 10500));
    CHECK(s.fault == NEXUS_INDUSTRIAL_FAULT_DEADLINE && f.feeds == 0);
    CHECK(nexus_industrial_rearm(&s, 11000));
    healthy_cycle(&s, 11000);
    CHECK(!nexus_industrial_poll(&s, 13000));
    CHECK(s.fault == NEXUS_INDUSTRIAL_FAULT_DEADLINE && f.feeds == 1);
}

static void test_supervisor_clock_and_ports(void) {
    supervisor_fixture_t f;
    nexus_industrial_supervisor_t s;
    supervisor_initialize(&f, &s);
    CHECK(!nexus_industrial_poll(&s, 9999));
    CHECK(s.fault == NEXUS_INDUSTRIAL_FAULT_CLOCK && f.feeds == 0);
    CHECK(nexus_industrial_rearm(&s, 11000));
    f.fail_feed = true;
    CHECK(nexus_industrial_report(&s, 0, 2, 11100, NEXUS_DATA_VALID));
    CHECK(nexus_industrial_report(&s, 1, 2, 11300, NEXUS_DATA_VALID));
    CHECK(!nexus_industrial_report(&s, 2, 2, 11600, NEXUS_DATA_VALID));
    CHECK(s.fault == NEXUS_INDUSTRIAL_FAULT_WATCHDOG_PORT);
    CHECK(s.watchdog_feeds == 0);
    f.fail_safe = true;
    CHECK(!nexus_industrial_rearm(&s, 12000));
    CHECK(s.state == NEXUS_INDUSTRIAL_SAFE_FAULT);
}

static void test_supervisor_diagnostics_and_configuration(void) {
    supervisor_fixture_t f;
    nexus_industrial_supervisor_t s;
    supervisor_initialize(&f, &s);
    CHECK(strcmp(s.build_id, "industrial-test-build") == 0);
    CHECK(s.config.reset_reason == 0x1234);
    for (uint64_t i = 0; i < 20; ++i) {
        CHECK(nexus_industrial_rearm(&s, 10000 + i * 1000));
        nexus_industrial_trip(&s, NEXUS_INDUSTRIAL_FAULT_EXTERNAL,
                              10000 + i * 1000);
    }
    CHECK(s.event_count == 16 && s.events_dropped == 25);
    nexus_industrial_event_t event;
    unsigned popped = 0;
    while (nexus_industrial_event_pop(&s, &event)) {
        ++popped;
        CHECK(event.kind == NEXUS_INDUSTRIAL_EVENT_RESET ||
              event.kind == NEXUS_INDUSTRIAL_EVENT_FAULT);
    }
    CHECK(popped == 16);
    nexus_industrial_config_t c = supervisor_config(&f);
    c.job_count = 9;
    CHECK(!nexus_industrial_init(&s, &c, 0));
    c.job_count = 3;
    c.jobs[0].deadline_us = 0;
    CHECK(!nexus_industrial_init(&s, &c, 0));
    c.jobs[0].deadline_us = 1001;
    CHECK(!nexus_industrial_init(&s, &c, 0));
    c.jobs[0].deadline_us = 200;
    c.build_id = "";
    CHECK(!nexus_industrial_init(&s, &c, 0));
    c.build_id = "valid";
    CHECK(!nexus_industrial_init(&s, &c, UINT64_MAX - 500));
}

typedef void (*test_fn)(void);
int main(void) {
    const struct { const char* name; test_fn run; } tests[] = {
        {"crc_and_config", test_crc_and_config},
        {"read_and_maximum", test_read_and_maximum},
        {"single_and_critical_write", test_single_and_critical_write},
        {"multiple_write_and_atomic_failure", test_multiple_write_and_atomic_failure},
        {"multiple_maximum", test_multiple_maximum},
        {"address_crc_and_broadcast", test_address_crc_and_broadcast},
        {"functions_lengths_and_ranges", test_functions_lengths_and_ranges},
        {"intercharacter_and_overrun", test_intercharacter_and_overrun},
        {"unserviced_silence_and_clock", test_unserviced_silence_and_clock},
        {"tx_abort_and_retained_ownership", test_tx_abort_and_retained_ownership},
        {"tx_write_and_direction_failure", test_tx_write_and_direction_failure},
        {"tx_absolute_deadline", test_tx_absolute_deadline},
        {"parser_fuzz_50000_bytes", test_parser_fuzz},
        {"structured_valid_crc_fuzz_10000_frames", test_structured_valid_crc_fuzz},
        {"supervisor_gating_and_cycles", test_supervisor_gating_and_cycles},
        {"supervisor_deadline_and_quality", test_supervisor_deadline_and_quality},
        {"supervisor_missing_job_and_skipped_cycles", test_supervisor_missing_job_and_skipped_cycles},
        {"supervisor_clock_and_ports", test_supervisor_clock_and_ports},
        {"supervisor_diagnostics_and_configuration", test_supervisor_diagnostics_and_configuration}
    };
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        tests[i].run();
        printf("PASS %s\n", tests[i].name);
    }
    printf("19 industrial contract cases passed; rtu=%zu supervisor=%zu bytes\n",
           sizeof(nexus_modbus_rtu_t), sizeof(nexus_industrial_supervisor_t));
    return 0;
}
