/**
 * \file            composition.c
 * \brief           Explicit static storage for the unexecuted ARM link fixture
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"
#include "lwip/init.h"
#include "nexus/mqtt/core_mqtt_storage.h"
#include "nexus/network/lwip_storage.h"
#include "nexus/soc/stm32f407_entropy.h"
#include "nexus/tls/storage.h"
#include "nexus_bindings.h"
#include "public_ca.h"
#include <stdalign.h>

static alignas(max_align_t) uint8_t tls_arena[NX_TLS_ARENA_BYTES];
static uint8_t tls_stage[NX_TLS_TX_STAGE_BYTES];
static nx_tls_runtime_t tls_runtime;
static nx_tls_session_t tls_session;
static nx_mqtt_t mqtt_session;
static uint8_t mqtt_network[1024U];
static MQTTPubAckInfo_t mqtt_outgoing[2U];
static MQTTPubAckInfo_t mqtt_incoming[2U];
static nx_mqtt_control_t mqtt_controls[2U];
static nx_lwip_adapter_t network_adapter;
static uint8_t network_tx[NX_ETH_FRAME_MAX];
static nx_eth_fragment_t network_rx_views[1U];
static bool software_mac_stopped;

static nx_time_us_t now(void* context) {
    (void)context;
    return nx_time_now_us();
}

uint32_t nx_lwip_port_now_ms(void) {
    return (uint32_t)(nx_time_now_us() / 1000U);
}

void nx_lwip_port_assert(const char* message) {
    (void)message;
    for (;;) {
    }
}

uint32_t nx_lwip_port_random_u32(void) {
    uint8_t bytes[4U];
    const nx_clock_t clock = {now, NULL};
    nx_result_t result =
        nx_entropy_fill(nx_stm32f407_entropy_instance(), bytes, sizeof(bytes),
                        &clock, nx_deadline_after(nx_time_now_us(), 100000U));
    if (result != NX_SUCCESS) {
        nx_lwip_port_assert("Unqualified link fixture RNG source failed");
        for (;;) {
        }
    }
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

/* This software MAC rejects every transmit and has no acquired receive loan. */
static nx_result_t software_submit(void* context,
                                   nx_eth_frames_tx_request_t* request) {
    (void)context;
    (void)request;
    return NX_ERROR_BUSY;
}

static nx_result_t software_acquire(void* context,
                                    nx_eth_frames_rx_loan_t* loan,
                                    nx_eth_fragment_t* views, size_t capacity) {
    (void)context;
    (void)loan;
    (void)views;
    (void)capacity;
    return NX_ERROR_EMPTY;
}

static nx_result_t software_release(void* context,
                                    const nx_eth_frames_rx_loan_t* loan) {
    (void)context;
    (void)loan;
    return NX_ERROR_INVALID;
}

static nx_result_t software_service(void* context) {
    return *(const bool*)context ? NX_ERROR_STATE : NX_SUCCESS;
}

static nx_result_t software_apply_link(void* context,
                                       const nx_eth_link_t* link) {
    (void)context;
    (void)link;
    return NX_ERROR_UNSUPPORTED;
}

static nx_result_t software_stop(void* context) {
    *(bool*)context = true;
    return NX_SUCCESS;
}

static const nx_eth_frames_ops_t software_mac_ops = {
    .submit = software_submit,
    .service = software_service,
    .acquire = software_acquire,
    .release = software_release,
    .apply_link = software_apply_link,
    .stop = software_stop};
static const nx_eth_frames_port_t software_mac = {&software_mac_ops,
                                                  &software_mac_stopped};

static nx_result_t start_network(void) {
    lwip_init();
    const nx_lwip_config_t config = {&software_mac,
                                     {now, NULL},
                                     100000U,
                                     network_tx,
                                     sizeof(network_tx),
                                     network_rx_views,
                                     1U,
                                     {0x02U, 0U, 0U, 0U, 0U, 0x02U},
                                     {192U, 0U, 2U, 2U},
                                     {255U, 255U, 255U, 0U},
                                     {192U, 0U, 2U, 1U}};
    return nx_lwip_adapter_start(&network_adapter, &config);
}

/* A product must replace this with an authenticated/trusted UTC source. */
static nx_result_t untrusted_utc(void* context, int64_t* seconds) {
    (void)context;
    (void)seconds;
    return NX_ERROR_PERMISSION;
}

/* No PCB/PHY/TCP is claimed: this explicit software stream never connects. */
static nx_result_t unconnected_write(void* context, const uint8_t* data,
                                     size_t bytes, size_t* accepted,
                                     nx_time_us_t limit) {
    (void)context;
    (void)data;
    (void)limit;
    *accepted = 0U;
    return bytes == 0U ? NX_SUCCESS : NX_ERROR_BUSY;
}

static nx_result_t unconnected_read(void* context, uint8_t* data, size_t bytes,
                                    size_t* received, nx_time_us_t limit) {
    (void)context;
    (void)data;
    (void)limit;
    *received = 0U;
    return bytes == 0U ? NX_SUCCESS : NX_ERROR_BUSY;
}

static const nx_byte_stream_ops_t transport_ops = {unconnected_write,
                                                   unconnected_read};
static uint8_t unconnected_context;
static const nx_byte_stream_t transport = {&transport_ops,
                                           &unconnected_context};

int main(void) {
    nx_platform_start_result_t boot = nx_platform_start();
    if (boot.primary != NX_SUCCESS || boot.cleanup != NX_SUCCESS ||
        boot.remaining_effects != 0U) {
        return 1;
    }
    /* Exact facts of the selected reviewed hse8-pll168 Qiming assembly. */
    const nx_stm32f407_entropy_config_t entropy_config = {8000000U, 168000000U};
    nx_result_t result = nx_stm32f407_entropy_instance_start(&entropy_config);
    bool joined = true;
    bool network_started = false;
    if (result == NX_SUCCESS) {
        result = start_network();
        network_started = result == NX_SUCCESS;
    }
    if (result == NX_SUCCESS) {
        result = nx_lwip_adapter_service(&network_adapter, 1U);
    }
    if (result == NX_SUCCESS) {
        const nx_tls_runtime_config_t runtime_config = {
            tls_arena,
            sizeof(tls_arena),
            nx_stm32f407_entropy_instance(),
            {now, NULL},
            {untrusted_utc, NULL},
            100000U,
            /* Explicit link-fixture assumption, not min-entropy evidence. */
            NX_TLS_ENTROPY_FULL_SOURCE};
        const nx_tls_config_t tls_config = {
            &tls_runtime,  &transport, test_ca,   sizeof(test_ca),
            NULL,          0U,         NULL,      0U,
            "server.test", 11U,        tls_stage, sizeof(tls_stage)};
        const nx_mqtt_memory_t mqtt_memory = {
            mqtt_network,  sizeof(mqtt_network),
            mqtt_outgoing, 2U,
            mqtt_incoming, 2U,
            mqtt_controls, 2U};
        result = security_link_run(&tls_runtime, &runtime_config, &tls_session,
                                   &tls_config, &mqtt_session, &mqtt_memory,
                                   &joined);
    }
    if (!joined) {
        /* Preserve all borrowed resources on any incomplete service join. */
        for (;;) {
        }
    }
    if (network_started) {
        nx_result_t cleanup = nx_lwip_adapter_stop(&network_adapter);
        if (cleanup != NX_SUCCESS) {
            for (;;) {
            }
        }
    }
    nx_result_t stopped = nx_stm32f407_entropy_instance_stop();
    if (stopped != NX_SUCCESS) {
        return 2;
    }
    stopped = nx_platform_stop();
    return result == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 3;
}
