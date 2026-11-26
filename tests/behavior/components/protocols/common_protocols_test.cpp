/**
 * \file            common_protocols_test.cpp
 * \brief           Bounded protocol framing and UTC quality contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/at_client.h"
#include "nexus/components/framing.h"
#include "nexus/components/modbus_tcp.h"
#include "nexus/components/wall_clock.h"
#include "nexus/components/xmodem.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <gtest/gtest.h>
#include <string>
#include <vector>
namespace {
struct Registers {
    unsigned writes{};
    bool allowed{true};
    uint16_t value{0x1234};
    nx_modbus_registers_t port() {
        return {this,
                [](void* p, uint16_t, uint16_t n, uint16_t* out) {
                    for (uint16_t i = 0; i < n; ++i) {
                        out[i] = static_cast<Registers*>(p)->value;
                    }
                    return NX_MODBUS_OK;
                },
                [](void* p, uint16_t, uint16_t, const uint16_t*) {
                    return static_cast<Registers*>(p)->allowed
                               ? NX_MODBUS_OK
                               : NX_MODBUS_DEVICE_BUSY;
                },
                [](void* p, uint16_t, uint16_t, const uint16_t* in) {
                    auto& r = *static_cast<Registers*>(p);
                    ++r.writes;
                    r.value = in[0];
                    return NX_MODBUS_OK;
                }};
    }
};
TEST(CommonProtocols, ModbusFragmentsOneResponseThenBackpressuresPipeline) {
    Registers registers;
    auto port = registers.port();
    nx_modbus_tcp_t session{};
    ASSERT_EQ(nx_modbus_tcp_init(&session, &port, 1), NX_SUCCESS);
    const uint8_t request[]{0x12, 0x34, 0, 0, 0, 6, 1, 3, 0, 0, 0, 1};
    size_t consumed = 999;
    EXPECT_EQ(nx_modbus_tcp_feed(&session, request, 3, &consumed),
              NX_ERROR_BUSY);
    EXPECT_EQ(consumed, 3U);
    ASSERT_EQ(nx_modbus_tcp_feed(&session, request + 3, 9, &consumed),
              NX_SUCCESS);
    EXPECT_EQ(consumed, 9U);
    const uint8_t* response = nullptr;
    size_t bytes = 0;
    ASSERT_EQ(nx_modbus_tcp_response(&session, &response, &bytes), NX_SUCCESS);
    const uint8_t expected[]{0x12, 0x34, 0, 0, 0, 5, 1, 3, 2, 0x12, 0x34};
    ASSERT_EQ(bytes, sizeof(expected));
    EXPECT_EQ(std::memcmp(response, expected, bytes), 0);
    EXPECT_EQ(nx_modbus_tcp_feed(&session, request, sizeof(request), &consumed),
              NX_ERROR_BUSY);
    EXPECT_EQ(consumed, 0U);
    ASSERT_EQ(nx_modbus_tcp_release(&session), NX_SUCCESS);
    EXPECT_EQ(nx_modbus_tcp_feed(&session, request, sizeof(request), &consumed),
              NX_SUCCESS);
}
TEST(CommonProtocols, ModbusWriteRequiresAuthorizationAndExecutesExactlyOnce) {
    Registers registers;
    registers.allowed = false;
    auto port = registers.port();
    nx_modbus_tcp_t session{};
    ASSERT_EQ(nx_modbus_tcp_init(&session, &port, 1), NX_SUCCESS);
    const uint8_t request[]{0, 1, 0, 0, 0, 6, 1, 6, 0, 0, 0, 7};
    size_t consumed = 0;
    ASSERT_EQ(nx_modbus_tcp_feed(&session, request, sizeof(request), &consumed),
              NX_SUCCESS);
    EXPECT_EQ(registers.writes, 0U);
    const uint8_t* response = nullptr;
    size_t bytes = 0;
    ASSERT_EQ(nx_modbus_tcp_response(&session, &response, &bytes), NX_SUCCESS);
    EXPECT_EQ(response[7], 0x86);
    EXPECT_EQ(response[8], NX_MODBUS_DEVICE_BUSY);
    ASSERT_EQ(nx_modbus_tcp_release(&session), NX_SUCCESS);
    registers.allowed = true;
    ASSERT_EQ(nx_modbus_tcp_feed(&session, request, sizeof(request), &consumed),
              NX_SUCCESS);
    EXPECT_EQ(registers.writes, 1U);
    EXPECT_EQ(nx_modbus_tcp_feed(&session, request, sizeof(request), &consumed),
              NX_ERROR_BUSY);
    EXPECT_EQ(registers.writes, 1U);
}
TEST(CommonProtocols, ModbusBadMbapPermanentlyClosesFraming) {
    Registers registers;
    auto port = registers.port();
    nx_modbus_tcp_t session{};
    ASSERT_EQ(nx_modbus_tcp_init(&session, &port, 1), NX_SUCCESS);
    const uint8_t bad[]{0, 1, 0, 1, 0, 6, 1};
    size_t consumed = 0;
    EXPECT_EQ(nx_modbus_tcp_feed(&session, bad, sizeof(bad), &consumed),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_modbus_tcp_feed(&session, bad, sizeof(bad), &consumed),
              NX_ERROR_STATE);
    EXPECT_EQ(consumed, 0U);
    EXPECT_EQ(registers.writes, 0U);
}
TEST(CommonProtocols, CobsRoundTripsAllBytesAndChecksBeforeMutating) {
    std::array<uint8_t, 256> data{};
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    std::array<uint8_t, 260> encoded{};
    size_t bytes = 0;
    ASSERT_EQ(nx_cobs_encode(data.data(), data.size(), encoded.data(),
                             encoded.size(), &bytes),
              NX_SUCCESS);
    ASSERT_GT(bytes, 1U);
    EXPECT_EQ(encoded[bytes - 1], 0);
    std::array<uint8_t, 256> decoded{};
    size_t actual = 0;
    ASSERT_EQ(nx_cobs_decode(encoded.data(), bytes - 1, decoded.data(),
                             decoded.size(), &actual),
              NX_SUCCESS);
    EXPECT_EQ(actual, data.size());
    EXPECT_EQ(decoded, data);
    uint8_t output[]{7, 7, 7};
    const uint8_t malformed[]{4, 1, 2};
    actual = 88;
    EXPECT_EQ(nx_cobs_decode(malformed, sizeof(malformed), output,
                             sizeof(output), &actual),
              NX_ERROR_INVALID);
    EXPECT_EQ(output[0], 7);
    EXPECT_EQ(actual, 88U);
}
TEST(CommonProtocols, FrameOverflowResynchronizesAndRetainsBorrowedPacket) {
    uint8_t storage[4]{};
    nx_frame_decoder_t decoder{};
    ASSERT_EQ(nx_frame_decoder_init(&decoder, storage, sizeof(storage)),
              NX_SUCCESS);
    for (unsigned i = 0; i < 4; ++i) {
        EXPECT_EQ(nx_frame_decoder_feed(&decoder, 1), NX_ERROR_BUSY);
    }
    EXPECT_EQ(nx_frame_decoder_feed(&decoder, 1), NX_ERROR_OVERFLOW);
    EXPECT_EQ(nx_frame_decoder_feed(&decoder, 9), NX_ERROR_BUSY);
    EXPECT_EQ(nx_frame_decoder_feed(&decoder, 0), NX_ERROR_BUSY);
    EXPECT_EQ(nx_frame_decoder_feed(&decoder, 2), NX_ERROR_BUSY);
    EXPECT_EQ(nx_frame_decoder_feed(&decoder, 42), NX_ERROR_BUSY);
    ASSERT_EQ(nx_frame_decoder_feed(&decoder, 0), NX_SUCCESS);
    const uint8_t* view = nullptr;
    size_t size = 0;
    ASSERT_EQ(nx_frame_decoder_peek(&decoder, &view, &size), NX_SUCCESS);
    ASSERT_EQ(size, 1U);
    EXPECT_EQ(view[0], 42);
    EXPECT_EQ(nx_frame_decoder_feed(&decoder, 1), NX_ERROR_BUSY);
    EXPECT_EQ(view[0], 42);
    EXPECT_EQ(nx_frame_decoder_release(&decoder), NX_SUCCESS);
}
TEST(CommonProtocols, UtcExpiryRollbackAndTrustNeverChangeDeadlineDomain) {
    nx_wall_clock_t clock{};
    nx_utc_sample_t sample{1700000000000000LL, 3, 100, NX_UTC_MEASURED};
    ASSERT_EQ(nx_wall_clock_set(&clock, &sample, 1000, 100), NX_SUCCESS);
    nx_utc_sample_t out{};
    ASSERT_EQ(nx_wall_clock_read(&clock, 201, &out), NX_SUCCESS);
    EXPECT_EQ(out.unix_us, sample.unix_us + 101);
    EXPECT_EQ(out.uncertainty_us, 4U);
    EXPECT_EQ(out.quality, NX_UTC_MEASURED);
    EXPECT_EQ(nx_wall_clock_read(&clock, 99, &out), NX_ERROR_STATE);
    EXPECT_EQ(nx_wall_clock_read(&clock, 1000, &out), NX_ERROR_TIMEOUT);
    sample.quality = NX_UTC_UNKNOWN;
    EXPECT_EQ(nx_wall_clock_set(&clock, &sample, 1000, 100), NX_ERROR_INVALID);
}
TEST(CommonProtocols, SntpRequiresExactOriginAndNeverClaimsTrustedTime) {
    std::array<uint8_t, 48> packet{};
    packet[0] = 0x24;
    packet[1] = 2;
    const uint8_t token[]{1, 2, 3, 4, 5, 6, 7, 8};
    std::memcpy(packet.data() + 24, token, 8);
    /* NTP 2208988801 = Unix 1 second, zero fraction. */
    const uint8_t stamp[]{0x83, 0xaa, 0x7e, 0x81};
    std::memcpy(packet.data() + 40, stamp, 4);
    nx_utc_sample_t sample{};
    ASSERT_EQ(
        nx_sntp_parse(packet.data(), packet.size(), token, 10, 210, 0, &sample),
        NX_SUCCESS);
    EXPECT_EQ(sample.unix_us, 1000100);
    EXPECT_GE(sample.uncertainty_us, 100U);
    EXPECT_EQ(sample.quality, NX_UTC_MEASURED);
    packet[24] ^= 1;
    EXPECT_EQ(
        nx_sntp_parse(packet.data(), packet.size(), token, 10, 210, 0, &sample),
        NX_ERROR_INVALID);
}
uint16_t xcrc(const uint8_t* bytes, size_t count) {
    uint16_t crc = 0;
    for (size_t i = 0; i < count; ++i) {
        crc =
            static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(bytes[i]) << 8));
        for (unsigned b = 0; b < 8; ++b) {
            crc = static_cast<uint16_t>((crc << 1) ^
                                        ((crc & 0x8000U) ? 0x1021U : 0U));
        }
    }
    return crc;
}
TEST(CommonProtocols, XmodemCrcDuplicateDoesNotWriteTwiceAndEotCompletes) {
    unsigned writes = 0;
    nx_xmodem_t receiver{};
    ASSERT_EQ(nx_xmodem_start(
                  &receiver,
                  [](void* p, uint32_t index, const uint8_t* data) {
                      EXPECT_EQ(index, 0U);
                      EXPECT_EQ(data[0], 42);
                      ++*static_cast<unsigned*>(p);
                      return NX_SUCCESS;
                  },
                  &writes, 0, 10000, 1000, 3),
              NX_SUCCESS);
    uint8_t control = 0;
    ASSERT_EQ(nx_xmodem_control(&receiver, &control), NX_SUCCESS);
    EXPECT_EQ(control, 'C');
    std::array<uint8_t, 133> packet{};
    packet[0] = 1;
    packet[1] = 1;
    packet[2] = 0xfe;
    packet[3] = 42;
    uint16_t crc = xcrc(packet.data() + 3, 128);
    packet[131] = static_cast<uint8_t>(crc >> 8);
    packet[132] = static_cast<uint8_t>(crc);
    for (unsigned repetition = 0; repetition < 2; ++repetition) {
        for (auto b : packet) {
            ASSERT_EQ(nx_xmodem_feed(&receiver, b, 10), NX_SUCCESS);
        }
        ASSERT_EQ(nx_xmodem_control(&receiver, &control), NX_SUCCESS);
        EXPECT_EQ(control, 6);
    }
    EXPECT_EQ(writes, 1U);
    EXPECT_EQ(nx_xmodem_feed(&receiver, 4, 20), NX_SUCCESS);
    EXPECT_TRUE(receiver.complete);
    ASSERT_EQ(nx_xmodem_control(&receiver, &control), NX_SUCCESS);
    EXPECT_EQ(control, 6);
}
TEST(CommonProtocols, XmodemDeadlineAndCancellationNeverEraseAcceptedData) {
    nx_xmodem_t receiver{};
    ASSERT_EQ(nx_xmodem_start(
                  &receiver,
                  [](void*, uint32_t, const uint8_t*) { return NX_SUCCESS; },
                  nullptr, 10, 100, 20, 2),
              NX_SUCCESS);
    uint8_t control = 0;
    ASSERT_EQ(nx_xmodem_control(&receiver, &control), NX_SUCCESS);
    EXPECT_EQ(nx_xmodem_poll(&receiver, 30), NX_ERROR_BUSY);
    ASSERT_EQ(nx_xmodem_control(&receiver, &control), NX_SUCCESS);
    EXPECT_EQ(control, 'C');
    EXPECT_EQ(nx_xmodem_poll(&receiver, 50), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_xmodem_feed(&receiver, 1, 51), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_xmodem_cancel(&receiver), NX_ERROR_CANCELLED);
}
TEST(CommonProtocols, XmodemCorruptFirstBlockRequestsNakAndDoesNotWrite) {
    unsigned writes = 0;
    nx_xmodem_t receiver{};
    ASSERT_EQ(nx_xmodem_start(
                  &receiver,
                  [](void* p, uint32_t, const uint8_t*) {
                      ++*static_cast<unsigned*>(p);
                      return NX_SUCCESS;
                  },
                  &writes, 0, 10000, 1000, 3),
              NX_SUCCESS);
    uint8_t control = 0;
    ASSERT_EQ(nx_xmodem_control(&receiver, &control), NX_SUCCESS);
    std::array<uint8_t, 133> corrupt{};
    corrupt[0] = 1;
    corrupt[1] = 1;
    corrupt[2] = 0xfe;
    corrupt[3] = 42;
    for (auto byte : corrupt) {
        ASSERT_EQ(nx_xmodem_feed(&receiver, byte, 10), NX_SUCCESS);
    }
    EXPECT_EQ(writes, 0U);
    ASSERT_EQ(nx_xmodem_control(&receiver, &control), NX_SUCCESS);
    EXPECT_EQ(control, 0x15);
}
TEST(CommonProtocols, XmodemSynchronousSinkCannotLatchBusyAsATerminalState) {
    nx_xmodem_t receiver{};
    ASSERT_EQ(nx_xmodem_start(
                  &receiver,
                  [](void*, uint32_t, const uint8_t*) { return NX_ERROR_BUSY; },
                  nullptr, 0, 10000, 1000, 3),
              NX_SUCCESS);
    uint8_t control = 0;
    ASSERT_EQ(nx_xmodem_control(&receiver, &control), NX_SUCCESS);
    std::array<uint8_t, 133> packet{};
    packet[0] = 1;
    packet[1] = 1;
    packet[2] = 0xfe;
    /* CRC16/XMODEM of 128 zero bytes is zero. */
    nx_result_t result = NX_SUCCESS;
    for (auto byte : packet) {
        result = nx_xmodem_feed(&receiver, byte, 10);
    }
    EXPECT_EQ(result, NX_ERROR_STATE);
    EXPECT_EQ(nx_xmodem_poll(&receiver, 20), NX_ERROR_STATE);
}
struct AtHarness {
    nx_at_client_t client{};
    nx_time_us_t now{10};
    uint8_t line[64]{};
    uint8_t tx[64]{};
    std::deque<uint8_t> rx;
    std::vector<uint8_t> written;
    std::vector<nx_at_event_kind_t> kinds;
    std::vector<std::string> views;
    static nx_result_t write(void* p, const uint8_t* bytes, size_t size,
                             size_t* count, nx_time_us_t) {
        auto& h = *static_cast<AtHarness*>(p);
        *count = std::min<size_t>(2, size);
        h.written.insert(h.written.end(), bytes, bytes + *count);
        return NX_SUCCESS;
    }
    static nx_result_t read(void* p, uint8_t* bytes, size_t size, size_t* count,
                            nx_time_us_t) {
        auto& h = *static_cast<AtHarness*>(p);
        *count = std::min(size, h.rx.size());
        for (size_t i = 0; i < *count; ++i) {
            bytes[i] = h.rx.front();
            h.rx.pop_front();
        }
        return *count ? NX_SUCCESS : NX_ERROR_BUSY;
    }
    static nx_time_us_t clock(void* p) {
        return static_cast<AtHarness*>(p)->now;
    }
    static void event(void* p, const nx_at_event_t* e) {
        auto& h = *static_cast<AtHarness*>(p);
        h.kinds.push_back(e->kind);
        h.views.emplace_back(reinterpret_cast<const char*>(e->data), e->size);
        if (e->kind == NX_AT_URC && h.views.back() == "+DATA:3") {
            EXPECT_EQ(nx_at_expect_binary(&h.client, 3), NX_SUCCESS);
        }
    }
    void open() {
        static const nx_byte_stream_ops_t ops{write, read};
        static const char* const prefixes[]{"+DATA:", "+READY"};
        nx_at_config_t config{
            {&ops, this}, {clock, this}, event, this,      prefixes, 2,
            line,         sizeof(line),  tx,    sizeof(tx)};
        ASSERT_EQ(nx_at_init(&client, &config), NX_SUCCESS);
    }
    void feed(const std::string& bytes) {
        rx.insert(rx.end(), bytes.begin(), bytes.end());
    }
};
TEST(CommonProtocols, AtCopiesCommandAndSeparatesUrcBinaryAndTerminalReply) {
    AtHarness h;
    h.open();
    uint8_t command[]{'A', 'T'};
    ASSERT_EQ(nx_at_submit(&h.client, command, sizeof(command), 100),
              NX_SUCCESS);
    command[0] = 'X';
    h.feed("+DATA:3\r\n");
    h.rx.push_back(0);
    h.rx.push_back('\n');
    h.rx.push_back(42);
    h.feed("\r\nOK\r\n");
    for (unsigned i = 0; i < 30 && h.client.command_active; ++i) {
        EXPECT_EQ(nx_at_service(&h.client, 100, 4), NX_SUCCESS);
    }
    EXPECT_EQ(h.written, (std::vector<uint8_t>{'A', 'T', '\r', '\n'}));
    EXPECT_FALSE(h.client.command_active);
    EXPECT_EQ(h.client.result, NX_SUCCESS);
    EXPECT_NE(std::find(h.kinds.begin(), h.kinds.end(), NX_AT_URC),
              h.kinds.end());
    std::string binary;
    for (size_t i = 0; i < h.kinds.size(); ++i) {
        if (h.kinds[i] == NX_AT_BINARY)
            binary += h.views[i];
    }
    EXPECT_EQ(binary, std::string("\0\n*", 3));
}
TEST(CommonProtocols, AtTimedOutCommandRequiresExplicitSynchronization) {
    AtHarness h;
    h.open();
    const uint8_t command[]{'A', 'T'};
    ASSERT_EQ(nx_at_submit(&h.client, command, sizeof(command), 20),
              NX_SUCCESS);
    h.now = 20;
    EXPECT_EQ(nx_at_service(&h.client, 30, 4), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_at_submit(&h.client, command, sizeof(command), 40),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_at_recover(&h.client), NX_SUCCESS);
    EXPECT_EQ(nx_at_submit(&h.client, command, sizeof(command), 40),
              NX_SUCCESS);
}
} /* namespace */
