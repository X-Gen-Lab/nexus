/**
 * \file            tls_front_test.cpp
 * \brief           Bounded TLS client, authentication and ownership contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 * \note            Host fixtures do not qualify MCU entropy or physical I/O.
 */
#include "mbedtls/platform.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/platform_util.h"
#include "nexus/core/fault.h"
#include "nexus/tls/storage.h"
#include "nexus/tls/tls.h"
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern "C" void nx_fault_observe(nx_fault_domain_t domain, nx_result_t reason,
                                 const char* file, unsigned line) {
    (void)file;
    (void)line;
    _exit(domain == NX_FAULT_PLATFORM && reason == NX_ERROR_STATE ? 73 : 74);
}

namespace {
std::vector<uint8_t> fixture(const char* name) {
    std::ifstream input(std::string(NX_TLS_FIXTURE_DIR) + "/" + name,
                        std::ios::binary);
    if (!input) {
        throw std::runtime_error("missing insecure certificate fixture");
    }
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}
struct Peer {
    int descriptor = -1;
    pid_t child = -1;
    bool block_write = false;
    bool block_read = false;
    uint32_t writes = 0U;
    uint32_t reads = 0U;
    nx_time_us_t* monotonic_probe = nullptr;
    mbedtls_ms_time_t expected_ms = -1;
    mbedtls_ms_time_t observed_ms = -1;
    nx_time_us_t* advance_clock = nullptr;
    nx_time_us_t advance_to = 0U;
    nx_tls_session_t* abort_during_write = nullptr;
    nx_result_t callback_abort = NX_SUCCESS;
    size_t callback_written = 17U;
    size_t callback_received = 17U;
    nx_result_t callback_write_result = NX_SUCCESS;
    nx_result_t callback_read_result = NX_SUCCESS;
    nx_byte_stream_t face{};
    static nx_result_t Write(void* context, const uint8_t* data, size_t bytes,
                             size_t* accepted, nx_time_us_t deadline) {
        auto& self = *static_cast<Peer*>(context);
        ++self.writes;
        if (self.monotonic_probe != nullptr) {
            self.expected_ms =
                static_cast<mbedtls_ms_time_t>(*self.monotonic_probe / 1000U);
            self.observed_ms = mbedtls_ms_time();
            self.monotonic_probe = nullptr;
        }
        if (self.advance_clock != nullptr) {
            *self.advance_clock = self.advance_to;
            self.advance_clock = nullptr;
        }
        if (self.abort_during_write != nullptr) {
            uint8_t nested_byte = 0U;
            self.callback_write_result =
                nx_tls_try_write(self.abort_during_write, &nested_byte, 1U,
                                 &self.callback_written, deadline);
            self.callback_read_result =
                nx_tls_try_read(self.abort_during_write, &nested_byte, 1U,
                                &self.callback_received, deadline);
            self.callback_abort = nx_tls_abort(self.abort_during_write);
            self.abort_during_write = nullptr;
        }
        *accepted = 0U;
        if (deadline == NX_DEADLINE_NEVER) {
            return NX_ERROR_INVALID;
        }
        if (self.block_write) {
            return NX_ERROR_BUSY;
        }
        ssize_t count =
            send(self.descriptor, data, std::min<size_t>(bytes, 73U),
                 MSG_DONTWAIT | MSG_NOSIGNAL);
        if (count > 0) {
            *accepted = static_cast<size_t>(count);
            return NX_SUCCESS;
        }
        return count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)
                   ? NX_ERROR_BUSY
                   : NX_ERROR_IO;
    }
    static nx_result_t Read(void* context, uint8_t* data, size_t bytes,
                            size_t* received, nx_time_us_t deadline) {
        auto& self = *static_cast<Peer*>(context);
        ++self.reads;
        *received = 0U;
        if (deadline == NX_DEADLINE_NEVER) {
            return NX_ERROR_INVALID;
        }
        if (self.block_read) {
            return NX_ERROR_BUSY;
        }
        ssize_t count = recv(self.descriptor, data,
                             std::min<size_t>(bytes, 97U), MSG_DONTWAIT);
        if (count >= 0) {
            *received = static_cast<size_t>(count);
            return NX_SUCCESS;
        }
        return errno == EAGAIN || errno == EWOULDBLOCK ? NX_ERROR_BUSY
                                                       : NX_ERROR_IO;
    }
    static const nx_byte_stream_ops_t ops;
    Peer() : face{&ops, this} {
    }
    void Start() {
        int descriptors[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) != 0) {
            throw std::runtime_error("TLS fixture socketpair failed");
        }
        child = fork();
        if (child == 0) {
            close(descriptors[0]);
            char selected[32];
            (void)snprintf(selected, sizeof(selected), "%d", descriptors[1]);
            execl(NX_TLS_TEST_SERVER, NX_TLS_TEST_SERVER, selected,
                  NX_TLS_FIXTURE_DIR "/ca.der",
                  NX_TLS_FIXTURE_DIR "/server.der",
                  NX_TLS_FIXTURE_DIR "/server.key.der", nullptr);
            _exit(91);
        }
        close(descriptors[1]);
        if (child < 0) {
            close(descriptors[0]);
            throw std::runtime_error("TLS fixture fork failed");
        }
        descriptor = descriptors[0];
    }
    int Stop() {
        if (descriptor >= 0) {
            close(descriptor);
            descriptor = -1;
        }
        if (child <= 0) {
            return 0;
        }
        int status = 0;
        for (unsigned attempt = 0U; attempt < 2000U; ++attempt) {
            const auto result = waitpid(child, &status, WNOHANG);
            if (result == child) {
                child = -1;
                return WIFEXITED(status) ? WEXITSTATUS(status) : 92;
            }
            usleep(1000U);
        }
        (void)kill(child, SIGTERM);
        (void)waitpid(child, &status, 0);
        child = -1;
        return 93;
    }
    ~Peer() {
        (void)Stop();
    }
};
const nx_byte_stream_ops_t Peer::ops{Peer::Write, Peer::Read};

class TlsFront : public testing::Test {
  protected:
    alignas(max_align_t) std::array<uint8_t, NX_TLS_ARENA_BYTES> arena{};
    alignas(max_align_t) std::array<uint8_t, NX_TLS_TX_STAGE_BYTES> stage{};
    nx_tls_runtime_t runtime{};
    nx_tls_session_t session{};
    nx_tls_runtime_config_t config{};
    nx_time_us_t now = 1U;
    bool entropy_failure = false;
    bool wall_failure = false;
    uint32_t words = 0U;
    uint32_t source_state = 1U;
    uint32_t wall_reads = 0U;
    uint32_t wall_fail_at = 0U;
    nx_tls_session_t* abort_from_utc = nullptr;
    nx_result_t utc_abort_result = NX_SUCCESS;
    int64_t wall_seconds = INT64_C(1800000000);
    Peer peer;
    std::vector<uint8_t> ca = fixture("ca.der");
    std::vector<uint8_t> identity = fixture("client.der");
    std::vector<uint8_t> key = fixture("client.key.der");
    static nx_time_us_t Clock(void* context) {
        auto& self = *static_cast<TlsFront*>(context);
        return self.now++;
    }
    static nx_result_t Entropy(void* context, uint32_t* word) {
        auto& self = *static_cast<TlsFront*>(context);
        ++self.words;
        if (self.entropy_failure) {
            return NX_ERROR_IO;
        }
        /* Deliberately insecure software fixture, never a product fallback. */
        self.source_state = self.source_state * 1664525U + 1013904223U;
        *word = self.source_state;
        return NX_SUCCESS;
    }
    static nx_result_t Utc(void* context, int64_t* seconds) {
        auto& self = *static_cast<TlsFront*>(context);
        ++self.wall_reads;
        if (self.abort_from_utc != nullptr) {
            self.utc_abort_result = nx_tls_abort(self.abort_from_utc);
            self.abort_from_utc = nullptr;
        }
        if (self.wall_failure ||
            (self.wall_fail_at != 0U && self.wall_reads >= self.wall_fail_at)) {
            return NX_ERROR_STATE;
        }
        *seconds = self.wall_seconds;
        return NX_SUCCESS;
    }
    const nx_entropy_ops_t entropy_ops{Entropy};
    nx_entropy_port_t entropy{&entropy_ops, this};
    void SetUp() override {
        config.arena = arena.data();
        config.arena_bytes = arena.size();
        config.entropy = &entropy;
        config.clock = {Clock, this};
        config.utc = {Utc, this};
        config.entropy_timeout_us = 5000U;
        config.entropy_quality = NX_TLS_ENTROPY_FULL_SOURCE;
    }
    void TearDown() override {
        nx_tls_abort(&session);
        (void)peer.Stop();
        (void)nx_tls_runtime_stop(&runtime);
    }
    nx_tls_config_t Client() {
        nx_tls_config_t selected{};
        selected.runtime = &runtime;
        selected.transport = &peer.face;
        selected.trust_der = ca.data();
        selected.trust_bytes = ca.size();
        selected.client_der = identity.data();
        selected.client_bytes = identity.size();
        selected.key_der = key.data();
        selected.key_bytes = key.size();
        selected.hostname = "server.test";
        selected.hostname_length = 11U;
        selected.tx_stage = stage.data();
        selected.tx_stage_bytes = stage.size();
        return selected;
    }
    nx_result_t Handshake(nx_time_us_t deadline = 1000000U) {
        nx_result_t result = NX_ERROR_BUSY;
        for (unsigned step = 0U; step < 5000U && result == NX_ERROR_BUSY;
             ++step) {
            result = nx_tls_handshake_step(&session, deadline);
            if (result == NX_ERROR_BUSY) {
                usleep(1000U);
            }
        }
        return result;
    }
    void StartClient() {
        ASSERT_EQ(NX_SUCCESS, nx_tls_runtime_start(&runtime, &config, 100000U));
        peer.Start();
        const auto selected = Client();
        ASSERT_EQ(NX_SUCCESS, nx_tls_initialize(&session, &selected, 100000U));
    }
};

TEST_F(TlsFront, MissingEntropyCreditRejectsWithoutSourceAccess) {
    config.entropy_quality = NX_TLS_ENTROPY_UNKNOWN;
    EXPECT_EQ(NX_ERROR_PERMISSION,
              nx_tls_runtime_start(&runtime, &config, 100000U));
    EXPECT_EQ(0U, words);
}
TEST_F(TlsFront, InfiniteEntropyDeadlineRejectsWithoutSourceAccess) {
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_tls_runtime_start(&runtime, &config, NX_DEADLINE_NEVER));
    EXPECT_EQ(0U, words);
}
TEST_F(TlsFront, EntropyFailureHasNoOperatingSystemOrFixedSeedFallback) {
    entropy_failure = true;
    EXPECT_EQ(NX_ERROR_IO, nx_tls_runtime_start(&runtime, &config, 100000U));
    EXPECT_GT(words, 0U);
}
TEST_F(TlsFront, GlobalOwnerCannotBeResetByAnotherRuntime) {
    ASSERT_EQ(NX_SUCCESS, nx_tls_runtime_start(&runtime, &config, 100000U));
    nx_tls_runtime_t another{};
    const uint32_t before = words;
    EXPECT_EQ(NX_ERROR_BUSY, nx_tls_runtime_start(&another, &config, 100000U));
    EXPECT_EQ(before, words);
    EXPECT_EQ(NX_SUCCESS, nx_tls_runtime_stop(&runtime));
    for (uint8_t value : arena) {
        EXPECT_EQ(0U, value);
    }
}

TEST_F(TlsFront, MutualHandshakeWantAndCopiedWriteDoNotBorrowMqttInput) {
    StartClient();
    peer.block_write = true;
    nx_tls_diagnostic_t diagnostic{};
    for (unsigned step = 0U; step < 10U; ++step) {
        ASSERT_EQ(NX_ERROR_BUSY, nx_tls_handshake_step(&session, 1000000U));
        ASSERT_EQ(NX_SUCCESS, nx_tls_diagnostic(&session, &diagnostic));
        if (diagnostic.want_write) {
            break;
        }
    }
    ASSERT_TRUE(diagnostic.want_write);
    peer.block_write = false;
    ASSERT_EQ(NX_SUCCESS, Handshake());
    EXPECT_EQ(NX_ERROR_BUSY, nx_tls_runtime_stop(&runtime));
    std::array<uint8_t, 512> input{};
    for (size_t i = 0U; i < input.size(); ++i) {
        input[i] = static_cast<uint8_t>(i % 251U);
    }
    const auto expected = input;
    peer.block_write = true;
    size_t accepted = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_tls_try_write(&session, input.data(), input.size(),
                                           &accepted, 1000000U));
    ASSERT_EQ(input.size(), accepted);
    input.fill(0xa5U);
    EXPECT_EQ(NX_ERROR_BUSY, nx_tls_service(&session));
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_tls_try_write(&session, input.data(), input.size(), &accepted,
                               1000000U));
    EXPECT_EQ(0U, accepted);
    EXPECT_EQ(expected, stage);
    peer.block_write = false;
    nx_result_t result = NX_ERROR_BUSY;
    for (unsigned step = 0U; step < 5000U && result == NX_ERROR_BUSY; ++step) {
        result = nx_tls_service(&session);
    }
    ASSERT_EQ(NX_SUCCESS, result);
    std::array<uint8_t, 512> output{};
    size_t received = 0U;
    for (unsigned step = 0U; step < 5000U && received == 0U; ++step) {
        result = nx_tls_try_read(&session, output.data(), output.size(),
                                 &received, 1000000U);
        ASSERT_TRUE(result == NX_SUCCESS || result == NX_ERROR_BUSY);
        if (result == NX_ERROR_BUSY) {
            usleep(1000U);
        }
    }
    ASSERT_EQ(output.size(), received);
    EXPECT_EQ(expected, output);
    result = NX_ERROR_BUSY;
    for (unsigned step = 0U; step < 5000U && result == NX_ERROR_BUSY; ++step) {
        result = nx_tls_close_step(&session, 1000000U);
    }
    ASSERT_EQ(NX_SUCCESS, result);
    for (unsigned step = 0U; step < 5000U; ++step) {
        result = nx_tls_try_read(&session, output.data(), output.size(),
                                 &received, 1000000U);
        if (result == NX_SUCCESS && received == 0U) {
            break;
        }
        ASSERT_EQ(NX_ERROR_BUSY, result);
        usleep(1000U);
    }
    EXPECT_EQ(NX_SUCCESS, result);
    EXPECT_EQ(0U, received);
    nx_tls_abort(&session);
    EXPECT_EQ(0, peer.Stop());
}
TEST_F(TlsFront, WrongHostnameCannotAdmitApplicationPlaintext) {
    ASSERT_EQ(NX_SUCCESS, nx_tls_runtime_start(&runtime, &config, 100000U));
    peer.Start();
    auto selected = Client();
    selected.hostname = "wrong.test";
    selected.hostname_length = 10U;
    ASSERT_EQ(NX_SUCCESS, nx_tls_initialize(&session, &selected, 100000U));
    EXPECT_EQ(NX_ERROR_PERMISSION, Handshake());
    size_t accepted = 17U;
    const uint8_t secret = 1U;
    EXPECT_EQ(NX_ERROR_STATE,
              nx_tls_try_write(&session, &secret, 1U, &accepted, 1000000U));
    EXPECT_EQ(0U, accepted);
}
TEST_F(TlsFront, ExpiredPeerCertificateIsRejectedUsingInjectedUtc) {
    StartClient();
    wall_seconds = INT64_C(1900000000);
    EXPECT_EQ(NX_ERROR_PERMISSION, Handshake());
}
TEST_F(TlsFront, UntrustedCaCannotAuthenticateAValidPeerCertificate) {
    ca = fixture("wrong-ca.der");
    StartClient();
    EXPECT_EQ(NX_ERROR_PERMISSION, Handshake());
}
TEST_F(TlsFront, UtcLossDuringAProgressStepOverridesLibrarySuccess) {
    StartClient();
    wall_fail_at = wall_reads + 2U;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_tls_handshake_step(&session, 1000000U));
    nx_tls_diagnostic_t diagnostic{};
    ASSERT_EQ(NX_SUCCESS, nx_tls_diagnostic(&session, &diagnostic));
    EXPECT_EQ(NX_TLS_FAILED, diagnostic.state);
    EXPECT_EQ(NX_ERROR_PERMISSION, diagnostic.result);
}
TEST_F(TlsFront, EntropyFailureBeforeHandshakeReseedIsFatal) {
    StartClient();
    entropy_failure = true;
    EXPECT_EQ(NX_ERROR_IO, nx_tls_handshake_step(&session, 1000000U));
    nx_tls_diagnostic_t diagnostic{};
    ASSERT_EQ(NX_SUCCESS, nx_tls_diagnostic(&session, &diagnostic));
    EXPECT_EQ(NX_TLS_FAILED, diagnostic.state);
}
TEST_F(TlsFront, HandshakeWantCannotExtendItsInitialAbsoluteDeadline) {
    StartClient();
    peer.block_write = true;
    const nx_time_us_t first = now + 100U;
    ASSERT_EQ(NX_ERROR_BUSY, nx_tls_handshake_step(&session, first));
    now = first;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_tls_handshake_step(&session, first + 100000U));
}
TEST_F(TlsFront, InvalidOutputAliasCannotOverwritePendingCiphertextOwnership) {
    StartClient();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    const std::array<uint8_t, 64> input{};
    size_t accepted = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_tls_try_write(&session, input.data(), input.size(),
                                           &accepted, 1000000U));
    ASSERT_EQ(input.size(), session.stage_bytes);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_tls_try_write(&session, input.data(), input.size(),
                               &session.stage_bytes, 1000000U));
    EXPECT_EQ(input.size(), session.stage_bytes);
    std::array<uint8_t, 64> output{};
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_tls_try_read(&session, output.data(), output.size(),
                              &session.stage_bytes, 1000000U));
    EXPECT_EQ(input.size(), session.stage_bytes);
}
TEST_F(TlsFront, ReentrantAbortCannotFreeAnExecutingLibraryFrame) {
    StartClient();
    peer.abort_during_write = &session;
    ASSERT_EQ(NX_SUCCESS, Handshake());
    EXPECT_EQ(NX_ERROR_BUSY, peer.callback_abort);
    EXPECT_EQ(NX_ERROR_BUSY, peer.callback_write_result);
    EXPECT_EQ(NX_ERROR_BUSY, peer.callback_read_result);
    EXPECT_EQ(0U, peer.callback_written);
    EXPECT_EQ(0U, peer.callback_received);
    EXPECT_TRUE(session.initialized);
    EXPECT_EQ(NX_ERROR_BUSY, nx_tls_runtime_stop(&runtime));
    EXPECT_EQ(NX_SUCCESS, nx_tls_abort(&session));
    EXPECT_FALSE(session.initialized);
    EXPECT_EQ(NX_SUCCESS, nx_tls_abort(&session));
}
TEST_F(TlsFront, ArenaCannotContainItsRuntimeConfigurationOrEntropyFace) {
    auto* selected = new (arena.data()) nx_tls_runtime_config_t(config);
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_tls_runtime_start(&runtime, selected, 100000U));
    EXPECT_EQ(0U, words);
}
TEST_F(TlsFront, CertificateCannotAliasSessionStorageClearedDuringInitialize) {
    ASSERT_EQ(NX_SUCCESS, nx_tls_runtime_start(&runtime, &config, 100000U));
    auto selected = Client();
    std::memcpy(&session, ca.data(), ca.size());
    selected.trust_der = reinterpret_cast<const uint8_t*>(&session);
    std::array<uint8_t, sizeof(session)> before{};
    std::memcpy(before.data(), &session, before.size());
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_tls_initialize(&session, &selected, 100000U));
    EXPECT_EQ(0, std::memcmp(before.data(), &session, before.size()));
    std::memset(&session, 0, sizeof(session));
}
TEST_F(TlsFront, DriverNeverCreditsOrLeaksBytesOutsideFiniteCryptoOperation) {
    std::array<uint8_t, 256> output{};
    output.fill(0xa5U);
    size_t credit = 17U;
    EXPECT_EQ(PSA_ERROR_BAD_STATE, mbedtls_platform_get_entropy(
                                       PSA_DRIVER_GET_ENTROPY_FLAGS_NONE,
                                       &credit, output.data(), output.size()));
    EXPECT_EQ(0U, credit);
    EXPECT_EQ((std::array<uint8_t, 256>{}), output);
    output.fill(0xa5U);
    credit = 17U;
    EXPECT_EQ(PSA_ERROR_NOT_SUPPORTED,
              mbedtls_platform_get_entropy(1U, &credit, output.data(),
                                           output.size()));
    EXPECT_EQ(0U, credit);
    EXPECT_EQ((std::array<uint8_t, 256>{}), output);
    EXPECT_EQ(0U, words);
}
TEST_F(TlsFront, DriverSizeOverflowCannotCreditOrTouchAnUnknownSpan) {
    std::array<uint8_t, 8> output{};
    output.fill(0xa5U);
    const auto before = output;
    size_t credit = 17U;
    EXPECT_EQ(PSA_ERROR_INVALID_ARGUMENT,
              mbedtls_platform_get_entropy(PSA_DRIVER_GET_ENTROPY_FLAGS_NONE,
                                           &credit, output.data(),
                                           SIZE_MAX / 8U + 1U));
    EXPECT_EQ(0U, credit);
    EXPECT_EQ(before, output);
    EXPECT_EQ(0U, words);
}
TEST_F(TlsFront, ExpiredQueuedWriteRetainsStageUntilExplicitAbort) {
    StartClient();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    std::array<uint8_t, 700> input{};
    input.fill(0xa5U);
    peer.block_write = true;
    const auto deadline = now + 100U;
    size_t accepted = 0U;
    ASSERT_EQ(NX_SUCCESS, nx_tls_try_write(&session, input.data(), input.size(),
                                           &accepted, deadline));
    EXPECT_EQ(NX_TLS_TX_STAGE_BYTES, accepted);
    EXPECT_EQ(NX_ERROR_BUSY, nx_tls_service(&session));
    now = deadline;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_tls_service(&session));
    EXPECT_EQ(NX_TLS_TX_STAGE_BYTES, session.stage_bytes);
    EXPECT_EQ(0xa5U, stage.front());
    EXPECT_EQ(NX_ERROR_BUSY, nx_tls_runtime_stop(&runtime));
    ASSERT_EQ(NX_SUCCESS, nx_tls_abort(&session));
    EXPECT_EQ((std::array<uint8_t, NX_TLS_TX_STAGE_BYTES>{}), stage);
}
TEST_F(TlsFront, RawTransportEofCannotBecomeAuthenticatedTlsEof) {
    StartClient();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    ASSERT_EQ(0, kill(peer.child, SIGTERM));
    int status = 0;
    ASSERT_EQ(peer.child, waitpid(peer.child, &status, 0));
    peer.child = -1;
    std::array<uint8_t, 32> output{};
    size_t received = 17U;
    EXPECT_EQ(NX_ERROR_IO, nx_tls_try_read(&session, output.data(),
                                           output.size(), &received, 1000000U));
    EXPECT_EQ(0U, received);
    EXPECT_FALSE(session.diagnostic.peer_closed);
    EXPECT_EQ(NX_TLS_FAILED, session.diagnostic.state);
}
TEST_F(TlsFront, SequentialClientsReuseOnePsaDomainWithoutArenaReset) {
    StartClient();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    EXPECT_EQ(NX_SUCCESS, nx_tls_abort(&session));
    (void)peer.Stop();
    EXPECT_TRUE(runtime.initialized);
    EXPECT_FALSE(runtime.session_active);
    const auto previous_words = words;
    peer.Start();
    auto selected = Client();
    ASSERT_EQ(NX_SUCCESS, nx_tls_initialize(&session, &selected, 1000000U));
    ASSERT_EQ(NX_SUCCESS, Handshake());
    EXPECT_GT(words, previous_words);
    EXPECT_TRUE(runtime.initialized);
}
TEST_F(TlsFront, TrustedClockCallbackCannotDestroyPendingHandshakeState) {
    StartClient();
    abort_from_utc = &session;
    EXPECT_EQ(NX_SUCCESS, Handshake());
    EXPECT_EQ(NX_ERROR_BUSY, utc_abort_result);
    EXPECT_TRUE(session.initialized);
}
TEST_F(TlsFront, DiagnosticCannotOverwriteAnyKnownLiveStorage) {
    struct alignas(max_align_t) PaddedTransportOps {
        nx_byte_stream_ops_t value;
        std::array<uint8_t, sizeof(nx_tls_diagnostic_t)> padding;
    } transport_ops{{Peer::Write, Peer::Read}, {}};
    struct alignas(max_align_t) PaddedTransport {
        nx_byte_stream_t value;
        std::array<uint8_t, sizeof(nx_tls_diagnostic_t)> padding;
    } transport{{&transport_ops.value, &peer}, {}};
    struct alignas(max_align_t) PaddedEntropyOps {
        nx_entropy_ops_t value;
        std::array<uint8_t, sizeof(nx_tls_diagnostic_t)> padding;
    } source_ops{{Entropy}, {}};
    struct alignas(max_align_t) PaddedEntropy {
        nx_entropy_port_t value;
        std::array<uint8_t, sizeof(nx_tls_diagnostic_t)> padding;
    } source{{&source_ops.value, this}, {}};
    config.entropy = &source.value;
    ASSERT_EQ(NX_SUCCESS, nx_tls_runtime_start(&runtime, &config, 100000U));
    peer.Start();
    auto selected = Client();
    selected.transport = &transport.value;
    ASSERT_EQ(NX_SUCCESS, nx_tls_initialize(&session, &selected, 100000U));
    const std::array<void*, 7> aliases{&runtime,
                                       arena.data() + arena.size() - 512U,
                                       stage.data(),
                                       &transport.value,
                                       &transport_ops.value,
                                       &source.value,
                                       &source_ops.value};
    for (void* alias : aliases) {
        std::array<uint8_t, sizeof(nx_tls_diagnostic_t)> before{};
        std::memcpy(before.data(), alias, before.size());
        EXPECT_EQ(NX_ERROR_INVALID,
                  nx_tls_diagnostic(&session,
                                    static_cast<nx_tls_diagnostic_t*>(alias)));
        EXPECT_EQ(0, std::memcmp(before.data(), alias, before.size()));
        /* Keep failing baseline cleanup safe after observing the corruption. */
        std::memcpy(alias, before.data(), before.size());
    }
    nx_tls_diagnostic_t diagnostic{};
    ASSERT_EQ(NX_SUCCESS, nx_tls_diagnostic(&session, &diagnostic));
    EXPECT_EQ(NX_TLS_INITIALIZED, diagnostic.state);
    EXPECT_TRUE(runtime.initialized);
    EXPECT_TRUE(session.initialized);
    EXPECT_EQ(NX_SUCCESS, nx_tls_abort(&session));
    EXPECT_EQ(NX_SUCCESS, nx_tls_runtime_stop(&runtime));
}
TEST_F(TlsFront, ShorterWriteAttemptCannotSpendOlderStageDeadline) {
    StartClient();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    const uint8_t input = 0xa5U;
    size_t accepted = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_tls_try_write(&session, &input, 1U, &accepted, 1000000U));
    const auto near = now + 100U;
    peer.advance_clock = &now;
    peer.advance_to = near + 1U;
    const auto writes = peer.writes;
    accepted = 17U;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_tls_try_write(&session, &input, 1U, &accepted, near));
    EXPECT_EQ(0U, accepted);
    EXPECT_EQ(writes, peer.writes);
    EXPECT_LT(now, near);
    EXPECT_EQ(1U, session.stage_bytes);
    EXPECT_EQ(input, stage.front());
}
TEST_F(TlsFront, ShorterReadAttemptCannotSpendOlderStageDeadline) {
    StartClient();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    uint8_t input = 0xa5U;
    size_t accepted = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_tls_try_write(&session, &input, 1U, &accepted, 1000000U));
    const auto near = now + 100U;
    peer.advance_clock = &now;
    peer.advance_to = near + 1U;
    const auto writes = peer.writes;
    const auto reads = peer.reads;
    size_t received = 17U;
    EXPECT_EQ(NX_ERROR_BUSY,
              nx_tls_try_read(&session, &input, 1U, &received, near));
    EXPECT_EQ(0U, received);
    EXPECT_EQ(writes, peer.writes);
    EXPECT_EQ(reads, peer.reads);
    EXPECT_EQ(1U, session.stage_bytes);
}
TEST_F(TlsFront, NewWriteRechecksDeadlineAfterSuccessfulOlderDrain) {
    StartClient();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    const uint8_t input = 0xa5U;
    size_t accepted = 0U;
    const auto deadline = now + 1000U;
    ASSERT_EQ(NX_SUCCESS,
              nx_tls_try_write(&session, &input, 1U, &accepted, deadline));
    now = deadline - 3U;
    EXPECT_EQ(NX_ERROR_TIMEOUT,
              nx_tls_try_write(&session, &input, 1U, &accepted, deadline));
    EXPECT_EQ(0U, accepted);
    EXPECT_EQ(0U, session.stage_bytes);
    EXPECT_EQ(NX_TLS_ESTABLISHED, session.diagnostic.state);
}
TEST_F(TlsFront, ReadProgressUsesCloseCappedEffectiveStageDeadline) {
    StartClient();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    uint8_t input = 0xa5U;
    size_t accepted = 0U;
    ASSERT_EQ(NX_SUCCESS,
              nx_tls_try_write(&session, &input, 1U, &accepted, 1000000U));
    const auto closing = now + 10000U;
    peer.block_write = true;
    ASSERT_EQ(NX_ERROR_BUSY, nx_tls_close_step(&session, closing));
    const auto writes = peer.writes;
    peer.block_write = false;
    size_t received = 0U;
    const auto result =
        nx_tls_try_read(&session, &input, 1U, &received, closing);
    EXPECT_TRUE(result == NX_SUCCESS || result == NX_ERROR_BUSY);
    EXPECT_GT(peer.writes, writes);
    EXPECT_EQ(0U, session.stage_bytes);
}
TEST_F(TlsFront, ZeroByteFaceCallsDoNotReadClockOrSpendAnExpiredDeadline) {
    StartClient();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    const auto before = now;
    const auto writes = peer.writes;
    const auto reads = peer.reads;
    size_t count = 17U;
    ASSERT_GT(before, 1U);
    EXPECT_EQ(NX_SUCCESS,
              nx_tls_try_write(&session, nullptr, 0U, &count, before - 1U));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(NX_SUCCESS,
              nx_tls_try_read(&session, nullptr, 0U, &count, before - 1U));
    EXPECT_EQ(0U, count);
    EXPECT_EQ(before, now);
    EXPECT_EQ(writes, peer.writes);
    EXPECT_EQ(reads, peer.reads);
}
TEST_F(TlsFront, PlatformMillisecondsUseTheOwnedMonotonicClock) {
    StartClient();
    now = 1234000U;
    peer.monotonic_probe = &now;
    ASSERT_EQ(NX_SUCCESS, Handshake(10000000U));
    EXPECT_GT(peer.expected_ms, 0);
    EXPECT_EQ(peer.expected_ms, peer.observed_ms);
}
TEST_F(TlsFront,
       AllocatorInvariantFailureUsesPlatformObservationBeforeParking) {
    ASSERT_EQ(NX_SUCCESS, nx_tls_runtime_start(&runtime, &config, 100000U));
    EXPECT_EXIT(
        {
            mbedtls_exit(1);
            _exit(99);
        },
        testing::ExitedWithCode(73), "");
}
TEST_F(TlsFront, UnrepresentableCertificateCalendarRejectsBeforeReseed) {
    StartClient();
    wall_seconds = INT64_C(253402300800);
    const auto before = words;
    EXPECT_EQ(NX_ERROR_PERMISSION, nx_tls_handshake_step(&session, 1000000U));
    EXPECT_EQ(before, words);
}
TEST(TlsUtcConversion, ReentrantUtcHandlesEpochAndGregorianCenturyRules) {
    struct tm result {};
    mbedtls_time_t value = 0;
    ASSERT_EQ(&result, mbedtls_platform_gmtime_r(&value, &result));
    EXPECT_EQ(70, result.tm_year);
    EXPECT_EQ(0, result.tm_mon);
    EXPECT_EQ(1, result.tm_mday);
    value = static_cast<mbedtls_time_t>(INT64_C(951782400));
    ASSERT_EQ(&result, mbedtls_platform_gmtime_r(&value, &result));
    EXPECT_EQ(100, result.tm_year);
    EXPECT_EQ(1, result.tm_mon);
    EXPECT_EQ(29, result.tm_mday);
    value = static_cast<mbedtls_time_t>(INT64_C(4107542400));
    ASSERT_EQ(&result, mbedtls_platform_gmtime_r(&value, &result));
    EXPECT_EQ(200, result.tm_year);
    EXPECT_EQ(2, result.tm_mon);
    EXPECT_EQ(1, result.tm_mday);
}

/** \brief Finite official-arena pressure, released before any domain teardown.
 */
struct ArenaPressure {
    static constexpr size_t block_bytes = 64U;
    std::array<void*, NX_TLS_ARENA_BYTES / block_bytes> blocks{};
    size_t count = 0U;

    ArenaPressure() = default;
    ArenaPressure(const ArenaPressure&) = delete;
    ArenaPressure& operator=(const ArenaPressure&) = delete;

    void Release() {
        while (count != 0U) {
            mbedtls_free(blocks[--count]);
            blocks[count] = nullptr;
        }
    }
    ~ArenaPressure() {
        Release();
    }
};

TEST_F(TlsFront,
       ArenaExhaustionRejectsWithoutIoAndRecoversAfterExplicitRelease) {
    ASSERT_EQ(NX_SUCCESS, nx_tls_runtime_start(&runtime, &config, 100000U));
    ArenaPressure pressure;
    bool exhausted = false;
    const auto first = reinterpret_cast<uintptr_t>(arena.data());
    const auto end = first + arena.size();
    for (; pressure.count < pressure.blocks.size(); ++pressure.count) {
        void* block = mbedtls_calloc(1U, ArenaPressure::block_bytes);
        if (block == nullptr) {
            exhausted = true;
            break;
        }
        pressure.blocks[pressure.count] = block;
        const auto address = reinterpret_cast<uintptr_t>(block);
        EXPECT_GE(address, first);
        EXPECT_LE(address, end - ArenaPressure::block_bytes);
        const auto* bytes = static_cast<const uint8_t*>(block);
        for (size_t index = 0U; index < ArenaPressure::block_bytes; ++index) {
            EXPECT_EQ(0U, bytes[index]);
        }
    }
    ASSERT_TRUE(exhausted);
    ASSERT_GT(pressure.count, 0U);
    void* refused = mbedtls_calloc(1U, ArenaPressure::block_bytes);
    EXPECT_EQ(nullptr, refused);
    mbedtls_free(refused);
    RecordProperty("arena_pressure_blocks", static_cast<int>(pressure.count));
    RecordProperty(
        "arena_pressure_payload_bytes",
        static_cast<int>(pressure.count * ArenaPressure::block_bytes));
    ASSERT_GT(ca.size(), ArenaPressure::block_bytes);
    const auto selected = Client();
    EXPECT_EQ(NX_ERROR_IO, nx_tls_initialize(&session, &selected, 100000U));
    EXPECT_EQ(0U, peer.writes);
    EXPECT_EQ(0U, peer.reads);
    EXPECT_EQ(nullptr, nx_tls_stream(&session));
    EXPECT_FALSE(session.initialized);
    EXPECT_FALSE(runtime.session_active);
    EXPECT_TRUE(runtime.initialized);
    nx_tls_diagnostic_t diagnostic{};
    ASSERT_EQ(NX_SUCCESS, nx_tls_diagnostic(&session, &diagnostic));
    EXPECT_EQ(NX_TLS_CLOSED, diagnostic.state);
    EXPECT_EQ(0U, diagnostic.pending_plaintext);
    EXPECT_EQ(NX_SUCCESS, nx_tls_abort(&session));

    /* Returning only explicit pressure allocations restores admission. */
    pressure.Release();
    ASSERT_EQ(NX_SUCCESS, nx_tls_initialize(&session, &selected, 100000U));
    peer.Start();
    ASSERT_EQ(NX_SUCCESS, Handshake());
    nx_result_t closing = NX_ERROR_BUSY;
    for (unsigned step = 0U; step < 5000U && closing == NX_ERROR_BUSY; ++step) {
        closing = nx_tls_close_step(&session, 1000000U);
    }
    ASSERT_EQ(NX_SUCCESS, closing);
    uint8_t output = 0U;
    size_t received = 1U;
    nx_result_t finished = NX_ERROR_BUSY;
    for (unsigned step = 0U; step < 5000U && finished == NX_ERROR_BUSY;
         ++step) {
        finished = nx_tls_try_read(&session, &output, 1U, &received, 1000000U);
        if (finished == NX_ERROR_BUSY) {
            usleep(1000U);
        }
    }
    EXPECT_EQ(NX_SUCCESS, finished);
    EXPECT_EQ(0U, received);
    EXPECT_EQ(NX_SUCCESS, nx_tls_abort(&session));
    EXPECT_EQ(0, peer.Stop());
    ASSERT_EQ(NX_SUCCESS, nx_tls_runtime_stop(&runtime));
    for (uint8_t byte : arena) {
        EXPECT_EQ(0U, byte);
    }
}
} /* namespace */
