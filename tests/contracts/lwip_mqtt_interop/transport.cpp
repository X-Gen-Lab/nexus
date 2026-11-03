/**
 * \file            transport.cpp
 * \brief           Test-only real lwIP Ethernet pair and ciphertext byte relay
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "fixture.h"
#include "nexus/network/lwip_tcp_storage.h"
#include "pcap.h"
#include "support/lwip_frame_pair.h"
extern "C" {
#include "lwip/init.h"
#include "lwip/tcp.h"
}
#include <algorithm>
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <sys/prctl.h>
#include <time.h>

using nexus::test::lwip_pair::FrameEndpoint;
constexpr size_t frame_budget = 4U;
constexpr size_t byte_budget = 113U;
constexpr nx_time_us_t fixture_limit = UINT64_C(15000000);
static uint64_t origin_us = 0U;
static size_t receive_calls = 0U;
static size_t transmit_calls = 0U;
static nexus::test::PcapCapture* current_capture = nullptr;

static uint64_t RawNow() {
    timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return NX_DEADLINE_NEVER;
    }
    return static_cast<uint64_t>(now.tv_sec) * UINT64_C(1000000) +
           static_cast<uint64_t>(now.tv_nsec) / UINT64_C(1000);
}
static nx_time_us_t Now(void*) {
    const uint64_t raw = RawNow();
    if (raw == NX_DEADLINE_NEVER || raw < origin_us) {
        return NX_DEADLINE_NEVER;
    }
    const uint64_t elapsed = raw - origin_us;
    nexus::test::lwip_pair::SetNowMs(static_cast<uint32_t>(elapsed / 1000U));
    return elapsed;
}
static nx_result_t Service(void* context) {
    (void)Now(nullptr);
    ++transmit_calls;
    return FrameEndpoint::Service(context);
}
static nx_result_t Acquire(void* context, nx_eth_frames_rx_loan_t* loan,
                           nx_eth_fragment_t* views, size_t capacity) {
    const nx_result_t result =
        FrameEndpoint::Acquire(context, loan, views, capacity);
    if (result == NX_SUCCESS) {
        ++receive_calls;
    }
    return result;
}
const nx_eth_frames_ops_t real_ops{FrameEndpoint::Submit,
                                   nullptr,
                                   Service,
                                   Acquire,
                                   FrameEndpoint::Release,
                                   FrameEndpoint::State,
                                   nullptr,
                                   FrameEndpoint::Apply,
                                   FrameEndpoint::Stop,
                                   nullptr,
                                   nullptr};

static bool Initialize(FrameEndpoint& endpoint) {
    nexus::test::lwip_pair::current_endpoint = &endpoint;
    endpoint.port = {&real_ops, &endpoint};
    endpoint.on_transmit = nexus::test::PcapCapture::Record;
    endpoint.on_transmit_context = current_capture;
    nx_lwip_config_t config{};
    config.port = &endpoint.port;
    config.clock = {Now, nullptr};
    config.tx_timeout_us = 2000000U;
    config.tx_buffer = endpoint.transmit_bytes.data();
    config.tx_capacity = endpoint.transmit_bytes.size();
    config.rx_views = endpoint.receive_views.data();
    config.rx_capacity = endpoint.receive_views.size();
    config.hardware_address[0] = 2U;
    config.hardware_address[5] = static_cast<uint8_t>(endpoint.role + 1U);
    config.address[0] = 10U;
    config.address[3] = static_cast<uint8_t>(endpoint.role + 1U);
    config.netmask[0] = 255U;
    config.netmask[1] = 255U;
    config.netmask[2] = 255U;
    lwip_init();
    const nx_eth_link_t link{true, true, 100U};
    return nx_lwip_adapter_start(&endpoint.adapter, &config) == NX_SUCCESS &&
           nx_lwip_adapter_apply_link(&endpoint.adapter, &link) == NX_SUCCESS;
}

/** One accepted chain, one copied socket-to-TCP staging buffer; no protocol. */
struct Relay {
    FrameEndpoint endpoint;
    tcp_pcb* listener = nullptr;
    tcp_pcb* connection = nullptr;
    pbuf* receive = nullptr;
    size_t receive_offset = 0U;
    std::array<uint8_t, byte_budget> outgoing{};
    size_t outgoing_size = 0U;
    size_t outgoing_offset = 0U;
    int socket = -1;
    int control;
    bool partial;
    bool blackhole = false;
    bool failed = false;
    bool peer_fin = false;
    bool socket_fin = false;
    bool shutdown_sent = false;
    bool fin_sent = false;
    nx_time_us_t pause_until = 0U;
    size_t socket_sent = 0U;
    size_t socket_received = 0U;
    size_t memory_busy = 0U;
    size_t maximum_chain = 0U;
    size_t paused_rounds = 0U;

    Relay(int wire, int commands, bool throttle)
        : endpoint(1U, wire), control(commands), partial(throttle) {
    }
    static err_t Receive(void* context, tcp_pcb*, pbuf* bytes, err_t error) {
        auto& relay = *static_cast<Relay*>(context);
        if (bytes == nullptr) {
            relay.peer_fin = true;
            return ERR_OK;
        }
        if (error != ERR_OK || relay.receive != nullptr) {
            ++relay.memory_busy;
            return ERR_MEM;
        }
        relay.receive = bytes;
        relay.receive_offset = 0U;
        relay.maximum_chain =
            std::max(relay.maximum_chain, static_cast<size_t>(bytes->tot_len));
        return ERR_OK;
    }
    static void Error(void* context, err_t) {
        auto& relay = *static_cast<Relay*>(context);
        relay.connection = nullptr;
        relay.failed = true;
    }
    static err_t Accept(void* context, tcp_pcb* pcb, err_t error) {
        auto& relay = *static_cast<Relay*>(context);
        if (error != ERR_OK || relay.connection != nullptr) {
            return ERR_MEM;
        }
        relay.connection = pcb;
        tcp_arg(pcb, &relay);
        tcp_recv(pcb, Receive);
        tcp_err(pcb, Error);
        relay.pause_until = Now(nullptr) + (relay.partial ? 40000U : 0U);
        return ERR_OK;
    }
    bool Start(unsigned port) {
        if (!Initialize(endpoint)) {
            return false;
        }
        socket =
            ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        const int socket_capacity = 4096;
        if (socket >= 0 &&
            (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &socket_capacity,
                        sizeof(socket_capacity)) != 0 ||
             setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &socket_capacity,
                        sizeof(socket_capacity)) != 0)) {
            return false;
        }
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        target.sin_port = htons(static_cast<uint16_t>(port));
        if (socket < 0 ||
            (::connect(socket, reinterpret_cast<sockaddr*>(&target),
                       sizeof(target)) != 0 &&
             errno != EINPROGRESS)) {
            return false;
        }
        pollfd ready{socket, POLLOUT, 0};
        if (poll(&ready, 1U, 1000) != 1) {
            return false;
        }
        int error = 0;
        socklen_t bytes = sizeof(error);
        if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &bytes) != 0 ||
            error != 0) {
            return false;
        }
        listener = tcp_new_ip_type(IPADDR_TYPE_V4);
        if (listener == nullptr) {
            return false;
        }
        if (tcp_bind(listener, IP_ANY_TYPE, 8883U) != ERR_OK) {
            tcp_abort(listener);
            listener = nullptr;
            return false;
        }
        tcp_pcb* listening = tcp_listen_with_backlog(listener, 1U);
        if (listening == nullptr) {
            tcp_abort(listener);
            listener = nullptr;
            return false;
        }
        listener = listening;
        tcp_arg(listener, this);
        tcp_accept(listener, Accept);
        return true;
    }
    void AbortConnection() {
        if (connection != nullptr) {
            tcp_arg(connection, nullptr);
            tcp_recv(connection, nullptr);
            tcp_err(connection, nullptr);
            tcp_sent(connection, nullptr);
            tcp_poll(connection, nullptr, 0U);
            tcp_abort(connection);
            connection = nullptr;
        }
        if (receive != nullptr) {
            pbuf_free(receive);
            receive = nullptr;
        }
    }
    bool Progress() {
        const nx_result_t result =
            nx_lwip_adapter_service(&endpoint.adapter, frame_budget);
        if (result != NX_SUCCESS && result != NX_ERROR_EXHAUSTED) {
            return false;
        }
        if (blackhole || failed || connection == nullptr) {
            return true;
        }
        if (Now(nullptr) < pause_until) {
            ++paused_rounds;
            return true;
        }
        const size_t budget = partial ? 17U : byte_budget;
        if (receive != nullptr) {
            std::array<uint8_t, byte_budget> bytes{};
            const size_t count = std::min(
                budget, static_cast<size_t>(receive->tot_len) - receive_offset);
            const u16_t copied = pbuf_copy_partial(
                receive, bytes.data(), static_cast<u16_t>(count),
                static_cast<u16_t>(receive_offset));
            if (copied != count) {
                return false;
            }
            const ssize_t sent =
                send(socket, bytes.data(), count, MSG_NOSIGNAL);
            if (sent > 0) {
                const size_t accepted = static_cast<size_t>(sent);
                receive_offset += accepted;
                socket_sent += accepted;
                tcp_recved(connection, static_cast<u16_t>(accepted));
                if (receive_offset == receive->tot_len) {
                    pbuf_free(receive);
                    receive = nullptr;
                    receive_offset = 0U;
                }
            } else if (sent == 0 || (errno != EAGAIN && errno != EWOULDBLOCK &&
                                     errno != EINTR)) {
                return false;
            }
        }
        if (peer_fin && receive == nullptr && !shutdown_sent) {
            if (shutdown(socket, SHUT_WR) != 0 && errno != ENOTCONN) {
                return false;
            }
            shutdown_sent = true;
        }
        if (outgoing_size == 0U && !socket_fin) {
            const ssize_t count = recv(socket, outgoing.data(), budget, 0);
            if (count > 0) {
                outgoing_size = static_cast<size_t>(count);
                socket_received += outgoing_size;
            } else if (count == 0) {
                socket_fin = true;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK &&
                       errno != EINTR) {
                return false;
            }
        }
        if (outgoing_size != 0U) {
            const size_t available =
                std::min(outgoing_size - outgoing_offset,
                         static_cast<size_t>(tcp_sndbuf(connection)));
            if (available != 0U) {
                const err_t written = tcp_write(
                    connection, outgoing.data() + outgoing_offset,
                    static_cast<u16_t>(available), TCP_WRITE_FLAG_COPY);
                if (written == ERR_OK) {
                    outgoing_offset += available;
                    if (outgoing_offset == outgoing_size) {
                        outgoing_size = 0U;
                        outgoing_offset = 0U;
                    }
                } else if (written != ERR_MEM) {
                    return false;
                } else {
                    ++memory_busy;
                }
            }
        }
        const err_t output = tcp_output(connection);
        if (output != ERR_OK && output != ERR_MEM) {
            return false;
        }
        if (socket_fin && outgoing_size == 0U && !fin_sent) {
            const err_t closed = tcp_shutdown(connection, 0, 1);
            if (closed == ERR_OK) {
                fin_sent = true;
            } else if (closed != ERR_MEM) {
                return false;
            }
        }
        return true;
    }
    bool Stop() {
        AbortConnection();
        if (listener != nullptr) {
            tcp_arg(listener, nullptr);
            tcp_accept(listener, nullptr);
            if (tcp_close(listener) != ERR_OK) {
                tcp_abort(listener);
            }
            listener = nullptr;
        }
        nx_result_t stopped = NX_ERROR_BUSY;
        const nx_time_us_t until = Now(nullptr) + 100000U;
        do {
            (void)Service(&endpoint);
            stopped = nx_lwip_adapter_stop(&endpoint.adapter);
        } while (stopped == NX_ERROR_BUSY && Now(nullptr) < until);
        const bool clean = stopped == NX_SUCCESS && !endpoint.receive_held &&
                           endpoint.pending == nullptr;
        if (socket >= 0) {
            close(socket);
            socket = -1;
        }
        std::printf("relay_sent=%zu relay_received=%zu memory_busy=%zu "
                    "max_chain=%zu relay_rx_frames=%zu relay_data_frames=%u "
                    "byte_budget=%zu paused_rounds=%zu loans_clear=%u\n",
                    socket_sent, socket_received, memory_busy, maximum_chain,
                    receive_calls, endpoint.data_attempts,
                    partial ? 17U : byte_budget, paused_rounds,
                    clean ? 1U : 0U);
        std::fflush(stdout);
        return clean;
    }
};

struct Client {
    FrameEndpoint endpoint;
    nx_lwip_tcp_t tcp{};
    int control;
    bool initialized = false;
    Client(int wire, int commands) : endpoint(0U, wire), control(commands) {
    }
};
static Client* current_client = nullptr;

static bool Exchange(int control, char command) {
    if (send(control, &command, 1U, MSG_NOSIGNAL) != 1) {
        return false;
    }
    pollfd ready{control, POLLIN, 0};
    char reply = 0;
    return poll(&ready, 1U, 1000) == 1 && recv(control, &reply, 1U, 0) == 1 &&
           reply == command;
}

[[noreturn]] static void RelayMain(int wire, int control, unsigned port,
                                   bool partial, const char* case_directory) {
    (void)prctl(PR_SET_PDEATHSIG, SIGTERM);
    nexus::test::PcapCapture capture;
    current_capture = &capture;
    Relay relay(wire, control, partial);
    bool okay =
        capture.Open(case_directory, "relay.pcap", Now) && relay.Start(port);
    char ready = okay ? 'R' : 'E';
    (void)send(control, &ready, 1U, MSG_NOSIGNAL);
    bool quit = !okay;
    while (!quit && Now(nullptr) < fixture_limit) {
        pollfd descriptors[3]{
            {wire, POLLIN, 0}, {control, POLLIN, 0}, {relay.socket, POLLIN, 0}};
        if (poll(descriptors, 3U, 1) < 0 && errno != EINTR) {
            okay = false;
            break;
        }
        if ((descriptors[1].revents & POLLIN) != 0) {
            char command = 0;
            if (recv(control, &command, 1U, 0) != 1) {
                okay = false;
                break;
            }
            if (command == 'F') {
                relay.AbortConnection();
                (void)Service(&relay.endpoint);
            } else if (command == 'B') {
                relay.blackhole = true;
            } else if (command == 'Q') {
                quit = true;
            } else {
                okay = false;
                break;
            }
            (void)send(control, &command, 1U, MSG_NOSIGNAL);
        }
        if (!quit && !relay.Progress()) {
            okay = false;
            break;
        }
    }
    okay = relay.Stop() && okay;
    const bool captured = capture.Finish();
    std::printf("relay_capture_frames=%zu relay_capture_bytes=%zu "
                "relay_capture_complete=%u\n",
                capture.Frames(), capture.Bytes(), captured ? 1U : 0U);
    std::fflush(stdout);
    current_capture = nullptr;
    okay = captured && okay;
    close(wire);
    close(control);
    _exit(okay ? 0 : 1);
}
extern "C" nx_time_us_t nx_chain_now_us(void) {
    return Now(nullptr);
}
extern "C" uint32_t nx_lwip_port_now_ms(void) {
    return static_cast<uint32_t>(Now(nullptr) / 1000U);
}
extern "C" uint32_t nx_lwip_port_random_u32(void) {
    return nexus::test::lwip_pair::RandomU32();
}
extern "C" void nx_lwip_port_assert(const char* message) {
    nexus::test::lwip_pair::Assert(message);
}
extern "C" bool nx_chain_connect(unsigned, const char*,
                                 const nx_byte_stream_t** stream,
                                 nx_time_us_t deadline) {
    auto& client = *current_client;
    if (!Initialize(client.endpoint)) {
        return false;
    }
    client.initialized = true;
    nx_lwip_tcp_config_t config{};
    config.network = &client.endpoint.adapter;
    config.receive_budget = frame_budget;
    config.address[0] = 10U;
    config.address[3] = 2U;
    config.port = 8883U;
    if (nx_lwip_tcp_start(&client.tcp, &config, deadline) != NX_SUCCESS) {
        return false;
    }
    nx_lwip_tcp_diagnostic_t diagnostic{};
    do {
        const nx_result_t result = nx_lwip_tcp_service(&client.tcp);
        if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
            return false;
        }
        if (nx_lwip_tcp_diagnostic(&client.tcp, &diagnostic) != NX_SUCCESS) {
            return false;
        }
        if (diagnostic.state == NX_LWIP_TCP_ESTABLISHED) {
            *stream = nx_lwip_tcp_stream(&client.tcp);
            return *stream != nullptr;
        }
        if (nx_chain_wait(deadline) != NX_SUCCESS) {
            return false;
        }
    } while (Now(nullptr) < deadline);
    return false;
}
extern "C" nx_result_t nx_chain_wait(nx_time_us_t deadline) {
    if (Now(nullptr) >= deadline) {
        return NX_ERROR_TIMEOUT;
    }
    pollfd ready{current_client->endpoint.wire, POLLIN, 0};
    const int result = poll(&ready, 1U, 1);
    return result >= 0 || errno == EINTR ? NX_SUCCESS : NX_ERROR_IO;
}
extern "C" bool nx_chain_fault(bool blackhole) {
    return Exchange(current_client->control, blackhole ? 'B' : 'F');
}
extern "C" void nx_chain_counters(size_t* receives, size_t* transmits) {
    *receives = receive_calls;
    *transmits = transmit_calls;
}
extern "C" bool nx_chain_finish(nx_time_us_t deadline) {
    auto& tcp = current_client->tcp;
    while (Now(nullptr) < deadline) {
        const nx_result_t closed = nx_lwip_tcp_close(&tcp, deadline);
        if (closed == NX_SUCCESS) {
            return true;
        }
        if (closed != NX_ERROR_BUSY ||
            nx_lwip_tcp_service(&tcp) != NX_SUCCESS ||
            nx_chain_wait(deadline) != NX_SUCCESS) {
            return false;
        }
    }
    return false;
}
extern "C" bool nx_chain_stop(void) {
    auto& client = *current_client;
    if (!client.initialized) {
        return true;
    }
    nx_lwip_tcp_diagnostic_t diagnostic{};
    (void)nx_lwip_tcp_diagnostic(&client.tcp, &diagnostic);
    std::printf("client_tcp_state=%u tcp_pending_rx=%zu tcp_pending_tx=%zu "
                "data_frames=%u\n",
                static_cast<unsigned>(diagnostic.state),
                diagnostic.pending_receive, diagnostic.pending_transmit,
                client.endpoint.data_attempts);
    bool okay = nx_lwip_tcp_abort(&client.tcp) == NX_SUCCESS;
    const nx_time_us_t until = Now(nullptr) + 100000U;
    nx_result_t stopped = NX_ERROR_BUSY;
    do {
        (void)Service(&client.endpoint);
        stopped = nx_lwip_adapter_stop(&client.endpoint.adapter);
    } while (stopped == NX_ERROR_BUSY && Now(nullptr) < until);
    okay = stopped == NX_SUCCESS && !client.endpoint.receive_held &&
           client.endpoint.pending == nullptr && okay;
    std::printf("client_loans_clear=%u\n", okay ? 1U : 0U);
    return okay;
}

int main(int argc, char** argv) {
    if (argc != 5) {
        return 2;
    }
    char* end = nullptr;
    const unsigned long port = std::strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || port == 0U || port > UINT16_MAX) {
        return 2;
    }
    origin_us = RawNow();
    if (origin_us == NX_DEADLINE_NEVER) {
        return 2;
    }
    int frames[2]{};
    int control[2]{};
    if (socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0, frames) != 0 ||
        socketpair(AF_UNIX, SOCK_SEQPACKET, 0, control) != 0) {
        return 2;
    }
    const int capacity = 32768;
    for (const int descriptor : frames) {
        (void)setsockopt(descriptor, SOL_SOCKET, SO_SNDBUF, &capacity,
                         sizeof(capacity));
    }
    std::fflush(nullptr);
    const pid_t child = fork();
    if (child == 0) {
        close(frames[0]);
        close(control[0]);
        RelayMain(frames[1], control[1], static_cast<unsigned>(port),
                  std::strcmp(argv[2], "backpressure") == 0, argv[4]);
    }
    if (child < 0) {
        return 2;
    }
    close(frames[1]);
    close(control[1]);
    nexus::test::PcapCapture capture;
    bool captured = capture.Open(argv[4], "client.pcap", Now);
    current_capture = &capture;
    Client client(frames[0], control[0]);
    current_client = &client;
    pollfd ready{control[0], POLLIN, 0};
    char response = 0;
    int result = 1;
    if (captured && poll(&ready, 1U, 2000) == 1 &&
        recv(control[0], &response, 1U, 0) == 1 && response == 'R') {
        result = nx_chain_client(4, argv);
    }
    bool joined = Exchange(control[0], 'Q');
    int status = 0;
    bool reaped = false;
    const nx_time_us_t until = Now(nullptr) + 1500000U;
    while (Now(nullptr) < until) {
        const pid_t observed = waitpid(child, &status, WNOHANG);
        if (observed == child) {
            reaped = true;
            break;
        }
        if (observed < 0 && errno != EINTR) {
            break;
        }
        (void)poll(nullptr, 0U, 1);
    }
    if (!reaped) {
        (void)kill(child, SIGTERM);
        while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
        }
    }
    joined = joined && reaped && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    close(frames[0]);
    close(control[0]);
    captured = capture.Finish() && captured;
    current_capture = nullptr;
    std::printf("client_capture_frames=%zu client_capture_bytes=%zu "
                "client_capture_complete=%u\n",
                capture.Frames(), capture.Bytes(), captured ? 1U : 0U);
    std::printf("relay_joined=%u two_tcp_connections=1 phy_qualified=0\n",
                joined ? 1U : 0U);
    return result == 0 && joined && captured ? 0 : 1;
}
