/**
 * \file            services.c
 * \brief           Installed SDK success paths for optional common services
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            Bounded software models only. RAM does not establish Flash
 *                  durability; model link facts do not qualify a PHY or board.
 */
#include "lwip/init.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "nexus/components/settings.h"
#include "nexus/network/lwip_adapter.h"
#include "nexus/network/lwip_storage.h"
#include "nexus/os/wait.h"
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include <string.h>

/** \brief Finite NOR model; all port calls are synchronous and settled. */
typedef struct {
    uint8_t bytes[512];
    size_t programs;
    size_t erases;
    size_t syncs;
} nor_model_t;

/** \brief One serialized notification model, without an execution worker. */
typedef struct {
    uint32_t sequence;
    unsigned waits;
    bool ready;
} wait_model_t;

/** \brief One whole frame loan with exact release and immediate TX completion.
 */
typedef struct {
    nx_eth_frames_state_t state;
    uint8_t transmitted[NX_ETH_FRAME_MAX];
    size_t transmitted_bytes;
    size_t transmits;
    size_t releases;
    size_t stops;
    bool receive_ready;
    bool receive_held;
} mac_model_t;

static nor_model_t s_nor;
static nx_storage_t s_storage;
static nx_storage_t s_reopened_storage;
static nx_settings_t s_settings;
static uint8_t s_storage_workspace[32];
static uint8_t s_reopened_workspace[32];
static uint8_t s_settings_image[128];
static wait_model_t s_wait;
static mac_model_t s_mac;
static nx_lwip_adapter_t s_adapter;
static uint8_t s_transmit[NX_ETH_FRAME_MAX];
static nx_eth_fragment_t s_receive_views[2];

static const uint8_t s_arp_request[60] = {
    255U, 255U, 255U, 255U, 255U, 255U, 2U, 0U, 0U, 0U, 0U,  1U, 8U, 6U,
    0U,   1U,   8U,   0U,   6U,   4U,   0U, 1U, 2U, 0U, 0U,  0U, 0U, 1U,
    10U,  0U,   0U,   1U,   0U,   0U,   0U, 0U, 0U, 0U, 10U, 0U, 0U, 2U};

/** \brief Validate the complete port span before a model byte is accessed. */
static bool nor_span(size_t offset, size_t size) {
    return offset <= sizeof(s_nor.bytes) &&
           size <= sizeof(s_nor.bytes) - offset;
}

/** \brief Copy stable model bytes without retaining the destination. */
static nx_storage_status_t nor_read(void* context, size_t offset, void* output,
                                    size_t size) {
    nor_model_t* nor = context;
    if (!nor_span(offset, size) || (output == NULL && size != 0U)) {
        return NX_STORAGE_INVALID;
    }
    if (size != 0U) {
        memcpy(output, nor->bytes + offset, size);
    }
    return NX_STORAGE_OK;
}

/** \brief Model only complete aligned programming with one-to-zero transitions.
 */
static nx_storage_status_t nor_program(void* context, size_t offset,
                                       const void* input, size_t size) {
    nor_model_t* nor = context;
    const uint8_t* bytes = input;
    if (!nor_span(offset, size) || (offset % 4U) != 0U || (size % 4U) != 0U ||
        (input == NULL && size != 0U)) {
        return NX_STORAGE_INVALID;
    }
    for (size_t i = 0U; i < size; ++i) {
        if ((nor->bytes[offset + i] & bytes[i]) != bytes[i]) {
            return NX_STORAGE_IO;
        }
    }
    for (size_t i = 0U; i < size; ++i) {
        nor->bytes[offset + i] &= bytes[i];
    }
    ++nor->programs;
    return NX_STORAGE_OK;
}

/** \brief Each bank has an explicitly uniform model erase unit. */
static nx_storage_status_t nor_erase(void* context, size_t offset,
                                     size_t size) {
    nor_model_t* nor = context;
    if (!nor_span(offset, size) || (offset % 256U) != 0U ||
        (size % 256U) != 0U) {
        return NX_STORAGE_INVALID;
    }
    memset(nor->bytes + offset, 0xff, size);
    ++nor->erases;
    return NX_STORAGE_OK;
}

/** \brief Record the requested model barrier without claiming real persistence.
 */
static nx_storage_status_t nor_sync(void* context) {
    ++((nor_model_t*)context)->syncs;
    return NX_STORAGE_OK;
}

/** \brief Commit one value, reopen the durable mechanism, and recover its
 * bytes. */
static int settings_success(void) {
    const nx_storage_port_t port = {
        &s_nor,   sizeof(s_nor.bytes), 256U,      4U,
        nor_read, nor_program,         nor_erase, nor_sync};
    const uint8_t value[] = {0x31U, 0x72U, 0xa5U};
    uint8_t recovered[sizeof(value)];
    size_t size = sizeof(recovered);
    memset(s_nor.bytes, 0xff, sizeof(s_nor.bytes));
    if (nx_storage_open(&s_storage, &port, 0U, 256U, s_storage_workspace,
                        sizeof(s_storage_workspace)) != NX_STORAGE_OK ||
        nx_settings_open(&s_settings, &s_storage, UINT32_C(0x53444b31),
                         s_settings_image, sizeof(s_settings_image),
                         4U) != NX_STORAGE_OK ||
        nx_settings_set(&s_settings, 7U, value, sizeof(value)) !=
            NX_STORAGE_OK ||
        nx_settings_commit(&s_settings) != NX_STORAGE_OK) {
        return 10;
    }
    nx_settings_close(&s_settings);
    if (s_nor.programs == 0U || s_nor.erases == 0U || s_nor.syncs == 0U ||
        nx_storage_open(&s_reopened_storage, &port, 0U, 256U,
                        s_reopened_workspace,
                        sizeof(s_reopened_workspace)) != NX_STORAGE_OK ||
        nx_settings_open(&s_settings, &s_reopened_storage, UINT32_C(0x53444b31),
                         s_settings_image, sizeof(s_settings_image),
                         4U) != NX_STORAGE_OK ||
        nx_settings_get(&s_settings, 7U, recovered, &size) != NX_STORAGE_OK ||
        size != sizeof(value) || memcmp(recovered, value, size) != 0) {
        return 11;
    }
    nx_settings_close(&s_settings);
    return 0;
}

/** \brief Snapshot the model notification sequence before rechecking sources.
 */
static uint32_t wait_arm(void* context) {
    return ((wait_model_t*)context)->sequence;
}

/** \brief One finite model notification also publishes authoritative readiness.
 */
static nx_result_t wait_once(void* context, uint32_t sequence,
                             uint64_t deadline_us) {
    wait_model_t* model = context;
    if (sequence != model->sequence || deadline_us != 20000U) {
        return NX_ERROR_STATE;
    }
    ++model->waits;
    ++model->sequence;
    model->ready = true;
    return NX_SUCCESS;
}

/** \brief Return the caller's stable predicate rather than consume a hint. */
static bool source_ready(void* context) {
    return *(const bool*)context;
}

/** \brief Observe the second source only after a real arm/wait/recheck pass. */
static int wait_success(void) {
    bool never_ready = false;
    const nx_wait_port_t port = {&s_wait, wait_arm, wait_once, NULL};
    const nx_wait_source_t sources[] = {{source_ready, &never_ready},
                                        {source_ready, &s_wait.ready}};
    size_t selected = SIZE_MAX;
    if (nx_wait_select(&port, sources, 2U, 0U, 20000U, &selected) !=
            NX_SUCCESS ||
        selected != 1U || s_wait.waits != 1U) {
        return 20;
    }
    return 0;
}

/** \brief Settle only after every submitted fragment has been copied. */
static nx_result_t mac_submit(void* context,
                              nx_eth_frames_tx_request_t* request) {
    mac_model_t* mac = context;
    size_t bytes = 0U;
    for (size_t i = 0U; i < request->fragment_count; ++i) {
        if (request->fragments[i].length > sizeof(mac->transmitted) - bytes) {
            return NX_ERROR_OVERFLOW;
        }
        bytes += request->fragments[i].length;
    }
    if (nx_request_admit(&request->base, NX_REQUEST_ACTIVE) != NX_SUCCESS) {
        return NX_ERROR_STATE;
    }
    size_t offset = 0U;
    for (size_t i = 0U; i < request->fragment_count; ++i) {
        const nx_eth_fragment_t* fragment = &request->fragments[i];
        memcpy(mac->transmitted + offset, fragment->data, fragment->length);
        offset += fragment->length;
    }
    mac->transmitted_bytes = bytes;
    ++mac->transmits;
    nx_request_settle(&request->base, NX_SUCCESS, bytes);
    return NX_SUCCESS;
}

/** \brief Progress is explicit even though this finite model has no IRQs. */
static nx_result_t mac_service(void* context) {
    (void)context;
    return NX_SUCCESS;
}

/** \brief Lend exactly one complete immutable software ARP frame. */
static nx_result_t mac_acquire(void* context, nx_eth_frames_rx_loan_t* loan,
                               nx_eth_fragment_t* views, size_t capacity) {
    mac_model_t* mac = context;
    if (!mac->receive_ready) {
        return NX_ERROR_EMPTY;
    }
    if (mac->receive_held || capacity == 0U) {
        return NX_ERROR_BUSY;
    }
    *loan = (nx_eth_frames_rx_loan_t){0};
    loan->identity = mac;
    loan->epoch = 1U;
    loan->descriptor_count = 1U;
    loan->fragment_count = 1U;
    loan->length = sizeof(s_arp_request);
    views[0] = (nx_eth_fragment_t){s_arp_request, sizeof(s_arp_request)};
    mac->receive_held = true;
    return NX_SUCCESS;
}

/** \brief Only the exact held incarnation releases its model bytes. */
static nx_result_t mac_release(void* context,
                               const nx_eth_frames_rx_loan_t* loan) {
    mac_model_t* mac = context;
    if (!mac->receive_held || loan->identity != mac || loan->epoch != 1U) {
        return NX_ERROR_STATE;
    }
    mac->receive_held = false;
    mac->receive_ready = false;
    ++mac->releases;
    return NX_SUCCESS;
}

/** \brief Return copied model state without touching a physical controller. */
static nx_result_t mac_state(void* context, nx_eth_frames_state_t* state) {
    *state = ((mac_model_t*)context)->state;
    return NX_SUCCESS;
}

/** \brief Apply an explicitly labelled software-only link observation. */
static nx_result_t mac_apply_link(void* context, const nx_eth_link_t* link) {
    ((mac_model_t*)context)->state.mac.link = *link;
    return NX_SUCCESS;
}

/** \brief No software frame borrow may survive the model's successful stop. */
static nx_result_t mac_stop(void* context) {
    mac_model_t* mac = context;
    if (mac->receive_held) {
        return NX_ERROR_BUSY;
    }
    mac->state.mac.started = false;
    ++mac->stops;
    return NX_SUCCESS;
}

/** \brief One frozen, unqualified model time domain for this finite smoke. */
uint32_t nx_lwip_port_now_ms(void) {
    return 10U;
}

/** \brief Deterministic software-fixture data, never a production entropy port.
 */
uint32_t nx_lwip_port_random_u32(void) {
    return UINT32_C(0x12345678);
}

/** \brief A stack invariant violation fails this smoke without inventing
 * recovery. */
void nx_lwip_port_assert(const char* message) {
    (void)message;
    __builtin_trap();
    for (;;) {
    }
}

/** \brief Provider deadlines use the same explicit software-fixture domain. */
static nx_time_us_t model_clock(void* context) {
    (void)context;
    return 10000U;
}

/** \brief Parse a genuine ARP request through a live installed stack and
 * adapter. */
static int network_success(void) {
    static const nx_eth_frames_ops_t ops = {
        mac_submit,  NULL,      mac_service, mac_acquire,
        mac_release, mac_state, NULL,        mac_apply_link,
        mac_stop,    NULL,      NULL};
    static const nx_eth_frames_port_t port = {&ops, &s_mac};
    const nx_eth_link_t model_link = {true, true, 100U};
    const nx_lwip_config_t config = {
        .port = &port,
        .clock = {model_clock, NULL},
        .tx_timeout_us = 100000U,
        .tx_buffer = s_transmit,
        .tx_capacity = sizeof(s_transmit),
        .rx_views = s_receive_views,
        .rx_capacity = 2U,
        .hardware_address = {2U, 0U, 0U, 0U, 0U, 2U},
        .address = {10U, 0U, 0U, 2U},
        .netmask = {255U, 255U, 255U, 0U}};
    s_mac.state.mac.started = true;
    lwip_init();
    if (nx_lwip_adapter_start(&s_adapter, &config) != NX_SUCCESS ||
        nx_lwip_adapter_apply_link(&s_adapter, &model_link) != NX_SUCCESS) {
        return 30;
    }
    s_mac.transmits = 0U;
    s_mac.transmitted_bytes = 0U;
    s_mac.receive_ready = true;
    int result = 0;
    if (nx_lwip_adapter_service(&s_adapter, 1U) != NX_SUCCESS ||
        s_mac.releases != 1U || s_mac.receive_held || s_mac.transmits != 1U ||
        s_mac.transmitted_bytes < 42U || s_mac.transmitted[12] != 8U ||
        s_mac.transmitted[13] != 6U || s_mac.transmitted[21] != 2U) {
        result = 31;
    }
    struct pbuf* packet = pbuf_alloc(PBUF_RAW, 67U, PBUF_RAM);
    struct tcp_pcb* tcp = tcp_new();
    struct udp_pcb* udp = udp_new();
    if (packet == NULL || tcp == NULL || udp == NULL) {
        result = 32;
    }
    if (packet != NULL) {
        pbuf_free(packet);
    }
    if (tcp != NULL) {
        tcp_abort(tcp);
    }
    if (udp != NULL) {
        udp_remove(udp);
    }
    if (nx_lwip_adapter_stop(&s_adapter) != NX_SUCCESS || s_mac.stops != 1U ||
        s_adapter.attached || s_mac.receive_held) {
        return 33;
    }
    return result;
}

/** \brief Run installed public success paths; ARM builds establish linking
 * only. */
int main(void) {
    nx_platform_start_result_t start = nx_platform_start();
    if (start.primary != NX_SUCCESS) {
        return 1;
    }
    int result = 0;
    uint32_t pins = 0U;
    const nx_gpio_port_t* gpio = nx_factory_gpio(0U);
    if (gpio == NULL || nx_gpio_port_read(gpio, &pins) != NX_SUCCESS) {
        result = 2;
    }
    if (result == 0) {
        result = settings_success();
    }
    if (result == 0) {
        result = wait_success();
    }
    if (result == 0) {
        result = network_success();
    }
    nx_result_t stopped = nx_platform_stop();
    return result != 0 ? result : (stopped == NX_SUCCESS ? 0 : 3);
}
