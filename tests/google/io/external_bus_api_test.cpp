/**
 * \file            external_bus_api_test.cpp
 * \brief           Typed bus face dispatch, prefix and loan rejection tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/external_bus.h"
#include "nexus/io/memory_window.h"
#include "nexus/io/parallel16.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

/** \brief Mock external methods, never the checked dispatch implementation. */
class Provider {
  public:
    MOCK_METHOD(nx_external_bus_state_t, State, ());
    MOCK_METHOD(nx_result_t, Stop, ());
    MOCK_METHOD(nx_result_t, Command, (uint16_t, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Write,
                (const uint16_t*, size_t, size_t*, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Read, (uint16_t*, size_t, size_t*, nx_time_us_t));
    MOCK_METHOD(nx_result_t, Acquire, (nx_memory_window_lease_t*));
    MOCK_METHOD(nx_result_t, Release, (nx_memory_window_lease_t*, uint64_t));
    MOCK_METHOD(nx_result_t, Geometry, (nx_memory_window_geometry_t*));
    static nx_external_bus_state_t state(const void* c) {
        return const_cast<Provider*>(static_cast<const Provider*>(c))->State();
    }
    static nx_result_t stop(void* c) {
        return static_cast<Provider*>(c)->Stop();
    }
    static nx_result_t command(void* c, uint16_t v, nx_time_us_t d) {
        return static_cast<Provider*>(c)->Command(v, d);
    }
    static nx_result_t write(void* c, const uint16_t* w, size_t n, size_t* t,
                             nx_time_us_t d) {
        return static_cast<Provider*>(c)->Write(w, n, t, d);
    }
    static nx_result_t read(void* c, uint16_t* w, size_t n, size_t* t,
                            nx_time_us_t d) {
        return static_cast<Provider*>(c)->Read(w, n, t, d);
    }
    static nx_result_t acquire(void* c, nx_memory_window_lease_t* l) {
        return static_cast<Provider*>(c)->Acquire(l);
    }
    static nx_result_t release(void* c, nx_memory_window_lease_t* l,
                               uint64_t e) {
        return static_cast<Provider*>(c)->Release(l, e);
    }
    static nx_result_t geometry(const void* c, nx_memory_window_geometry_t* g) {
        return const_cast<Provider*>(static_cast<const Provider*>(c))
            ->Geometry(g);
    }
    static constexpr nx_external_bus_ops_t bus_ops{state, stop};
    static constexpr nx_parallel16_ops_t parallel_ops{command, write, read};
    static constexpr nx_memory_window_ops_t memory_ops{acquire, release,
                                                       geometry};
};

TEST(ExternalBusAPI, NullFacesNeverDispatchAndKeepLease) {
    nx_memory_window_lease_t lease{};
    EXPECT_EQ(nx_external_bus_port_state(nullptr), NX_EXTERNAL_BUS_UNAVAILABLE);
    EXPECT_EQ(nx_external_bus_port_stop(nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_memory_window_port_acquire(nullptr, &lease), NX_ERROR_INVALID);
    EXPECT_EQ(lease.owner, nullptr);
    size_t transferred = 91U;
    uint16_t word{};
    EXPECT_EQ(
        nx_parallel16_endpoint_read(nullptr, &word, 1U, &transferred, 50U),
        NX_ERROR_INVALID);
    EXPECT_EQ(transferred, 91U);
}

TEST(ExternalBusAPI, OptionalMethodsRemainUnsupported) {
    StrictMock<Provider> provider;
    const nx_external_bus_ops_t bo{};
    const nx_memory_window_ops_t mo{};
    const nx_parallel16_ops_t po{};
    const nx_external_bus_port_t bus{&bo, &provider};
    const nx_memory_window_port_t memory{&mo, &provider};
    const nx_parallel16_endpoint_t parallel{&po, &provider};
    nx_memory_window_lease_t lease{};
    uint16_t word{};
    size_t transferred = 92U;
    EXPECT_EQ(nx_external_bus_port_state(&bus), NX_EXTERNAL_BUS_UNAVAILABLE);
    EXPECT_EQ(nx_external_bus_port_stop(&bus), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_memory_window_port_acquire(&memory, &lease),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_memory_window_port_release(&memory, &lease, 1U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_parallel16_endpoint_command(&parallel, 0U, 50U),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(
        nx_parallel16_endpoint_write(&parallel, &word, 1U, &transferred, 50U),
        NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(transferred, 92U);
}

TEST(ExternalBusAPI, ControllerDispatchUsesExactContextAndPreservesFailure) {
    StrictMock<Provider> first;
    StrictMock<Provider> second;
    const nx_external_bus_port_t a{&Provider::bus_ops, &first};
    const nx_external_bus_port_t b{&Provider::bus_ops, &second};
    EXPECT_CALL(first, State()).WillOnce(Return(NX_EXTERNAL_BUS_PREPARED));
    EXPECT_CALL(second, Stop()).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_external_bus_port_state(&a), NX_EXTERNAL_BUS_PREPARED);
    EXPECT_EQ(nx_external_bus_port_stop(&b), NX_ERROR_BUSY);
}

TEST(ExternalBusAPI, ParallelDispatchPreservesExactWordsAndDeadline) {
    StrictMock<Provider> provider;
    const nx_parallel16_endpoint_t face{&Provider::parallel_ops, &provider};
    const uint16_t words[]{0x1234U, 0xabcdU};
    size_t transferred = 99U;
    EXPECT_CALL(provider, Command(0x1234U, 700U)).WillOnce(Return(NX_SUCCESS));
    EXPECT_CALL(provider, Write(words, 2U, _, 700U))
        .WillOnce(
            Invoke([](const uint16_t*, size_t n, size_t* t, nx_time_us_t) {
                *t = n;
                return NX_SUCCESS;
            }));
    EXPECT_EQ(nx_parallel16_endpoint_command(&face, 0x1234U, 700U), NX_SUCCESS);
    EXPECT_EQ(
        nx_parallel16_endpoint_write(&face, words, 2U, &transferred, 700U),
        NX_SUCCESS);
    EXPECT_EQ(transferred, 2U);
}

TEST(ExternalBusAPI, InvalidSpansAndControlAliasesDoNotDispatch) {
    StrictMock<Provider> provider;
    const nx_parallel16_endpoint_t face{&Provider::parallel_ops, &provider};
    uint16_t words[4]{};
    size_t transferred = 90U;
    EXPECT_EQ(
        nx_parallel16_endpoint_write(&face, words, 0U, &transferred, 700U),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_parallel16_endpoint_write(&face, words, 1U, &transferred,
                                           NX_DEADLINE_NEVER),
              NX_ERROR_INVALID);
    EXPECT_EQ(
        nx_parallel16_endpoint_read(&face, words, SIZE_MAX, &transferred, 700U),
        NX_ERROR_INVALID);
    EXPECT_EQ(nx_parallel16_endpoint_read(
                  &face, reinterpret_cast<uint16_t*>(&transferred), 1U,
                  &transferred, 700U),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_parallel16_endpoint_write(
                  &face, reinterpret_cast<const uint16_t*>(&face), 1U,
                  &transferred, 700U),
              NX_ERROR_INVALID);
    EXPECT_EQ(transferred, 90U);
}

TEST(ExternalBusAPI, SuccessRequiresCompletePrefixAndFailureRetainsPrefix) {
    StrictMock<Provider> provider;
    const nx_parallel16_endpoint_t face{&Provider::parallel_ops, &provider};
    uint16_t words[2]{};
    size_t transferred = 90U;
    EXPECT_CALL(provider, Read(words, 2U, _, 700U))
        .WillOnce(Invoke([](uint16_t*, size_t, size_t* t, nx_time_us_t) {
            *t = 1U;
            return NX_SUCCESS;
        }))
        .WillOnce(Invoke([](uint16_t*, size_t, size_t* t, nx_time_us_t) {
            *t = 1U;
            return NX_ERROR_TIMEOUT;
        }))
        .WillOnce(Invoke([](uint16_t*, size_t, size_t* t, nx_time_us_t) {
            *t = 3U;
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_parallel16_endpoint_read(&face, words, 2U, &transferred, 700U),
              NX_ERROR_IO);
    EXPECT_EQ(transferred, 1U);
    EXPECT_EQ(nx_parallel16_endpoint_read(&face, words, 2U, &transferred, 700U),
              NX_ERROR_TIMEOUT);
    EXPECT_EQ(transferred, 1U);
    EXPECT_EQ(nx_parallel16_endpoint_read(&face, words, 2U, &transferred, 700U),
              NX_ERROR_IO);
    EXPECT_EQ(transferred, 0U);
}

TEST(ExternalBusAPI, MemoryMethodsReceiveOriginalStorageAndEpoch) {
    StrictMock<Provider> provider;
    const nx_memory_window_port_t face{&Provider::memory_ops, &provider};
    nx_memory_window_lease_t lease{};
    EXPECT_CALL(provider, Acquire(&lease)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_CALL(provider, Release(&lease, 17U))
        .WillOnce(Return(NX_ERROR_STATE));
    EXPECT_EQ(nx_memory_window_port_acquire(&face, &lease), NX_ERROR_BUSY);
    EXPECT_EQ(nx_memory_window_port_release(&face, &lease, 17U),
              NX_ERROR_STATE);
}

TEST(ExternalBusAPI, MemoryLeaseMayNotOverwriteItsFaceOrMethods) {
    StrictMock<Provider> provider;
    const nx_memory_window_port_t face{&Provider::memory_ops, &provider};
    EXPECT_EQ(nx_memory_window_port_acquire(
                  &face, reinterpret_cast<nx_memory_window_lease_t*>(
                             const_cast<nx_memory_window_port_t*>(&face))),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_memory_window_port_release(&face, nullptr, 1U),
              NX_ERROR_INVALID);
    nx_memory_window_lease_t lease{};
    EXPECT_EQ(nx_memory_window_port_release(&face, &lease, 0U),
              NX_ERROR_INVALID);
}
TEST(ExternalBusAPI, GeometryDispatchIsReadonlyAndUsesTheExactOwner) {
    StrictMock<Provider> provider;
    const nx_memory_window_port_t face{&Provider::memory_ops, &provider};
    nx_memory_window_geometry_t geometry{123U, 456U, false};
    EXPECT_CALL(provider, Geometry(&geometry))
        .WillOnce(Invoke([](nx_memory_window_geometry_t* out) {
            *out = {0x68000000U, 0x100000U, true};
            return NX_SUCCESS;
        }));
    EXPECT_EQ(nx_memory_window_port_geometry(&face, &geometry), NX_SUCCESS);
    EXPECT_EQ(geometry.address, 0x68000000U);
    EXPECT_EQ(geometry.bytes, 0x100000U);
    EXPECT_TRUE(geometry.startup_backing);
}

TEST(ExternalBusAPI, GeometryRejectsMissingAliasedAndUnsupportedOutputs) {
    StrictMock<Provider> provider;
    const nx_memory_window_port_t face{&Provider::memory_ops, &provider};
    nx_memory_window_geometry_t geometry{123U, 456U, false};
    EXPECT_EQ(nx_memory_window_port_geometry(nullptr, &geometry),
              NX_ERROR_INVALID);
    EXPECT_EQ(nx_memory_window_port_geometry(&face, nullptr), NX_ERROR_INVALID);
    EXPECT_EQ(nx_memory_window_port_geometry(
                  &face, reinterpret_cast<nx_memory_window_geometry_t*>(
                             const_cast<nx_memory_window_port_t*>(&face))),
              NX_ERROR_INVALID);
    const nx_memory_window_ops_t absent{};
    const nx_memory_window_port_t optional{&absent, &provider};
    EXPECT_EQ(nx_memory_window_port_geometry(&optional, &geometry),
              NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(geometry.address, 123U);
    EXPECT_EQ(geometry.bytes, 456U);
}
TEST(ExternalBusAPI, ProviderAdmissionRejectionPreservesProgressOutput) {
    StrictMock<Provider> provider;
    const nx_parallel16_endpoint_t face{&Provider::parallel_ops, &provider};
    uint16_t words[2]{};
    size_t transferred = 90U;
    EXPECT_CALL(provider, Read(words, 2U, &transferred, 700U))
        .WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_parallel16_endpoint_read(&face, words, 2U, &transferred, 700U),
              NX_ERROR_BUSY);
    EXPECT_EQ(transferred, 90U);
}
} /* namespace */
