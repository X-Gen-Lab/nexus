/**
 * \file            main.c
 * \brief           Compile-only selected lwIP RX profile and resource probe
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            No MAC, PHY, device execution or usable random source is
 *                  supplied. The random hook deliberately enters a fault loop.
 */
#include "lwip/init.h"
#include "nexus_bindings.h"
#if defined(NEXUS_LWIP_LOANED_RX)
#include "nexus/network/lwip_loaned_rx_storage.h"
#else
#include "nexus/network/lwip_storage.h"
#endif

static nx_lwip_adapter_t s_adapter;
static uint8_t s_transmit[NX_ETH_FRAME_MAX];
static nx_eth_fragment_t s_views[2];
static unsigned s_port_context;
#if defined(NEXUS_LWIP_LOANED_RX)
static nx_lwip_rx_slot_t s_slots[2];
static nx_lwip_rx_piece_t s_pieces[4];
#endif

/** \brief No concrete MAC capability is claimed by this CPU link probe. */
static nx_result_t submit(void* context, nx_eth_frames_tx_request_t* request) {
    (void)context;
    (void)request;
    return NX_ERROR_UNSUPPORTED;
}
static nx_result_t acquire(void* context, nx_eth_frames_rx_loan_t* loan,
                           nx_eth_fragment_t* fragments, size_t capacity) {
    (void)context;
    (void)loan;
    (void)fragments;
    (void)capacity;
    return NX_ERROR_EMPTY;
}
static nx_result_t release(void* context, const nx_eth_frames_rx_loan_t* loan) {
    (void)context;
    (void)loan;
    return NX_ERROR_STATE;
}
static nx_result_t service(void* context) {
    (void)context;
    return NX_SUCCESS;
}
static nx_result_t apply_link(void* context, const nx_eth_link_t* observed) {
    (void)context;
    return observed->up ? NX_ERROR_UNSUPPORTED : NX_SUCCESS;
}
static const nx_eth_frames_ops_t s_ops = {
    .submit = submit,
    .service = service,
    .acquire = acquire,
    .release = release,
    .apply_link = apply_link,
    .stop = service,
};
static const nx_eth_frames_port_t s_port = {&s_ops, &s_port_context};

static nx_time_us_t read_clock(void* context) {
    (void)context;
    return nx_time_now_us();
}

uint32_t nx_lwip_port_now_ms(void) {
    return (uint32_t)(nx_time_now_us() / UINT64_C(1000));
}
uint32_t nx_lwip_port_random_u32(void) {
    /* An unavailable random source never falls back to a fixed MCU seed. */
    for (;;) {
    }
}
void nx_lwip_port_assert(const char* message) {
    (void)message;
    for (;;) {
    }
}

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    lwip_init();
    const nx_lwip_config_t base = {
        .port = &s_port,
        .clock = {read_clock, NULL},
        .tx_timeout_us = 100000U,
        .tx_buffer = s_transmit,
        .tx_capacity = sizeof(s_transmit),
        .rx_views = s_views,
        .rx_capacity = 2U,
        .hardware_address = {2U, 0U, 0U, 0U, 0U, 2U},
        .address = {10U, 0U, 0U, 2U},
        .netmask = {255U, 255U, 255U, 0U},
    };
#if defined(NEXUS_LWIP_LOANED_RX)
    const nx_lwip_loaned_config_t selected = {
        .base = base,
        .slots = s_slots,
        .slot_count = 2U,
        .pieces = s_pieces,
        .piece_count = 4U,
    };
    nx_result_t result = nx_lwip_loaned_adapter_start(&s_adapter, &selected);
#else
    nx_result_t result = nx_lwip_adapter_start(&s_adapter, &base);
#endif
    if (result != NX_SUCCESS) {
        return 2;
    }
    if (nx_lwip_adapter_service(&s_adapter, 1U) != NX_SUCCESS ||
        nx_lwip_adapter_stop(&s_adapter) != NX_SUCCESS) {
        return 3;
    }
    return nx_platform_stop() == NX_SUCCESS ? 0 : 4;
}
