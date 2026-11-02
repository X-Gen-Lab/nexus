/**
 * \file            application.h
 * \brief           Public-only bounded secure API consumption
 * \author          Nexus Team
 */
#ifndef SDK_SECURE_APPLICATION_H
#define SDK_SECURE_APPLICATION_H
#include "nexus/mqtt/client.h"
#include "nexus/tls/tls.h"

/** \brief Exercise initialized API paths; no server authentication is claimed.
 */
nx_result_t security_api_run(nx_tls_runtime_t* runtime,
                             const nx_tls_runtime_config_t* runtime_config,
                             nx_tls_session_t* tls,
                             const nx_tls_config_t* tls_config, nx_mqtt_t* mqtt,
                             const nx_mqtt_memory_t* mqtt_memory);
#endif /* SDK_SECURE_APPLICATION_H */
