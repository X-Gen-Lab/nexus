/**
 * \file            core_mqtt_storage.h
 * \brief           Explicit exact-profile caller storage for coreMQTT 2.3.1
 * \author          Nexus Team
 */
#ifndef NEXUS_CORE_MQTT_STORAGE_H
#define NEXUS_CORE_MQTT_STORAGE_H
#include "core_mqtt.h"
#include "nexus/mqtt/client.h"

/** \brief Caller control ACK correlation, borrowed while initialized. */
typedef struct {
    uint16_t packet_id;
    uint8_t kind;
    uint8_t qos;
} nx_mqtt_control_t;

/**
 * \brief           Disjoint caller arrays; no maximum pool is hidden
 * \note            Counts zero require NULL pointers. Records are not message
 *                  payload retention; products keep semantic retry data.
 */
struct nx_mqtt_memory {
    uint8_t* buffer;
    size_t buffer_size;
    MQTTPubAckInfo_t* outgoing;
    size_t outgoing_count;
    MQTTPubAckInfo_t* incoming;
    size_t incoming_count;
    nx_mqtt_control_t* controls;
    size_t control_count;
};

/** \brief Exact backend storage, not a stable ABI or a public vendor face. */
struct nx_mqtt_session {
    MQTTContext_t mqtt;
    nx_mqtt_config_t config;
    nx_mqtt_memory_t memory;
    nx_time_us_t deadline;
    nx_time_us_t last_time;
    uint32_t remaining_calls;
    uint32_t io_calls;
    nx_result_t fault;
    bool opened;
    bool active;
};
#endif /* NEXUS_CORE_MQTT_STORAGE_H */
