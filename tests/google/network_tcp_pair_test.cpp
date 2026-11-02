/**
 * \file            network_tcp_pair_test.cpp
 * \brief           Two real lwIP processes over bounded Ethernet frame IPC
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            Unqualified host fixture; no PHY, entropy or timing proof.
 */
#include "support/lwip_frame_pair.h"
extern "C" {
#include "lwip/priv/tcp_priv.h"
#include "lwip/tcp.h"
}
#include <algorithm>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>

namespace {
using nexus::test::lwip_pair::FrameEndpoint;
using nexus::test::lwip_pair::ProcessPair;
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
uint8_t pattern(uint32_t index, uint32_t role) {
    return static_cast<uint8_t>((index * 7U + role * 41U) % 251U);
}

struct Worker : FrameEndpoint {
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
    uint32_t errors = 0U;
    uint32_t corrupt_payload = 0U;
    err_t last_error = ERR_OK;
    bool connected = false;
    bool peer_fin = false;
    bool hold_credit = false;
    bool write_closed = false;
    bool auto_close_on_fin = false;
    bool application_released = false;
    err_t command_result = ERR_OK;

    explicit Worker(uint32_t identity, int socket)
        : FrameEndpoint(identity, socket) {
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
    void Execute(Command command) {
        command_result = ERR_OK;
        switch (command.action) {
            case Action::Tick:
                nexus::test::lwip_pair::SetNowMs(command.value);
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
        (void)Service(static_cast<FrameEndpoint*>(this));
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
    static bool IsExit(const Command& command) {
        return command.action == Action::Exit;
    }
    static bool ExitSucceeded(const Snapshot& state) {
        return state.command_result == ERR_OK;
    }
    static void ValidateExit(const Snapshot& state) {
        EXPECT_EQ(ERR_OK, state.command_result);
        EXPECT_FALSE(state.transmit_borrowed);
        EXPECT_FALSE(state.receive_held);
    }
};

using RawProcessPair = ProcessPair<Worker, Command, Snapshot>;
class Pair : public RawProcessPair {
  public:
    Pair() : RawProcessPair({Action::Exit, 0U}) {
    }
    Snapshot Call(size_t role, Action action, uint32_t value = 0U) {
        return RawProcessPair::Call(role, {action, value});
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
    uint32_t now = 0U;
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
    return nexus::test::lwip_pair::NowMs();
}
extern "C" uint32_t nx_lwip_port_random_u32(void) {
    return nexus::test::lwip_pair::RandomU32();
}
extern "C" void nx_lwip_port_assert(const char* message) {
    nexus::test::lwip_pair::Assert(message);
}
