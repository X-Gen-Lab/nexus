/**
 * \file            core_mqtt_config.h
 * \brief           Reviewed finite polling profile; explicit adapter I/O budget
 * \author          Nexus Team
 */
#ifndef NEXUS_CORE_MQTT_CONFIG_H
#define NEXUS_CORE_MQTT_CONFIG_H
#include "nexus/mqtt/client.h"
#define MQTT_RECV_POLLING_TIMEOUT_MS         1U
#define MQTT_SEND_TIMEOUT_MS                 1000U
#define MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT 1U
#define MQTT_PINGRESP_TIMEOUT_MS             5000U
#undef assert
#define assert(condition)                                                      \
    ((condition) ? (void)0 : nx_mqtt_port_assert(__FILE__, (unsigned)__LINE__))
#endif /* NEXUS_CORE_MQTT_CONFIG_H */
