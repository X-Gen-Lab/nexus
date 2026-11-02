/**
 * \file            network_tcp_stream_test.cpp
 * \brief           Real two-stack TCP byte-stream ownership and lifecycle tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            Host Ethernet IPC fixture; no MCU, PHY or timing proof.
 */
#include "nexus/network/lwip_tcp_storage.h"
#include "support/lwip_frame_pair.h"
extern "C" {
#include "lwip/priv/tcp_priv.h"
}
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>

namespace {
using namespace nexus::test::lwip_pair;
enum class Action : uint32_t {
    Tick,
    Listen,
    Start,
    Write,
    Read,
    Close,
    Abort,
    ServerSend,
    ServerFin,
    ServerAbort,
    Exhaust,
    Free,
    Poison,
    Hook,
    ClockHook,
    PortError,
    ClientService,
    CrossHook,
    Drop,
    DropNextData,
    Exit
};
struct Command {
    Action action;
    uint32_t index;
    uint32_t value;
};
struct Snapshot {
    nx_lwip_tcp_diagnostic_t diagnostic[2];
    uint32_t received[2];
    uint32_t enqueued[2];
    uint32_t acknowledged[2];
    uint32_t corrupt[2];
    uint32_t callback_refs[2];
    bool connected[2];
    bool held[2];
    bool refused[2];
    bool config_reference[2];
    bool pcb[2];
    bool poisoned[2];
    uint32_t extras;
    uint32_t active;
    uint32_t hook_calls;
    uint32_t dropped;
    uint32_t data_attempts;
    uint32_t provider_calls;
    nx_result_t cross_read;
    nx_result_t cross_write;
    nx_result_t cross_service;
    size_t cross_read_count;
    size_t cross_write_count;
    uint32_t cross_provider_before;
    uint32_t cross_provider_after;
    uint32_t clock_hook_calls;
    bool active_in_clock_hook;
    bool callback_in_clock_hook;
    nx_result_t result;
    nx_result_t abort_in_hook;
    nx_result_t close_in_hook;
    bool transmit_borrowed;
    bool receive_held;
};
uint8_t pattern(size_t offset, uint32_t identity) {
    return static_cast<uint8_t>((offset * 7U + identity * 41U) % 251U);
}
struct Worker : FrameEndpoint {
    struct Peer {
        Worker* worker{};
        size_t index{};
        tcp_pcb* pcb{};
        tcp_pcb* listener{};
        uint32_t received{};
        uint32_t enqueued{};
        uint32_t acknowledged{};
        uint32_t corrupt{};
        uint32_t goal{};
        bool connected{};
        bool closed_write{};
        static err_t Receive(void* argument, tcp_pcb* pcb, pbuf* packet,
                             err_t error) {
            auto& self = *static_cast<Peer*>(argument);
            if (error != ERR_OK) {
                return error;
            }
            if (packet == nullptr) {
                return ERR_OK;
            }
            std::array<uint8_t, 512> bytes{};
            for (u16_t offset = 0U; offset < packet->tot_len;) {
                const auto n = static_cast<u16_t>(
                    std::min<size_t>(bytes.size(), packet->tot_len - offset));
                if (pbuf_copy_partial(packet, bytes.data(), n, offset) != n) {
                    ++self.corrupt;
                    break;
                }
                for (u16_t i = 0U; i < n; ++i) {
                    self.corrupt += bytes[i] != pattern(self.received + i, 1U);
                }
                self.received += n;
                offset = static_cast<u16_t>(offset + n);
            }
            tcp_recved(pcb, packet->tot_len);
            pbuf_free(packet);
            return ERR_OK;
        }
        static err_t Sent(void* argument, tcp_pcb*, u16_t bytes) {
            static_cast<Peer*>(argument)->acknowledged += bytes;
            return ERR_OK;
        }
        static void Error(void* argument, err_t) {
            auto& self = *static_cast<Peer*>(argument);
            self.pcb = nullptr;
            self.connected = false;
        }
        static err_t Accept(void* argument, tcp_pcb* pcb, err_t error) {
            auto& self = *static_cast<Peer*>(argument);
            if (error != ERR_OK || self.pcb != nullptr) {
                return ERR_MEM;
            }
            self.pcb = pcb;
            self.connected = true;
            tcp_arg(pcb, &self);
            tcp_recv(pcb, Receive);
            tcp_sent(pcb, Sent);
            tcp_err(pcb, Error);
            return ERR_OK;
        }
        void Pump() {
            if (pcb == nullptr || closed_write) {
                return;
            }
            std::array<uint8_t, 512> bytes{};
            for (unsigned attempt = 0U; attempt < 4U && enqueued < goal;
                 ++attempt) {
                const auto n = static_cast<u16_t>(std::min<size_t>(
                    {bytes.size(), goal - enqueued, tcp_sndbuf(pcb)}));
                if (n == 0U) {
                    break;
                }
                for (u16_t i = 0U; i < n; ++i) {
                    bytes[i] = pattern(enqueued + i, 0U);
                }
                if (tcp_write(pcb, bytes.data(), n, TCP_WRITE_FLAG_COPY) !=
                    ERR_OK) {
                    break;
                }
                enqueued += n;
            }
            (void)tcp_output(pcb);
        }
    };
    std::array<Peer, 2> peers{};
    std::array<nx_lwip_tcp_t, 2> clients{};
    std::array<bool, 2> started{};
    std::array<bool, 2> poisoned{};
    std::array<std::array<uint8_t, 512>, 2> producer_bytes{};
    std::array<uint32_t, 2> received{};
    std::array<uint32_t, 2> enqueued{};
    std::array<uint32_t, 2> corrupt{};
    std::array<tcp_pcb*, 4> extra{};
    uint32_t extras{};
    nx_result_t result = NX_SUCCESS;
    uint32_t hook_calls{};
    uint32_t provider_calls = 0U;
    bool fail_next_service = false;
    nx_result_t cross_read = NX_SUCCESS;
    nx_result_t cross_write = NX_SUCCESS;
    nx_result_t cross_service = NX_SUCCESS;
    size_t cross_read_count = 999U;
    size_t cross_write_count = 999U;
    uint32_t cross_provider_before = 0U;
    uint32_t cross_provider_after = 0U;
    static void CrossSubmit(void* context) {
        auto& self = *static_cast<Worker*>(context);
        self.on_submit = nullptr;
        uint8_t byte = pattern(0U, 1U);
        const auto deadline = Clock(nullptr) + 100000U;
        self.cross_provider_before = self.provider_calls;
        self.cross_read = nx_lwip_tcp_try_read(
            &self.clients[1], &byte, 1U, &self.cross_read_count, deadline);
        self.cross_write = nx_lwip_tcp_try_write(
            &self.clients[1], &byte, 1U, &self.cross_write_count, deadline);
        self.cross_service = nx_lwip_tcp_service(&self.clients[1]);
        self.cross_provider_after = self.provider_calls;
    }
    static const nx_eth_frames_ops_t service_ops;
    static nx_result_t ProviderService(void* context) {
        auto& self =
            *static_cast<Worker*>(static_cast<FrameEndpoint*>(context));
        ++self.provider_calls;
        if (self.fail_next_service) {
            self.fail_next_service = false;
            return NX_ERROR_IO;
        }
        return FrameEndpoint::Service(context);
    }
    size_t hook_index{};
    nx_result_t abort_in_hook = NX_SUCCESS;
    nx_result_t close_in_hook = NX_SUCCESS;
    struct ClockContext {
        Worker* owner;
    } clock_context{this};
    bool clock_hook = false;
    uint32_t clock_hook_calls = 0U;
    bool active_in_clock_hook = false;
    bool callback_in_clock_hook = false;
    static nx_time_us_t ObservedClock(void* context) {
        auto& self = *static_cast<ClockContext*>(context)->owner;
        auto& client = self.clients[0];
        if (self.clock_hook && client.pcb != nullptr &&
            client.pcb->state == ESTABLISHED &&
            client.diagnostic.state == NX_LWIP_TCP_CONNECTING) {
            self.clock_hook = false;
            ++self.clock_hook_calls;
            self.active_in_clock_hook = client.active;
            self.callback_in_clock_hook = client.callback_active;
            self.abort_in_hook = nx_lwip_tcp_abort(&client);
            self.close_in_hook = nx_lwip_tcp_close(
                &client, FrameEndpoint::Clock(nullptr) + 100000U);
        }
        return FrameEndpoint::Clock(nullptr);
    }
    void Initialize() {
        FrameEndpoint::Initialize();
        port.ops = &service_ops;
        adapter.config.clock = {ObservedClock, &clock_context};
    }
    Worker(uint32_t identity, int socket) : FrameEndpoint(identity, socket) {
        for (size_t i = 0U; i < peers.size(); ++i) {
            peers[i] = {this, i};
        }
    }
    static void SubmitHook(void* argument) {
        auto& self = *static_cast<Worker*>(argument);
        ++self.hook_calls;
        self.on_submit = nullptr;
        self.abort_in_hook = nx_lwip_tcp_abort(&self.clients[self.hook_index]);
        self.close_in_hook = nx_lwip_tcp_close(&self.clients[self.hook_index],
                                               Clock(nullptr) + 100000U);
    }
    static bool IsExit(Command command) {
        return command.action == Action::Exit;
    }
    static bool ExitSucceeded(const Snapshot& state) {
        return state.result == NX_SUCCESS && !state.transmit_borrowed &&
               !state.receive_held;
    }
    static void ValidateExit(const Snapshot& state) {
        EXPECT_TRUE(ExitSucceeded(state));
        EXPECT_EQ(0U, state.callback_refs[0]);
        EXPECT_EQ(0U, state.callback_refs[1]);
    }
    void Execute(Command command) {
        const size_t i = command.index;
        if (i >= 2U) {
            result = NX_ERROR_INVALID;
            return;
        }
        result = NX_SUCCESS;
        auto& peer = peers[i];
        auto& client = clients[i];
        switch (command.action) {
            case Action::Tick:
                SetNowMs(command.value);
                result = nx_lwip_adapter_service(&adapter, 8U);
                for (size_t j = 0U; j < 2U; ++j) {
                    if (started[j] && !poisoned[j]) {
                        (void)nx_lwip_tcp_service(&clients[j]);
                    }
                    peers[j].Pump();
                }
                break;
            case Action::Listen: {
                auto* pcb = tcp_new();
                if (pcb == nullptr) {
                    result = NX_ERROR_EXHAUSTED;
                    break;
                }
                if (tcp_bind(pcb, IP_ANY_TYPE,
                             static_cast<u16_t>(32123U + i)) != ERR_OK) {
                    tcp_abort(pcb);
                    result = NX_ERROR_IO;
                    break;
                }
                peer.listener = tcp_listen_with_backlog(pcb, 1U);
                if (peer.listener == nullptr) {
                    tcp_abort(pcb);
                    result = NX_ERROR_EXHAUSTED;
                    break;
                }
                tcp_arg(peer.listener, &peer);
                tcp_accept(peer.listener, Peer::Accept);
                break;
            }
            case Action::Start: {
                const nx_lwip_tcp_config_t config{
                    &adapter,
                    2U,
                    {10U, 0U, 0U, 1U},
                    static_cast<uint16_t>(32123U + i)};
                result = nx_lwip_tcp_start(
                    &client, &config,
                    Clock(nullptr) + (command.value ? command.value : 5000U) *
                                         UINT64_C(1000));
                started[i] = result == NX_SUCCESS;
                break;
            }
            case Action::Write: {
                auto& bytes = producer_bytes[i];
                const size_t n = std::min<size_t>(command.value, bytes.size());
                for (size_t j = 0U; j < n; ++j) {
                    bytes[j] = pattern(enqueued[i] + j, 1U);
                }
                const auto* stream = nx_lwip_tcp_stream(&client);
                size_t done = 999U;
                result = stream ? stream->ops->write(stream->context,
                                                     bytes.data(), n, &done,
                                                     Clock(nullptr) + 100000U)
                                : NX_ERROR_STATE;
                if (stream && done <= n) {
                    enqueued[i] += static_cast<uint32_t>(done);
                } else if (stream) {
                    ++corrupt[i];
                }
                bytes.fill(0xeeU);
                break;
            }
            case Action::Read: {
                std::array<uint8_t, 512> bytes{};
                const size_t n = std::min<size_t>(command.value, bytes.size());
                size_t done = 999U;
                const auto* stream = nx_lwip_tcp_stream(&client);
                result =
                    stream ? stream->ops->read(stream->context, bytes.data(), n,
                                               &done, Clock(nullptr) + 100000U)
                           : NX_ERROR_STATE;
                if (stream && done <= n) {
                    for (size_t j = 0U; j < done; ++j) {
                        corrupt[i] += bytes[j] != pattern(received[i] + j, 0U);
                    }
                    received[i] += static_cast<uint32_t>(done);
                } else if (stream) {
                    ++corrupt[i];
                }
                break;
            }
            case Action::Close:
                result = nx_lwip_tcp_close(
                    &client,
                    Clock(nullptr) + (command.value ? command.value : 1000U) *
                                         UINT64_C(1000));
                break;
            case Action::Abort:
                result = nx_lwip_tcp_abort(&client);
                break;
            case Action::ServerSend:
                peer.goal += command.value;
                peer.Pump();
                break;
            case Action::ServerFin:
                if (peer.pcb == nullptr ||
                    tcp_shutdown(peer.pcb, 0, 1) != ERR_OK) {
                    result = NX_ERROR_IO;
                } else {
                    peer.closed_write = true;
                }
                break;
            case Action::ServerAbort:
                if (peer.pcb != nullptr) {
                    auto* pcb = peer.pcb;
                    peer.pcb = nullptr;
                    tcp_abort(pcb);
                }
                break;
            case Action::Exhaust:
                while (extras < extra.size()) {
                    auto* pcb = tcp_new();
                    if (pcb == nullptr) {
                        break;
                    }
                    extra[extras++] = pcb;
                }
                break;
            case Action::Free:
                while (extras != 0U) {
                    tcp_abort(extra[--extras]);
                }
                break;
            case Action::Poison:
                std::memset(&client, 0xa5, sizeof(client));
                poisoned[i] = true;
                break;
            case Action::Hook:
                hook_index = i;
                on_submit = SubmitHook;
                on_submit_context = this;
                break;
            case Action::PortError:
                fail_next_service = true;
                break;
            case Action::ClientService:
                result = nx_lwip_tcp_service(&client);
                break;
            case Action::CrossHook:
                on_submit = CrossSubmit;
                on_submit_context = this;
                break;
            case Action::ClockHook:
                clock_hook = true;
                break;
            case Action::Drop:
                drop_tcp = command.value != 0U;
                break;
            case Action::DropNextData:
                drop_next_data = true;
                break;
            case Action::Exit:
                on_submit = nullptr;
                for (size_t j = 0U; j < 2U; ++j) {
                    if (started[j] && !poisoned[j]) {
                        (void)nx_lwip_tcp_abort(&clients[j]);
                    }
                    if (peers[j].pcb != nullptr) {
                        tcp_abort(peers[j].pcb);
                        peers[j].pcb = nullptr;
                    }
                    if (peers[j].listener != nullptr) {
                        (void)tcp_close(peers[j].listener);
                        peers[j].listener = nullptr;
                    }
                }
                while (extras != 0U) {
                    tcp_abort(extra[--extras]);
                }
                while (tcp_active_pcbs != nullptr) {
                    tcp_abort(tcp_active_pcbs);
                }
                while (tcp_tw_pcbs != nullptr) {
                    tcp_abort(tcp_tw_pcbs);
                }
                (void)Service(static_cast<FrameEndpoint*>(this));
                result = nx_lwip_adapter_stop(&adapter);
                break;
        }
    }
    Snapshot Observe() const {
        Snapshot value{};
        value.result = result;
        value.extras = extras;
        value.hook_calls = hook_calls;
        value.dropped = dropped;
        value.data_attempts = data_attempts;
        value.provider_calls = provider_calls;
        value.cross_read = cross_read;
        value.cross_write = cross_write;
        value.cross_service = cross_service;
        value.cross_read_count = cross_read_count;
        value.cross_write_count = cross_write_count;
        value.cross_provider_before = cross_provider_before;
        value.cross_provider_after = cross_provider_after;
        value.clock_hook_calls = clock_hook_calls;
        value.active_in_clock_hook = active_in_clock_hook;
        value.callback_in_clock_hook = callback_in_clock_hook;
        value.abort_in_hook = abort_in_hook;
        value.close_in_hook = close_in_hook;
        value.transmit_borrowed = pending != nullptr;
        value.receive_held = receive_held;
        for (size_t i = 0U; i < 2U; ++i) {
            if (!poisoned[i]) {
                if (started[i]) {
                    (void)nx_lwip_tcp_diagnostic(&clients[i],
                                                 &value.diagnostic[i]);
                }
                value.held[i] = clients[i].receive != nullptr;
                value.pcb[i] = clients[i].pcb != nullptr;
                value.refused[i] = clients[i].pcb != nullptr &&
                                   clients[i].pcb->refused_data != nullptr;
                value.config_reference[i] =
                    clients[i].config.network != nullptr;
            }
            value.received[i] = role == 0U ? peers[i].received : received[i];
            value.enqueued[i] = role == 0U ? peers[i].enqueued : enqueued[i];
            value.acknowledged[i] = peers[i].acknowledged;
            value.corrupt[i] = role == 0U ? peers[i].corrupt : corrupt[i];
            value.connected[i] = peers[i].connected;
            value.poisoned[i] = poisoned[i];
            if (poisoned[i]) {
                const auto* bytes =
                    reinterpret_cast<const uint8_t*>(&clients[i]);
                for (size_t j = 0U; j < sizeof(clients[i]); ++j) {
                    value.poisoned[i] = value.poisoned[i] && bytes[j] == 0xa5U;
                }
            }
            for (auto* pcb = tcp_active_pcbs; pcb != nullptr; pcb = pcb->next) {
                ++value.active;
                value.callback_refs[i] += pcb->callback_arg == &clients[i];
            }
            for (auto* pcb = tcp_tw_pcbs; pcb != nullptr; pcb = pcb->next) {
                value.callback_refs[i] += pcb->callback_arg == &clients[i];
            }
        }
        value.active /= 2U;
        return value;
    }
};
const nx_eth_frames_ops_t Worker::service_ops{FrameEndpoint::Submit,
                                              nullptr,
                                              Worker::ProviderService,
                                              FrameEndpoint::Acquire,
                                              FrameEndpoint::Release,
                                              FrameEndpoint::State,
                                              nullptr,
                                              FrameEndpoint::Apply,
                                              FrameEndpoint::Stop,
                                              nullptr,
                                              nullptr};
class Pair : public ProcessPair<Worker, Command, Snapshot> {
    using Base = ProcessPair<Worker, Command, Snapshot>;

  public:
    Pair() : Base({Action::Exit, 0U, 0U}) {
    }
    Snapshot Call(size_t role, Action action, uint32_t index = 0U,
                  uint32_t value = 0U) {
        return Base::Call(role, {action, index, value});
    }
    void Step(uint32_t milliseconds = 10U) {
        now += milliseconds;
        for (size_t role = 0U; role < 2U; ++role) {
            if (Call(role, Action::Tick, 0U, now).result != NX_SUCCESS) {
                throw std::runtime_error("network service failed");
            }
        }
    }
    template <typename Predicate>
    bool Until(Predicate predicate, unsigned limit = 1000U) {
        for (unsigned step = 0U; step < limit && !predicate(states); ++step) {
            Step();
        }
        return predicate(states);
    }
    void Connect(uint32_t index = 0U) {
        for (unsigned step = 0U; step < 8U; ++step) {
            Step();
        }
        if (Call(0U, Action::Listen, index).result != NX_SUCCESS ||
            Call(1U, Action::Start, index).result != NX_SUCCESS ||
            !Until([index](const auto& s) {
                return s[0].connected[index] &&
                       s[1].diagnostic[index].state == NX_LWIP_TCP_ESTABLISHED;
            })) {
            throw std::runtime_error("byte stream actual handshake failed");
        }
    }
    uint32_t now{};
};
TEST(NxLwipTcpPair, ExplicitConnectionAdmissionCreatesTheExistingByteFace) {
    Pair pair;
    ASSERT_EQ(NX_SUCCESS, pair.Call(0U, Action::Listen).result);
    const auto admission = pair.Call(1U, Action::Start);
    EXPECT_EQ(NX_SUCCESS, admission.result);
    EXPECT_EQ(NX_LWIP_TCP_CONNECTING, admission.diagnostic[0].state);
}
TEST(NxLwipTcpPair, CopiedWritesAndPartialReadsUseRealTcpAcknowledgments) {
    Pair pair;
    pair.Connect();
    pair.Call(1U, Action::DropNextData);
    ASSERT_EQ(NX_SUCCESS, pair.Call(1U, Action::Write, 0U, 257U).result);
    ASSERT_TRUE(pair.Until([](const auto& s) {
        return s[0].received[0] == 257U &&
               s[1].diagnostic[0].pending_transmit == 0U;
    }));
    EXPECT_EQ(0U, pair.states[0].corrupt[0]);
    EXPECT_EQ(1U, pair.states[1].dropped);
    EXPECT_GE(pair.states[1].data_attempts, 2U);
    pair.Call(0U, Action::ServerSend, 0U, 401U);
    ASSERT_TRUE(pair.Until([](const auto& s) { return s[1].held[0]; }));
    const auto before = pair.states[1].diagnostic[0].pending_receive;
    ASSERT_GT(before, 17U);
    auto read = pair.Call(1U, Action::Read, 0U, 17U);
    ASSERT_EQ(NX_SUCCESS, read.result);
    EXPECT_EQ(17U, read.received[0]);
    EXPECT_EQ(before - 17U, read.diagnostic[0].pending_receive);
    for (unsigned i = 0U; i < 50U && pair.states[1].received[0] < 401U; ++i) {
        pair.Call(1U, Action::Read, 0U, 97U);
        pair.Step();
    }
    EXPECT_EQ(401U, pair.states[1].received[0]);
    EXPECT_EQ(0U, pair.states[1].corrupt[0]);
}
TEST(NxLwipTcpPair, HeldReceiveBackpressureCanBeConsumedWithoutPoolDeadlock) {
    Pair pair;
    pair.Connect();
    pair.Call(0U, Action::ServerSend, 0U, 12000U);
    ASSERT_TRUE(pair.Until([](const auto& s) { return s[1].held[0]; }));
    for (unsigned i = 0U; i < 30U; ++i) {
        pair.Step();
    }
    EXPECT_EQ(0U, pair.states[1].received[0]);
    EXPECT_GT(pair.states[1].diagnostic[0].pending_receive, 0U);
    for (unsigned i = 0U; i < 1000U && pair.states[1].received[0] < 12000U;
         ++i) {
        pair.Call(1U, Action::Read, 0U, 113U);
        pair.Step();
    }
    EXPECT_EQ(12000U, pair.states[1].received[0]);
    EXPECT_EQ(0U, pair.states[1].corrupt[0]);
}
TEST(NxLwipTcpPair, CloseBusyRetainsHeldReceiveUntilExplicitAbort) {
    Pair pair;
    pair.Connect();
    pair.Call(0U, Action::ServerSend, 0U, 99U);
    ASSERT_TRUE(pair.Until([](const auto& state) { return state[1].held[0]; }));
    const auto before = pair.states[1].diagnostic[0].pending_receive;
    EXPECT_EQ(NX_ERROR_BUSY, pair.Call(1U, Action::Close).result);
    EXPECT_TRUE(pair.states[1].held[0]);
    EXPECT_EQ(before, pair.states[1].diagnostic[0].pending_receive);
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Abort).result);
    EXPECT_FALSE(pair.states[1].held[0]);
    EXPECT_FALSE(pair.states[1].pcb[0]);
    EXPECT_EQ(0U, pair.states[1].callback_refs[0]);
}
TEST(NxLwipTcpPair,
     PeerFinFollowsBufferedBytesAndStillAllowsOppositeDirection) {
    Pair pair;
    pair.Connect();
    pair.Call(0U, Action::ServerSend, 0U, 99U);
    ASSERT_TRUE(pair.Until([](const auto& s) { return s[1].held[0]; }));
    ASSERT_EQ(NX_SUCCESS, pair.Call(0U, Action::ServerFin).result);
    ASSERT_TRUE(
        pair.Until([](const auto& s) { return s[1].diagnostic[0].peer_fin; }));
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Write, 0U, 71U).result);
    ASSERT_TRUE(
        pair.Until([](const auto& s) { return s[0].received[0] == 71U; }));
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Read, 0U, 512U).result);
    EXPECT_EQ(99U, pair.states[1].received[0]);
    auto eof = pair.Call(1U, Action::Read, 0U, 512U);
    EXPECT_EQ(NX_SUCCESS, eof.result);
    EXPECT_EQ(99U, eof.received[0]);
}
TEST(NxLwipTcpPair,
     ResetFreesThePcbBeforeExplicitAbortAndNoStaleCallbackRemains) {
    Pair pair;
    pair.Connect();
    pair.Call(0U, Action::ServerSend, 0U, 99U);
    ASSERT_TRUE(pair.Until([](const auto& state) { return state[1].held[0]; }));
    pair.Call(0U, Action::ServerAbort);
    ASSERT_TRUE(pair.Until([](const auto& s) {
        return s[1].diagnostic[0].state == NX_LWIP_TCP_FAILED;
    }));
    EXPECT_FALSE(pair.states[1].pcb[0]);
    EXPECT_EQ(NX_ERROR_IO, pair.Call(1U, Action::Write, 0U, 12U).result);
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Abort).result);
    EXPECT_EQ(0U, pair.states[1].callback_refs[0]);
    pair.Call(1U, Action::Poison);
    for (unsigned i = 0U; i < 50U; ++i) {
        pair.Step();
    }
    EXPECT_TRUE(pair.states[1].poisoned[0]);
}
TEST(NxLwipTcpPair, CloseRetainsStackOwnedRefusedReceiveUntilConsumerDrains) {
    Pair pair;
    pair.Connect();
    pair.Call(0U, Action::ServerSend, 0U, 1460U);
    ASSERT_TRUE(pair.Until([](const auto& state) { return state[1].held[0]; }));
    pair.Call(0U, Action::ServerSend, 0U, 1460U);
    ASSERT_TRUE(
        pair.Until([](const auto& state) { return state[1].refused[0]; }));
    ASSERT_LT(pair.now, 250U);
    while (pair.states[1].received[0] < 1460U) {
        ASSERT_EQ(NX_SUCCESS, pair.Call(1U, Action::Read, 0U, 512U).result);
    }
    ASSERT_FALSE(pair.states[1].held[0]);
    ASSERT_TRUE(pair.states[1].refused[0]);
    const auto closing = pair.Call(1U, Action::Close);
    EXPECT_EQ(NX_ERROR_BUSY, closing.result);
    EXPECT_TRUE(closing.pcb[0]);
    EXPECT_TRUE(closing.refused[0]);
}
TEST(NxLwipTcpPair,
     FatalProgressDoesNotResumeImplicitlyOrReleaseReceiveOwnership) {
    Pair pair;
    pair.Connect();
    pair.Call(0U, Action::ServerSend, 0U, 99U);
    ASSERT_TRUE(pair.Until([](const auto& state) { return state[1].held[0]; }));
    pair.Call(1U, Action::PortError);
    const auto fatal = pair.Call(1U, Action::ClientService);
    EXPECT_EQ(NX_ERROR_IO, fatal.result);
    EXPECT_EQ(NX_LWIP_TCP_FAILED, fatal.diagnostic[0].state);
    EXPECT_TRUE(fatal.pcb[0]);
    EXPECT_TRUE(fatal.held[0]);
    const auto calls = fatal.provider_calls;
    EXPECT_EQ(NX_ERROR_IO, pair.Call(1U, Action::Write, 0U, 17U).result);
    EXPECT_EQ(NX_ERROR_IO, pair.Call(1U, Action::Read, 0U, 17U).result);
    EXPECT_EQ(NX_ERROR_IO, pair.Call(1U, Action::ClientService).result);
    EXPECT_EQ(calls, pair.states[1].provider_calls);
    EXPECT_EQ(0U, pair.states[1].received[0]);
    EXPECT_EQ(0U, pair.states[1].enqueued[0]);
    EXPECT_TRUE(pair.states[1].held[0]);
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Abort).result);
    EXPECT_FALSE(pair.states[1].held[0]);
    EXPECT_FALSE(pair.states[1].pcb[0]);
}
TEST(NxLwipTcpPair, UnacknowledgedCloseKeepsTheFirstDeadlineAndItsContext) {
    Pair pair;
    pair.Connect();
    pair.Call(0U, Action::Drop, 0U, 1U);
    ASSERT_EQ(NX_SUCCESS, pair.Call(1U, Action::Write, 0U, 257U).result);
    EXPECT_GT(pair.states[1].diagnostic[0].pending_transmit, 0U);
    EXPECT_EQ(NX_ERROR_BUSY, pair.Call(1U, Action::Close, 0U, 5U).result);
    pair.Step(10U);
    const auto late = pair.Call(1U, Action::Close, 0U, 5000U);
    EXPECT_EQ(NX_ERROR_TIMEOUT, late.result);
    EXPECT_TRUE(late.pcb[0]);
    EXPECT_GT(late.callback_refs[0], 0U);
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Abort).result);
    EXPECT_FALSE(pair.states[1].pcb[0]);
    EXPECT_EQ(0U, pair.states[1].callback_refs[0]);
}
TEST(NxLwipTcpPair, ConnectTimeoutRetainsAnExplicitAbortPath) {
    Pair pair;
    ASSERT_EQ(NX_SUCCESS, pair.Call(0U, Action::Listen).result);
    pair.Call(1U, Action::Drop, 0U, 1U);
    ASSERT_EQ(NX_SUCCESS, pair.Call(1U, Action::Start, 0U, 20U).result);
    pair.Step(10U);
    EXPECT_EQ(NX_LWIP_TCP_CONNECTING, pair.states[1].diagnostic[0].state);
    EXPECT_TRUE(pair.states[1].pcb[0]);
    pair.Step(10U);
    EXPECT_EQ(NX_LWIP_TCP_FAILED, pair.states[1].diagnostic[0].state);
    EXPECT_EQ(NX_ERROR_TIMEOUT, pair.states[1].diagnostic[0].result);
    EXPECT_TRUE(pair.states[1].pcb[0]);
    EXPECT_GT(pair.states[1].callback_refs[0], 0U);
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Abort).result);
    EXPECT_FALSE(pair.states[1].pcb[0]);
    EXPECT_EQ(0U, pair.states[1].callback_refs[0]);
}
TEST(NxLwipTcpPair, PoolAdmissionAndTwoInstancesRemainIndependent) {
    Pair pair;
    pair.Connect();
    ASSERT_EQ(3U, pair.Call(1U, Action::Exhaust).extras);
    EXPECT_EQ(NX_ERROR_EXHAUSTED, pair.Call(1U, Action::Start, 1U).result);
    EXPECT_FALSE(pair.states[1].pcb[1]);
    EXPECT_FALSE(pair.states[1].held[1]);
    EXPECT_FALSE(pair.states[1].config_reference[1]);
    EXPECT_EQ(0U, pair.states[1].callback_refs[1]);
    pair.Call(1U, Action::Free);
    pair.Connect(1U);
    ASSERT_EQ(NX_SUCCESS, pair.Call(1U, Action::Write, 0U, 37U).result);
    ASSERT_EQ(NX_SUCCESS, pair.Call(1U, Action::Write, 1U, 93U).result);
    ASSERT_TRUE(pair.Until([](const auto& s) {
        return s[0].received[0] == 37U && s[0].received[1] == 93U;
    }));
    EXPECT_EQ(0U, pair.states[0].corrupt[0]);
    EXPECT_EQ(0U, pair.states[0].corrupt[1]);
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Abort, 0U).result);
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Write, 1U, 51U).result);
    ASSERT_TRUE(
        pair.Until([](const auto& s) { return s[0].received[1] == 144U; }));
}
TEST(NxLwipTcpPair, CloseDetachesContextWhileTheGlobalStackFinishes) {
    Pair pair;
    pair.Connect();
    ASSERT_EQ(NX_SUCCESS, pair.Call(1U, Action::Close).result);
    EXPECT_FALSE(pair.states[1].pcb[0]);
    EXPECT_EQ(0U, pair.states[1].callback_refs[0]);
    pair.Call(1U, Action::Poison);
    pair.Call(0U, Action::ServerFin);
    for (unsigned i = 0U; i < 100U; ++i) {
        pair.Step();
    }
    EXPECT_TRUE(pair.states[1].poisoned[0]);
}
TEST(NxLwipTcpPair, CloseAndAbortReentryCannotReclaimTheActiveIoContext) {
    Pair pair;
    pair.Connect();
    pair.Call(1U, Action::Hook);
    ASSERT_EQ(NX_SUCCESS, pair.Call(1U, Action::Write, 0U, 17U).result);
    ASSERT_TRUE(
        pair.Until([](const auto& s) { return s[0].received[0] == 17U; }));
    EXPECT_GT(pair.states[1].hook_calls, 0U);
    EXPECT_EQ(NX_ERROR_BUSY, pair.states[1].abort_in_hook);
    EXPECT_EQ(NX_ERROR_BUSY, pair.states[1].close_in_hook);
    EXPECT_EQ(NX_LWIP_TCP_ESTABLISHED, pair.states[1].diagnostic[0].state);
}
TEST(NxLwipTcpPair, RawConnectedCallbackJoinsBeforeItsContextCanBeReclaimed) {
    Pair pair;
    pair.Call(1U, Action::ClockHook);
    pair.Connect();
    EXPECT_EQ(1U, pair.states[1].clock_hook_calls);
    EXPECT_FALSE(pair.states[1].active_in_clock_hook);
    EXPECT_TRUE(pair.states[1].callback_in_clock_hook);
    EXPECT_EQ(NX_ERROR_BUSY, pair.states[1].abort_in_hook);
    EXPECT_EQ(NX_ERROR_BUSY, pair.states[1].close_in_hook);
    EXPECT_EQ(NX_LWIP_TCP_ESTABLISHED, pair.states[1].diagnostic[0].state);
}
TEST(NxLwipTcpPair, ASecondInstanceCannotReenterTheSameInFlightStackDomain) {
    Pair pair;
    pair.Connect();
    pair.Connect(1U);
    pair.Call(0U, Action::ServerSend, 1U, 99U);
    ASSERT_TRUE(pair.Until([](const auto& state) { return state[1].held[1]; }));
    pair.Call(1U, Action::CrossHook);
    ASSERT_EQ(NX_SUCCESS, pair.Call(1U, Action::Write, 0U, 17U).result);
    const auto nested = pair.states[1];
    EXPECT_EQ(NX_ERROR_BUSY, nested.cross_read);
    EXPECT_EQ(NX_ERROR_BUSY, nested.cross_write);
    EXPECT_EQ(NX_ERROR_BUSY, nested.cross_service);
    EXPECT_EQ(0U, nested.cross_read_count);
    EXPECT_EQ(0U, nested.cross_write_count);
    EXPECT_EQ(nested.cross_provider_before, nested.cross_provider_after);
    ASSERT_TRUE(pair.Until(
        [](const auto& state) { return state[0].received[0] == 17U; }));
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Read, 1U, 512U).result);
    EXPECT_EQ(99U, pair.states[1].received[1]);
    EXPECT_EQ(0U, pair.states[1].corrupt[1]);
    EXPECT_EQ(NX_SUCCESS, pair.Call(1U, Action::Write, 1U, 31U).result);
    ASSERT_TRUE(pair.Until(
        [](const auto& state) { return state[0].received[1] == 31U; }));
    EXPECT_EQ(0U, pair.states[0].corrupt[0]);
    EXPECT_EQ(0U, pair.states[0].corrupt[1]);
}
} /* namespace */
extern "C" uint32_t nx_lwip_port_now_ms(void) {
    return NowMs();
}
extern "C" uint32_t nx_lwip_port_random_u32(void) {
    return RandomU32();
}
extern "C" void nx_lwip_port_assert(const char* message) {
    Assert(message);
}
