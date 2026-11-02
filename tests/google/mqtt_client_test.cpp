/**
 * \file            mqtt_client_test.cpp
 * \brief           Real coreMQTT protocol and bounded transport regressions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/mqtt/core_mqtt_storage.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using testing::_;
using testing::Invoke;
using testing::NiceMock;
static nx_time_us_t test_now = 1000;
extern "C" nx_time_us_t nx_mqtt_port_now_us(void) {
    return test_now;
}
extern "C" void nx_mqtt_port_assert(const char*, unsigned) {
    std::abort();
}

struct Stream {
    std::deque<uint8_t> rx;
    std::vector<uint8_t> tx;
    size_t chunk = SIZE_MAX;
    size_t writes = 0;
    size_t reads = 0;
    bool eof = false;
    bool write_busy = false;
    bool invalid_count = false;
    nx_time_us_t advance_write = 0;
    MOCK_METHOD(nx_result_t, Write,
                (const uint8_t*, size_t, size_t*, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Read, (uint8_t*, size_t, size_t*, nx_time_us_t));
    Stream() {
        ON_CALL(*this, Write(_, _, _, _))
            .WillByDefault(Invoke(this, &Stream::write));
        ON_CALL(*this, Read(_, _, _, _))
            .WillByDefault(Invoke(this, &Stream::read));
    }
    nx_result_t write(const uint8_t* p, size_t n, size_t* out, nx_time_us_t) {
        ++writes;
        test_now += advance_write;
        *out = 0;
        if (write_busy) {
            return NX_ERROR_BUSY;
        }
        if (invalid_count) {
            *out = n + 1;
            return NX_SUCCESS;
        }
        *out = std::min(n, chunk);
        tx.insert(tx.end(), p, p + *out);
        return NX_SUCCESS;
    }
    nx_result_t read(uint8_t* p, size_t n, size_t* out, nx_time_us_t) {
        ++reads;
        *out = 0;
        if (eof && rx.empty()) {
            return NX_SUCCESS;
        }
        if (rx.empty()) {
            return NX_ERROR_BUSY;
        }
        *out = std::min({n, chunk, rx.size()});
        for (size_t i = 0; i < *out; ++i) {
            p[i] = rx.front();
            rx.pop_front();
        }
        return NX_SUCCESS;
    }
    void feed(std::initializer_list<uint8_t> bytes) {
        rx.insert(rx.end(), bytes.begin(), bytes.end());
    }
};
static nx_result_t stream_write(void* p, const uint8_t* b, size_t n,
                                size_t* out, nx_time_us_t deadline) {
    return static_cast<Stream*>(p)->Write(b, n, out, deadline);
}
static nx_result_t stream_read(void* p, uint8_t* b, size_t n, size_t* out,
                               nx_time_us_t deadline) {
    return static_cast<Stream*>(p)->Read(b, n, out, deadline);
}
static const nx_byte_stream_ops_t stream_ops = {stream_write, stream_read};
struct Observed {
    nx_mqtt_event_kind_t kind;
    uint16_t id;
    uint8_t qos;
    nx_result_t status;
    std::string topic;
    std::vector<uint8_t> payload;
};
class Mqtt : public testing::Test {
  protected:
    NiceMock<Stream> stream;
    nx_mqtt_t client{};
    std::array<uint8_t, 256> buffer{};
    std::array<MQTTPubAckInfo_t, 2> outgoing{};
    std::array<MQTTPubAckInfo_t, 2> incoming{};
    std::array<nx_mqtt_control_t, 2> controls{};
    nx_mqtt_memory_t memory{buffer.data(),   buffer.size(),   outgoing.data(),
                            outgoing.size(), incoming.data(), incoming.size(),
                            controls.data(), controls.size()};
    nx_mqtt_config_t config{
        {&stream_ops, &stream}, callback, this, nullptr, nullptr};
    std::vector<Observed> events;
    std::function<void()> on_event;
    nx_mqtt_connect_t connection{"unit", 4, nullptr, 0, nullptr, 0, 30};
    nx_mqtt_call_t limit() const {
        return {test_now + 100000, 64};
    }
    static void callback(void* p, const nx_mqtt_event_t* e) {
        auto* f = static_cast<Mqtt*>(p);
        Observed value{e->kind, e->packet_id, e->qos, e->status, {}, {}};
        if (e->topic_size) {
            value.topic.assign(e->topic, e->topic_size);
        }
        if (e->payload_size) {
            value.payload.assign(e->payload, e->payload + e->payload_size);
        }
        f->events.push_back(value);
        if (f->on_event) {
            f->on_event();
        }
    }
    void SetUp() override {
        test_now = 1000;
    }
    void open() {
        ASSERT_EQ(nx_mqtt_init(&client, &config, &memory), NX_SUCCESS);
    }
    void connect() {
        open();
        stream.feed({0x20, 0x02, 0, 0});
        ASSERT_EQ(nx_mqtt_connect(&client, &connection, limit()), NX_SUCCESS);
        stream.tx.clear();
    }
    nx_result_t publish(uint8_t qos = 0, uint16_t* id = nullptr) {
        uint16_t unused = 0;
        nx_mqtt_publish_t p{"a", 1,   reinterpret_cast<const uint8_t*>("x"),
                            1,   qos, false};
        return nx_mqtt_publish(&client, &p, id ? id : &unused, limit());
    }
};
TEST_F(Mqtt, InitializationDoesNotOpenOrUseTransport) {
    EXPECT_CALL(stream, Write(_, _, _, _)).Times(0);
    EXPECT_CALL(stream, Read(_, _, _, _)).Times(0);
    EXPECT_EQ(nx_mqtt_init(&client, &config, &memory), NX_SUCCESS);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_SUCCESS);
}
TEST_F(Mqtt, OverlappingInitStorageRejectsWithoutEffects) {
    memory.buffer = reinterpret_cast<uint8_t*>(&client);
    EXPECT_EQ(nx_mqtt_init(&client, &config, &memory), NX_ERROR_INVALID);
    EXPECT_EQ(stream.writes + stream.reads, 0U);
}
TEST_F(Mqtt, ConnectUsesActualMqtt311CleanSessionWireFormat) {
    open();
    stream.feed({0x20, 2, 0, 0});
    EXPECT_EQ(nx_mqtt_connect(&client, &connection, limit()), NX_SUCCESS);
    EXPECT_EQ(stream.tx,
              (std::vector<uint8_t>{0x10, 0x10, 0, 4, 'M', 'Q', 'T', 'T', 4, 2,
                                    0, 30, 0, 4, 'u', 'n', 'i', 't'}));
    EXPECT_EQ(client.mqtt.connectStatus, MQTTConnected);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_SUCCESS);
}
TEST_F(Mqtt, ConnectRefusalPoisonsThisSessionWithoutReplay) {
    open();
    stream.feed({0x20, 0x02, 0, 5});
    EXPECT_EQ(nx_mqtt_connect(&client, &connection, limit()),
              NX_ERROR_PERMISSION);
    const auto writes = stream.writes;
    EXPECT_EQ(publish(), NX_ERROR_STATE);
    EXPECT_EQ(stream.writes, writes);
}
TEST_F(Mqtt, PublishQosZeroUsesTheExpectedWireFormat) {
    connect();
    EXPECT_EQ(publish(), NX_SUCCESS);
    EXPECT_EQ(stream.tx, (std::vector<uint8_t>{0x30, 4, 0, 1, 'a', 'x'}));
}
TEST_F(Mqtt, PartialWritesAdvanceWithoutDuplicatingTheWireBytes) {
    connect();
    stream.chunk = 1;
    EXPECT_EQ(publish(), NX_SUCCESS);
    EXPECT_EQ(stream.tx, (std::vector<uint8_t>{0x30, 4, 0, 1, 'a', 'x'}));
}
TEST_F(Mqtt, EofIsFatalRatherThanTemporaryZeroBytes) {
    connect();
    stream.eof = true;
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_ERROR_IO);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_ERROR_IO);
    EXPECT_EQ(publish(), NX_ERROR_STATE);
}
TEST_F(Mqtt, BusyReadWithoutDataRemainsAUsableSession) {
    connect();
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    EXPECT_EQ(publish(), NX_SUCCESS);
}
TEST_F(Mqtt, QosOneUsesOfficialRecordsUntilPuback) {
    connect();
    uint16_t id = 0;
    ASSERT_EQ(publish(1, &id), NX_SUCCESS);
    EXPECT_NE(id, 0U);
    EXPECT_TRUE(events.empty());
    EXPECT_EQ(outgoing[0].publishState, MQTTPubAckPending);
    stream.feed(
        {0x40, 2, static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id)});
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].kind, NX_MQTT_EVENT_PUBACK);
    EXPECT_EQ(events[0].id, id);
    EXPECT_EQ(outgoing[0].publishState, MQTTStateNull);
}
TEST_F(Mqtt, FullQosRecordsRejectBeforeWritingAndRecoverOnAck) {
    memory.outgoing_count = 1;
    connect();
    uint16_t id = 0;
    ASSERT_EQ(publish(1, &id), NX_SUCCESS);
    const auto written = stream.tx;
    EXPECT_EQ(publish(1), NX_ERROR_EXHAUSTED);
    EXPECT_EQ(stream.tx, written);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_SUCCESS);
    stream.feed(
        {0x40, 2, static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id)});
    ASSERT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    EXPECT_EQ(publish(1), NX_SUCCESS);
}
TEST_F(Mqtt, IncomingQosOneBorrowsOnlyDuringCallbackAndSendsPuback) {
    connect();
    stream.feed({0x32, 6, 0, 1, 'a', 0, 42, 'z'});
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].topic, "a");
    EXPECT_EQ(events[0].payload, (std::vector<uint8_t>{'z'}));
    EXPECT_EQ(stream.tx, (std::vector<uint8_t>{0x40, 2, 0, 42}));
}
TEST_F(Mqtt, SubscribeAndUnsubscribeHaveBoundedAckLifecycle) {
    connect();
    uint16_t id = 0;
    ASSERT_EQ(nx_mqtt_subscribe(&client, "a/+", 3, 1, &id, limit()),
              NX_SUCCESS);
    stream.feed(
        {0x90, 3, static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id), 1});
    ASSERT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].kind, NX_MQTT_EVENT_SUBACK);
    EXPECT_EQ(events[0].status, NX_SUCCESS);
    ASSERT_EQ(nx_mqtt_unsubscribe(&client, "a/+", 3, &id, limit()), NX_SUCCESS);
    stream.feed(
        {0xB0, 2, static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id)});
    ASSERT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    ASSERT_EQ(events.size(), 2U);
    EXPECT_EQ(events[1].kind, NX_MQTT_EVENT_UNSUBACK);
}
TEST_F(Mqtt, ControlCapacityRejectsBeforeWireEffects) {
    memory.control_count = 1;
    connect();
    uint16_t id = 0;
    ASSERT_EQ(nx_mqtt_subscribe(&client, "a", 1, 0, &id, limit()), NX_SUCCESS);
    const auto written = stream.tx;
    EXPECT_EQ(nx_mqtt_unsubscribe(&client, "a", 1, &id, limit()),
              NX_ERROR_EXHAUSTED);
    EXPECT_EQ(stream.tx, written);
}
TEST_F(Mqtt, UnknownControlAckFailsClosed) {
    connect();
    stream.feed({0x90, 3, 0, 99, 0});
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_ERROR_STATE);
    EXPECT_EQ(publish(), NX_ERROR_STATE);
}
TEST_F(Mqtt, QosTwoAdmissionIsUnsupportedWithoutIo) {
    connect();
    const auto writes = stream.writes;
    EXPECT_EQ(publish(2), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(stream.writes, writes);
}
TEST_F(Mqtt, ExpiredCallRejectsWithoutPoisoningExistingSession) {
    connect();
    EXPECT_EQ(nx_mqtt_process(&client, {test_now, 64}), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_SUCCESS);
}
TEST_F(Mqtt, PartialWriteThenBudgetExhaustionIsStickyAndNeverReplayed) {
    connect();
    stream.chunk = 1;
    nx_mqtt_publish_t p{"a", 1, reinterpret_cast<const uint8_t*>("x"),
                        1,   0, false};
    uint16_t id = 55;
    EXPECT_EQ(nx_mqtt_publish(&client, &p, &id, {test_now + 10000, 1}),
              NX_ERROR_EXHAUSTED);
    const auto written = stream.tx;
    EXPECT_FALSE(written.empty());
    EXPECT_EQ(id, 55U);
    EXPECT_EQ(publish(), NX_ERROR_STATE);
    EXPECT_EQ(stream.tx, written);
}
TEST_F(Mqtt, DeadlineAfterAcceptedWriteIsFatal) {
    connect();
    stream.advance_write = 2000;
    nx_mqtt_publish_t p{"a", 1, reinterpret_cast<const uint8_t*>("x"),
                        1,   0, false};
    uint16_t id = 0;
    EXPECT_EQ(nx_mqtt_publish(&client, &p, &id, {test_now + 1000, 64}),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_ERROR_TIMEOUT);
}
TEST_F(Mqtt, InvalidTransportCountFailsBeforeUpstreamAssertion) {
    connect();
    stream.invalid_count = true;
    EXPECT_EQ(publish(), NX_ERROR_IO);
}
TEST_F(Mqtt, CallbackReentryCannotOverwriteBorrowedIncomingBytes) {
    connect();
    nx_result_t nested = NX_SUCCESS;
    on_event = [&] { nested = publish(); };
    stream.feed({0x30, 4, 0, 1, 'a', 'z'});
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    EXPECT_EQ(nested, NX_ERROR_BUSY);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_SUCCESS);
}
TEST_F(Mqtt, KeepaliveProgressAndPingTimeoutAreCallerDriven) {
    connection.keepalive_seconds = 1;
    connect();
    test_now += 1100000;
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    EXPECT_EQ(stream.tx, (std::vector<uint8_t>{0xC0, 0}));
    test_now += 5100000;
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_ERROR_TIMEOUT);
}
TEST_F(Mqtt, DisconnectDoesNotCloseOrReclaimTheUnderlyingStream) {
    connect();
    EXPECT_EQ(nx_mqtt_disconnect(&client, limit()), NX_SUCCESS);
    EXPECT_EQ(stream.tx, (std::vector<uint8_t>{0xE0, 0}));
    EXPECT_EQ(nx_mqtt_deinit(&client), NX_SUCCESS);
    EXPECT_EQ(publish(), NX_ERROR_STATE);
}
TEST_F(Mqtt, DeinitDuringCallbackRejectsAndCanSucceedAfterReturn) {
    connect();
    nx_result_t nested = NX_SUCCESS;
    on_event = [&] { nested = nx_mqtt_deinit(&client); };
    stream.feed({0x30, 4, 0, 1, 'a', 'z'});
    ASSERT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    EXPECT_EQ(nested, NX_ERROR_BUSY);
    EXPECT_EQ(nx_mqtt_deinit(&client), NX_SUCCESS);
}
TEST_F(Mqtt, ClockRegressionPoisonsTheClockDomain) {
    connect();
    --test_now;
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_ERROR_STATE);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_ERROR_STATE);
}
TEST_F(Mqtt, MisalignedQosRecordsRejectBeforeBorrowingThem) {
    alignas(MQTTPubAckInfo_t) std::array<uint8_t, 64> bytes{};
    memory.outgoing = reinterpret_cast<MQTTPubAckInfo_t*>(bytes.data() + 1);
    memory.outgoing_count = 1;
    EXPECT_EQ(nx_mqtt_init(&client, &config, &memory), NX_ERROR_INVALID);
    EXPECT_EQ(stream.writes + stream.reads, 0U);
}
TEST_F(Mqtt, DeadlineIncludesTheApplicationCallbackBeforeReturning) {
    connect();
    on_event = [] { test_now += 200000; };
    stream.feed({0x30, 4, 0, 1, 'a', 'z'});
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_ERROR_TIMEOUT);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_ERROR_TIMEOUT);
}
TEST_F(Mqtt, ClientIdentifierDoesNotUseTopicWildcardGrammar) {
    connection.client_id = "unit+node";
    connection.client_id_size = 9;
    connect();
    EXPECT_EQ(nx_mqtt_fault(&client), NX_SUCCESS);
}
TEST_F(Mqtt, EmptyExplicitUsernameDoesNotBecomeMissingUsernamePolicy) {
    connection.username = "";
    connection.password = reinterpret_cast<const uint8_t*>("x");
    connection.password_size = 1;
    connect();
    EXPECT_EQ(nx_mqtt_fault(&client), NX_SUCCESS);
}
TEST_F(Mqtt, PartialIncomingPacketPersistsAcrossCallerDrivenSteps) {
    connect();
    stream.chunk = 1;
    stream.feed({0x30, 4, 0, 1, 'a', 'z'});
    for (unsigned i = 0; i < 5; ++i) {
        EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_ERROR_BUSY);
        EXPECT_TRUE(events.empty());
    }
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].payload, (std::vector<uint8_t>{'z'}));
}
TEST_F(Mqtt, RefusedSubscriptionReportsItsEventAndKeepsTheSession) {
    connect();
    uint16_t id = 0;
    ASSERT_EQ(nx_mqtt_subscribe(&client, "a", 1, 1, &id, limit()), NX_SUCCESS);
    stream.feed({0x90, 3, static_cast<uint8_t>(id >> 8),
                 static_cast<uint8_t>(id), 0x80});
    EXPECT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0].status, NX_ERROR_PERMISSION);
    EXPECT_EQ(events[0].qos, 0U);
    EXPECT_EQ(publish(), NX_SUCCESS);
}
TEST_F(Mqtt, MissingConnackHasFiniteIoBudgetEvenIfClockIsStopped) {
    open();
    EXPECT_EQ(nx_mqtt_connect(&client, &connection, {test_now + 10000, 5}),
              NX_ERROR_EXHAUSTED);
    EXPECT_LE(stream.reads + stream.writes, 5U);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_ERROR_EXHAUSTED);
}
TEST_F(Mqtt, ClientsDoNotShareAnActiveInstanceOrRouteTheWrongTransport) {
    connect();
    NiceMock<Stream> other;
    nx_mqtt_t second{};
    std::array<uint8_t, 128> second_buffer{};
    nx_mqtt_memory_t second_memory{second_buffer.data(),
                                   second_buffer.size(),
                                   nullptr,
                                   0,
                                   nullptr,
                                   0,
                                   nullptr,
                                   0};
    nx_mqtt_config_t second_config{
        {&stream_ops, &other}, callback, this, nullptr, nullptr};
    ASSERT_EQ(nx_mqtt_init(&second, &second_config, &second_memory),
              NX_SUCCESS);
    other.feed({0x20, 2, 0, 0});
    ASSERT_EQ(nx_mqtt_connect(&second, &connection, limit()), NX_SUCCESS);
    other.tx.clear();
    nx_result_t nested = NX_ERROR_STATE;
    on_event = [&] {
        nx_mqtt_publish_t p{"b", 1, reinterpret_cast<const uint8_t*>("y"),
                            1,   0, false};
        uint16_t id = 99;
        nested = nx_mqtt_publish(&second, &p, &id, limit());
    };
    stream.feed({0x30, 4, 0, 1, 'a', 'z'});
    ASSERT_EQ(nx_mqtt_process(&client, limit()), NX_SUCCESS);
    EXPECT_EQ(nested, NX_SUCCESS);
    EXPECT_TRUE(stream.tx.empty());
    EXPECT_EQ(other.tx, (std::vector<uint8_t>{0x30, 4, 0, 1, 'b', 'y'}));
    EXPECT_EQ(nx_mqtt_deinit(&second), NX_SUCCESS);
}

TEST_F(Mqtt, ExplicitIdleMayReportBusyWhileServicingDelayedConnack) {
    unsigned idle_calls = 0;
    struct Idle {
        Stream* stream;
        unsigned* calls;
    } idle{&stream, &idle_calls};
    config.idle_context = &idle;
    config.idle = [](void* context, nx_time_us_t) {
        auto* state = static_cast<Idle*>(context);
        ++*state->calls;
        test_now += 1000;
        if (*state->calls == 2) {
            state->stream->feed({0x20, 2, 0, 0});
        }
        return NX_ERROR_BUSY;
    };
    open();
    EXPECT_EQ(nx_mqtt_connect(&client, &connection, limit()), NX_SUCCESS);
    EXPECT_EQ(idle_calls, 2U);
    EXPECT_EQ(nx_mqtt_fault(&client), NX_SUCCESS);
}

TEST_F(Mqtt, EveryTransportCallbackReceivesTheExplicitOperationDeadline) {
    open();
    stream.feed({0x20, 2, 0, 0});
    auto bound = limit();
    EXPECT_CALL(stream, Write(_, _, _, bound.deadline))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(stream, Read(_, _, _, bound.deadline))
        .Times(testing::AtLeast(1));
    EXPECT_EQ(nx_mqtt_connect(&client, &connection, bound), NX_SUCCESS);
}
TEST_F(Mqtt, CleanSessionReconnectDoesNotRetainPriorControlRequests) {
    memory.control_count = 1u;
    connect();
    uint16_t first = 0;
    ASSERT_EQ(nx_mqtt_subscribe(&client, "a", 1, 0, &first, limit()),
              NX_SUCCESS);
    ASSERT_NE(first, 0);
    ASSERT_EQ(nx_mqtt_disconnect(&client, limit()), NX_SUCCESS);
    /* The product has drained/replaced the stream; model its fresh CONNACK. */
    stream.tx.clear();
    stream.feed({0x20, 2, 0, 0});
    ASSERT_EQ(nx_mqtt_connect(&client, &connection, limit()), NX_SUCCESS);
    uint16_t second = 0;
    EXPECT_EQ(nx_mqtt_subscribe(&client, "b", 1, 0, &second, limit()),
              NX_SUCCESS);
    EXPECT_NE(second, 0);
}
