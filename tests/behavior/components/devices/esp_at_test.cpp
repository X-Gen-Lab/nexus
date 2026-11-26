/**
 * \file            esp_at_test.cpp
 * \brief           ESP32-C3 selected firmware with real AT framing and bytes
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/esp_at.h"
#include "nexus/components/uart_stream.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <gtest/gtest.h>
#include <new>
#include <string>
#include <vector>
namespace {
class EspAt : public testing::Test {
  protected:
    nx_esp_at_t module{};
    std::array<uint8_t, 128> line{};
    std::array<uint8_t, 224> command{};
    std::array<uint8_t, 32> transmit{};
    alignas(nx_byte_stream_ops_t) std::array<uint8_t, 64> receive{};
    std::deque<uint8_t> incoming;
    std::string outgoing;
    std::vector<std::string> commands;
    std::vector<uint8_t> sent;
    nx_time_us_t now{10U};
    size_t raw_pending{};
    size_t fragment{64U};
    bool correct_version{true};
    bool different_core{};
    bool omit_prompt{};
    nx_uart_stream_t uart{};
    std::array<uint8_t, 16> uart_copy{};
    nx_uart_ops_t uart_ops{};
    nx_uart_port_t uart_port{&uart_ops, this};
    nx_uart_tx_request_t* uart_request{};
    size_t uart_offset{};
    bool bridged{};
    static nx_time_us_t clock(void* context) {
        return static_cast<EspAt*>(context)->now;
    }
    void feed(const std::string& text) {
        incoming.insert(incoming.end(), text.begin(), text.end());
    }
    void respond(const std::string& text) {
        commands.push_back(text);
        if (text == "AT+GMR") {
            feed(different_core ? "AT version:4.0.0.0(core - ESP32C3)\r\n"
                                : "AT version:4.1.1.0(core - ESP32C3)\r\n");
            feed(correct_version ? "Bin version:4.1.1.0(MINI-1)\r\nOK\r\n"
                                 : "Bin version:4.2.0.0(MINI-1)\r\nOK\r\n");
        } else if (text.rfind("AT+CWJAP=", 0U) == 0U) {
            feed("WIFI CONNECTED\r\nWIFI GOT IP\r\nOK\r\n");
        } else if (text.rfind("AT+CIPSTART=", 0U) == 0U) {
            feed("CONNECT\r\nOK\r\n");
        } else if (text.rfind("AT+CIPSEND=", 0U) == 0U) {
            raw_pending = static_cast<size_t>(std::stoul(text.substr(11U)));
            feed(omit_prompt ? "OK\r\n" : "OK\r\n>");
        } else if (text == "AT+CIPCLOSE") {
            feed("CLOSED\r\nOK\r\n");
        } else {
            feed("OK\r\n");
        }
    }
    static nx_result_t write(void* context, const uint8_t* bytes, size_t size,
                             size_t* count, nx_time_us_t) {
        auto& self = *static_cast<EspAt*>(context);
        *count = std::min<size_t>(3U, size);
        for (size_t i = 0U; i < *count; ++i) {
            if (self.raw_pending != 0U) {
                self.sent.push_back(bytes[i]);
                if (--self.raw_pending == 0U) {
                    self.feed("\r\nSEND OK\r\n");
                }
            } else {
                self.outgoing.push_back(static_cast<char>(bytes[i]));
                if (self.outgoing.size() >= 2U &&
                    self.outgoing.substr(self.outgoing.size() - 2U) == "\r\n") {
                    const auto text =
                        self.outgoing.substr(0U, self.outgoing.size() - 2U);
                    self.outgoing.clear();
                    self.respond(text);
                }
            }
        }
        return NX_SUCCESS;
    }
    static nx_result_t read(void* context, uint8_t* bytes, size_t size,
                            size_t* count, nx_time_us_t) {
        auto& self = *static_cast<EspAt*>(context);
        *count = std::min({size, self.fragment, self.incoming.size()});
        for (size_t i = 0U; i < *count; ++i) {
            bytes[i] = self.incoming.front();
            self.incoming.pop_front();
        }
        return *count != 0U ? NX_SUCCESS : NX_ERROR_BUSY;
    }
    static const nx_byte_stream_ops_t ops;
    static nx_result_t uart_submit(void* context,
                                   nx_uart_tx_request_t* request) {
        auto& self = *static_cast<EspAt*>(context);
        if (self.uart_request != nullptr) {
            return NX_ERROR_BUSY;
        }
        nx_result_t result =
            nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS) {
            self.uart_request = request;
            self.uart_offset = 0U;
        }
        return result;
    }
    static nx_result_t uart_cancel(void* context,
                                   nx_uart_tx_request_t* request) {
        auto& self = *static_cast<EspAt*>(context);
        EXPECT_EQ(request, self.uart_request);
        self.uart_request = nullptr;
        nx_request_settle(&request->base, NX_ERROR_CANCELLED, 0U);
        return NX_SUCCESS;
    }
    static void uart_service(void* context) {
        auto& self = *static_cast<EspAt*>(context);
        auto* request = self.uart_request;
        if (request == nullptr) {
            return;
        }
        size_t count{};
        EXPECT_EQ(write(context, request->data + self.uart_offset,
                        request->length - self.uart_offset, &count,
                        request->base.deadline),
                  NX_SUCCESS);
        self.uart_offset += count;
        if (self.uart_offset == request->length) {
            self.uart_request = nullptr;
            nx_request_settle(&request->base, NX_SUCCESS, request->length);
        }
    }
    static nx_result_t uart_read(void* context, uint8_t* data, size_t capacity,
                                 size_t* count) {
        nx_result_t result = read(context, data, capacity, count, 100U);
        return result == NX_ERROR_BUSY ? NX_ERROR_EMPTY : result;
    }
    void bind_uart() {
        uart_ops.submit = uart_submit;
        uart_ops.cancel = uart_cancel;
        uart_ops.service = uart_service;
        uart_ops.read_bytes = uart_read;
        ASSERT_EQ(nx_uart_stream_init(&uart, &uart_port, {clock, this},
                                      uart_copy.data(), uart_copy.size()),
                  NX_SUCCESS);
        const nx_esp_at_config_t config{*nx_uart_stream_face(&uart),
                                        {clock, this},
                                        line.data(),
                                        line.size(),
                                        command.data(),
                                        command.size(),
                                        transmit.data(),
                                        transmit.size(),
                                        receive.data(),
                                        receive.size()};
        ASSERT_EQ(nx_esp_at_init(&module, &config), NX_SUCCESS);
        bridged = true;
    }
    nx_result_t progress() {
        if (bridged) {
            nx_result_t result = nx_uart_stream_service(&uart);
            if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
                return result;
            }
        }
        return nx_esp_at_service(&module, 100U, 2U);
    }
    void SetUp() override {
        const nx_esp_at_config_t config{
            {&ops, this},   {clock, this},  line.data(),     line.size(),
            command.data(), command.size(), transmit.data(), transmit.size(),
            receive.data(), receive.size()};
        ASSERT_EQ(nx_esp_at_init(&module, &config), NX_SUCCESS);
    }
    nx_esp_at_status_t status() {
        nx_esp_at_status_t result{};
        EXPECT_EQ(nx_esp_at_status(&module, &result), NX_SUCCESS);
        return result;
    }
    void until(nx_esp_at_phase_t phase) {
        for (unsigned i = 0U; i < 4096U && status().phase != phase; ++i) {
            const auto result = progress();
            ASSERT_TRUE(result == NX_SUCCESS || result == NX_ERROR_BUSY);
        }
        ASSERT_EQ(status().phase, phase);
    }
    void connect() {
        ASSERT_EQ(nx_esp_at_configure(&module, 100U), NX_SUCCESS);
        until(NX_ESP_AT_READY);
        ASSERT_EQ(nx_esp_at_join(&module, "test", 4U, "password", 8U, 100U),
                  NX_SUCCESS);
        until(NX_ESP_AT_READY);
        ASSERT_EQ(nx_esp_at_connect(&module, "192.0.2.1", 9U, 1883U, 100U),
                  NX_SUCCESS);
        until(NX_ESP_AT_CONNECTED);
    }
};
const nx_byte_stream_ops_t EspAt::ops{write, read};

TEST_F(EspAt, ConfigurationChecksExactVersionAndSelectsVolatileSingleTcpMode) {
    ASSERT_EQ(nx_esp_at_configure(&module, 100U), NX_SUCCESS);
    until(NX_ESP_AT_READY);
    EXPECT_TRUE(status().firmware_verified);
    EXPECT_EQ(commands, (std::vector<std::string>{
                            "AT+GMR", "ATE0", "AT+SYSSTORE=0", "AT+CWMODE=1,0",
                            "AT+CIPMUX=0", "AT+CIPMODE=0", "AT+CIPDINFO=0",
                            "AT+CIPRECVTYPE=0"}));
}

TEST_F(EspAt, OtherFirmwareRejectsBeforeModeChanges) {
    correct_version = false;
    ASSERT_EQ(nx_esp_at_configure(&module, 100U), NX_SUCCESS);
    for (unsigned i = 0U; i < 128U && status().phase != NX_ESP_AT_FAILED; ++i) {
        (void)nx_esp_at_service(&module, 100U, 2U);
    }
    EXPECT_EQ(status().phase, NX_ESP_AT_FAILED);
    EXPECT_EQ(status().failure, NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(commands, (std::vector<std::string>{"AT+GMR"}));
}

TEST_F(EspAt, ClosedCoreVersionIsSeparateFromTheSelectedBinaryRelease) {
    different_core = true;
    ASSERT_EQ(nx_esp_at_configure(&module, 100U), NX_SUCCESS);
    until(NX_ESP_AT_READY);
    EXPECT_TRUE(status().firmware_verified);
}

TEST_F(EspAt, ConnectCompletionAndImmediateBinaryShareOneUartRead) {
    ASSERT_EQ(nx_esp_at_configure(&module, 100U), NX_SUCCESS);
    until(NX_ESP_AT_READY);
    ASSERT_EQ(nx_esp_at_join(&module, "test", 4U, "password", 8U, 100U),
              NX_SUCCESS);
    until(NX_ESP_AT_READY);
    ASSERT_EQ(nx_esp_at_connect(&module, "192.0.2.1", 9U, 1883U, 100U),
              NX_SUCCESS);
    for (unsigned i = 0U; i < 32U && incoming.empty(); ++i) {
        ASSERT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_SUCCESS);
    }
    ASSERT_FALSE(incoming.empty());
    feed("+IPD,5:");
    const std::array<uint8_t, 5> payload{'a', '\r', '\n', 0U, '>'};
    incoming.insert(incoming.end(), payload.begin(), payload.end());
    ASSERT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_SUCCESS);
    EXPECT_TRUE(incoming.empty());
    EXPECT_EQ(status().phase, NX_ESP_AT_CONNECTED);
    const auto* stream = nx_esp_at_stream(&module);
    std::array<uint8_t, 5> result{};
    size_t count{};
    ASSERT_EQ(stream->ops->read(stream->context, result.data(), result.size(),
                                &count, 100U),
              NX_SUCCESS);
    EXPECT_EQ(count, payload.size());
    EXPECT_EQ(result, payload);
}

TEST_F(EspAt, ConnectCompletionAndImmediateCloseShareOneUartRead) {
    ASSERT_EQ(nx_esp_at_configure(&module, 100U), NX_SUCCESS);
    until(NX_ESP_AT_READY);
    ASSERT_EQ(nx_esp_at_join(&module, "test", 4U, "password", 8U, 100U),
              NX_SUCCESS);
    until(NX_ESP_AT_READY);
    ASSERT_EQ(nx_esp_at_connect(&module, "192.0.2.1", 9U, 1883U, 100U),
              NX_SUCCESS);
    for (unsigned i = 0U; i < 32U && incoming.empty(); ++i) {
        ASSERT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_SUCCESS);
    }
    ASSERT_FALSE(incoming.empty());
    feed("CLOSED\r\n");
    ASSERT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_SUCCESS);
    EXPECT_TRUE(incoming.empty());
    EXPECT_EQ(status().phase, NX_ESP_AT_CLOSED);
    EXPECT_FALSE(status().connected);
    const auto* stream = nx_esp_at_stream(&module);
    uint8_t result{};
    size_t count{42U};
    EXPECT_EQ(stream->ops->read(stream->context, &result, 1U, &count, 100U),
              NX_SUCCESS);
    EXPECT_EQ(count, 0U);
}

TEST_F(EspAt, PromptCopiesTransmissionAndBinaryReceivePreservesControlBytes) {
    connect();
    const nx_byte_stream_t* stream = nx_esp_at_stream(&module);
    std::array<uint8_t, 5> payload{'a', '\r', '\n', 0U, '>'};
    const auto expected = payload;
    size_t count{};
    ASSERT_EQ(stream->ops->write(stream->context, payload.data(),
                                 payload.size(), &count, 100U),
              NX_SUCCESS);
    EXPECT_EQ(count, payload.size());
    payload.fill(99U);
    until(NX_ESP_AT_CONNECTED);
    EXPECT_EQ(sent, (std::vector<uint8_t>(expected.begin(), expected.end())));
    feed("+IPD,5:");
    incoming.insert(incoming.end(), expected.begin(), expected.end());
    feed("\r\n");
    while (!incoming.empty()) {
        EXPECT_EQ(nx_esp_at_service(&module, 100U, 2U), NX_SUCCESS);
    }
    std::array<uint8_t, 5> output{};
    ASSERT_EQ(stream->ops->read(stream->context, output.data(), output.size(),
                                &count, 100U),
              NX_SUCCESS);
    EXPECT_EQ(output, expected);
}

TEST_F(EspAt, FragmentedBinaryHeaderAndPartialReadsKeepExactBytes) {
    fragment = 1U;
    connect();
    feed("+IPD,5:hello\r\n");
    while (!incoming.empty()) {
        EXPECT_EQ(nx_esp_at_service(&module, 100U, 2U), NX_SUCCESS);
    }
    const auto* stream = nx_esp_at_stream(&module);
    uint8_t out[8]{};
    size_t count{};
    EXPECT_EQ(stream->ops->read(stream->context, out, 2U, &count, 100U),
              NX_SUCCESS);
    EXPECT_EQ(count, 2U);
    EXPECT_EQ(stream->ops->read(stream->context, out + 2U, 6U, &count, 100U),
              NX_SUCCESS);
    EXPECT_EQ(count, 3U);
    EXPECT_EQ(std::memcmp(out, "hello", 5U), 0);
}

TEST_F(EspAt, MalformedOrOverflowingLengthFailsBeforePublishingPayload) {
    connect();
    feed("+IPD,999999999999999999999999999:payload");
    EXPECT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_ERROR_OVERFLOW);
    EXPECT_EQ(status().received_bytes, 0U);
    EXPECT_EQ(status().phase, NX_ESP_AT_FAILED);
}

TEST_F(EspAt, CompleteIpdLineWithoutDelimiterCannotBecomeControlText) {
    connect();
    feed("+IPD,2\r\nhi\r\n");
    EXPECT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_ERROR_INVALID);
    EXPECT_EQ(status().phase, NX_ESP_AT_FAILED);
    EXPECT_EQ(status().received_bytes, 0U);
}

TEST_F(EspAt, ObservedCloseWithdrawsReceiveBeforeTheCloseCommandOk) {
    connect();
    ASSERT_EQ(nx_esp_at_close(&module, 100U), NX_SUCCESS);
    for (unsigned i = 0U; i < 32U && incoming.empty(); ++i) {
        ASSERT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_SUCCESS);
    }
    ASSERT_FALSE(incoming.empty());
    incoming.clear();
    feed("CLOSED\r\n");
    ASSERT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_SUCCESS);
    ASSERT_EQ(status().phase, NX_ESP_AT_CLOSING);
    EXPECT_FALSE(status().connected);
    feed("+IPD,2:hi");
    EXPECT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_ERROR_STATE);
    EXPECT_EQ(status().phase, NX_ESP_AT_FAILED);
    EXPECT_EQ(status().received_bytes, 0U);
}

TEST_F(EspAt, ReceiveCapacityFailureAndUnexpectedResetCloseAdmission) {
    connect();
    feed("+IPD,65:");
    EXPECT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(status().received_bytes, 0U);
    EXPECT_EQ(nx_esp_at_connect(&module, "192.0.2.1", 9U, 1883U, 100U),
              NX_ERROR_STATE);
    incoming.clear();
    ASSERT_EQ(nx_esp_at_recover(&module), NX_SUCCESS);
    connect();
    feed("ready\r\n");
    EXPECT_EQ(nx_esp_at_service(&module, 100U, 1U), NX_ERROR_STATE);
}

TEST_F(EspAt, MissingPromptTimeoutNeverReplaysPreviouslyAcceptedBytes) {
    connect();
    omit_prompt = true;
    const auto* stream = nx_esp_at_stream(&module);
    const uint8_t payload[]{1U, 2U};
    size_t count{};
    ASSERT_EQ(stream->ops->write(stream->context, payload, sizeof(payload),
                                 &count, 100U),
              NX_SUCCESS);
    for (unsigned i = 0U; i < 32U; ++i) {
        (void)nx_esp_at_service(&module, 100U, 2U);
    }
    now = 100U;
    EXPECT_EQ(nx_esp_at_service(&module, 101U, 1U), NX_ERROR_TIMEOUT);
    EXPECT_TRUE(sent.empty());
    EXPECT_EQ(std::count(commands.begin(), commands.end(), "AT+CIPSEND=2"), 1);
    EXPECT_EQ(stream->ops->write(stream->context, payload, sizeof(payload),
                                 &count, 101U),
              NX_ERROR_STATE);
    EXPECT_EQ(count, 0U);
}

TEST_F(EspAt, RemoteCloseDrainsCopiedReceiveBytesBeforeEof) {
    connect();
    feed("+IPD,2:hi\r\nCLOSED\r\n");
    while (!incoming.empty()) {
        EXPECT_EQ(nx_esp_at_service(&module, 100U, 2U), NX_SUCCESS);
    }
    const auto* stream = nx_esp_at_stream(&module);
    uint8_t out[4]{};
    size_t count{};
    EXPECT_EQ(
        stream->ops->read(stream->context, out, sizeof(out), &count, 100U),
        NX_SUCCESS);
    EXPECT_EQ(count, 2U);
    EXPECT_EQ(
        stream->ops->read(stream->context, out, sizeof(out), &count, 100U),
        NX_SUCCESS);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(nx_esp_at_connect(&module, "192.0.2.1", 9U, 1883U, 100U),
              NX_ERROR_STATE);
}

TEST_F(EspAt, ExplicitCloseWaitsForBothClosedAndOkThenRequiresRecovery) {
    connect();
    ASSERT_EQ(nx_esp_at_close(&module, 100U), NX_SUCCESS);
    until(NX_ESP_AT_CLOSED);
    EXPECT_FALSE(status().connected);
    EXPECT_EQ(commands.back(), "AT+CIPCLOSE");
    EXPECT_EQ(nx_esp_at_connect(&module, "192.0.2.1", 9U, 1883U, 100U),
              NX_ERROR_STATE);
}

TEST_F(EspAt, LateSendResultCannotCompleteANewUnsentPayload) {
    connect();
    const auto* stream = nx_esp_at_stream(&module);
    const uint8_t payload[]{1U};
    size_t count{};
    ASSERT_EQ(stream->ops->write(stream->context, payload, 1U, &count, 100U),
              NX_SUCCESS);
    feed("SEND OK\r\n");
    for (unsigned i = 0U; i < 64U && status().phase != NX_ESP_AT_FAILED; ++i) {
        (void)nx_esp_at_service(&module, 100U, 1U);
    }
    EXPECT_EQ(status().phase, NX_ESP_AT_FAILED);
    EXPECT_EQ(status().failure, NX_ERROR_STATE);
    EXPECT_TRUE(sent.empty());
}

TEST_F(EspAt, JoinQuotesArgumentsAndClearsTheCompletedCredentialCommand) {
    ASSERT_EQ(nx_esp_at_configure(&module, 100U), NX_SUCCESS);
    until(NX_ESP_AT_READY);
    ASSERT_EQ(nx_esp_at_join(&module, "a,b", 3U, "pass\"word", 9U, 100U),
              NX_SUCCESS);
    until(NX_ESP_AT_READY);
    EXPECT_EQ(commands.back(), "AT+CWJAP=\"a\\,b\",\"pass\\\"word\"");
    EXPECT_TRUE(std::all_of(command.begin(), command.end(),
                            [](uint8_t byte) { return byte == 0U; }));
}

TEST_F(EspAt, TransportOperationsCannotAliasAReceiveBuffer) {
    auto* aliased = new (receive.data()) nx_byte_stream_ops_t{write, read};
    const nx_esp_at_config_t config{
        {aliased, this}, {clock, this},  line.data(),     line.size(),
        command.data(),  command.size(), transmit.data(), transmit.size(),
        receive.data(),  receive.size()};
    nx_esp_at_t fresh{};
    EXPECT_EQ(nx_esp_at_init(&fresh, &config), NX_ERROR_INVALID);
    EXPECT_FALSE(fresh.initialized);
}

TEST_F(EspAt, RealUartCopyBridgeAndRealModuleCompleteOneDuplexSession) {
    bind_uart();
    connect();
    const auto* stream = nx_esp_at_stream(&module);
    const uint8_t data[]{'a', 0U, 'b'};
    size_t count{};
    ASSERT_EQ(
        stream->ops->write(stream->context, data, sizeof(data), &count, 100U),
        NX_SUCCESS);
    until(NX_ESP_AT_CONNECTED);
    EXPECT_EQ(sent, (std::vector<uint8_t>{'a', 0U, 'b'}));
    feed("+IPD,2:hi\r\n");
    while (!incoming.empty()) {
        EXPECT_EQ(progress(), NX_SUCCESS);
    }
    uint8_t output[2]{};
    ASSERT_EQ(stream->ops->read(stream->context, output, sizeof(output), &count,
                                100U),
              NX_SUCCESS);
    EXPECT_EQ(count, 2U);
    EXPECT_EQ(output[0], 'h');
    EXPECT_EQ(output[1], 'i');
    ASSERT_EQ(nx_esp_at_close(&module, 100U), NX_SUCCESS);
    until(NX_ESP_AT_CLOSED);
    EXPECT_EQ(nx_uart_stream_stop(&uart), NX_SUCCESS);
    EXPECT_EQ(uart_request, nullptr);
}
} /* namespace */
