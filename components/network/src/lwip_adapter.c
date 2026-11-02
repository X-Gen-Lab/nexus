/**
 * \file            lwip_adapter.c
 * \brief           Finite copying bridge from Nexus frame loans to lwIP
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "lwip/etharp.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "nexus/network/lwip_storage.h"
#include <string.h>

static bool overlaps(const void* first, size_t first_size, const void* second,
                     size_t second_size) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    if (first_size > UINTPTR_MAX - a || second_size > UINTPTR_MAX - b) {
        return true;
    }
    return a < b + second_size && b < a + first_size;
}

static bool transmit_borrowed(const nx_lwip_adapter_t* adapter) {
    nx_request_state_t state = nx_request_state(&adapter->transmit.base);
    return state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED;
}

static err_t link_output(struct netif* interface, struct pbuf* packet) {
    nx_lwip_adapter_t* adapter = interface->state;
    if (!adapter->attached || adapter->closing) {
        return ERR_IF;
    }
    if (transmit_borrowed(adapter)) {
        return ERR_MEM;
    }
    if (packet == NULL || packet->tot_len < NX_ETH_FRAME_MIN ||
        packet->tot_len > NX_ETH_FRAME_MAX) {
        return ERR_BUF;
    }
    nx_time_us_t now =
        adapter->config.clock.read(adapter->config.clock.context);
    if (now >= NX_DEADLINE_NEVER - adapter->config.tx_timeout_us) {
        return ERR_IF;
    }
    if (pbuf_copy_partial(packet, adapter->config.tx_buffer, packet->tot_len,
                          0U) != packet->tot_len) {
        return ERR_BUF;
    }
    adapter->transmit_fragment.data = adapter->config.tx_buffer;
    adapter->transmit_fragment.length = packet->tot_len;
    nx_result_t result = nx_eth_frames_tx_prepare(
        &adapter->transmit, &adapter->transmit_fragment, 1U, false,
        now + adapter->config.tx_timeout_us);
    if (result == NX_SUCCESS) {
        result =
            nx_eth_frames_port_submit(adapter->config.port, &adapter->transmit);
    }
    if (result == NX_ERROR_BUSY || result == NX_ERROR_EXHAUSTED) {
        return ERR_MEM;
    }
    return result == NX_SUCCESS ? ERR_OK : ERR_IF;
}

static err_t initialize_netif(struct netif* interface) {
    nx_lwip_adapter_t* adapter = interface->state;
    interface->name[0] = 'n';
    interface->name[1] = 'x';
    interface->hwaddr_len = 6U;
    memcpy(interface->hwaddr, adapter->config.hardware_address, 6U);
    interface->mtu = 1500U;
    interface->flags =
        NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    interface->output = etharp_output;
    interface->linkoutput = link_output;
    return ERR_OK;
}

static bool valid_config(nx_lwip_adapter_t* adapter,
                         const nx_lwip_config_t* config) {
    if (adapter == NULL || config == NULL || config->port == NULL ||
        config->port->ops == NULL || config->port->context == NULL ||
        config->clock.read == NULL || config->tx_buffer == NULL ||
        config->tx_capacity < NX_ETH_FRAME_MAX || config->rx_views == NULL ||
        config->rx_capacity == 0U ||
        config->rx_capacity > SIZE_MAX / sizeof(*config->rx_views) ||
        config->tx_timeout_us == 0U ||
        config->tx_timeout_us == NX_DEADLINE_NEVER ||
        (config->hardware_address[0] & 1U) != 0U) {
        return false;
    }
    const nx_eth_frames_ops_t* ops = config->port->ops;
    if (ops->submit == NULL || ops->acquire == NULL || ops->release == NULL ||
        ops->service == NULL || ops->stop == NULL || ops->apply_link == NULL) {
        return false;
    }
    uint8_t hardware_bits = 0U;
    for (size_t i = 0U; i < sizeof(config->hardware_address); ++i) {
        hardware_bits |= config->hardware_address[i];
    }
    if (hardware_bits == 0U) {
        return false;
    }
    size_t view_size = config->rx_capacity * sizeof(*config->rx_views);
    return !overlaps(adapter, sizeof(*adapter), config, sizeof(*config)) &&
           !overlaps(adapter, sizeof(*adapter), config->tx_buffer,
                     config->tx_capacity) &&
           !overlaps(adapter, sizeof(*adapter), config->rx_views, view_size) &&
           !overlaps(config->tx_buffer, config->tx_capacity, config->rx_views,
                     view_size);
}

nx_result_t nx_lwip_adapter_start(nx_lwip_adapter_t* adapter,
                                  const nx_lwip_config_t* config) {
    if (!valid_config(adapter, config)) {
        return NX_ERROR_INVALID;
    }
    if (adapter->attached || netif_default != NULL) {
        return NX_ERROR_BUSY;
    }
    adapter->config = *config;
    adapter->closing = false;
    adapter->receive_held = false;
    nx_request_initialize(&adapter->transmit.base);
    ip4_addr_t address;
    ip4_addr_t netmask;
    ip4_addr_t gateway;
    IP4_ADDR(&address, config->address[0], config->address[1],
             config->address[2], config->address[3]);
    IP4_ADDR(&netmask, config->netmask[0], config->netmask[1],
             config->netmask[2], config->netmask[3]);
    IP4_ADDR(&gateway, config->gateway[0], config->gateway[1],
             config->gateway[2], config->gateway[3]);
    if (netif_add(&adapter->interface, &address, &netmask, &gateway, adapter,
                  initialize_netif, ethernet_input) == NULL) {
        return NX_ERROR_IO;
    }
    adapter->attached = true;
    netif_set_default(&adapter->interface);
    netif_set_up(&adapter->interface);
    return NX_SUCCESS;
}

static nx_result_t receive_one(nx_lwip_adapter_t* adapter) {
    nx_result_t result = nx_eth_frames_port_acquire(
        adapter->config.port, &adapter->receive, adapter->config.rx_views,
        adapter->config.rx_capacity);
    if (result != NX_SUCCESS) {
        return result;
    }
    adapter->receive_held = true;
    const nx_eth_frames_rx_loan_t* loan = &adapter->receive;
    size_t length = 0U;
    bool valid = loan->fragment_count <= adapter->config.rx_capacity &&
                 loan->length >= NX_ETH_FRAME_MIN &&
                 loan->length <= NX_ETH_FRAME_MAX && !loan->vlan.present;
    for (size_t i = 0U; valid && i < loan->fragment_count; ++i) {
        const nx_eth_fragment_t* view = &adapter->config.rx_views[i];
        valid = view->data != NULL && view->length != 0U &&
                view->length <= loan->length - length;
        if (valid) {
            length += view->length;
        }
    }
    valid = valid && length == loan->length;
    struct pbuf* packet =
        valid ? pbuf_alloc(PBUF_RAW, (u16_t)length, PBUF_POOL) : NULL;
    bool copied = packet != NULL;
    u16_t offset = 0U;
    for (size_t i = 0U; copied && i < loan->fragment_count; ++i) {
        const nx_eth_fragment_t* view = &adapter->config.rx_views[i];
        copied = pbuf_take_at(packet, view->data, (u16_t)view->length,
                              offset) == ERR_OK;
        offset = (u16_t)(offset + (u16_t)view->length);
    }
    result = nx_eth_frames_port_release(adapter->config.port, loan);
    if (result != NX_SUCCESS) {
        if (packet != NULL) {
            pbuf_free(packet);
        }
        adapter->closing = true;
        return result;
    }
    adapter->receive_held = false;
    if (!valid || !copied) {
        if (packet != NULL) {
            pbuf_free(packet);
        }
        return valid ? NX_ERROR_EXHAUSTED : NX_ERROR_INVALID;
    }
    if (adapter->interface.input(packet, &adapter->interface) != ERR_OK) {
        pbuf_free(packet);
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}

nx_result_t nx_lwip_adapter_service(nx_lwip_adapter_t* adapter,
                                    size_t receive_budget) {
    if (adapter == NULL || receive_budget == 0U) {
        return NX_ERROR_INVALID;
    }
    if (!adapter->attached || adapter->closing || adapter->receive_held) {
        return NX_ERROR_STATE;
    }
    nx_result_t result = nx_eth_frames_port_service(adapter->config.port);
    if (result != NX_SUCCESS) {
        return result;
    }
    /* RX overload must not starve the timers which release protocol state. */
    sys_check_timeouts();
    for (size_t i = 0U; i < receive_budget; ++i) {
        result = receive_one(adapter);
        if (result == NX_ERROR_EMPTY) {
            break;
        }
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return NX_SUCCESS;
}

nx_result_t nx_lwip_adapter_apply_link(nx_lwip_adapter_t* adapter,
                                       const nx_eth_link_t* link) {
    if (adapter == NULL || link == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!adapter->attached || adapter->closing) {
        return NX_ERROR_STATE;
    }
    nx_result_t result =
        nx_eth_frames_port_apply_link(adapter->config.port, link);
    if (result == NX_SUCCESS) {
        if (link->up) {
            netif_set_link_up(&adapter->interface);
        } else {
            netif_set_link_down(&adapter->interface);
        }
    }
    return result;
}

nx_result_t nx_lwip_adapter_stop(nx_lwip_adapter_t* adapter) {
    if (adapter == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!adapter->attached) {
        return NX_SUCCESS;
    }
    adapter->closing = true;
    netif_set_link_down(&adapter->interface);
    netif_set_down(&adapter->interface);
    if (adapter->receive_held) {
        nx_result_t release =
            nx_eth_frames_port_release(adapter->config.port, &adapter->receive);
        if (release != NX_SUCCESS) {
            return release;
        }
        adapter->receive_held = false;
    }
    nx_result_t result = nx_eth_frames_port_stop(adapter->config.port);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (transmit_borrowed(adapter)) {
        return NX_ERROR_BUSY;
    }
    netif_remove(&adapter->interface);
    adapter->attached = false;
    return NX_SUCCESS;
}
