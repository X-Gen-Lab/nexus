/**
 * \file            application.h
 * \brief           Public-only service consumption for an ARM link fixture
 * \author          Nexus Team
 */
#ifndef SECURITY_LINK_APPLICATION_H
#define SECURITY_LINK_APPLICATION_H

#include "nexus/mqtt/client.h"
#include "nexus/tls/tls.h"

/** \brief Run the normal service path and report whether all borrowers joined.
 */
nx_result_t security_link_run(
    nx_tls_runtime_t* runtime, const nx_tls_runtime_config_t* runtime_config,
    nx_tls_session_t* tls, const nx_tls_config_t* tls_config, nx_mqtt_t* mqtt,
    const nx_mqtt_memory_t* mqtt_memory, bool* joined);

#endif /* SECURITY_LINK_APPLICATION_H */
