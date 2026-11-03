/**
 * \file            lwip_tcp.c
 * \brief           Explicit copying byte facade over one reviewed raw TCP PCB
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/network/lwip_storage.h"
#include "nexus/network/lwip_tcp_storage.h"
#include <string.h>

/* One non-reentrant facade execution domain for the selected NO_SYS stack. */
static bool owner_active;

static bool overlaps(const void* first, size_t first_size, const void* second,
                     size_t second_size) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    if (first_size > UINTPTR_MAX - a || second_size > UINTPTR_MAX - b) {
        return true;
    }
    return first_size != 0U && second_size != 0U && a < b + second_size &&
           b < a + first_size;
}

static bool live(const nx_lwip_tcp_t* client) {
    return client != NULL && client->initialized;
}

static bool busy(const nx_lwip_tcp_t* client) {
    return owner_active || client->active || client->callback_active;
}

static void enter_call(nx_lwip_tcp_t* client) {
    owner_active = true;
    client->active = true;
}

static void leave_call(nx_lwip_tcp_t* client) {
    client->active = false;
    owner_active = false;
}

static bool enter_callback(nx_lwip_tcp_t* client) {
    bool previous = owner_active;
    owner_active = true;
    client->callback_active = true;
    return previous;
}

static void leave_callback(nx_lwip_tcp_t* client, bool previous) {
    client->callback_active = false;
    owner_active = previous;
}

static bool outside_storage(const nx_lwip_tcp_t* client, const void* pointer,
                            size_t bytes) {
    if (overlaps(pointer, bytes, client, sizeof(*client))) {
        return false;
    }
    const nx_lwip_adapter_t* network = client->config.network;
    if (network != NULL &&
        (overlaps(pointer, bytes, network, sizeof(*network)) ||
         overlaps(pointer, bytes, network->config.tx_buffer,
                  network->config.tx_capacity) ||
         overlaps(pointer, bytes, network->config.rx_views,
                  network->config.rx_capacity *
                      sizeof(*network->config.rx_views)) ||
         overlaps(pointer, bytes, network->config.port,
                  sizeof(*network->config.port)) ||
         overlaps(pointer, bytes, network->config.port->ops,
                  sizeof(*network->config.port->ops)))) {
        return false;
    }
    if (client->pcb != NULL &&
        overlaps(pointer, bytes, client->pcb, sizeof(*client->pcb))) {
        return false;
    }
    for (const struct pbuf* packet = client->receive; packet != NULL;
         packet = packet->next) {
        if (overlaps(pointer, bytes, packet, sizeof(*packet)) ||
            overlaps(pointer, bytes, packet->payload, packet->len)) {
            return false;
        }
    }
    return true;
}

static nx_time_us_t now_us(const nx_lwip_tcp_t* client) {
    const nx_clock_t* clock = &client->config.network->config.clock;
    return clock->read(clock->context);
}

static void refresh(nx_lwip_tcp_t* client) {
    client->diagnostic.pending_receive =
        client->receive != NULL
            ? (size_t)client->receive->tot_len - client->receive_offset
            : 0U;
    client->diagnostic.pending_transmit =
        client->pcb != NULL ? (size_t)TCP_SND_BUF - tcp_sndbuf(client->pcb)
                            : 0U;
}

static nx_result_t failed(nx_lwip_tcp_t* client, nx_result_t result,
                          int error) {
    client->diagnostic.state = NX_LWIP_TCP_FAILED;
    client->diagnostic.result = result;
    client->diagnostic.stack_error = error;
    refresh(client);
    return result;
}

static nx_result_t state_result(const nx_lwip_tcp_t* client) {
    if (client->diagnostic.state == NX_LWIP_TCP_FAILED) {
        return client->diagnostic.result != NX_SUCCESS
                   ? client->diagnostic.result
                   : NX_ERROR_IO;
    }
    return NX_ERROR_STATE;
}

static void detach(struct tcp_pcb* pcb) {
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_err(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_poll(pcb, NULL, 0U);
}

static err_t received(void* context, struct tcp_pcb* pcb, struct pbuf* packet,
                      err_t error) {
    nx_lwip_tcp_t* client = context;
    if (!live(client) || client->pcb != pcb) {
        return ERR_VAL;
    }
    bool previous_owner = enter_callback(client);
    if (error != ERR_OK) {
        (void)failed(client, NX_ERROR_IO, error);
        leave_callback(client, previous_owner);
        /* No ERR_OK, hence no application ownership of this packet. */
        return error;
    }
    if (packet == NULL) {
        client->diagnostic.peer_fin = true;
        leave_callback(client, previous_owner);
        return ERR_OK;
    }
    if (client->receive != NULL ||
        client->diagnostic.state == NX_LWIP_TCP_FAILED) {
        leave_callback(client, previous_owner);
        /* lwIP retains/refuses this chain; neither store it nor release it. */
        return ERR_MEM;
    }
    if (packet->tot_len == 0U) {
        (void)pbuf_free(packet);
    } else {
        client->receive = packet;
        client->receive_offset = 0U;
    }
    refresh(client);
    leave_callback(client, previous_owner);
    return ERR_OK;
}

static void connection_error(void* context, err_t error) {
    nx_lwip_tcp_t* client = context;
    if (!live(client)) {
        return;
    }
    bool previous_owner = enter_callback(client);
    /* The stack has already freed this PCB before its error callback. */
    client->pcb = NULL;
    (void)failed(client, NX_ERROR_IO, error);
    leave_callback(client, previous_owner);
}

static err_t connected(void* context, struct tcp_pcb* pcb, err_t error) {
    nx_lwip_tcp_t* client = context;
    if (!live(client) || client->pcb != pcb) {
        return ERR_VAL;
    }
    bool previous_owner = enter_callback(client);
    if (error == ERR_OK) {
        if (now_us(client) >= client->connect_deadline) {
            (void)failed(client, NX_ERROR_TIMEOUT, ERR_TIMEOUT);
        } else {
            client->diagnostic.state = NX_LWIP_TCP_ESTABLISHED;
            client->diagnostic.result = NX_SUCCESS;
            client->diagnostic.stack_error = ERR_OK;
        }
    } else {
        (void)failed(client, NX_ERROR_IO, error);
    }
    refresh(client);
    leave_callback(client, previous_owner);
    return ERR_OK;
}

static void attach(nx_lwip_tcp_t* client) {
    tcp_arg(client->pcb, client);
    tcp_recv(client->pcb, received);
    tcp_err(client->pcb, connection_error);
    tcp_sent(client->pcb, NULL);
    tcp_poll(client->pcb, NULL, 0U);
}

static void release_receive(nx_lwip_tcp_t* client) {
    if (client->receive != NULL) {
        struct pbuf* packet = client->receive;
        client->receive = NULL;
        client->receive_offset = 0U;
        (void)pbuf_free(packet);
    }
}

static void release_pcb(nx_lwip_tcp_t* client) {
    if (client->pcb != NULL) {
        struct tcp_pcb* pcb = client->pcb;
        client->pcb = NULL;
        detach(pcb);
        tcp_abort(pcb);
    }
}

static nx_result_t write_face(void* context, const uint8_t* data, size_t bytes,
                              size_t* accepted, nx_time_us_t deadline) {
    return nx_lwip_tcp_try_write(context, data, bytes, accepted, deadline);
}
static nx_result_t read_face(void* context, uint8_t* data, size_t bytes,
                             size_t* received_bytes, nx_time_us_t deadline) {
    return nx_lwip_tcp_try_read(context, data, bytes, received_bytes, deadline);
}
static const nx_byte_stream_ops_t stream_ops = {write_face, read_face};

static bool valid_config(nx_lwip_tcp_t* client,
                         const nx_lwip_tcp_config_t* config) {
    if (client == NULL || config == NULL || config->network == NULL ||
        config->receive_budget == 0U || config->port == 0U ||
        (config->address[0] >= 224U && config->address[0] <= 239U) ||
        ((config->address[0] | config->address[1] | config->address[2] |
          config->address[3]) == 0U) ||
        (config->address[0] == 255U && config->address[1] == 255U &&
         config->address[2] == 255U && config->address[3] == 255U) ||
        overlaps(client, sizeof(*client), config, sizeof(*config))) {
        return false;
    }
    const nx_lwip_adapter_t* network = config->network;
    return network->attached && !network->closing &&
           network->config.clock.read != NULL &&
           !overlaps(client, sizeof(*client), network, sizeof(*network)) &&
           !overlaps(client, sizeof(*client), network->config.tx_buffer,
                     network->config.tx_capacity) &&
           !overlaps(client, sizeof(*client), network->config.rx_views,
                     network->config.rx_capacity *
                         sizeof(*network->config.rx_views));
}

nx_result_t nx_lwip_tcp_start(nx_lwip_tcp_t* client,
                              const nx_lwip_tcp_config_t* config,
                              nx_time_us_t deadline) {
    if (!valid_config(client, config) || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (client->initialized || busy(client)) {
        return NX_ERROR_BUSY;
    }
    enter_call(client);
    const nx_clock_t* clock = &config->network->config.clock;
    if (clock->read(clock->context) >= deadline) {
        leave_call(client);
        return NX_ERROR_TIMEOUT;
    }
    struct tcp_pcb* pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (pcb == NULL) {
        leave_call(client);
        return NX_ERROR_EXHAUSTED;
    }
    memset(client, 0, sizeof(*client));
    enter_call(client);
    client->initialized = true;
    client->config = *config;
    client->pcb = pcb;
    client->connect_deadline = deadline;
    client->stream.ops = &stream_ops;
    client->stream.context = client;
    client->diagnostic.state = NX_LWIP_TCP_CONNECTING;
    attach(client);
    tcp_bind_netif(pcb, &config->network->interface);
    ip_addr_t address;
    IP_ADDR4(&address, config->address[0], config->address[1],
             config->address[2], config->address[3]);
    err_t error = tcp_connect(pcb, &address, config->port, connected);
    if (error != ERR_OK) {
        release_pcb(client);
        memset(client, 0, sizeof(*client));
        leave_call(client);
        return error == ERR_MEM || error == ERR_BUF ? NX_ERROR_EXHAUSTED
                                                    : NX_ERROR_IO;
    }
    refresh(client);
    leave_call(client);
    return NX_SUCCESS;
}

static bool temporary(nx_result_t result) {
    return result == NX_SUCCESS || result == NX_ERROR_BUSY ||
           result == NX_ERROR_EXHAUSTED;
}

/* Called only with the serialized public-operation join guard already held. */
static nx_result_t progress(nx_lwip_tcp_t* client) {
    if (client->diagnostic.state == NX_LWIP_TCP_FAILED) {
        return state_result(client);
    }
    nx_time_us_t deadline = NX_DEADLINE_NEVER;
    if (client->diagnostic.state == NX_LWIP_TCP_CONNECTING) {
        deadline = client->connect_deadline;
    } else if (client->diagnostic.state == NX_LWIP_TCP_CLOSING) {
        deadline = client->close_deadline;
    }
    if (deadline != NX_DEADLINE_NEVER && now_us(client) >= deadline) {
        return client->diagnostic.state == NX_LWIP_TCP_CONNECTING
                   ? failed(client, NX_ERROR_TIMEOUT, ERR_TIMEOUT)
                   : NX_ERROR_TIMEOUT;
    }
    nx_result_t result = nx_lwip_adapter_service(client->config.network,
                                                 client->config.receive_budget);
    if (client->diagnostic.state == NX_LWIP_TCP_FAILED) {
        return state_result(client);
    }
    if (!temporary(result)) {
        return failed(client, result, ERR_IF);
    }
    if (deadline != NX_DEADLINE_NEVER && now_us(client) >= deadline) {
        return client->diagnostic.state == NX_LWIP_TCP_CLOSING
                   ? NX_ERROR_TIMEOUT
                   : failed(client, NX_ERROR_TIMEOUT, ERR_TIMEOUT);
    }
    if (client->pcb != NULL &&
        client->diagnostic.state != NX_LWIP_TCP_CONNECTING) {
        err_t output = tcp_output(client->pcb);
        if (output != ERR_OK) {
            /* Keep the stack's signed numeric error in the diagnostic. */
            client->diagnostic.stack_error = (int)output;
            if (output != ERR_MEM && output != ERR_BUF) {
                return failed(client, NX_ERROR_IO, output);
            }
            if (result == NX_SUCCESS) {
                result = NX_ERROR_BUSY;
            }
        }
    }
    refresh(client);
    if (result == NX_SUCCESS &&
        client->diagnostic.state == NX_LWIP_TCP_CONNECTING) {
        return NX_ERROR_BUSY;
    }
    return result;
}

nx_result_t nx_lwip_tcp_service(nx_lwip_tcp_t* client) {
    if (client == NULL) {
        return NX_ERROR_INVALID;
    }
    if (busy(client)) {
        return NX_ERROR_BUSY;
    }
    if (!live(client)) {
        return NX_ERROR_STATE;
    }
    enter_call(client);
    nx_result_t result = progress(client);
    leave_call(client);
    return result;
}

static bool valid_count(const nx_lwip_tcp_t* client, const void* data,
                        size_t bytes, const size_t* count) {
    return count != NULL &&
           !(client != NULL &&
             overlaps(count, sizeof(*count), client, sizeof(*client))) &&
           !overlaps(count, sizeof(*count), data, bytes) &&
           (!live(client) || outside_storage(client, count, sizeof(*count)));
}

static nx_result_t preflight(nx_lwip_tcp_t* client, const void* data,
                             size_t bytes, nx_time_us_t deadline) {
    if (client == NULL || !live(client)) {
        return NX_ERROR_STATE;
    }
    if (busy(client)) {
        return NX_ERROR_BUSY;
    }
    if ((data == NULL && bytes != 0U) || deadline == NX_DEADLINE_NEVER ||
        !outside_storage(client, data, bytes)) {
        return NX_ERROR_INVALID;
    }
    return client->diagnostic.state == NX_LWIP_TCP_FAILED ? state_result(client)
                                                          : NX_SUCCESS;
}

nx_result_t nx_lwip_tcp_try_write(nx_lwip_tcp_t* client, const uint8_t* data,
                                  size_t bytes, size_t* accepted,
                                  nx_time_us_t deadline) {
    if (!valid_count(client, data, bytes, accepted)) {
        return NX_ERROR_INVALID;
    }
    *accepted = 0U;
    nx_result_t result = preflight(client, data, bytes, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (client->diagnostic.state == NX_LWIP_TCP_CLOSING) {
        return NX_ERROR_STATE;
    }
    if (bytes == 0U) {
        return NX_SUCCESS;
    }
    enter_call(client);
    if (now_us(client) >= deadline) {
        leave_call(client);
        return NX_ERROR_TIMEOUT;
    }
    result = progress(client);
    if (!temporary(result) ||
        client->diagnostic.state != NX_LWIP_TCP_ESTABLISHED ||
        client->pcb == NULL) {
        leave_call(client);
        return temporary(result) ? NX_ERROR_BUSY : result;
    }
    if (now_us(client) >= deadline) {
        leave_call(client);
        return NX_ERROR_TIMEOUT;
    }
    size_t count =
        bytes < tcp_sndbuf(client->pcb) ? bytes : tcp_sndbuf(client->pcb);
    if (count > UINT16_MAX) {
        count = UINT16_MAX;
    }
    if (count == 0U) {
        leave_call(client);
        return NX_ERROR_BUSY;
    }
    err_t error =
        tcp_write(client->pcb, data, (u16_t)count, TCP_WRITE_FLAG_COPY);
    if (error != ERR_OK) {
        result = error == ERR_MEM || error == ERR_BUF
                     ? NX_ERROR_BUSY
                     : failed(client, NX_ERROR_IO, error);
    } else {
        /* Acceptance is committed before output; later congestion cannot undo
         * it. */
        *accepted = count;
        err_t output = tcp_output(client->pcb);
        client->diagnostic.stack_error = (int)output;
        if (output != ERR_OK && output != ERR_MEM && output != ERR_BUF) {
            (void)failed(client, NX_ERROR_IO, output);
        }
        result = NX_SUCCESS;
    }
    refresh(client);
    leave_call(client);
    return result;
}

nx_result_t nx_lwip_tcp_try_read(nx_lwip_tcp_t* client, uint8_t* data,
                                 size_t bytes, size_t* received_bytes,
                                 nx_time_us_t deadline) {
    if (!valid_count(client, data, bytes, received_bytes)) {
        return NX_ERROR_INVALID;
    }
    *received_bytes = 0U;
    nx_result_t result = preflight(client, data, bytes, deadline);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (bytes == 0U ||
        (client->receive == NULL && client->diagnostic.peer_fin)) {
        return NX_SUCCESS;
    }
    enter_call(client);
    if (now_us(client) >= deadline) {
        leave_call(client);
        return NX_ERROR_TIMEOUT;
    }
    /* Consume held data before allocating another RX pbuf under pressure. */
    if (client->receive == NULL) {
        result = progress(client);
        if (client->diagnostic.state == NX_LWIP_TCP_FAILED) {
            leave_call(client);
            return state_result(client);
        }
    }
    if (now_us(client) >= deadline) {
        leave_call(client);
        return NX_ERROR_TIMEOUT;
    }
    if (client->receive != NULL) {
        size_t available =
            (size_t)client->receive->tot_len - client->receive_offset;
        size_t count = bytes < available ? bytes : available;
        if (pbuf_copy_partial(client->receive, data, (u16_t)count,
                              client->receive_offset) != count) {
            result = failed(client, NX_ERROR_IO, ERR_BUF);
        } else {
            client->receive_offset = (u16_t)(client->receive_offset + count);
            if (client->receive_offset == client->receive->tot_len) {
                release_receive(client);
            }
            if (client->pcb != NULL) {
                tcp_recved(client->pcb, (u16_t)count);
            }
            *received_bytes = count;
            result = NX_SUCCESS;
        }
    } else if (client->diagnostic.peer_fin) {
        result = NX_SUCCESS;
    } else if (temporary(result)) {
        result = NX_ERROR_BUSY;
    }
    refresh(client);
    leave_call(client);
    return result;
}

nx_result_t nx_lwip_tcp_close(nx_lwip_tcp_t* client, nx_time_us_t deadline) {
    if (client == NULL || deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_INVALID;
    }
    if (busy(client)) {
        return NX_ERROR_BUSY;
    }
    if (!live(client)) {
        return NX_SUCCESS;
    }
    if (client->diagnostic.state == NX_LWIP_TCP_FAILED || client->pcb == NULL) {
        return state_result(client);
    }
    bool connecting = client->diagnostic.state == NX_LWIP_TCP_CONNECTING;
    if (client->diagnostic.state != NX_LWIP_TCP_CLOSING) {
        client->close_deadline = deadline;
        client->diagnostic.state = NX_LWIP_TCP_CLOSING;
    }
    deadline = client->close_deadline;
    enter_call(client);
    if (now_us(client) >= deadline) {
        leave_call(client);
        return NX_ERROR_TIMEOUT;
    }
    if (!connecting) {
        nx_result_t result = progress(client);
        if (!temporary(result)) {
            leave_call(client);
            return result;
        }
        if (client->receive != NULL || client->pcb->refused_data != NULL ||
            tcp_sndbuf(client->pcb) != TCP_SND_BUF) {
            leave_call(client);
            return NX_ERROR_BUSY;
        }
    }
    if (now_us(client) >= deadline) {
        leave_call(client);
        return NX_ERROR_TIMEOUT;
    }
    struct tcp_pcb* pcb = client->pcb;
    detach(pcb);
    err_t error = tcp_close(pcb);
    if (error != ERR_OK) {
        attach(client);
        client->diagnostic.stack_error = (int)error;
        leave_call(client);
        return error == ERR_MEM ? NX_ERROR_BUSY
                                : failed(client, NX_ERROR_IO, error);
    }
    /* Success may immediately free the PCB. Do not dereference or abort it. */
    client->pcb = NULL;
    client->initialized = false;
    client->config.network = NULL;
    client->diagnostic.state = NX_LWIP_TCP_CLOSED;
    client->diagnostic.result = NX_SUCCESS;
    client->diagnostic.stack_error = ERR_OK;
    refresh(client);
    leave_call(client);
    return NX_SUCCESS;
}

nx_result_t nx_lwip_tcp_abort(nx_lwip_tcp_t* client) {
    if (client == NULL) {
        return NX_ERROR_INVALID;
    }
    if (busy(client)) {
        return NX_ERROR_BUSY;
    }
    if (!live(client)) {
        return NX_SUCCESS;
    }
    enter_call(client);
    release_pcb(client);
    release_receive(client);
    client->initialized = false;
    client->config.network = NULL;
    client->diagnostic.state = NX_LWIP_TCP_CLOSED;
    refresh(client);
    leave_call(client);
    return NX_SUCCESS;
}

const nx_byte_stream_t* nx_lwip_tcp_stream(const nx_lwip_tcp_t* client) {
    return live(client) ? &client->stream : NULL;
}

nx_result_t nx_lwip_tcp_diagnostic(const nx_lwip_tcp_t* client,
                                   nx_lwip_tcp_diagnostic_t* diagnostic) {
    if (client == NULL || diagnostic == NULL ||
        overlaps(client, sizeof(*client), diagnostic, sizeof(*diagnostic)) ||
        (live(client) &&
         !outside_storage(client, diagnostic, sizeof(*diagnostic)))) {
        return NX_ERROR_INVALID;
    }
    *diagnostic = client->diagnostic;
    if (live(client)) {
        diagnostic->pending_receive =
            client->receive != NULL
                ? (size_t)client->receive->tot_len - client->receive_offset
                : 0U;
        diagnostic->pending_transmit =
            client->pcb != NULL ? (size_t)TCP_SND_BUF - tcp_sndbuf(client->pcb)
                                : 0U;
    }
    return NX_SUCCESS;
}
