/**
 * \file            modbus_rtu_test.cpp
 * \brief           Modbus RTU wire codec and explicit RS485 lifetime tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-26
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/modbus_rtu.h"

#include <cstring>
#include <gtest/gtest.h>

namespace {

struct ModbusBoundary {
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
};

/** \brief The caller schedules every timestamp used by the real parser. */
static uint64_t port_now(void* context) {
    return static_cast<ModbusBoundary*>(context)->now;
}

/** \brief DE cannot deassert while the transport retains reply ownership. */
static bool port_direction(void* context, bool tx) {
    auto* f = static_cast<ModbusBoundary*>(context);
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

/** \brief A failed write still requires a successful drain or abort. */
static bool port_write(void* context, const uint8_t* bytes, size_t length,
                       uint64_t deadline) {
    auto* f = static_cast<ModbusBoundary*>(context);
    EXPECT_TRUE(f->transmit);
    EXPECT_FALSE(f->owned);
    EXPECT_LE(length, sizeof(f->reply));
    EXPECT_GE(deadline, f->now);
    if (!f->transmit || f->owned || length > sizeof(f->reply) ||
        deadline < f->now) {
        return false;
    }
    std::memcpy(f->reply, bytes, length);
    f->reply_length = length;
    ++f->sends;
    f->owned = true;
    return !f->fail_write;
}

/** \brief Advance wire time only after the port observes completion. */
static bool port_drain(void* context, uint64_t deadline) {
    auto* f = static_cast<ModbusBoundary*>(context);
    EXPECT_TRUE(f->owned);
    EXPECT_GE(deadline, f->now);
    if (!f->owned || deadline < f->now) {
        return false;
    }
    if (f->fail_drain) {
        return false;
    }
    f->now += f->reply_length * 100u;
    f->owned = false;
    return true;
}

/** \brief Failed abort preserves the outstanding transport ownership. */
static bool port_abort(void* context, uint64_t deadline) {
    auto* f = static_cast<ModbusBoundary*>(context);
    EXPECT_GE(deadline, f->now);
    if (deadline < f->now) {
        return false;
    }
    ++f->aborts;
    if (f->fail_abort) {
        return false;
    }
    f->owned = false;
    return true;
}

/** \brief Read one complete requested range from the external register bank. */
static nx_modbus_register_result_t read_registers(void* context, uint16_t start,
                                                  uint16_t count,
                                                  uint16_t* values) {
    auto* f = static_cast<ModbusBoundary*>(context);
    if (static_cast<uint32_t>(start) + count > 128u) {
        return NX_MODBUS_ILLEGAL_ADDRESS;
    }
    std::memcpy(values, &f->registers[start],
                static_cast<size_t>(count) * sizeof(uint16_t));
    return NX_MODBUS_OK;
}

/** \brief Authorize the complete register range before any commit. */
static nx_modbus_register_result_t authorize_write(void* context,
                                                   uint16_t start,
                                                   uint16_t count,
                                                   const uint16_t* values) {
    auto* f = static_cast<ModbusBoundary*>(context);
    (void)values;
    if (static_cast<uint32_t>(start) + count > 128u) {
        return NX_MODBUS_ILLEGAL_ADDRESS;
    }
    if (static_cast<uint32_t>(start) + count > 120u && !f->allow_critical) {
        return NX_MODBUS_DEVICE_BUSY;
    }
    return NX_MODBUS_OK;
}

/** \brief A rejected commit leaves the complete register range untouched. */
static nx_modbus_register_result_t write_registers(void* context,
                                                   uint16_t start,
                                                   uint16_t count,
                                                   const uint16_t* values) {
    auto* f = static_cast<ModbusBoundary*>(context);
    if (f->fail_commit) {
        return NX_MODBUS_DEVICE_FAILURE;
    }
    std::memcpy(&f->registers[start], values,
                static_cast<size_t>(count) * sizeof(uint16_t));
    ++f->writes;
    return NX_MODBUS_OK;
}

static nx_modbus_config_t modbus_config(ModbusBoundary* f) {
    const nx_modbus_config_t config{
        17,
        115200,
        11,
        100000,
        1000,
        {f, read_registers, authorize_write, write_registers},
        {f, port_now, port_direction, port_write, port_drain, port_abort}};
    return config;
}

/** \brief Acquire initial RTU silence before accepting request bytes. */
static void initialize(ModbusBoundary* f, nx_modbus_rtu_t* s) {
    std::memset(f, 0, sizeof(*f));
    f->now = 10000;
    for (unsigned i = 0; i < 128; ++i) {
        f->registers[i] = static_cast<uint16_t>(i + 1000U);
    }
    const nx_modbus_config_t config = modbus_config(f);
    ASSERT_TRUE(nx_modbus_rtu_init(s, &config));
    f->now += s->t3_5_us;
    ASSERT_TRUE(nx_modbus_rtu_poll(s, f->now));
}

static size_t append_crc(uint8_t* bytes, size_t length) {
    const uint16_t crc = nx_modbus_crc16(bytes, length);
    bytes[length] = static_cast<uint8_t>(crc);
    bytes[length + 1] = static_cast<uint8_t>(crc >> 8U);
    return length + 2;
}

/** \brief Deliver timed bytes and service the completed t3.5 silence. */
static bool request(ModbusBoundary* f, nx_modbus_rtu_t* s, uint8_t* bytes,
                    size_t length) {
    length = append_crc(bytes, length);
    f->now += s->t3_5_us + 100u;
    for (size_t i = 0; i < length; ++i) {
        const bool accepted = nx_modbus_rtu_feed(s, bytes[i], f->now);
        EXPECT_TRUE(accepted);
        if (!accepted) {
            return false;
        }
        f->now += 100;
    }
    f->now += s->t3_5_us;
    return nx_modbus_rtu_poll(s, f->now);
}

/** \brief Protocol exceptions remain complete CRC-valid wire frames. */
static void check_exception(const ModbusBoundary* f, uint8_t function,
                            uint8_t code) {
    ASSERT_TRUE(f->reply_length == 5);
    ASSERT_TRUE(f->reply[0] == 17 && f->reply[1] == (function | 0x80u));
    ASSERT_TRUE(f->reply[2] == code);
    ASSERT_TRUE(nx_modbus_crc16(f->reply, f->reply_length) == 0);
}

TEST(ModbusRtu, CrcAndConfig) {
    const uint8_t vector[] = {1, 3, 0, 0, 0, 10};
    ASSERT_TRUE(nx_modbus_crc16(vector, sizeof(vector)) == 0xcdc5);
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    ASSERT_TRUE(s.t1_5_us == 750 && s.t3_5_us == 1750);
    nx_modbus_config_t c = modbus_config(&f);
    c.baudrate = 9600;
    ASSERT_TRUE(nx_modbus_rtu_init(&s, &c));
    ASSERT_TRUE(s.t1_5_us == 1719 && s.t3_5_us == 4011);
    c.address = 0;
    ASSERT_FALSE(nx_modbus_rtu_init(&s, &c));
    c.address = 248;
    ASSERT_FALSE(nx_modbus_rtu_init(&s, &c));
    c.address = 17;
    c.registers.authorize_write = nullptr;
    ASSERT_FALSE(nx_modbus_rtu_init(&s, &c));
    ASSERT_FALSE(nx_modbus_rtu_init(nullptr, &c));
}

TEST(ModbusRtu, ReadAndMaximum) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    uint8_t packet[256] = {17, 3, 0, 1, 0, 2};
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.reply_length == 9 && f.reply[2] == 4);
    ASSERT_TRUE(f.reply[3] == 3 && f.reply[4] == 0xe9);
    ASSERT_TRUE(nx_modbus_crc16(f.reply, f.reply_length) == 0);
    ASSERT_FALSE(f.transmit && !f.owned);
    packet[3] = 0;
    packet[5] = 125;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.reply_length == 255 && f.reply[2] == 250);
    packet[5] = 126;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 3, 3));
}

TEST(ModbusRtu, SingleAndCriticalWrite) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    uint8_t packet[16] = {17, 6, 0, 5, 0xab, 0xcd};
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.registers[5] == 0xabcd && f.writes == 1);
    ASSERT_TRUE(f.reply_length == 8 && std::memcmp(f.reply, packet, 8) == 0);
    packet[3] = 120;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.registers[120] == 1120 && f.writes == 1);
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 6, 6));
    f.allow_critical = true;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.registers[120] == 0xabcd && f.writes == 2);
}

TEST(ModbusRtu, MultipleWriteAndAtomicFailure) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    uint8_t packet[256] = {17, 16, 0, 10, 0, 2, 4, 0, 11, 0, 12};
    ASSERT_TRUE(request(&f, &s, packet, 11));
    ASSERT_TRUE(f.registers[10] == 11 && f.registers[11] == 12);
    ASSERT_TRUE(f.reply_length == 8 && std::memcmp(f.reply, packet, 6) == 0);
    f.fail_commit = true;
    packet[8] = 55;
    packet[10] = 66;
    ASSERT_TRUE(request(&f, &s, packet, 11));
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 16, 4));
    ASSERT_TRUE(f.registers[10] == 11 && f.registers[11] == 12);
    ASSERT_TRUE(f.writes == 1);
    packet[6] = 3;
    ASSERT_TRUE(request(&f, &s, packet, 11));
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 16, 3));
}

TEST(ModbusRtu, MultipleMaximum) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    f.allow_critical = true;
    uint8_t packet[256] = {17, 16, 0, 0, 0, 123, 246};
    for (unsigned i = 0; i < 123; ++i) {
        packet[7 + i * 2] = 0;
        packet[8 + i * 2] = static_cast<uint8_t>(i);
    }
    ASSERT_TRUE(request(&f, &s, packet, 253));
    ASSERT_TRUE(f.writes == 1 && f.registers[122] == 122);
    ASSERT_TRUE(f.reply_length == 8);
    packet[5] = 124;
    ASSERT_TRUE(request(&f, &s, packet, 253));
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 16, 3));
}

TEST(ModbusRtu, AddressCrcAndBroadcast) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    uint8_t packet[16] = {18, 3, 0, 0, 0, 1};
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.sends == 0);
    packet[0] = 0;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.sends == 0 && f.writes == 0);
    packet[1] = 6;
    packet[3] = 7;
    packet[5] = 77;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.sends == 0 && f.writes == 1 && f.registers[7] == 77);
    packet[3] = 120;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.sends == 0 && f.writes == 1 && f.registers[120] == 1120);
    packet[0] = 17;
    size_t length = append_crc(packet, 6);
    packet[length - 1] ^= 1;
    f.now += s.t3_5_us + 100;
    for (size_t i = 0; i < length; ++i) {
        ASSERT_TRUE(nx_modbus_rtu_feed(&s, packet[i], f.now++));
    }
    f.now += s.t3_5_us;
    ASSERT_TRUE(nx_modbus_rtu_poll(&s, f.now));
    ASSERT_TRUE(s.counters.crc_errors == 1 && f.sends == 0);
}

TEST(ModbusRtu, FunctionsLengthsAndRanges) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    uint8_t packet[16] = {17, 99, 0, 0, 0, 1};
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 99, 1));
    packet[1] = 3;
    ASSERT_TRUE(request(&f, &s, packet, 5));
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 3, 3));
    packet[2] = 0xff;
    packet[3] = 0xff;
    packet[4] = 0;
    packet[5] = 2;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 3, 3));
    packet[2] = 0;
    packet[3] = 127;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 3, 2));
    packet[5] = 0;
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_NO_FATAL_FAILURE(check_exception(&f, 3, 3));
}

TEST(ModbusRtu, IntercharacterAndOverrun) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    ASSERT_TRUE(nx_modbus_rtu_feed(&s, 17, f.now));
    f.now += s.t1_5_us + 1;
    ASSERT_FALSE(nx_modbus_rtu_feed(&s, 6, f.now));
    ASSERT_TRUE(s.state == NX_MODBUS_RX_DISCARD);
    f.now += s.t3_5_us;
    ASSERT_TRUE(nx_modbus_rtu_poll(&s, f.now));
    ASSERT_TRUE(s.state == NX_MODBUS_RX_IDLE && f.writes == 0);
    for (unsigned i = 0; i < 256; ++i) {
        ASSERT_TRUE(nx_modbus_rtu_feed(&s, 0x55, ++f.now));
    }
    ASSERT_FALSE(nx_modbus_rtu_feed(&s, 0x55, ++f.now));
    ASSERT_TRUE(s.counters.overflows == 1 && s.rx_length == 0);
    nx_modbus_rtu_rx_error(&s, ++f.now);
    ASSERT_TRUE(s.counters.overflows == 2);
    f.now += s.t3_5_us;
    ASSERT_TRUE(nx_modbus_rtu_poll(&s, f.now));
    uint8_t packet[8] = {17, 3, 0, 0, 0, 1};
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.sends == 1 && s.counters.timing_errors == 1);
}

TEST(ModbusRtu, UnservicedSilenceAndClock) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    uint8_t packet[8] = {17, 6, 0, 0, 0, 1};
    const size_t length = append_crc(packet, 6);
    for (size_t i = 0; i < length; ++i) {
        ASSERT_TRUE(nx_modbus_rtu_feed(&s, packet[i], ++f.now));
    }
    f.now += s.t3_5_us;
    ASSERT_TRUE(nx_modbus_rtu_feed(&s, 17, f.now));
    ASSERT_TRUE(f.sends == 0 && f.writes == 0 && s.counters.timing_errors == 1);
    ASSERT_FALSE(nx_modbus_rtu_poll(&s, f.now - 1));
    ASSERT_TRUE(s.counters.clock_errors == 1);
    f.now += s.t3_5_us;
    ASSERT_TRUE(nx_modbus_rtu_poll(&s, f.now));
    ASSERT_TRUE(s.state == NX_MODBUS_RX_IDLE);
}

TEST(ModbusRtu, TxAbortAndRetainedOwnership) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    uint8_t packet[8] = {17, 3, 0, 0, 0, 1};
    f.fail_drain = true;
    ASSERT_FALSE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.aborts == 1 && !f.owned && !f.transmit);
    ASSERT_TRUE(s.state != NX_MODBUS_LINK_FAULTED);
    f.fail_abort = true;
    ASSERT_FALSE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.owned && f.transmit && !f.unsafe_deassert);
    ASSERT_TRUE(s.state == NX_MODBUS_LINK_FAULTED && s.tx_length != 0);
    ASSERT_FALSE(nx_modbus_rtu_stop(&s));
    ASSERT_TRUE(s.initialized && f.owned && f.transmit);
    const unsigned sends = f.sends;
    ASSERT_FALSE(nx_modbus_rtu_feed(&s, 17, ++f.now));
    ASSERT_FALSE(nx_modbus_rtu_poll(&s, ++f.now));
    ASSERT_TRUE(f.sends == sends);
    ASSERT_FALSE(nx_modbus_rtu_recover(&s));
    f.fail_abort = false;
    f.fail_drain = false;
    ASSERT_TRUE(nx_modbus_rtu_recover(&s));
    ASSERT_FALSE(f.owned && !f.transmit && s.tx_length == 0);
    ASSERT_TRUE(request(&f, &s, packet, 6));
    ASSERT_FALSE(f.unsafe_deassert);
}

TEST(ModbusRtu, TxWriteAndDirectionFailure) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    uint8_t packet[8] = {17, 3, 0, 0, 0, 1};
    f.fail_write = true;
    ASSERT_FALSE(request(&f, &s, packet, 6));
    ASSERT_TRUE(f.aborts == 1 && !f.owned && !f.transmit);
    f.fail_write = false;
    f.fail_direction = true;
    ASSERT_FALSE(request(&f, &s, packet, 6));
    ASSERT_TRUE(s.state == NX_MODBUS_LINK_FAULTED);
    f.fail_direction = false;
    ASSERT_TRUE(nx_modbus_rtu_recover(&s));
    ASSERT_TRUE(request(&f, &s, packet, 6));
}

TEST(ModbusRtu, TxAbsoluteDeadline) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    /* A port that reports success after its absolute deadline is rejected. */
    s.config.tx_timeout_us = 10;
    uint8_t packet[8] = {17, 3, 0, 0, 0, 1};
    ASSERT_FALSE(request(&f, &s, packet, 6));
    ASSERT_TRUE(s.counters.tx_errors == 1 && f.aborts == 1);
    ASSERT_FALSE(f.owned && !f.transmit && !f.unsafe_deassert);
}

TEST(ModbusRtu, ParserFuzz) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    uint32_t seed = 0x541dc37u;
    for (unsigned i = 0; i < 50000; ++i) {
        /* Deterministic input generator is test data, never an entropy source.
         */
        seed = seed * 1664525u + 1013904223u;
        f.now += (seed & 4095u);
        (void)nx_modbus_rtu_feed(&s, static_cast<uint8_t>(seed >> 24U), f.now);
        if ((i & 31u) == 0) {
            f.now += s.t3_5_us;
            (void)nx_modbus_rtu_poll(&s, f.now);
        }
        ASSERT_TRUE(s.rx_length <= NX_MODBUS_ADU_MAX);
        ASSERT_TRUE(s.tx_length <= NX_MODBUS_ADU_MAX);
        if (f.reply_length != 0) {
            ASSERT_TRUE(nx_modbus_crc16(f.reply, f.reply_length) == 0);
        }
    }
    ASSERT_FALSE(f.unsafe_deassert && !f.owned);
}

TEST(ModbusRtu, StructuredValidCrcFuzz) {
    ModbusBoundary f{};
    nx_modbus_rtu_t s{};
    ASSERT_NO_FATAL_FAILURE(initialize(&f, &s));
    f.allow_critical = true;
    uint32_t seed = 0x719de25u;
    uint8_t packet[256];
    for (unsigned iteration = 0; iteration < 10000; ++iteration) {
        for (size_t i = 0; i < sizeof(packet); ++i) {
            seed = seed * 1664525u + 1013904223u;
            packet[i] = static_cast<uint8_t>(seed >> 24U);
        }
        packet[0] = (iteration % 7u == 0) ? 0 : 17;
        const uint8_t functions[] = {3, 6, 16, 4, 0x80};
        packet[1] = functions[iteration % 5u];
        size_t length = 2u + (seed % 252u);
        if (iteration % 3u == 0) {
            packet[2] = 0;
            packet[3] = static_cast<uint8_t>(iteration % 140U);
            packet[4] = 0;
            packet[5] = static_cast<uint8_t>(iteration % 126U);
            length = 6;
            if (packet[1] == 16) {
                /* Permit malformed count and byte-count combinations too. */
                const unsigned count = iteration % 125u;
                packet[5] = static_cast<uint8_t>(count);
                packet[6] = static_cast<uint8_t>(count * 2U);
                length = 7u + count * 2u;
                if (length > 253u) {
                    length = 253;
                }
            }
        }
        const unsigned sends = f.sends;
        ASSERT_TRUE(request(&f, &s, packet, length));
        ASSERT_FALSE(f.owned && !f.unsafe_deassert);
        ASSERT_TRUE(s.rx_length == 0 && s.tx_length == 0);
        if (packet[0] == 0) {
            ASSERT_TRUE(f.sends == sends);
        } else if (f.sends != sends) {
            ASSERT_TRUE(f.reply_length <= 256);
            ASSERT_TRUE(nx_modbus_crc16(f.reply, f.reply_length) == 0);
        }
    }
}

/** \brief Successful stop proves no retained transport reference remains. */
TEST(ModbusRtu, ExplicitStop) {
    ModbusBoundary fixture{};
    nx_modbus_rtu_t server{};
    ASSERT_NO_FATAL_FAILURE(initialize(&fixture, &server));
    ASSERT_TRUE(nx_modbus_rtu_stop(&server));
    ASSERT_FALSE(server.initialized && !fixture.owned && !fixture.transmit);
    ASSERT_FALSE(nx_modbus_rtu_feed(&server, 17, fixture.now));
    ASSERT_FALSE(nx_modbus_rtu_poll(&server, fixture.now));
}

} /* namespace */
