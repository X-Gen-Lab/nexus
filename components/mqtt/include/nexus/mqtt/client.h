/**
 * \file            client.h
 * \brief           Bounded synchronous MQTT 3.1.1 client over explicit streams
 * \author          Nexus Team
 */
#ifndef NEXUS_MQTT_CLIENT_H
#define NEXUS_MQTT_CLIENT_H

#include "nexus/network/byte_stream.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct nx_mqtt_session nx_mqtt_t;
typedef struct nx_mqtt_memory nx_mqtt_memory_t;

typedef enum {
    NX_MQTT_EVENT_PUBLISH,
    NX_MQTT_EVENT_PUBACK,
    NX_MQTT_EVENT_SUBACK,
    NX_MQTT_EVENT_UNSUBACK
} nx_mqtt_event_kind_t;

/** \brief Callback-scoped facts; all pointers expire with callback return. */
typedef struct {
    nx_mqtt_event_kind_t kind;
    uint16_t packet_id;
    uint8_t qos;
    bool retain;
    bool duplicate;
    nx_result_t status;
    const char* topic;
    size_t topic_size;
    const uint8_t* payload;
    size_t payload_size;
} nx_mqtt_event_t;

/** \brief Explicit finite operation deadline and transport-call allocation. */
typedef struct {
    nx_time_us_t deadline;
    uint32_t io_budget;
} nx_mqtt_call_t;

/**
 * \brief           Caller-owned ports retained until deinit
 * \note            All calls/callbacks use one serialized task context. Event
 *                  and optional idle callbacks must be bounded and may not
 *                  reenter this client. Idle may service/wait on lower layers;
 *                  no worker, lock, scheduler or implicit reconnect is created.
 *                  Idle SUCCESS/BUSY permit retry; other results fail the
 *                  session. Event views are not NUL-terminated strings.
 */
typedef struct {
    nx_byte_stream_t stream;
    void (*event)(void* context, const nx_mqtt_event_t* event);
    void* event_context;
    nx_result_t (*idle)(void* context, nx_time_us_t deadline);
    void* idle_context;
} nx_mqtt_config_t;

/** \brief Transient CONNECT parameters; this profile always uses clean session.
 */
typedef struct {
    const char* client_id;
    size_t client_id_size;
    const char* username;
    size_t username_size;
    const uint8_t* password;
    size_t password_size;
    uint16_t keepalive_seconds;
} nx_mqtt_connect_t;

/** \brief Transient publish; QoS1 confirmation is a later PUBACK event. */
typedef struct {
    const char* topic;
    size_t topic_size;
    const uint8_t* payload;
    size_t payload_size;
    uint8_t qos;
    bool retain;
} nx_mqtt_publish_t;

/**
 * \brief           Product clock shared by all clients and their transports
 * \return          Absolute monotonic microseconds in one stable boot/domain
 * \note            Required context-free hook matches upstream's timer API.
 *                  Never silently change epoch/domain. UINT64_MAX is invalid.
 *                  No active-instance global registry is used by this adapter.
 */
nx_time_us_t nx_mqtt_port_now_us(void);
/**
 * \brief           Nonreturning product invariant-failure hook
 * \param[in]       file: Static source location
 * \param[in]       line: Source line
 * \note            Must not reclaim DMA or transport storage on an assertion.
 */
void nx_mqtt_port_assert(const char* file, unsigned line);

/**
 * \brief           Initialize exact caller storage without opening transport
 * \param[in,out]   client: Zero-initialized or deinited metadata
 * \param[in]       config: Valid stable ports; values copied, contexts borrowed
 * \param[in,out]   memory: Disjoint network, QoS and control storage
 * \return          SUCCESS, INVALID, STATE, OVERFLOW or EXHAUSTED
 * \note            Task-only, no I/O/heap. All storage and port contexts remain
 *                  valid and exclusively owned through successful deinit.
 */
nx_result_t nx_mqtt_init(nx_mqtt_t* client, const nx_mqtt_config_t* config,
                         const nx_mqtt_memory_t* memory);
/**
 * \brief           Send clean-session CONNECT and await CONNACK synchronously
 * \param[in,out]   client: Initialized inactive client on a fresh open stream
 * \param[in]       connect: Legal MQTT UTF-8 fields, borrowed for this call
 * \param[in]       call: Finite absolute deadline and nonzero I/O budget
 * \return          SUCCESS, admission error or sticky session/transport failure
 * \note            Does not connect TCP/TLS. The product provides transport
 *                  and authentication policy. Failed CONNECT is not replayed.
 */
nx_result_t nx_mqtt_connect(nx_mqtt_t* client, const nx_mqtt_connect_t* connect,
                            nx_mqtt_call_t call);
/**
 * \brief           Publish QoS0/1 with call-scoped topic and payload loans
 * \param[in,out]   client: Connected serialized client
 * \param[in]       publish: Valid UTF-8 topic and byte payload; no aliases
 * \param[out]      packet_id: Written on success only; zero for QoS0
 * \param[in]       call: Finite deadline and nonzero I/O budget
 * \return          SUCCESS for local stream admission, never broker delivery
 * \note            QoS1 records remain borrowed through deinit. PUBACK signals
 *                  confirmation; the application owns message retry/data
 *                  retention. No persistent-session replay or QoS2 is provided.
 */
nx_result_t nx_mqtt_publish(nx_mqtt_t* client, const nx_mqtt_publish_t* publish,
                            uint16_t* packet_id, nx_mqtt_call_t call);
/**
 * \brief           Send one bounded subscription request
 * \param[in,out]   client: Connected client with a free caller control slot
 * \param[in]       topic: Call-scoped valid MQTT UTF-8 filter
 * \param[in]       topic_size: Nonzero bytes, at most UINT16_MAX
 * \param[in]       qos: Requested maximum zero or one
 * \param[out]      packet_id: Written on successful send only
 * \param[in]       call: Explicit deadline and I/O budget
 * \return          SUCCESS means sent; SUBACK event reports grant/refusal
 */
nx_result_t nx_mqtt_subscribe(nx_mqtt_t* client, const char* topic,
                              size_t topic_size, uint8_t qos,
                              uint16_t* packet_id, nx_mqtt_call_t call);
/**
 * \brief           Send one bounded unsubscribe request
 * \param[in,out]   client: Connected client with a free caller control slot
 * \param[in]       topic: Call-scoped valid MQTT UTF-8 filter
 * \param[in]       topic_size: Nonzero bytes, at most UINT16_MAX
 * \param[out]      packet_id: Written on successful send only
 * \param[in]       call: Explicit deadline and I/O budget
 * \return          SUCCESS means sent; UNSUBACK is observed during process
 */
nx_result_t nx_mqtt_unsubscribe(nx_mqtt_t* client, const char* topic,
                                size_t topic_size, uint16_t* packet_id,
                                nx_mqtt_call_t call);
/**
 * \brief           Caller-driven receive, acknowledgments and keepalive step
 * \param[in,out]   client: Connected serialized client
 * \param[in]       call: Explicit deadline and I/O budget
 * \return          SUCCESS, BUSY for incomplete data, or sticky failure
 * \note            Callback views expire on return; copy needed data there.
 *                  No implicit timer/task registration or automatic replay.
 */
nx_result_t nx_mqtt_process(nx_mqtt_t* client, nx_mqtt_call_t call);
/**
 * \brief           Send MQTT DISCONNECT without closing the transport
 * \param[in,out]   client: Connected serialized client
 * \param[in]       call: Explicit deadline and I/O budget
 * \return          SUCCESS means local admission, not TCP/TLS drain
 */
nx_result_t nx_mqtt_disconnect(nx_mqtt_t* client, nx_mqtt_call_t call);
/**
 * \brief           End local borrows after all synchronous calls return
 * \param[in,out]   client: Inactive initialized client, including failed client
 * \return          SUCCESS, BUSY for reentry or INVALID
 * \note            No I/O. The product separately drains/closes TCP/TLS before
 *                  another MQTT session; deinit cannot retract accepted bytes.
 */
nx_result_t nx_mqtt_deinit(nx_mqtt_t* client);
/**
 * \brief           Inspect sticky failure in the same serialized task context
 * \param[in]       client: Initialized instance
 * \return          First failure, SUCCESS, or STATE for an inactive instance
 */
nx_result_t nx_mqtt_fault(const nx_mqtt_t* client);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_MQTT_CLIENT_H */
