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
#if defined(NEXUS_LWIP_LOANED_RX)
#include "nexus/network/lwip_loaned_rx_storage.h"
#endif
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

/** \brief Report a settled failure once, without choosing provider recovery. */
static nx_result_t consume_transmit_result(nx_lwip_adapter_t* adapter) {
    if (nx_request_state(&adapter->transmit.base) != NX_REQUEST_SETTLED) {
        return NX_SUCCESS;
    }
    /* The acquire observation precedes reading the final provider result. */
    nx_result_t result = adapter->transmit.base.result;
    if (result != NX_SUCCESS) {
        /* This already-settled request has no provider borrow or observers. */
        (void)nx_request_prepare(&adapter->transmit.base, NX_DEADLINE_NEVER);
    }
    return result;
}

static err_t link_output(struct netif* interface, struct pbuf* packet) {
    nx_lwip_adapter_t* adapter = interface->state;
    if (!adapter->attached || adapter->closing) {
        return ERR_IF;
    }
    nx_request_state_t state = nx_request_state(&adapter->transmit.base);
    if (state != NX_REQUEST_READY && state != NX_REQUEST_SETTLED) {
        return ERR_MEM;
    }
    /* Successful inline completion remains reusable without a service call. */
    if (state == NX_REQUEST_SETTLED &&
        adapter->transmit.base.result != NX_SUCCESS) {
        return ERR_IF;
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

#if defined(NEXUS_LWIP_LOANED_RX)
static bool valid_loaned_config(nx_lwip_adapter_t* adapter,
                                const nx_lwip_loaned_config_t* config) {
    const nx_lwip_config_t* base = &config->base;
    if (config->slots == NULL || config->pieces == NULL ||
        config->slot_count == 0U ||
        config->slot_count > SIZE_MAX / sizeof(*config->slots) ||
        config->piece_count > SIZE_MAX / sizeof(*config->pieces) ||
        config->slot_count > SIZE_MAX / base->rx_capacity ||
        config->piece_count < config->slot_count * base->rx_capacity) {
        return false;
    }
    const void* storage[] = {adapter,        config,        base->tx_buffer,
                             base->rx_views, config->slots, config->pieces};
    const size_t sizes[] = {sizeof(*adapter),
                            sizeof(*config),
                            base->tx_capacity,
                            base->rx_capacity * sizeof(*base->rx_views),
                            config->slot_count * sizeof(*config->slots),
                            config->piece_count * sizeof(*config->pieces)};
    for (size_t i = 0U; i < sizeof(storage) / sizeof(storage[0]); ++i) {
        for (size_t j = i + 1U; j < sizeof(storage) / sizeof(storage[0]); ++j) {
            if (overlaps(storage[i], sizes[i], storage[j], sizes[j])) {
                return false;
            }
        }
    }
    for (size_t i = 0U; i < config->slot_count; ++i) {
        if (config->slots[i].held || config->slots[i].live_pieces != 0U) {
            return false;
        }
    }
    return true;
}

static void final_piece(struct pbuf* packet) {
    nx_lwip_rx_piece_t* piece = (nx_lwip_rx_piece_t*)packet;
    /* NO_SYS requires the same owner for every pbuf reference operation.
     * Callbacks do not rearm descriptors or enter a provider recursively. */
    if (piece->slot->live_pieces != 0U) {
        --piece->slot->live_pieces;
    }
}

nx_result_t
nx_lwip_loaned_adapter_start(nx_lwip_adapter_t* adapter,
                             const nx_lwip_loaned_config_t* selected) {
    const nx_lwip_config_t* config = selected == NULL ? NULL : &selected->base;
#else
nx_result_t nx_lwip_adapter_start(nx_lwip_adapter_t* adapter,
                                  const nx_lwip_config_t* config) {
#endif
    if (!valid_config(adapter, config)) {
        return NX_ERROR_INVALID;
    }
#if defined(NEXUS_LWIP_LOANED_RX)
    if (adapter->in_call || adapter->attached || netif_default != NULL) {
        return NX_ERROR_BUSY;
    }
    if (!valid_loaned_config(adapter, selected)) {
        return NX_ERROR_INVALID;
    }
#endif
#if !defined(NEXUS_LWIP_LOANED_RX)
    if (adapter->attached || netif_default != NULL) {
        return NX_ERROR_BUSY;
    }
#endif
    adapter->config = *config;
    adapter->closing = false;
#if !defined(NEXUS_LWIP_LOANED_RX)
    adapter->receive_held = false;
#endif
#if defined(NEXUS_LWIP_LOANED_RX)
    adapter->in_call = true;
    adapter->slots = selected->slots;
    adapter->slot_count = selected->slot_count;
    adapter->pieces = selected->pieces;
    adapter->piece_count = selected->piece_count;
    for (size_t i = 0U; i < adapter->slot_count; ++i) {
        adapter->slots[i].header.slot = &adapter->slots[i];
        adapter->slots[i].header.custom.custom_free_function = final_piece;
    }
#endif
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
#if defined(NEXUS_LWIP_LOANED_RX)
        adapter->in_call = false;
#endif
        return NX_ERROR_IO;
    }
    adapter->attached = true;
    netif_set_default(&adapter->interface);
    netif_set_up(&adapter->interface);
#if defined(NEXUS_LWIP_LOANED_RX)
    adapter->in_call = false;
#endif
    return NX_SUCCESS;
}

#if !defined(NEXUS_LWIP_LOANED_RX)
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
#else
static nx_result_t release_ready(nx_lwip_adapter_t* adapter, bool require_all) {
    nx_result_t pending = NX_SUCCESS;
    for (size_t i = 0U; i < adapter->slot_count; ++i) {
        nx_lwip_rx_slot_t* slot = &adapter->slots[i];
        if (!slot->held) {
            continue;
        }
        if (slot->live_pieces != 0U) {
            if (require_all) {
                pending = NX_ERROR_BUSY;
            }
            continue;
        }
        nx_result_t result =
            nx_eth_frames_port_release(adapter->config.port, &slot->loan);
        if (result != NX_SUCCESS) {
            adapter->closing = true;
            return result;
        }
        slot->held = false;
    }
    return pending;
}

static struct pbuf* loan_packet(nx_lwip_adapter_t* adapter,
                                nx_lwip_rx_slot_t* slot, size_t ordinal) {
    const size_t prefix = slot->loan.length < sizeof(slot->prefix.bytes)
                              ? slot->loan.length
                              : sizeof(slot->prefix.bytes);
    size_t copied = 0U;
    for (size_t i = 0U; i < slot->loan.fragment_count && copied < prefix; ++i) {
        const nx_eth_fragment_t* view = &adapter->config.rx_views[i];
        size_t bytes = view->length;
        if (bytes > prefix - copied) {
            bytes = prefix - copied;
        }
        memcpy(slot->prefix.bytes + copied, view->data, bytes);
        copied += bytes;
    }
    struct pbuf* head = pbuf_alloced_custom(
        PBUF_RAW, (u16_t)prefix, PBUF_REF, &slot->header.custom,
        slot->prefix.bytes, (u16_t)sizeof(slot->prefix.bytes));
    if (head == NULL) {
        return NULL;
    }
    slot->live_pieces = 1U;
    head->tot_len = (u16_t)slot->loan.length;
    struct pbuf* tail = head;
    size_t skipped = prefix;
    size_t remaining = slot->loan.length - prefix;
    size_t piece_index = ordinal * adapter->config.rx_capacity;
    for (size_t i = 0U; i < slot->loan.fragment_count && remaining != 0U; ++i) {
        const nx_eth_fragment_t* view = &adapter->config.rx_views[i];
        if (skipped >= view->length) {
            skipped -= view->length;
            continue;
        }
        const size_t bytes = view->length - skipped;
        nx_lwip_rx_piece_t* piece = &adapter->pieces[piece_index++];
        piece->slot = slot;
        piece->custom.custom_free_function = final_piece;
        /* The official ROM API takes void*, but ROM tails remain immutable.
         * TCP header writes target the separate writable prefix above. */
        struct pbuf* next = pbuf_alloced_custom(
            PBUF_RAW, (u16_t)bytes, PBUF_ROM, &piece->custom,
            (void*)(view->data + skipped), (u16_t)bytes);
        if (next == NULL) {
            pbuf_free(head);
            return NULL;
        }
        ++slot->live_pieces;
        next->tot_len = (u16_t)remaining;
        tail->next = next;
        tail = next;
        remaining -= bytes;
        skipped = 0U;
    }
    return head;
}

static bool valid_loan_view(const nx_lwip_adapter_t* adapter,
                            const nx_eth_fragment_t* view, size_t remaining) {
    return view->data != NULL && view->length != 0U &&
           view->length <= remaining &&
           !overlaps(view->data, view->length, adapter, sizeof(*adapter)) &&
           !overlaps(view->data, view->length, adapter->slots,
                     adapter->slot_count * sizeof(*adapter->slots)) &&
           !overlaps(view->data, view->length, adapter->pieces,
                     adapter->piece_count * sizeof(*adapter->pieces)) &&
           !overlaps(view->data, view->length, adapter->config.tx_buffer,
                     adapter->config.tx_capacity) &&
           !overlaps(view->data, view->length, adapter->config.rx_views,
                     adapter->config.rx_capacity *
                         sizeof(*adapter->config.rx_views));
}

static nx_result_t receive_one(nx_lwip_adapter_t* adapter) {
    size_t ordinal = 0U;
    while (ordinal < adapter->slot_count && adapter->slots[ordinal].held) {
        ++ordinal;
    }
    if (ordinal == adapter->slot_count) {
        return NX_ERROR_EXHAUSTED;
    }
    nx_lwip_rx_slot_t* slot = &adapter->slots[ordinal];
    nx_result_t result = nx_eth_frames_port_acquire(
        adapter->config.port, &slot->loan, adapter->config.rx_views,
        adapter->config.rx_capacity);
    if (result != NX_SUCCESS) {
        return result;
    }
    slot->held = true;
    slot->live_pieces = 0U;
    size_t length = 0U;
    bool valid = slot->loan.fragment_count <= adapter->config.rx_capacity &&
                 slot->loan.length >= NX_ETH_FRAME_MIN &&
                 slot->loan.length <= NX_ETH_FRAME_MAX &&
                 !slot->loan.vlan.present;
    for (size_t i = 0U; valid && i < slot->loan.fragment_count; ++i) {
        const nx_eth_fragment_t* view = &adapter->config.rx_views[i];
        valid = valid_loan_view(adapter, view, slot->loan.length - length);
        if (valid) {
            length += view->length;
        }
    }
    valid = valid && length == slot->loan.length;
    struct pbuf* packet = valid ? loan_packet(adapter, slot, ordinal) : NULL;
    nx_result_t input = !valid ? NX_ERROR_INVALID : NX_ERROR_IO;
    if (packet != NULL) {
        if (adapter->interface.input(packet, &adapter->interface) != ERR_OK) {
            pbuf_free(packet);
        } else {
            input = NX_SUCCESS;
        }
    }
    result = release_ready(adapter, false);
    return result != NX_SUCCESS ? result : input;
}
#endif

#if defined(NEXUS_LWIP_LOANED_RX)
static nx_result_t service_owner(nx_lwip_adapter_t* adapter,
                                 size_t receive_budget) {
#else
nx_result_t nx_lwip_adapter_service(nx_lwip_adapter_t* adapter,
                                    size_t receive_budget) {
#endif
    if (adapter == NULL || receive_budget == 0U) {
        return NX_ERROR_INVALID;
    }
    if (!adapter->attached || adapter->closing
#if !defined(NEXUS_LWIP_LOANED_RX)
        || adapter->receive_held
#endif
    ) {
        return NX_ERROR_STATE;
    }
    nx_result_t completed = consume_transmit_result(adapter);
    if (completed != NX_SUCCESS) {
        return completed;
    }
    nx_result_t result = nx_eth_frames_port_service(adapter->config.port);
    completed = consume_transmit_result(adapter);
    if (completed != NX_SUCCESS) {
        return completed;
    }
    if (result != NX_SUCCESS) {
        return result;
    }
    /* RX overload must not starve the timers which release protocol state. */
    sys_check_timeouts();
    completed = consume_transmit_result(adapter);
    if (completed != NX_SUCCESS) {
        return completed;
    }
#if defined(NEXUS_LWIP_LOANED_RX)
    result = release_ready(adapter, false);
    completed = consume_transmit_result(adapter);
    if (completed != NX_SUCCESS) {
        return completed;
    }
    if (result != NX_SUCCESS) {
        return result;
    }
#endif
    for (size_t i = 0U; i < receive_budget; ++i) {
        result = receive_one(adapter);
        completed = consume_transmit_result(adapter);
        if (completed != NX_SUCCESS) {
            return completed;
        }
        if (result == NX_ERROR_EMPTY) {
            break;
        }
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    return NX_SUCCESS;
}

#if defined(NEXUS_LWIP_LOANED_RX)
nx_result_t nx_lwip_adapter_service(nx_lwip_adapter_t* adapter,
                                    size_t receive_budget) {
    if (adapter == NULL || receive_budget == 0U) {
        return NX_ERROR_INVALID;
    }
    if (adapter->in_call) {
        return NX_ERROR_BUSY;
    }
    adapter->in_call = true;
    nx_result_t result = service_owner(adapter, receive_budget);
    adapter->in_call = false;
    return result;
}
#endif

nx_result_t nx_lwip_adapter_apply_link(nx_lwip_adapter_t* adapter,
                                       const nx_eth_link_t* link) {
    if (adapter == NULL || link == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!adapter->attached || adapter->closing) {
        return NX_ERROR_STATE;
    }
#if defined(NEXUS_LWIP_LOANED_RX)
    if (adapter->in_call) {
        return NX_ERROR_BUSY;
    }
    adapter->in_call = true;
#endif
    nx_result_t result =
        nx_eth_frames_port_apply_link(adapter->config.port, link);
    if (result == NX_SUCCESS) {
        if (link->up) {
            netif_set_link_up(&adapter->interface);
        } else {
            netif_set_link_down(&adapter->interface);
        }
    }
#if defined(NEXUS_LWIP_LOANED_RX)
    adapter->in_call = false;
#endif
    return result;
}

#if defined(NEXUS_LWIP_LOANED_RX)
static nx_result_t stop_owner(nx_lwip_adapter_t* adapter) {
#else
nx_result_t nx_lwip_adapter_stop(nx_lwip_adapter_t* adapter) {
#endif
    if (adapter == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!adapter->attached) {
        return NX_SUCCESS;
    }
    adapter->closing = true;
    netif_set_link_down(&adapter->interface);
    netif_set_down(&adapter->interface);
#if defined(NEXUS_LWIP_LOANED_RX)
    nx_result_t release = release_ready(adapter, true);
    if (release != NX_SUCCESS) {
        return release;
    }
#else
    if (adapter->receive_held) {
        nx_result_t release =
            nx_eth_frames_port_release(adapter->config.port, &adapter->receive);
        if (release != NX_SUCCESS) {
            return release;
        }
        adapter->receive_held = false;
    }
#endif
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

#if defined(NEXUS_LWIP_LOANED_RX)
nx_result_t nx_lwip_adapter_stop(nx_lwip_adapter_t* adapter) {
    if (adapter == NULL) {
        return NX_ERROR_INVALID;
    }
    if (adapter->in_call) {
        return NX_ERROR_BUSY;
    }
    adapter->in_call = true;
    nx_result_t result = stop_owner(adapter);
    adapter->in_call = false;
    return result;
}
#endif
