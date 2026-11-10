/**
 * \file            main.c
 * \brief           Exact F407 factory Ethernet, PHY, TCP, TLS and MQTT linking
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-03
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"
#include "lwip/init.h"
#include "nexus/dp83848/dp83848.h"
#include "nexus/mqtt/core_mqtt_storage.h"
#include "nexus/network/lwip_storage.h"
#include "nexus/network/lwip_tcp_storage.h"
#include "nexus/tls/storage.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "public_ca.h"
#include <stdalign.h>

#define OPERATION_US 5000000U

static nx_lwip_adapter_t network;
static nx_lwip_tcp_t tcp_client;
static nx_dp83848_t phy;
static alignas(4) uint8_t network_tx[NX_ETH_FRAME_MAX];
static nx_eth_fragment_t network_rx_views[2U];
static alignas(max_align_t) uint8_t tls_arena[NX_TLS_ARENA_BYTES];
static uint8_t tls_stage[NX_TLS_TX_STAGE_BYTES];
static nx_tls_runtime_t tls_runtime;
static nx_tls_session_t tls_session;
static nx_mqtt_t mqtt_session;
static uint8_t mqtt_network[1024U];
static MQTTPubAckInfo_t mqtt_outgoing[2U];
static MQTTPubAckInfo_t mqtt_incoming[2U];
static nx_mqtt_control_t mqtt_controls[2U];

/* Failed withdrawal never revokes a DMA, pbuf, TLS or callback borrow. */
static void quarantine(void) {
    for (;;) {
    }
}

static nx_time_us_t now(void* context) {
    (void)context;
    return nx_time_now_us();
}

uint32_t nx_lwip_port_now_ms(void) {
    return (uint32_t)(nx_time_now_us() / 1000U);
}

void nx_lwip_port_assert(const char* message) {
    (void)message;
    quarantine();
}

uint32_t nx_lwip_port_random_u32(void) {
    uint8_t bytes[4U];
    const nx_clock_t clock = {now, NULL};
    nx_result_t result = nx_entropy_fill(
        nx_factory_entropy(NX_ENTROPY_ID_RANDOM), bytes, sizeof(bytes), &clock,
        nx_deadline_after(nx_time_now_us(), 100000U));
    if (result != NX_SUCCESS) {
        quarantine();
    }
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

/* Product UTC authority is absent: certificate admission must fail closed. */
static nx_result_t untrusted_utc(void* context, int64_t* seconds) {
    (void)context;
    (void)seconds;
    return NX_ERROR_PERMISSION;
}

static nx_result_t prepare_phy(const nx_mdio_port_t* mdio) {
    phy.mdio = mdio;
    phy.expected_id = NX_DP83848_TI_ID;
    phy.address = 1U;
    nx_time_us_t limit = nx_deadline_after(nx_time_now_us(), OPERATION_US);
    nx_result_t result = nx_dp83848_probe(&phy, limit);
    if (result == NX_SUCCESS) {
        result = nx_dp83848_reset(&phy, limit);
    }
    if (result == NX_SUCCESS) {
        const nx_dp83848_mode_t mode = {
            .advertisement = NX_DP83848_10_HALF | NX_DP83848_10_FULL |
                             NX_DP83848_100_HALF | NX_DP83848_100_FULL,
            .autonegotiation = true,
            .full_duplex = false,
            .speed_mbps = 0U};
        result = nx_dp83848_configure(&phy, &mode, limit);
    }
    return result;
}

static nx_result_t wait_link(void) {
    nx_time_us_t limit = nx_deadline_after(nx_time_now_us(), OPERATION_US);
    do {
        nx_eth_link_t link;
        nx_result_t result = nx_dp83848_link(&phy, &link, limit);
        if (result != NX_SUCCESS) {
            return result;
        }
        result = nx_lwip_adapter_apply_link(&network, &link);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            return result;
        }
        if (result == NX_SUCCESS && link.up) {
            return NX_SUCCESS;
        }
        result = nx_lwip_adapter_service(&network, 2U);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            return result;
        }
    } while (nx_time_now_us() < limit);
    return NX_ERROR_TIMEOUT;
}

static nx_result_t connect_tcp(void) {
    const nx_lwip_tcp_config_t config = {
        &network, 2U, {192U, 0U, 2U, 1U}, 8883U};
    nx_time_us_t limit = nx_deadline_after(nx_time_now_us(), OPERATION_US);
    nx_result_t result = nx_lwip_tcp_start(&tcp_client, &config, limit);
    if (result != NX_SUCCESS) {
        return result;
    }
    do {
        nx_lwip_tcp_diagnostic_t diagnostic;
        result = nx_lwip_tcp_service(&tcp_client);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            return result;
        }
        result = nx_lwip_tcp_diagnostic(&tcp_client, &diagnostic);
        if (result != NX_SUCCESS) {
            return result;
        }
        if (diagnostic.state == NX_LWIP_TCP_ESTABLISHED) {
            return NX_SUCCESS;
        }
    } while (nx_time_now_us() < limit);
    return NX_ERROR_TIMEOUT;
}

static nx_result_t run_services(bool* joined) {
    const nx_tls_runtime_config_t runtime_config = {
        tls_arena,
        sizeof(tls_arena),
        nx_factory_entropy(NX_ENTROPY_ID_RANDOM),
        {now, NULL},
        {untrusted_utc, NULL},
        100000U,
        /* Software assumption only; physical min-entropy is unmeasured. */
        NX_TLS_ENTROPY_FULL_SOURCE};
    const nx_tls_config_t tls_config = {
        &tls_runtime,  nx_lwip_tcp_stream(&tcp_client),
        test_ca,       sizeof(test_ca),
        NULL,          0U,
        NULL,          0U,
        "server.test", 11U,
        tls_stage,     sizeof(tls_stage)};
    const nx_mqtt_memory_t mqtt_memory = {mqtt_network,  sizeof(mqtt_network),
                                          mqtt_outgoing, 2U,
                                          mqtt_incoming, 2U,
                                          mqtt_controls, 2U};
    return security_link_run(&tls_runtime, &runtime_config, &tls_session,
                             &tls_config, &mqtt_session, &mqtt_memory, joined);
}

int main(void) {
    nx_platform_start_result_t boot = nx_platform_start();
    if (boot.primary != NX_SUCCESS || boot.cleanup != NX_SUCCESS ||
        boot.remaining_effects != 0U) {
        if (nx_platform_stop() != NX_SUCCESS) {
            quarantine();
        }
        return 1;
    }
    nx_result_t result = nx_factory_entropy(NX_ENTROPY_ID_RANDOM) != NULL
                             ? NX_SUCCESS
                             : NX_ERROR_STATE;
    const nx_eth_frames_port_t* mac =
        nx_factory_eth_frames(NX_ETH_FRAMES_ID_MAC);
    const nx_mdio_port_t* mdio = nx_factory_mdio(NX_MDIO_ID_MAC);
    if (result == NX_SUCCESS && mac != NULL && mdio != NULL) {
        result = prepare_phy(mdio);
    } else if (result == NX_SUCCESS) {
        result = NX_ERROR_STATE;
    }
    if (result != NX_SUCCESS) {
        if (nx_platform_stop() != NX_SUCCESS) {
            quarantine();
        }
        return 2;
    }
    /* Initialize once; this clock/RNG domain remains live until reset. */
    lwip_init();
    const nx_lwip_config_t config = {mac,
                                     {now, NULL},
                                     100000U,
                                     network_tx,
                                     sizeof(network_tx),
                                     network_rx_views,
                                     2U,
                                     {2U, 0U, 0U, 0U, 0U, 2U},
                                     {192U, 0U, 2U, 2U},
                                     {255U, 255U, 255U, 0U},
                                     {192U, 0U, 2U, 1U}};
    result = nx_lwip_adapter_start(&network, &config);
    bool attached = result == NX_SUCCESS;
    if (result == NX_SUCCESS) {
        result = wait_link();
    }
    if (result == NX_SUCCESS) {
        result = connect_tcp();
    }
    bool joined = true;
    if (result == NX_SUCCESS) {
        result = run_services(&joined);
    }
    if (!joined || nx_lwip_tcp_abort(&tcp_client) != NX_SUCCESS) {
        quarantine();
    }
    if (attached && nx_lwip_adapter_stop(&network) != NX_SUCCESS) {
        quarantine();
    }
    /* Netif removal does not retire global lwIP timers or their clock epoch.
     * No further stack calls run; preserve platform/RNG/storage until reset. */
    (void)result;
    quarantine();
    return 3;
}
