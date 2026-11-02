/**
 * \file            fixture.h
 * \brief           Native-only isolated lwIP-to-broker fixture ports
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_TESTS_LWIP_MQTT_FIXTURE_H
#define NEXUS_TESTS_LWIP_MQTT_FIXTURE_H
#include "nexus/network/byte_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Real monotonic elapsed clock shared by all test-client layers. */
nx_time_us_t nx_chain_now_us(void);
/** \brief Owner admits and progresses the actual TCP facade until established.
 *
ote Returns a borrowed stream into the live client; finite same-clock DL.
 */
bool nx_chain_connect(unsigned broker_port, const char* mode,
                      const nx_byte_stream_t** stream, nx_time_us_t deadline);
/** \brief Bound host waiting; it neither changes time nor runs a worker. */
nx_result_t nx_chain_wait(nx_time_us_t deadline);
/** \brief Ask the isolated relay to inject RST or stop ciphertext forwarding.
 */
bool nx_chain_fault(bool blackhole);
/** \brief Read completed RX and provider-service counters, without progress. */
void nx_chain_counters(size_t* receives, size_t* transmits);
/** \brief Close the TCP context after peer TLS alert consumption; finite DL. */
bool nx_chain_finish(nx_time_us_t deadline);
/** \brief Explicitly abort remaining TCP ownership and join MAC frame loans. */
bool nx_chain_stop(void);
/** \brief Run one configured real MQTT/TLS scenario in the current owner. */
int nx_chain_client(int argc, char** argv);
#ifdef __cplusplus
}
#endif
#endif
