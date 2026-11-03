/**
 * \file            lwip_frame_pair.h
 * \brief           Test-only lwIP frame endpoint and typed process-pair IPC
 * \author          Nexus Team
 * \note            Unqualified host fixture; no PHY, entropy or timing proof.
 */
#ifndef NEXUS_TESTS_SUPPORT_LWIP_FRAME_PAIR_H
#define NEXUS_TESTS_SUPPORT_LWIP_FRAME_PAIR_H

#include "nexus/network/lwip_storage.h"
#if defined(NEXUS_LWIP_LOANED_RX)
#include "nexus/network/lwip_loaned_rx_storage.h"
#endif
extern "C" {
#include "lwip/init.h"
}
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
#include <type_traits>
#include <unistd.h>

namespace nexus::test::lwip_pair {
struct FrameEndpoint;
inline FrameEndpoint* current_endpoint = nullptr;
inline uint32_t virtual_now = 0U;

/**
 * \brief Ethernet frame provider for one isolated real lwIP process.
 * \note Run every endpoint and stack operation in the child owner context.
 * The request remains borrowed until Service detaches and settles it. RX loans
 * retain exact identity, epoch and length until Release succeeds. Initialize
 * installs the one process clock/random domain before initializing lwIP.
 * The caller owns the socket and endpoint until adapter stop and child exit.
 * Busy provider operations retain outstanding requests and loans.
 * The optional submit hook runs synchronously before request admission; its
 * context stays caller-owned until the hook is cleared or the child exits.
 * The optional transmit hook runs only after one complete successful IPC send,
 * before request settlement. It copies spans synchronously, must not throw or
 * reenter, and cannot change the provider's borrow or settlement contract.
 * Retries and deliberate discards do not invoke that hook.
 */
struct FrameEndpoint {
    uint32_t role;
    int wire;
    uint32_t random_state;
    nx_eth_frames_port_t port{};
    nx_lwip_adapter_t adapter{};
    alignas(32) std::array<uint8_t, NX_ETH_FRAME_MAX> transmit_bytes{};
    std::array<nx_eth_fragment_t, 2> receive_views{};
#if defined(NEXUS_LWIP_LOANED_RX)
    struct ReceiveSlot {
        std::array<uint8_t, NX_ETH_FRAME_MAX> bytes{};
        uint64_t epoch{};
        size_t length{};
        bool held{};
    };
    std::array<ReceiveSlot, 8> receive_slots{};
    std::array<nx_lwip_rx_slot_t, 8> stack_slots{};
    std::array<nx_lwip_rx_piece_t, 16> stack_pieces{};
#else
    std::array<uint8_t, NX_ETH_FRAME_MAX> receive_bytes{};
#endif
    nx_eth_frames_tx_request_t* pending = nullptr;
    uint64_t receive_epoch = 0U;
    bool receive_held = false;
    size_t received_length = 0U;
    nx_eth_link_t link{};
    uint32_t data_attempts = 0U;
    uint32_t dropped = 0U;
    uint32_t fin_transmitted = 0U;
    bool drop_next_data = false;
    bool drop_tcp = false;
    void (*on_submit)(void*) = nullptr;
    void* on_submit_context = nullptr;
    void (*on_transmit)(void*, const uint8_t*, size_t) = nullptr;
    void* on_transmit_context = nullptr;

    explicit FrameEndpoint(uint32_t identity, int socket)
        : role(identity), wire(socket),
          random_state(UINT32_C(0x12345678) + identity) {
    }

    static nx_time_us_t Clock(void*) {
        return static_cast<nx_time_us_t>(virtual_now) * 1000U;
    }
    static nx_result_t Submit(void* context,
                              nx_eth_frames_tx_request_t* request) {
        auto& worker = *static_cast<FrameEndpoint*>(context);
        if (worker.pending != nullptr) {
            return NX_ERROR_BUSY;
        }
        if (request->fragment_count != 1U ||
            request->fragments[0].length > NX_ETH_FRAME_MAX) {
            return NX_ERROR_INVALID;
        }
        if (worker.on_submit != nullptr) {
            worker.on_submit(worker.on_submit_context);
        }
        const auto result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS) {
            worker.pending = request;
        }
        return result;
    }
    static nx_result_t Service(void* context) {
        auto& worker = *static_cast<FrameEndpoint*>(context);
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
            if (worker.on_transmit != nullptr) {
                worker.on_transmit(worker.on_transmit_context, bytes,
                                   fragment.length);
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
        auto& worker = *static_cast<FrameEndpoint*>(context);
#if defined(NEXUS_LWIP_LOANED_RX)
        size_t index = 0U;
        while (index < worker.receive_slots.size() &&
               worker.receive_slots[index].held) {
            ++index;
        }
        if (index == worker.receive_slots.size()) {
            return NX_ERROR_BUSY;
        }
        auto& receive = worker.receive_slots[index];
        auto& receive_bytes = receive.bytes;
#else
        if (worker.receive_held) {
            return NX_ERROR_BUSY;
        }
        auto& receive_bytes = worker.receive_bytes;
#endif
        if (capacity < 2U) {
            return NX_ERROR_OVERFLOW;
        }
        const auto length = recv(worker.wire, receive_bytes.data(),
                                 receive_bytes.size(), MSG_DONTWAIT);
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
#if defined(NEXUS_LWIP_LOANED_RX)
        receive.epoch = worker.receive_epoch;
        receive.length = worker.received_length;
        receive.held = true;
        loan->first_slot = index;
        loan->descriptor_count = 1U;
#endif
        fragments[0] = {receive_bytes.data(), 13U};
        fragments[1] = {receive_bytes.data() + 13U,
                        worker.received_length - 13U};
        return NX_SUCCESS;
    }
    static nx_result_t Release(void* context,
                               const nx_eth_frames_rx_loan_t* loan) {
        auto& worker = *static_cast<FrameEndpoint*>(context);
#if defined(NEXUS_LWIP_LOANED_RX)
        if (loan->identity != &worker ||
            loan->first_slot >= worker.receive_slots.size()) {
            return NX_ERROR_STATE;
        }
        auto& receive = worker.receive_slots[loan->first_slot];
        if (!receive.held || loan->epoch != receive.epoch ||
            loan->length != receive.length || loan->descriptor_count != 1U ||
            loan->fragment_count != 2U) {
            return NX_ERROR_STATE;
        }
        receive.held = false;
        worker.receive_held = false;
        for (const auto& slot : worker.receive_slots) {
            worker.receive_held = worker.receive_held || slot.held;
        }
#else
        if (!worker.receive_held || loan->identity != &worker ||
            loan->epoch != worker.receive_epoch ||
            loan->length != worker.received_length) {
            return NX_ERROR_STATE;
        }
        worker.receive_held = false;
#endif
        return NX_SUCCESS;
    }
    static nx_result_t Apply(void* context, const nx_eth_link_t* observed) {
        auto& worker = *static_cast<FrameEndpoint*>(context);
        if (worker.pending != nullptr) {
            return NX_ERROR_BUSY;
        }
        worker.link = *observed;
        return NX_SUCCESS;
    }
    static nx_result_t Stop(void* context) {
        auto& worker = *static_cast<FrameEndpoint*>(context);
        return worker.pending != nullptr || worker.receive_held ? NX_ERROR_BUSY
                                                                : NX_SUCCESS;
    }
    static nx_result_t State(void* context, nx_eth_frames_state_t* state) {
        auto& worker = *static_cast<FrameEndpoint*>(context);
        *state = {};
        state->mac.started = true;
        state->mac.link = worker.link;
        state->mac.pending_tx = worker.pending != nullptr ? 1U : 0U;
        state->mac.held_rx = worker.receive_held ? 1U : 0U;
        return NX_SUCCESS;
    }
    void Initialize();
};

inline const nx_eth_frames_ops_t model_ops{FrameEndpoint::Submit,
                                           nullptr,
                                           FrameEndpoint::Service,
                                           FrameEndpoint::Acquire,
                                           FrameEndpoint::Release,
                                           FrameEndpoint::State,
                                           nullptr,
                                           FrameEndpoint::Apply,
                                           FrameEndpoint::Stop,
                                           nullptr,
                                           nullptr};

inline void FrameEndpoint::Initialize() {
    current_endpoint = this;
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
#if defined(NEXUS_LWIP_LOANED_RX)
    const nx_lwip_loaned_config_t loaned{
        config, stack_slots.data(), stack_slots.size(), stack_pieces.data(),
        stack_pieces.size()};
    const auto started = nx_lwip_loaned_adapter_start(&adapter, &loaned);
#else
    const auto started = nx_lwip_adapter_start(&adapter, &config);
#endif
    if (started != NX_SUCCESS) {
        _exit(81);
    }
    const nx_eth_link_t observed{true, true, 100U};
    if (nx_lwip_adapter_apply_link(&adapter, &observed) != NX_SUCCESS) {
        _exit(82);
    }
}

/** \brief Forward the executable's required lwIP clock hook to its domain. */
inline uint32_t NowMs() {
    return virtual_now;
}

/** \brief Advance virtual time only from the serialized child command loop. */
inline void SetNowMs(uint32_t milliseconds) {
    virtual_now = milliseconds;
}

/** \brief Deterministic test RNG; Initialize must install the current endpoint.
 */
inline uint32_t RandomU32() {
    current_endpoint->random_state =
        current_endpoint->random_state * 1664525U + 1013904223U;
    return current_endpoint->random_state;
}

/** \brief Report a stack assertion and terminate its isolated worker process.
 */
[[noreturn]] inline void Assert(const char* message) {
    (void)write(STDERR_FILENO, message, std::strlen(message));
    _exit(90);
}

/**
 * \brief Own two child stacks, their frame wire and typed command channels.
 * \note Use only from the parent test context; each command or snapshot poll
 * has a two-second budget. Worker supplies Initialize, Execute, Observe and
 * static IsExit, ExitSucceeded and ValidateExit policies. Command and Snapshot
 * must be trivially copyable. Shutdown receives an acknowledged snapshot before
 * joining each owned child; failures surface as exceptions or GoogleTest
 * assertions. The pair cannot be copied; its destructor closes channels and
 * joins children.
 */
template <typename Worker, typename Command, typename Snapshot>
class ProcessPair {
    static_assert(std::is_trivially_copyable_v<Command>);
    static_assert(std::is_trivially_copyable_v<Snapshot>);

  public:
    std::array<Snapshot, 2> states{};
    explicit ProcessPair(Command shutdown) : exit_command(shutdown) {
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
                    WorkerMain(static_cast<uint32_t>(role), frames[role],
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
    ProcessPair(const ProcessPair&) = delete;
    ProcessPair& operator=(const ProcessPair&) = delete;
    ~ProcessPair() {
        for (size_t role = 0U; role < 2U; ++role) {
            if (sockets[role] >= 0) {
                if (!exit_requested[role]) {
                    try {
                        const auto state = Call(role, exit_command);
                        Worker::ValidateExit(state);
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
    Snapshot Call(size_t role, Command command) {
        pollfd ready{sockets[role], POLLOUT, 0};
        if (poll(&ready, 1, 2000) != 1 ||
            send(sockets[role], &command, sizeof(command), MSG_NOSIGNAL) !=
                sizeof(command)) {
            throw std::runtime_error("worker command failed");
        }
        states[role] = Read(role);
        if (Worker::IsExit(command)) {
            exit_requested[role] = true;
        }
        return states[role];
    }

  private:
    std::array<int, 2> sockets{{-1, -1}};
    std::array<pid_t, 2> children{{-1, -1}};
    std::array<bool, 2> exit_requested{};
    Command exit_command;
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
    [[noreturn]] static void WorkerMain(uint32_t role, int wire, int control) {
        Worker worker(role, wire);
        worker.Initialize();
        auto state = worker.Observe();
        if (send(control, &state, sizeof(state), MSG_NOSIGNAL) !=
            sizeof(state)) {
            _exit(83);
        }
        for (size_t iteration = 0U; iteration < 20000U; ++iteration) {
            Command command{};
            if (recv(control, &command, sizeof(command), 0) !=
                sizeof(command)) {
                _exit(84);
            }
            worker.Execute(command);
            state = worker.Observe();
            if (send(control, &state, sizeof(state), MSG_NOSIGNAL) !=
                sizeof(state)) {
                _exit(85);
            }
            if (Worker::IsExit(command)) {
                _exit(Worker::ExitSucceeded(state) ? 0 : 86);
            }
        }
        _exit(87);
    }
};
} /* namespace nexus::test::lwip_pair */

#endif /* NEXUS_TESTS_SUPPORT_LWIP_FRAME_PAIR_H */
