/**
 * \file            network_tcp_pair_test.cpp
 * \brief           Two real lwIP processes over bounded Ethernet frame IPC
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            Unqualified host fixture; no PHY, entropy or timing proof.
 */
#include "nexus/network/lwip_storage.h"
extern "C" {
#include "lwip/init.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/tcp.h"
}
#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace {
enum class Action : uint32_t {
    Tick,
    Listen,
    Connect,
    Send,
    HoldCredit,
    ResumeCredit,
    HalfClose,
    Close,
    DropNextData,
    DropTcp,
    ExhaustPcbs,
    FreePcbs,
    Abort,
    Exit
};
struct Command {
    Action action;
    uint32_t value;
};
struct Snapshot {
    uint32_t received;
    uint32_t enqueued;
    uint32_t acknowledged;
    uint32_t send_stalls;
    uint32_t data_attempts;
    uint32_t dropped;
    uint32_t fin_transmitted;
    uint32_t errors;
    uint32_t corrupt_payload;
    uint32_t active_pcbs;
    uint32_t time_wait_pcbs;
    uint32_t extra_pcbs;
    int32_t command_result;
    int32_t last_error;
    bool connected;
    bool peer_fin;
    bool application_released;
    bool transmit_borrowed;
    bool receive_held;
};
struct Worker;
Worker* current_worker = nullptr;
uint32_t virtual_now = 0U;

uint8_t pattern(uint32_t index, uint32_t role) {
    return static_cast<uint8_t>((index * 7U + role * 41U) % 251U);
}

struct Worker {
    uint32_t role;
    int wire;
    uint32_t random_state;
    nx_eth_frames_port_t port{};
    nx_lwip_adapter_t adapter{};
    alignas(32) std::array<uint8_t, NX_ETH_FRAME_MAX> transmit_bytes{};
    std::array<nx_eth_fragment_t, 2> receive_views{};
    std::array<uint8_t, NX_ETH_FRAME_MAX> receive_bytes{};
    nx_eth_frames_tx_request_t* pending = nullptr;
    uint64_t receive_epoch = 0U;
    bool receive_held = false;
    size_t received_length = 0U;
    nx_eth_link_t link{};
    struct tcp_pcb* connection = nullptr;
    struct tcp_pcb* listener = nullptr;
    std::array<struct tcp_pcb*, 4> extra{};
    uint32_t extra_count = 0U;
    uint32_t send_goal = 0U;
    uint32_t enqueued = 0U;
    uint32_t received = 0U;
    uint32_t acknowledged = 0U;
    uint32_t credit_held = 0U;
    uint32_t send_stalls = 0U;
    uint32_t data_attempts = 0U;
    uint32_t dropped = 0U;
    uint32_t fin_transmitted = 0U;
    uint32_t errors = 0U;
    uint32_t corrupt_payload = 0U;
    err_t last_error = ERR_OK;
    bool connected = false;
    bool peer_fin = false;
    bool hold_credit = false;
    bool write_closed = false;
    bool auto_close_on_fin = false;
    bool application_released = false;
    bool drop_next_data = false;
    bool drop_tcp = false;
    err_t command_result = ERR_OK;

    explicit Worker(uint32_t identity, int socket)
        : role(identity), wire(socket),
          random_state(UINT32_C(0x12345678) + identity) {
    }

    static nx_time_us_t Clock(void*) {
        return static_cast<nx_time_us_t>(virtual_now) * 1000U;
    }
    static nx_result_t Submit(void* context,
                              nx_eth_frames_tx_request_t* request) {
        auto& worker = *static_cast<Worker*>(context);
        if (worker.pending != nullptr) {
            return NX_ERROR_BUSY;
        }
        if (request->fragment_count != 1U ||
            request->fragments[0].length > NX_ETH_FRAME_MAX) {
            return NX_ERROR_INVALID;
        }
        const auto result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS) {
            worker.pending = request;
        }
        return result;
    }
    static nx_result_t Service(void* context) {
        auto& worker = *static_cast<Worker*>(context);
        if (worker.pending == nullptr) {
            return NX_SUCCESS;
        }
        auto* request = worker.pending;
        const auto& fragment = request->fragments[0];
        const auto* bytes = fragment.data;
        bool tcp_frame = fragment.length >= 54U && bytes[12] == 8U &&
                         bytes[13] == 0U && bytes[23] == 6U;
        bool data = false;
        if (tcp_frame) {
            const size_t ip_header = static_cast<size_t>(bytes[14] & 15U) * 4U;
            if (ip_header >= 20U && 14U + ip_header + 20U <= fragment.length) {
                const size_t tcp_start = 14U + ip_header;
                const size_t tcp_header =
                    static_cast<size_t>(bytes[tcp_start + 12U] >> 4U) * 4U;
                data = tcp_header >= 20U &&
                       tcp_start + tcp_header < fragment.length;
                if (data) {
                    ++worker.data_attempts;
                }
                if ((bytes[tcp_start + 13U] & 1U) != 0U) {
                    ++worker.fin_transmitted;
                }
            }
        }
        bool discard = tcp_frame && worker.drop_tcp;
        if (data && worker.drop_next_data) {
            discard = true;
            worker.drop_next_data = false;
        }
        if (!discard) {
            const auto sent = send(worker.wire, bytes, fragment.length,
                                   MSG_DONTWAIT | MSG_NOSIGNAL);
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (!nx_deadline_expired(request->base.deadline,
                                         Clock(nullptr))) {
                    return NX_SUCCESS;
                }
                worker.pending = nullptr;
                nx_request_settle(&request->base, NX_ERROR_TIMEOUT, 0U);
                return NX_SUCCESS;
            }
            if (sent != static_cast<ssize_t>(fragment.length)) {
                worker.pending = nullptr;
                nx_request_settle(&request->base, NX_ERROR_IO, 0U);
                return NX_ERROR_IO;
            }
        } else {
            ++worker.dropped;
        }
        worker.pending = nullptr;
        nx_request_settle(&request->base, NX_SUCCESS, fragment.length);
        return NX_SUCCESS;
    }
    static nx_result_t Acquire(void* context, nx_eth_frames_rx_loan_t* loan,
                               nx_eth_fragment_t* fragments, size_t capacity) {
        auto& worker = *static_cast<Worker*>(context);
        if (worker.receive_held) {
            return NX_ERROR_BUSY;
        }
        if (capacity < 2U) {
            return NX_ERROR_OVERFLOW;
        }
        const auto length = recv(worker.wire, worker.receive_bytes.data(),
                                 worker.receive_bytes.size(), MSG_DONTWAIT);
        if (length < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return NX_ERROR_EMPTY;
        }
        if (length < static_cast<ssize_t>(NX_ETH_FRAME_MIN)) {
            return NX_ERROR_IO;
        }
        worker.received_length = static_cast<size_t>(length);
        worker.receive_held = true;
        ++worker.receive_epoch;
        *loan = {};
        loan->identity = &worker;
        loan->epoch = worker.receive_epoch;
        loan->length = worker.received_length;
        loan->fragment_count = 2U;
        fragments[0] = {worker.receive_bytes.data(), 13U};
        fragments[1] = {worker.receive_bytes.data() + 13U,
                        worker.received_length - 13U};
        return NX_SUCCESS;
    }
    static nx_result_t Release(void* context,
                               const nx_eth_frames_rx_loan_t* loan) {
        auto& worker = *static_cast<Worker*>(context);
        if (!worker.receive_held || loan->identity != &worker ||
            loan->epoch != worker.receive_epoch ||
            loan->length != worker.received_length) {
            return NX_ERROR_STATE;
        }
        worker.receive_held = false;
        return NX_SUCCESS;
    }
    static nx_result_t Apply(void* context, const nx_eth_link_t* observed) {
        auto& worker = *static_cast<Worker*>(context);
        if (worker.pending != nullptr) {
            return NX_ERROR_BUSY;
        }
        worker.link = *observed;
        return NX_SUCCESS;
    }
    static nx_result_t Stop(void* context) {
        auto& worker = *static_cast<Worker*>(context);
        return worker.pending != nullptr || worker.receive_held ? NX_ERROR_BUSY
                                                                : NX_SUCCESS;
    }
    static nx_result_t State(void* context, nx_eth_frames_state_t* state) {
        auto& worker = *static_cast<Worker*>(context);
        *state = {};
        state->mac.started = true;
        state->mac.link = worker.link;
        state->mac.pending_tx = worker.pending != nullptr ? 1U : 0U;
        state->mac.held_rx = worker.receive_held ? 1U : 0U;
        return NX_SUCCESS;
    }
    static err_t Receive(void* context, struct tcp_pcb* pcb,
                         struct pbuf* packet, err_t error) {
        auto& worker = *static_cast<Worker*>(context);
        if (error != ERR_OK) {
            return error;
        }
        if (packet == nullptr) {
            worker.peer_fin = true;
            if (worker.auto_close_on_fin && tcp_close(pcb) == ERR_OK) {
                worker.connection = nullptr;
                worker.application_released = true;
            }
            return ERR_OK;
        }
        std::array<uint8_t, 256> bytes{};
        for (u16_t offset = 0U; offset < packet->tot_len;) {
            const u16_t length = static_cast<u16_t>(
                std::min<size_t>(bytes.size(), packet->tot_len - offset));
            if (pbuf_copy_partial(packet, bytes.data(), length, offset) !=
                length) {
                ++worker.corrupt_payload;
                break;
            }
            for (u16_t i = 0U; i < length; ++i) {
                if (bytes[i] !=
                    pattern(worker.received + i, worker.role ^ 1U)) {
                    ++worker.corrupt_payload;
                }
            }
            worker.received += length;
            offset = static_cast<u16_t>(offset + length);
        }
        if (worker.hold_credit) {
            worker.credit_held += packet->tot_len;
        } else {
            tcp_recved(pcb, packet->tot_len);
        }
        pbuf_free(packet);
        return ERR_OK;
    }
    static err_t Sent(void* context, struct tcp_pcb*, u16_t length) {
        static_cast<Worker*>(context)->acknowledged += length;
        return ERR_OK;
    }
    static void Error(void* context, err_t error) {
        auto& worker = *static_cast<Worker*>(context);
        worker.connection = nullptr;
        worker.connected = false;
        worker.last_error = error;
        ++worker.errors;
    }
    void Attach(struct tcp_pcb* pcb) {
        connection = pcb;
        tcp_arg(pcb, this);
        tcp_recv(pcb, Receive);
        tcp_sent(pcb, Sent);
        tcp_err(pcb, Error);
    }
    static err_t Accepted(void* context, struct tcp_pcb* pcb, err_t error) {
        auto& worker = *static_cast<Worker*>(context);
        if (error != ERR_OK || worker.connection != nullptr) {
            return ERR_MEM;
        }
        worker.Attach(pcb);
        worker.connected = true;
        return ERR_OK;
    }
    static err_t Connected(void* context, struct tcp_pcb*, err_t error) {
        auto& worker = *static_cast<Worker*>(context);
        worker.connected = error == ERR_OK;
        worker.last_error = error;
        return error;
    }
    void Pump() {
        if (connection == nullptr || !connected || write_closed) {
            return;
        }
        std::array<uint8_t, 512> bytes{};
        for (unsigned attempt = 0U; attempt < 4U && enqueued < send_goal;
             ++attempt) {
            const size_t available = tcp_sndbuf(connection);
            const size_t length = std::min<size_t>(
                std::min<size_t>(send_goal - enqueued, bytes.size()),
                available);
            if (length == 0U) {
                ++send_stalls;
                break;
            }
            for (size_t i = 0U; i < length; ++i) {
                bytes[i] = pattern(enqueued + static_cast<uint32_t>(i), role);
            }
            const err_t result =
                tcp_write(connection, bytes.data(), static_cast<u16_t>(length),
                          TCP_WRITE_FLAG_COPY);
            if (result != ERR_OK) {
                ++send_stalls;
                break;
            }
            enqueued += static_cast<uint32_t>(length);
        }
        (void)tcp_output(connection);
    }
    void Initialize();
    void Execute(Command command) {
        command_result = ERR_OK;
        switch (command.action) {
            case Action::Tick:
                virtual_now = command.value;
                if (nx_lwip_adapter_service(&adapter, 8U) != NX_SUCCESS) {
                    command_result = ERR_IF;
                }
                Pump();
                break;
            case Action::Listen: {
                auto* pcb = tcp_new();
                if (pcb == nullptr ||
                    tcp_bind(pcb, IP_ANY_TYPE, 32123U) != ERR_OK) {
                    command_result = ERR_MEM;
                    break;
                }
                listener = tcp_listen_with_backlog(pcb, 1U);
                if (listener == nullptr) {
                    tcp_abort(pcb);
                    command_result = ERR_MEM;
                    break;
                }
                tcp_arg(listener, this);
                tcp_accept(listener, Accepted);
                break;
            }
            case Action::Connect: {
                auto* pcb = tcp_new();
                if (pcb == nullptr) {
                    command_result = ERR_MEM;
                    break;
                }
                Attach(pcb);
                ip_addr_t peer{};
                IP_ADDR4(&peer, 10U, 0U, 0U, 1U);
                command_result = tcp_connect(pcb, &peer, 32123U, Connected);
                break;
            }
            case Action::Send:
                if (command.value > 65536U - send_goal) {
                    command_result = ERR_VAL;
                } else {
                    send_goal += command.value;
                    Pump();
                }
                break;
            case Action::HoldCredit:
                hold_credit = true;
                break;
            case Action::ResumeCredit:
                hold_credit = false;
                if (connection != nullptr) {
                    tcp_recved(connection, static_cast<u16_t>(credit_held));
                    credit_held = 0U;
                }
                break;
            case Action::HalfClose:
                if (connection == nullptr) {
                    command_result = ERR_CONN;
                } else {
                    command_result = tcp_shutdown(connection, 0, 1);
                    if (command_result == ERR_OK) {
                        write_closed = true;
                        auto_close_on_fin = true;
                    }
                }
                break;
            case Action::Close:
                if (connection == nullptr) {
                    command_result = ERR_CONN;
                } else {
                    command_result = tcp_close(connection);
                    if (command_result == ERR_OK) {
                        connection = nullptr;
                        application_released = true;
                        write_closed = true;
                    }
                }
                break;
            case Action::DropNextData:
                drop_next_data = true;
                break;
            case Action::DropTcp:
                drop_tcp = command.value != 0U;
                break;
            case Action::ExhaustPcbs:
                while (extra_count < extra.size()) {
                    auto* pcb = tcp_new();
                    if (pcb == nullptr) {
                        break;
                    }
                    extra[extra_count++] = pcb;
                }
                break;
            case Action::FreePcbs:
                while (extra_count != 0U) {
                    tcp_abort(extra[--extra_count]);
                }
                break;
            case Action::Abort:
                if (connection != nullptr) {
                    auto* pcb = connection;
                    connection = nullptr;
                    tcp_abort(pcb);
                }
                break;
            case Action::Exit:
                Cleanup();
                break;
        }
    }
    void Cleanup() {
        Execute({Action::FreePcbs, 0U});
        Execute({Action::Abort, 0U});
        if (listener != nullptr) {
            (void)tcp_close(listener);
            listener = nullptr;
        }
        (void)Service(this);
        const auto result = nx_lwip_adapter_stop(&adapter);
        command_result = result == NX_SUCCESS ? ERR_OK : ERR_IF;
    }
    Snapshot Observe() const {
        uint32_t active = 0U;
        uint32_t waiting = 0U;
        for (auto* pcb = tcp_active_pcbs; pcb != nullptr; pcb = pcb->next) {
            ++active;
        }
        for (auto* pcb = tcp_tw_pcbs; pcb != nullptr; pcb = pcb->next) {
            ++waiting;
        }
        return {received,
                enqueued,
                acknowledged,
                send_stalls,
                data_attempts,
                dropped,
                fin_transmitted,
                errors,
                corrupt_payload,
                active,
                waiting,
                extra_count,
                command_result,
                last_error,
                connected,
                peer_fin,
                application_released,
                pending != nullptr,
                receive_held};
    }
};

const nx_eth_frames_ops_t model_ops{
    Worker::Submit,  nullptr,       Worker::Service, Worker::Acquire,
    Worker::Release, Worker::State, nullptr,         Worker::Apply,
    Worker::Stop,    nullptr,       nullptr};

void Worker::Initialize() {
    port = {&model_ops, this};
    nx_lwip_config_t config{};
    config.port = &port;
    config.clock = {Clock, nullptr};
    config.tx_timeout_us = 2000000U;
    config.tx_buffer = transmit_bytes.data();
    config.tx_capacity = transmit_bytes.size();
    config.rx_views = receive_views.data();
    config.rx_capacity = receive_views.size();
    config.hardware_address[0] = 2U;
    config.hardware_address[5] = static_cast<uint8_t>(role + 1U);
    config.address[0] = 10U;
    config.address[3] = static_cast<uint8_t>(role + 1U);
    config.netmask[0] = 255U;
    config.netmask[1] = 255U;
    config.netmask[2] = 255U;
    lwip_init();
    if (nx_lwip_adapter_start(&adapter, &config) != NX_SUCCESS) {
        _exit(81);
    }
    const nx_eth_link_t observed{true, true, 100U};
    if (nx_lwip_adapter_apply_link(&adapter, &observed) != NX_SUCCESS) {
        _exit(82);
    }
}

void worker_main(uint32_t role, int wire, int control) {
    Worker worker(role, wire);
    current_worker = &worker;
    worker.Initialize();
    auto state = worker.Observe();
    if (send(control, &state, sizeof(state), MSG_NOSIGNAL) != sizeof(state)) {
        _exit(83);
    }
    for (size_t iteration = 0U; iteration < 20000U; ++iteration) {
        Command command{};
        if (recv(control, &command, sizeof(command), 0) != sizeof(command)) {
            _exit(84);
        }
        worker.Execute(command);
        state = worker.Observe();
        if (send(control, &state, sizeof(state), MSG_NOSIGNAL) !=
            sizeof(state)) {
            _exit(85);
        }
        if (command.action == Action::Exit) {
            _exit(state.command_result == ERR_OK ? 0 : 86);
        }
    }
    _exit(87);
}

class Pair {
  public:
    std::array<Snapshot, 2> states{};
    Pair() {
        int frames[2]{-1, -1};
        int controls[2][2]{{-1, -1}, {-1, -1}};
        try {
            if (socketpair(AF_UNIX, SOCK_DGRAM, 0, frames) != 0 ||
                socketpair(AF_UNIX, SOCK_SEQPACKET, 0, controls[0]) != 0 ||
                socketpair(AF_UNIX, SOCK_SEQPACKET, 0, controls[1]) != 0) {
                throw std::runtime_error("socketpair failed");
            }
            const int capacity = 32768;
            for (int descriptor : frames) {
                (void)setsockopt(descriptor, SOL_SOCKET, SO_SNDBUF, &capacity,
                                 sizeof(capacity));
            }
            for (size_t role = 0U; role < 2U; ++role) {
                const pid_t child = fork();
                if (child == 0) {
                    close(frames[role ^ 1U]);
                    close(controls[role][0]);
                    close(controls[role ^ 1U][0]);
                    close(controls[role ^ 1U][1]);
                    worker_main(static_cast<uint32_t>(role), frames[role],
                                controls[role][1]);
                }
                if (child < 0) {
                    throw std::runtime_error("fork failed");
                }
                children[role] = child;
                sockets[role] = controls[role][0];
            }
            close(frames[0]);
            frames[0] = -1;
            close(frames[1]);
            frames[1] = -1;
            close(controls[0][1]);
            controls[0][1] = -1;
            close(controls[1][1]);
            controls[1][1] = -1;
            for (size_t role = 0U; role < 2U; ++role) {
                states[role] = Read(role);
            }
        } catch (...) {
            for (int descriptor : frames) {
                if (descriptor >= 0) {
                    close(descriptor);
                }
            }
            for (const auto& pair : controls) {
                for (int descriptor : pair) {
                    if (descriptor >= 0) {
                        close(descriptor);
                    }
                }
            }
            for (pid_t child : children) {
                if (child > 0) {
                    (void)kill(child, SIGTERM);
                    while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {
                    }
                }
            }
            throw;
        }
    }
    ~Pair() {
        for (size_t role = 0U; role < 2U; ++role) {
            if (sockets[role] >= 0) {
                if (!exit_requested[role]) {
                    try {
                        const auto state = Call(role, Action::Exit);
                        EXPECT_EQ(ERR_OK, state.command_result);
                        EXPECT_FALSE(state.transmit_borrowed);
                        EXPECT_FALSE(state.receive_held);
                    } catch (const std::exception& error) {
                        ADD_FAILURE() << error.what();
                    }
                }
                close(sockets[role]);
            }
            if (children[role] > 0) {
                int status = 0;
                bool reaped = false;
                for (unsigned attempt = 0U; attempt < 20U; ++attempt) {
                    if (waitpid(children[role], &status, WNOHANG) ==
                        children[role]) {
                        reaped = true;
                        break;
                    }
                    const timespec delay{0, 1000000};
                    (void)nanosleep(&delay, nullptr);
                }
                if (!reaped) {
                    (void)kill(children[role], SIGTERM);
                    (void)waitpid(children[role], &status, 0);
                }
                EXPECT_TRUE(reaped);
                EXPECT_TRUE(WIFEXITED(status));
                if (WIFEXITED(status)) {
                    EXPECT_EQ(0, WEXITSTATUS(status));
                }
            }
        }
    }
    Snapshot Call(size_t role, Action action, uint32_t value = 0U) {
        const Command command{action, value};
        pollfd ready{sockets[role], POLLOUT, 0};
        if (poll(&ready, 1, 2000) != 1 ||
            send(sockets[role], &command, sizeof(command), MSG_NOSIGNAL) !=
                sizeof(command)) {
            throw std::runtime_error("worker command failed");
        }
        states[role] = Read(role);
        if (action == Action::Exit) {
            exit_requested[role] = true;
        }
        return states[role];
    }
    void Step(uint32_t milliseconds = 10U) {
        now += milliseconds;
        for (size_t role = 0U; role < 2U; ++role) {
            const auto snapshot = Call(role, Action::Tick, now);
            if (snapshot.command_result != ERR_OK) {
                throw std::runtime_error("adapter progress rejected");
            }
        }
    }
    template <typename Predicate>
    bool Until(Predicate predicate, unsigned limit = 1000U,
               uint32_t step = 10U) {
        for (unsigned attempt = 0U; attempt < limit; ++attempt) {
            if (predicate(states)) {
                return true;
            }
            Step(step);
        }
        return predicate(states);
    }
    void Connect() {
        for (unsigned i = 0U; i < 8U; ++i) {
            Step();
        }
        if (Call(0U, Action::Listen).command_result != ERR_OK ||
            Call(1U, Action::Connect).command_result != ERR_OK ||
            !Until([](const auto& value) {
                return value[0].connected && value[1].connected;
            })) {
            throw std::runtime_error("actual TCP handshake did not complete");
        }
    }
    uint32_t Time() const {
        return now;
    }

  private:
    std::array<int, 2> sockets{{-1, -1}};
    std::array<pid_t, 2> children{{-1, -1}};
    std::array<bool, 2> exit_requested{};
    uint32_t now = 0U;
    Snapshot Read(size_t role) {
        pollfd ready{sockets[role], POLLIN, 0};
        Snapshot state{};
        if (poll(&ready, 1, 2000) != 1 ||
            recv(sockets[role], &state, sizeof(state), 0) != sizeof(state)) {
            throw std::runtime_error("worker snapshot failed for role " +
                                     std::to_string(role));
        }
        return state;
    }
};

TEST(ActualTcpPair, HandshakeBidirectionalDataHalfCloseAndFinalAck) {
    Pair pair;
    pair.Connect();
    ASSERT_EQ(ERR_OK, pair.Call(1U, Action::Send, 257U).command_result);
    ASSERT_EQ(ERR_OK, pair.Call(0U, Action::Send, 389U).command_result);
    ASSERT_TRUE(pair.Until([](const auto& state) {
        return state[0].received == 257U && state[1].received == 389U &&
               state[0].acknowledged == 389U && state[1].acknowledged == 257U;
    }));
    EXPECT_EQ(0U, pair.states[0].corrupt_payload);
    EXPECT_EQ(0U, pair.states[1].corrupt_payload);
    ASSERT_EQ(ERR_OK, pair.Call(1U, Action::HalfClose).command_result);
    ASSERT_TRUE(
        pair.Until([](const auto& state) { return state[0].peer_fin; }));
    ASSERT_EQ(ERR_OK, pair.Call(0U, Action::Send, 123U).command_result);
    ASSERT_TRUE(pair.Until([](const auto& state) {
        return state[1].received == 512U && state[0].acknowledged == 512U;
    }));
    ASSERT_EQ(ERR_OK, pair.Call(0U, Action::Close).command_result);
    ASSERT_TRUE(pair.Until([](const auto& state) {
        return state[1].peer_fin && state[0].application_released &&
               state[1].application_released && state[0].active_pcbs == 0U &&
               state[1].active_pcbs == 0U;
    }));
    EXPECT_GE(pair.states[0].fin_transmitted, 1U);
    EXPECT_GE(pair.states[1].fin_transmitted, 1U);
    ASSERT_TRUE(pair.Until(
        [](const auto& state) {
            return state[0].time_wait_pcbs == 0U &&
                   state[1].time_wait_pcbs == 0U;
        },
        500U, 250U));
    for (size_t role = 0U; role < 2U; ++role) {
        const auto final = pair.Call(role, Action::Exit);
        EXPECT_EQ(ERR_OK, final.command_result);
        EXPECT_FALSE(final.transmit_borrowed);
        EXPECT_FALSE(final.receive_held);
    }
}

TEST(ActualTcpPair, ReceiveWindowBackpressureResumesAndCapacityAbortReclaims) {
    Pair pair;
    pair.Connect();
    pair.Call(0U, Action::HoldCredit);
    pair.Call(1U, Action::Send, 12000U);
    ASSERT_TRUE(pair.Until(
        [](const auto& state) { return state[0].received >= TCP_WND; }));
    for (unsigned i = 0U; i < 30U; ++i) {
        pair.Step();
    }
    EXPECT_EQ(static_cast<uint32_t>(TCP_WND), pair.states[0].received);
    EXPECT_GT(pair.states[1].send_stalls, 0U);
    EXPECT_LT(pair.states[0].received, 12000U);
    pair.Call(0U, Action::ResumeCredit);
    ASSERT_TRUE(pair.Until(
        [](const auto& state) {
            return state[0].received == 12000U &&
                   state[1].acknowledged == 12000U;
        },
        2500U));
    EXPECT_EQ(0U, pair.states[0].corrupt_payload);
    const auto full = pair.Call(1U, Action::ExhaustPcbs);
    EXPECT_EQ(3U, full.extra_pcbs);
    EXPECT_EQ(0U, pair.Call(1U, Action::FreePcbs).extra_pcbs);
    pair.Call(1U, Action::Abort);
    ASSERT_TRUE(pair.Until([](const auto& state) {
        return state[0].last_error == ERR_RST && state[0].active_pcbs == 0U;
    }));
    EXPECT_EQ(4U, pair.Call(1U, Action::ExhaustPcbs).extra_pcbs);
    pair.Call(1U, Action::FreePcbs);
}

TEST(ActualTcpPair, DroppedPayloadRetransmitsAndBlackholedSynExhaustsRetries) {
    {
        Pair pair;
        pair.Connect();
        pair.Call(1U, Action::DropNextData);
        pair.Call(1U, Action::Send, 512U);
        for (unsigned i = 0U; i < 20U; ++i) {
            pair.Step();
        }
        EXPECT_EQ(0U, pair.states[0].received);
        EXPECT_EQ(1U, pair.states[1].dropped);
        ASSERT_TRUE(pair.Until(
            [](const auto& state) {
                return state[0].received == 512U &&
                       state[1].acknowledged == 512U;
            },
            1000U));
        EXPECT_GE(pair.states[1].data_attempts, 2U);
        EXPECT_EQ(0U, pair.states[0].corrupt_payload);
    }
    {
        Pair pair;
        for (unsigned i = 0U; i < 8U; ++i) {
            pair.Step();
        }
        pair.Call(0U, Action::Listen);
        pair.Call(1U, Action::DropTcp, 1U);
        ASSERT_EQ(ERR_OK, pair.Call(1U, Action::Connect).command_result);
        ASSERT_TRUE(
            pair.Until([](const auto& state) { return state[1].errors != 0U; },
                       1000U, 250U));
        EXPECT_EQ(ERR_ABRT, pair.states[1].last_error);
        EXPECT_FALSE(pair.states[1].connected);
        EXPECT_EQ(0U, pair.states[1].active_pcbs);
        EXPECT_GE(pair.states[1].dropped, 2U);
        EXPECT_LT(pair.Time(), 500000U);
    }
}
} /* namespace */

extern "C" uint32_t nx_lwip_port_now_ms(void) {
    return virtual_now;
}
extern "C" uint32_t nx_lwip_port_random_u32(void) {
    current_worker->random_state =
        current_worker->random_state * 1664525U + 1013904223U;
    return current_worker->random_state;
}
extern "C" void nx_lwip_port_assert(const char* message) {
    (void)write(STDERR_FILENO, message, std::strlen(message));
    _exit(90);
}
