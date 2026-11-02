/**
 * \file            client.c
 * \brief           Explicit bounded coreMQTT transport and ownership adapter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/mqtt/core_mqtt_storage.h"
#include <limits.h>
#include <string.h>

static bool span(const void* p, size_t n) {
    return n == 0u || (p != NULL && (uintptr_t)p <= UINTPTR_MAX - (n - 1u));
}

static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    if (an == 0u || bn == 0u) {
        return false;
    }
    if (!span(a, an) || !span(b, bn)) {
        return true;
    }
    return (uintptr_t)a <= (uintptr_t)b + bn - 1u &&
           (uintptr_t)b <= (uintptr_t)a + an - 1u;
}

static bool external(const nx_mqtt_t* s, const void* p, size_t n) {
    const nx_mqtt_memory_t* m = &s->memory;
    return span(p, n) && !overlap(p, n, s, sizeof(*s)) &&
           !overlap(p, n, m->buffer, m->buffer_size) &&
           !overlap(p, n, m->outgoing,
                    m->outgoing_count * sizeof(*m->outgoing)) &&
           !overlap(p, n, m->incoming,
                    m->incoming_count * sizeof(*m->incoming)) &&
           !overlap(p, n, m->controls, m->control_count * sizeof(*m->controls));
}

static void fail(nx_mqtt_t* s, nx_result_t error) {
    if (s->fault == NX_SUCCESS) {
        s->fault = error;
    }
}

static uint32_t mqtt_time_ms(void) {
    return (uint32_t)(nx_mqtt_port_now_us() / UINT64_C(1000));
}

static bool time_valid(nx_mqtt_t* s, nx_time_us_t now) {
    if (now == NX_DEADLINE_NEVER) {
        fail(s, NX_ERROR_EXHAUSTED);
    } else if (now < s->last_time) {
        fail(s, NX_ERROR_STATE);
    } else {
        s->last_time = now;
    }
    return s->fault == NX_SUCCESS;
}

static bool transport_admit(nx_mqtt_t* s) {
    if (s->fault != NX_SUCCESS) {
        return false;
    }
    nx_time_us_t now = nx_mqtt_port_now_us();
    if (!time_valid(s, now)) {
        return false;
    }
    if (now >= s->deadline) {
        fail(s, NX_ERROR_TIMEOUT);
        return false;
    }
    if (s->remaining_calls == 0u) {
        fail(s, NX_ERROR_EXHAUSTED);
        return false;
    }
    --s->remaining_calls;
    ++s->io_calls;
    return true;
}

static int32_t transferred(nx_mqtt_t* s, nx_result_t status, size_t done,
                           size_t requested) {
    if (!time_valid(s, nx_mqtt_port_now_us())) {
        return -1;
    }
    if (s->last_time >= s->deadline) {
        fail(s, NX_ERROR_TIMEOUT);
        return -1;
    }
    if (status == NX_ERROR_BUSY && done == 0u) {
        if (s->config.idle != NULL) {
            nx_result_t idle =
                s->config.idle(s->config.idle_context, s->deadline);
            if (idle != NX_SUCCESS && idle != NX_ERROR_BUSY) {
                fail(s, idle);
                return -1;
            }
        }
        return 0;
    }
    if (status != NX_SUCCESS || done > requested || done == 0u) {
        /* SUCCESS/read0 is EOF. It must never become upstream's retryable 0. */
        fail(s, status == NX_SUCCESS || status == NX_ERROR_BUSY ? NX_ERROR_IO
                                                                : status);
        return -1;
    }
    return (int32_t)done;
}

static int32_t mqtt_send(NetworkContext_t* context, const void* data,
                         size_t bytes) {
    nx_mqtt_t* s = (nx_mqtt_t*)context;
    if (bytes == 0u) {
        return 0;
    }
    if (bytes > INT32_MAX || !transport_admit(s)) {
        return -1;
    }
    size_t done = 0u;
    nx_result_t result = s->config.stream.ops->write(
        s->config.stream.context, data, bytes, &done, s->deadline);
    return transferred(s, result, done, bytes);
}

static int32_t mqtt_recv(NetworkContext_t* context, void* data, size_t bytes) {
    nx_mqtt_t* s = (nx_mqtt_t*)context;
    if (bytes == 0u) {
        return 0;
    }
    if (bytes > INT32_MAX || !transport_admit(s)) {
        return -1;
    }
    size_t done = 0u;
    nx_result_t result = s->config.stream.ops->read(
        s->config.stream.context, data, bytes, &done, s->deadline);
    return transferred(s, result, done, bytes);
}

static nx_mqtt_control_t* find_control(nx_mqtt_t* s, uint16_t id) {
    for (size_t i = 0u; i < s->memory.control_count; ++i) {
        if (s->memory.controls[i].packet_id == id) {
            return &s->memory.controls[i];
        }
    }
    return NULL;
}

static void mqtt_event(MQTTContext_t* context, MQTTPacketInfo_t* packet,
                       MQTTDeserializedInfo_t* info) {
    /* MQTTContext is the first member; no active-instance registry is needed.
     */
    nx_mqtt_t* s = (nx_mqtt_t*)context;
    nx_mqtt_event_t e = {0};
    e.packet_id = info->packetIdentifier;
    e.status = NX_SUCCESS;
    uint8_t type = packet->type & 0xf0u;
    if (type == MQTT_PACKET_TYPE_PUBLISH) {
        const MQTTPublishInfo_t* p = info->pPublishInfo;
        if (p == NULL || p->qos > MQTTQoS1) {
            fail(s, NX_ERROR_UNSUPPORTED);
            return;
        }
        e.kind = NX_MQTT_EVENT_PUBLISH;
        e.qos = (uint8_t)p->qos;
        e.retain = p->retain;
        e.duplicate = p->dup;
        e.topic = p->pTopicName;
        e.topic_size = p->topicNameLength;
        e.payload = p->pPayload;
        e.payload_size = p->payloadLength;
    } else if (type == MQTT_PACKET_TYPE_PUBACK) {
        e.kind = NX_MQTT_EVENT_PUBACK;
        e.qos = 1u;
    } else if (type == MQTT_PACKET_TYPE_SUBACK ||
               type == MQTT_PACKET_TYPE_UNSUBACK) {
        nx_mqtt_control_t* control = find_control(s, e.packet_id);
        if (control == NULL || control->kind != type) {
            fail(s, NX_ERROR_STATE);
            return;
        }
        if (type == MQTT_PACKET_TYPE_SUBACK) {
            uint8_t* codes = NULL;
            size_t count = 0u;
            if (MQTT_GetSubAckStatusCodes(packet, &codes, &count) !=
                    MQTTSuccess ||
                count != 1u) {
                fail(s, NX_ERROR_STATE);
                return;
            }
            uint8_t grant = codes[0];
            if (grant != 0x80u && grant > control->qos) {
                fail(s, NX_ERROR_STATE);
                return;
            }
            e.kind = NX_MQTT_EVENT_SUBACK;
            e.qos = grant == 0x80u ? 0u : grant;
            e.status = grant == 0x80u ? NX_ERROR_PERMISSION : NX_SUCCESS;
        } else {
            e.kind = NX_MQTT_EVENT_UNSUBACK;
        }
        control->packet_id = 0u;
    } else {
        fail(s, NX_ERROR_UNSUPPORTED);
        return;
    }
    s->config.event(s->config.event_context, &e);
}

static nx_result_t admit(nx_mqtt_t* s, nx_mqtt_call_t call, bool connected) {
    if (s == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!s->opened) {
        return NX_ERROR_STATE;
    }
    if (s->active) {
        return NX_ERROR_BUSY;
    }
    if (s->fault != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    if (connected && s->mqtt.connectStatus != MQTTConnected) {
        return NX_ERROR_STATE;
    }
    if (call.deadline == NX_DEADLINE_NEVER || call.io_budget == 0u) {
        return NX_ERROR_INVALID;
    }
    nx_time_us_t now = nx_mqtt_port_now_us();
    if (!time_valid(s, now)) {
        return s->fault;
    }
    if (now >= call.deadline) {
        return NX_ERROR_TIMEOUT;
    }
    s->deadline = call.deadline;
    s->remaining_calls = call.io_budget;
    s->io_calls = 0u;
    s->active = true;
    return NX_SUCCESS;
}

static nx_result_t finished(nx_mqtt_t* s, MQTTStatus_t status,
                            bool connecting) {
    /* A callback may consume time without another transport boundary. */
    if (time_valid(s, nx_mqtt_port_now_us()) && s->last_time >= s->deadline) {
        fail(s, NX_ERROR_TIMEOUT);
    }
    nx_result_t result = NX_SUCCESS;
    switch (status) {
        case MQTTSuccess:
            break;
        case MQTTNeedMoreBytes:
            result = NX_ERROR_BUSY;
            break;
        case MQTTNoDataAvailable:
            result = connecting ? NX_ERROR_TIMEOUT : NX_SUCCESS;
            break;
        case MQTTNoMemory:
            result = NX_ERROR_EXHAUSTED;
            break;
        case MQTTBadParameter:
            result = NX_ERROR_INVALID;
            break;
        case MQTTServerRefused:
            result = NX_ERROR_PERMISSION;
            break;
        case MQTTKeepAliveTimeout:
            result = NX_ERROR_TIMEOUT;
            break;
        case MQTTBadResponse:
        case MQTTIllegalState:
        case MQTTStateCollision:
            result = NX_ERROR_STATE;
            break;
        case MQTTSendFailed:
        case MQTTRecvFailed:
            result = NX_ERROR_IO;
            break;
        default:
            result = NX_ERROR_STATE;
            break;
    }
    bool recoverable = status == MQTTNeedMoreBytes ||
                       (status == MQTTServerRefused && !connecting) ||
                       (s->io_calls == 0u &&
                        (status == MQTTNoMemory || status == MQTTBadParameter));
    if (result != NX_SUCCESS && !recoverable) {
        fail(s, result);
    }
    if (s->fault != NX_SUCCESS) {
        result = s->fault;
    }
    s->active = false;
    return result;
}

nx_result_t nx_mqtt_init(nx_mqtt_t* s, const nx_mqtt_config_t* config,
                         const nx_mqtt_memory_t* memory) {
    if (s == NULL || config == NULL || memory == NULL) {
        return NX_ERROR_INVALID;
    }
    if ((uintptr_t)s % _Alignof(nx_mqtt_t) != 0u ||
        (uintptr_t)config % _Alignof(nx_mqtt_config_t) != 0u ||
        (uintptr_t)memory % _Alignof(nx_mqtt_memory_t) != 0u) {
        return NX_ERROR_INVALID;
    }
    if (s->opened) {
        return NX_ERROR_STATE;
    }
    if (config->stream.ops == NULL ||
        (uintptr_t)config->stream.ops % _Alignof(nx_byte_stream_ops_t) != 0u ||
        (uintptr_t)memory->outgoing % _Alignof(MQTTPubAckInfo_t) != 0u ||
        (uintptr_t)memory->incoming % _Alignof(MQTTPubAckInfo_t) != 0u ||
        (uintptr_t)memory->controls % _Alignof(nx_mqtt_control_t) != 0u ||
        config->stream.ops->read == NULL || config->stream.ops->write == NULL ||
        config->event == NULL || memory->buffer == NULL ||
        memory->buffer_size == 0u) {
        return NX_ERROR_INVALID;
    }
    if (memory->buffer_size > INT32_MAX ||
        memory->outgoing_count > UINT16_MAX ||
        memory->incoming_count > UINT16_MAX ||
        memory->control_count >= UINT16_MAX - memory->outgoing_count) {
        return NX_ERROR_OVERFLOW;
    }
    if ((memory->outgoing_count == 0u) != (memory->outgoing == NULL) ||
        (memory->incoming_count == 0u) != (memory->incoming == NULL) ||
        (memory->control_count == 0u) != (memory->controls == NULL)) {
        return NX_ERROR_INVALID;
    }
    const void* bases[] = {s,
                           config,
                           memory,
                           memory->buffer,
                           memory->outgoing,
                           memory->incoming,
                           memory->controls,
                           config->stream.ops};
    const size_t lengths[] = {
        sizeof(*s),
        sizeof(*config),
        sizeof(*memory),
        memory->buffer_size,
        memory->outgoing_count * sizeof(*memory->outgoing),
        memory->incoming_count * sizeof(*memory->incoming),
        memory->control_count * sizeof(*memory->controls),
        sizeof(*config->stream.ops)};
    for (size_t i = 0u; i < 8u; ++i) {
        if (!span(bases[i], lengths[i])) {
            return NX_ERROR_OVERFLOW;
        }
        for (size_t j = 0u; j < i; ++j) {
            if (overlap(bases[i], lengths[i], bases[j], lengths[j])) {
                return NX_ERROR_INVALID;
            }
        }
    }
    nx_time_us_t now = nx_mqtt_port_now_us();
    if (now == NX_DEADLINE_NEVER) {
        return NX_ERROR_EXHAUSTED;
    }
    memset(s, 0, sizeof(*s));
    s->config = *config;
    s->memory = *memory;
    s->last_time = now;
    if (memory->outgoing_count) {
        memset(memory->outgoing, 0, lengths[4]);
    }
    if (memory->incoming_count) {
        memset(memory->incoming, 0, lengths[5]);
    }
    if (memory->control_count) {
        memset(memory->controls, 0, lengths[6]);
    }
    TransportInterface_t transport = {mqtt_recv, mqtt_send, NULL,
                                      (NetworkContext_t*)s};
    MQTTFixedBuffer_t buffer = {memory->buffer, memory->buffer_size};
    MQTTStatus_t result =
        MQTT_Init(&s->mqtt, &transport, mqtt_time_ms, mqtt_event, &buffer);
    if (result == MQTTSuccess) {
        result = MQTT_InitStatefulQoS(&s->mqtt, memory->outgoing,
                                      memory->outgoing_count, memory->incoming,
                                      memory->incoming_count);
    }
    if (result != MQTTSuccess) {
        memset(s, 0, sizeof(*s));
        return NX_ERROR_INVALID;
    }
    s->opened = true;
    return NX_SUCCESS;
}

typedef enum { TEXT_RAW, TEXT_TOPIC, TEXT_FILTER } text_rule_t;

static bool text(const nx_mqtt_t* s, const char* p, size_t n,
                 text_rule_t rule) {
    if (n == 0u || n > UINT16_MAX || !external(s, p, n)) {
        return false;
    }
    for (size_t i = 0u; i < n; ++i) {
        if (p[i] == '\0') {
            return false;
        }
        if (rule == TEXT_TOPIC && (p[i] == '#' || p[i] == '+')) {
            return false;
        }
        if (rule == TEXT_FILTER && p[i] == '#' &&
            (i + 1u != n || (i && p[i - 1u] != '/'))) {
            return false;
        }
        if (rule == TEXT_FILTER && p[i] == '+' &&
            ((i && p[i - 1u] != '/') || (i + 1u < n && p[i + 1u] != '/'))) {
            return false;
        }
    }
    return true;
}

nx_result_t nx_mqtt_connect(nx_mqtt_t* s, const nx_mqtt_connect_t* connect,
                            nx_mqtt_call_t call) {
    if (s == NULL || connect == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!s->opened) {
        return NX_ERROR_STATE;
    }
    if (s->mqtt.connectStatus == MQTTConnected) {
        return NX_ERROR_STATE;
    }
    if (!external(s, connect, sizeof(*connect)) ||
        !text(s, connect->client_id, connect->client_id_size, TEXT_RAW) ||
        connect->username_size > UINT16_MAX ||
        connect->password_size > UINT16_MAX ||
        !external(s, connect->username, connect->username_size) ||
        !external(s, connect->password, connect->password_size) ||
        (connect->username_size &&
         !text(s, connect->username, connect->username_size, TEXT_RAW)) ||
        (connect->password != NULL && connect->username == NULL)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t admitted = admit(s, call, false);
    if (admitted != NX_SUCCESS) {
        return admitted;
    }
    MQTTConnectInfo_t c = {true,
                           connect->keepalive_seconds,
                           connect->client_id,
                           (uint16_t)connect->client_id_size,
                           connect->username,
                           (uint16_t)connect->username_size,
                           (const char*)connect->password,
                           (uint16_t)connect->password_size};
    uint64_t remaining = (s->deadline - s->last_time) / UINT64_C(1000);
    uint32_t timeout =
        remaining > UINT32_MAX ? UINT32_MAX : (uint32_t)remaining;
    bool present = false;
    MQTTStatus_t result = MQTT_Connect(&s->mqtt, &c, NULL, timeout, &present);
    if (result == MQTTSuccess && present) {
        fail(s, NX_ERROR_STATE);
    } else if (result == MQTTSuccess && s->memory.control_count != 0u) {
        /* A clean session cannot inherit requests from its predecessor. */
        memset(s->memory.controls, 0,
               s->memory.control_count * sizeof(*s->memory.controls));
    }
    return finished(s, result, true);
}

static uint16_t next_id(nx_mqtt_t* s) {
    size_t limit = s->memory.outgoing_count + s->memory.control_count;
    for (size_t i = 0u; i <= limit; ++i) {
        uint16_t id = MQTT_GetPacketId(&s->mqtt);
        bool used = find_control(s, id) != NULL;
        for (size_t j = 0u; !used && j < s->memory.outgoing_count; ++j) {
            const MQTTPubAckInfo_t* record = &s->memory.outgoing[j];
            used =
                record->packetId == id && record->publishState != MQTTStateNull;
        }
        if (!used) {
            return id;
        }
    }
    return 0u;
}

static bool output_id(const nx_mqtt_t* s, uint16_t* id) {
    return id != NULL && (uintptr_t)id % _Alignof(uint16_t) == 0u &&
           external(s, id, sizeof(*id));
}

nx_result_t nx_mqtt_publish(nx_mqtt_t* s, const nx_mqtt_publish_t* publish,
                            uint16_t* id, nx_mqtt_call_t call) {
    if (s == NULL || publish == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!s->opened) {
        return NX_ERROR_STATE;
    }
    if (s->active) {
        return NX_ERROR_BUSY;
    }
    if (publish->qos > 1u) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (!external(s, publish, sizeof(*publish)) || !output_id(s, id) ||
        !text(s, publish->topic, publish->topic_size, TEXT_TOPIC) ||
        !external(s, publish->payload, publish->payload_size) ||
        publish->payload_size > INT32_MAX ||
        overlap(id, sizeof(*id), publish, sizeof(*publish)) ||
        overlap(id, sizeof(*id), publish->topic, publish->topic_size) ||
        overlap(id, sizeof(*id), publish->payload, publish->payload_size)) {
        return NX_ERROR_INVALID;
    }
    if (publish->qos && !s->memory.outgoing_count) {
        return NX_ERROR_EXHAUSTED;
    }
    nx_result_t admitted = admit(s, call, true);
    if (admitted != NX_SUCCESS) {
        return admitted;
    }
    uint16_t packet = publish->qos ? next_id(s) : 0u;
    if (publish->qos && packet == 0u) {
        s->active = false;
        return NX_ERROR_EXHAUSTED;
    }
    MQTTPublishInfo_t p = {(MQTTQoS_t)publish->qos,
                           publish->retain,
                           false,
                           publish->topic,
                           (uint16_t)publish->topic_size,
                           publish->payload,
                           publish->payload_size};
    nx_result_t result = finished(s, MQTT_Publish(&s->mqtt, &p, packet), false);
    if (result == NX_SUCCESS) {
        *id = packet;
    }
    return result;
}

static nx_result_t subscription(nx_mqtt_t* s, const char* topic, size_t size,
                                uint8_t qos, uint16_t* id, nx_mqtt_call_t call,
                                bool remove) {
    if (s == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!s->opened) {
        return NX_ERROR_STATE;
    }
    if (s->active) {
        return NX_ERROR_BUSY;
    }
    if (qos > 1u) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (!output_id(s, id) || !text(s, topic, size, TEXT_FILTER) ||
        overlap(id, sizeof(*id), topic, size)) {
        return NX_ERROR_INVALID;
    }
    if (!remove && qos && !s->memory.incoming_count) {
        return NX_ERROR_EXHAUSTED;
    }
    nx_mqtt_control_t* slot = find_control(s, 0u);
    if (slot == NULL) {
        return NX_ERROR_EXHAUSTED;
    }
    nx_result_t admitted = admit(s, call, true);
    if (admitted != NX_SUCCESS) {
        return admitted;
    }
    uint16_t packet = next_id(s);
    if (packet == 0u) {
        s->active = false;
        return NX_ERROR_EXHAUSTED;
    }
    slot->packet_id = packet;
    slot->kind = remove ? MQTT_PACKET_TYPE_UNSUBACK : MQTT_PACKET_TYPE_SUBACK;
    slot->qos = qos;
    MQTTSubscribeInfo_t sub = {(MQTTQoS_t)qos, topic, (uint16_t)size};
    MQTTStatus_t status = remove ? MQTT_Unsubscribe(&s->mqtt, &sub, 1u, packet)
                                 : MQTT_Subscribe(&s->mqtt, &sub, 1u, packet);
    nx_result_t result = finished(s, status, false);
    if (result == NX_SUCCESS) {
        *id = packet;
    } else if (s->fault == NX_SUCCESS) {
        slot->packet_id = 0u;
    }
    return result;
}

nx_result_t nx_mqtt_subscribe(nx_mqtt_t* s, const char* topic, size_t size,
                              uint8_t qos, uint16_t* id, nx_mqtt_call_t call) {
    return subscription(s, topic, size, qos, id, call, false);
}

nx_result_t nx_mqtt_unsubscribe(nx_mqtt_t* s, const char* topic, size_t size,
                                uint16_t* id, nx_mqtt_call_t call) {
    return subscription(s, topic, size, 0u, id, call, true);
}

nx_result_t nx_mqtt_process(nx_mqtt_t* s, nx_mqtt_call_t call) {
    nx_result_t admitted = admit(s, call, true);
    if (admitted != NX_SUCCESS) {
        return admitted;
    }
    return finished(s, MQTT_ProcessLoop(&s->mqtt), false);
}

nx_result_t nx_mqtt_disconnect(nx_mqtt_t* s, nx_mqtt_call_t call) {
    nx_result_t admitted = admit(s, call, true);
    if (admitted != NX_SUCCESS) {
        return admitted;
    }
    return finished(s, MQTT_Disconnect(&s->mqtt), false);
}

nx_result_t nx_mqtt_deinit(nx_mqtt_t* s) {
    if (s == NULL) {
        return NX_ERROR_INVALID;
    }
    if (s->active) {
        return NX_ERROR_BUSY;
    }
    memset(s, 0, sizeof(*s));
    return NX_SUCCESS;
}

nx_result_t nx_mqtt_fault(const nx_mqtt_t* s) {
    return s == NULL || !s->opened ? NX_ERROR_STATE : s->fault;
}
