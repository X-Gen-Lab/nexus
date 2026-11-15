/**
 * \file            ds18b20_test.cpp
 * \brief           DS18B20 protocol, CRC and cooperative conversion contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/ds18b20.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::StrictMock;
constexpr std::array<uint8_t, 8> rom{0x28U, 1U, 2U, 3U, 4U, 5U, 6U, 0x9eU};

/** \brief Independent MSB-first polynomial division of wire-order bits. */
static uint8_t reference_crc(const uint8_t* bytes, size_t count) {
    unsigned shift = 0U;
    for (size_t i = 0U; i < count; ++i) {
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            const unsigned feedback = ((shift >> 7U) ^ (bytes[i] >> bit)) & 1U;
            shift = ((shift << 1U) & 0xffU) ^ (feedback ? 0x31U : 0U);
        }
    }
    unsigned reversed = 0U;
    for (unsigned bit = 0U; bit < 8U; ++bit)
        reversed |= ((shift >> bit) & 1U) << (7U - bit);
    return static_cast<uint8_t>(reversed);
}

static std::array<uint8_t, 9> scratch(uint16_t raw = 0x0550U,
                                      uint8_t resolution = 12U) {
    std::array<uint8_t, 9> bytes{
        static_cast<uint8_t>(raw),
        static_cast<uint8_t>(raw >> 8U),
        0x4bU,
        0x46U,
        static_cast<uint8_t>(0x1fU | ((resolution - 9U) << 5U)),
        0xffU,
        0x0cU,
        0x10U,
        0U};
    bytes[8] = reference_crc(bytes.data(), 8U);
    return bytes;
}

/** \brief Only settled one-wire transactions and its clock are mocked. */
class Wire {
  public:
    nx_onewire_t bus{};
    nx_onewire_t second_bus{};
    nx_time_us_t now = 1000U;
    nx_result_t clock_status = NX_SUCCESS;
    std::function<void()> clock_hook;
    testing::Sequence sequence;
    MOCK_METHOD(nx_result_t, Exchange,
                (nx_onewire_t*, const uint8_t*, size_t, uint8_t*, size_t,
                 nx_time_us_t));
    MOCK_METHOD(nx_result_t, Now, (nx_onewire_t*, nx_time_us_t*));
};
static Wire* current_wire;

extern "C" nx_result_t nx_onewire_exchange(nx_onewire_t* owner,
                                           const uint8_t* tx, size_t tx_bytes,
                                           uint8_t* rx, size_t rx_bits,
                                           nx_time_us_t deadline) {
    return current_wire->Exchange(owner, tx, tx_bytes, rx, rx_bits, deadline);
}
extern "C" nx_result_t nx_onewire_now(nx_onewire_t* owner, nx_time_us_t* now) {
    return current_wire->Now(owner, now);
}

class Ds18b20 : public testing::Test {
  protected:
    StrictMock<Wire> wire;
    nx_ds18b20_t device{};
    nx_ds18b20_config_t config{};
    static constexpr nx_time_us_t budget = 2000000U;
    void SetUp() override {
        current_wire = &wire;
        config.bus = &wire.bus;
        std::copy(rom.begin(), rom.end(), config.rom);
        config.external_vdd = true;
        EXPECT_CALL(wire, Now(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(Invoke([this](nx_onewire_t*, nx_time_us_t* out) {
                if (wire.clock_hook)
                    wire.clock_hook();
                if (wire.clock_status == NX_SUCCESS)
                    *out = wire.now;
                return wire.clock_status;
            }));
    }
    void TearDown() override {
        current_wire = nullptr;
    }
    std::vector<uint8_t> selected(uint8_t command) {
        if (config.skip_rom)
            return {0xccU, command};
        std::vector<uint8_t> bytes{0x55U};
        bytes.insert(bytes.end(), rom.begin(), rom.end());
        bytes.push_back(command);
        return bytes;
    }
    void exchange(std::vector<uint8_t> expected, size_t bits,
                  std::vector<uint8_t> response = {},
                  nx_result_t status = NX_SUCCESS,
                  std::function<void()> hook = {},
                  nx_onewire_t* bus = nullptr) {
        if (bus == nullptr)
            bus = config.bus;
        EXPECT_CALL(wire, Exchange(bus, _, expected.size(), _, bits, _))
            .InSequence(wire.sequence)
            .WillOnce(Invoke([=](nx_onewire_t*, const uint8_t* tx, size_t count,
                                 uint8_t* rx, size_t received_bits,
                                 nx_time_us_t deadline) {
                EXPECT_NE(NX_DEADLINE_NEVER, deadline);
                EXPECT_EQ(expected, std::vector<uint8_t>(tx, tx + count));
                if (hook)
                    hook();
                if (status == NX_SUCCESS && received_bits != 0U) {
                    EXPECT_NE(nullptr, rx);
                    EXPECT_EQ((received_bits + 7U) / 8U, response.size());
                    if (rx != nullptr &&
                        response.size() == (received_bits + 7U) / 8U)
                        std::copy(response.begin(), response.end(), rx);
                }
                return status;
            }));
    }
    void identity(std::array<uint8_t, 8> value = rom) {
        exchange({0x33U}, 64U, {value.begin(), value.end()});
    }
    void power(bool external = true) {
        exchange(selected(0xb4U), 1U, {static_cast<uint8_t>(external)});
    }
    void sample(std::array<uint8_t, 9> value = scratch()) {
        exchange(selected(0xbeU), 72U, {value.begin(), value.end()});
    }
    void initialize(uint8_t resolution = 12U) {
        if (config.skip_rom)
            identity();
        power();
        sample(scratch(0x0550U, resolution));
        ASSERT_EQ(NX_SUCCESS, nx_ds18b20_init(&device, &config, budget));
    }
    void start(uint8_t resolution = 12U, nx_time_us_t deadline = budget) {
        if (config.skip_rom)
            identity();
        sample(scratch(0x0550U, resolution));
        exchange(selected(0x44U), 0U);
        ASSERT_EQ(NX_SUCCESS, nx_ds18b20_start(&device, deadline));
    }
};

TEST(Ds18b20Crc, MatchesIndependentRomAndDatasheetScratchpadVectors) {
    const std::array<uint8_t, 9> power_on{0x50U, 0x05U, 0x4bU, 0x46U, 0x7fU,
                                          0xffU, 0x0cU, 0x10U, 0x1cU};
    EXPECT_EQ(0x9eU, nx_onewire_crc8(rom.data(), 7U));
    EXPECT_EQ(0U, nx_onewire_crc8(rom.data(), rom.size()));
    EXPECT_EQ(0x1cU, nx_onewire_crc8(power_on.data(), 8U));
    EXPECT_EQ(0U, nx_onewire_crc8(power_on.data(), power_on.size()));
    EXPECT_EQ(0U, nx_onewire_crc8(nullptr, 0U));
}
TEST_F(Ds18b20, KnownRomInitChecksActualPowerAndScratchpad) {
    initialize();
    EXPECT_TRUE(device.initialized);
    EXPECT_FALSE(device.converting);
}
TEST_F(Ds18b20, InvalidRomCrcOrDeclaredPowerRejectsWithoutTraffic) {
    config.rom[7] ^= 1U;
    EXPECT_NE(NX_SUCCESS, nx_ds18b20_init(&device, &config, budget));
    config.rom[7] ^= 1U;
    config.external_vdd = false;
    EXPECT_NE(NX_SUCCESS, nx_ds18b20_init(&device, &config, budget));
    EXPECT_FALSE(device.initialized);
}
TEST_F(Ds18b20, SkipRomRequiresExplicitSoleDeviceAndProbesItsIdentity) {
    config.skip_rom = true;
    EXPECT_EQ(NX_ERROR_INVALID, nx_ds18b20_init(&device, &config, budget));
    config.single_device = true;
    initialize();
    EXPECT_EQ(0, std::memcmp(rom.data(), device.rom, rom.size()));
}
TEST_F(Ds18b20, ParasitePowerReadbackRejectsBeforeScratchOrConversion) {
    power(false);
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_ds18b20_init(&device, &config, budget));
    EXPECT_FALSE(device.initialized);
}
TEST_F(Ds18b20, SingleDropWrongFamilyWithValidCrcIsRejected) {
    config.skip_rom = config.single_device = true;
    auto other = rom;
    other[0] = 0x10U;
    other[7] = reference_crc(other.data(), 7U);
    identity(other);
    EXPECT_NE(NX_SUCCESS, nx_ds18b20_init(&device, &config, budget));
}
TEST_F(Ds18b20, BadScratchpadCrcNeverOpensAdmission) {
    power();
    auto bad = scratch();
    bad[8] ^= 1U;
    sample(bad);
    EXPECT_NE(NX_SUCCESS, nx_ds18b20_init(&device, &config, budget));
    EXPECT_FALSE(device.initialized);
}
TEST_F(Ds18b20, InvalidReservedConfigurationBitsAreRejected) {
    power();
    auto bad = scratch();
    bad[4] = 0xffU;
    bad[8] = reference_crc(bad.data(), 8U);
    sample(bad);
    EXPECT_NE(NX_SUCCESS, nx_ds18b20_init(&device, &config, budget));
}
TEST_F(Ds18b20, Initial85DegreeScratchpadIsNotAFreshConversion) {
    initialize();
    int16_t out = 123;
    EXPECT_EQ(NX_ERROR_STATE, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
}
TEST_F(Ds18b20, ServiceWaitsActualResolutionMaximumWithoutWireTraffic) {
    initialize();
    start(9U);
    EXPECT_EQ(94750U, device.ready_at);
    wire.now = 94749U;
    int16_t out = 123;
    EXPECT_EQ(NX_ERROR_BUSY, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
    wire.now = 94750U;
    sample(scratch(0x0190U, 9U));
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(400, out);
    EXPECT_FALSE(device.converting);
    EXPECT_EQ(NX_ERROR_STATE, nx_ds18b20_service(&device, &out));
}
TEST_F(Ds18b20, ResolutionMaximaAreNotAnUncheckedAuthoredAssumption) {
    initialize(9U);
    start(12U);
    EXPECT_EQ(751000U, device.ready_at);
    wire.now = 100000U;
    int16_t out = 123;
    EXPECT_EQ(NX_ERROR_BUSY, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
}
TEST_F(Ds18b20, DeadlineEqualityTerminatesWaitWithoutPhysicalCancel) {
    initialize();
    start(12U, 800000U);
    wire.now = 800000U;
    int16_t out = 123;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
    EXPECT_FALSE(device.initialized);
    EXPECT_FALSE(device.converting);
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_stop(&device));
}
TEST_F(Ds18b20, ClockFaultTerminatesAdmissionAndKeepsOutputUnchanged) {
    initialize();
    start();
    wire.clock_status = NX_ERROR_IO;
    int16_t out = 123;
    EXPECT_EQ(NX_ERROR_IO, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
    EXPECT_FALSE(device.initialized);
}
TEST_F(Ds18b20, StopIsSoftwareOnlyIdempotentAndNeverTouchesWire) {
    initialize();
    start();
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_stop(&device));
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_stop(&device));
    int16_t out = 123;
    EXPECT_EQ(NX_ERROR_STATE, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
}
TEST_F(Ds18b20, ReadyReadCrcFailureClosesAdmissionWithoutPublication) {
    initialize();
    start();
    wire.now = 751000U;
    auto bad = scratch(0x0191U);
    bad[8] ^= 1U;
    sample(bad);
    int16_t out = 123;
    EXPECT_NE(NX_SUCCESS, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
    EXPECT_FALSE(device.initialized);
}
TEST_F(Ds18b20, ReadyReadResolutionChangeCannotUseEarlierShorterWait) {
    initialize();
    start(9U);
    wire.now = 94750U;
    sample(scratch(0x0191U, 12U));
    int16_t out = 123;
    EXPECT_EQ(NX_ERROR_STATE, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
    EXPECT_FALSE(device.initialized);
}
TEST_F(Ds18b20, RawOutsideDocumentedTemperatureRangeRejectsPublication) {
    initialize();
    start();
    wire.now = 751000U;
    sample(scratch(0x07d1U));
    int16_t out = 123;
    EXPECT_NE(NX_SUCCESS, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
}
TEST_F(Ds18b20, BusFailureIsPropagatedAndNoScratchpadIsInvented) {
    exchange(selected(0xb4U), 1U, {}, NX_ERROR_IO);
    EXPECT_EQ(NX_ERROR_IO, nx_ds18b20_init(&device, &config, budget));
    EXPECT_FALSE(device.initialized);
}
TEST_F(Ds18b20, SkipRomDetectsReplacementIdentityBeforeConversion) {
    config.skip_rom = config.single_device = true;
    initialize();
    auto replacement = rom;
    replacement[1] = 9U;
    replacement[7] = reference_crc(replacement.data(), 7U);
    identity(replacement);
    EXPECT_EQ(NX_ERROR_STATE, nx_ds18b20_start(&device, budget));
}
TEST_F(Ds18b20, ExpiredAndInfiniteBudgetsRejectWithoutWireTraffic) {
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_ds18b20_init(&device, &config, wire.now));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_ds18b20_init(&device, &config, NX_DEADLINE_NEVER));
}
TEST_F(Ds18b20, InCallStopAndInitReentryAreRejectedWithoutReplacingBinding) {
    exchange(selected(0xb4U), 1U, {1U}, NX_SUCCESS, [this] {
        EXPECT_EQ(NX_ERROR_BUSY, nx_ds18b20_stop(&device));
        EXPECT_EQ(NX_ERROR_BUSY, nx_ds18b20_init(&device, &config, budget));
    });
    sample();
    ASSERT_EQ(NX_SUCCESS, nx_ds18b20_init(&device, &config, budget));
    EXPECT_EQ(&config, device.config);
}
TEST_F(Ds18b20, InvalidOutputAndStateAliasesDoNotIssueWireCommands) {
    initialize();
    start();
    wire.now = 751000U;
    EXPECT_EQ(NX_ERROR_INVALID, nx_ds18b20_service(&device, nullptr));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_ds18b20_service(&device, reinterpret_cast<int16_t*>(&device)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_ds18b20_service(&device, reinterpret_cast<int16_t*>(&config)));
}

TEST_F(Ds18b20, TenAndElevenBitWaitsUseTheirDocumentedMaxima) {
    for (const auto& entry :
         {std::pair<uint8_t, nx_time_us_t>{10U, 187500U}, {11U, 375000U}}) {
        wire.now = 1000U;
        initialize(entry.first);
        start(entry.first);
        EXPECT_EQ(1000U + entry.second, device.ready_at);
        wire.now = 999U + entry.second;
        int16_t out = 123;
        EXPECT_EQ(NX_ERROR_BUSY, nx_ds18b20_service(&device, &out));
        EXPECT_EQ(123, out);
        wire.now = 1000U + entry.second;
        sample(scratch(0x0190U, entry.first));
        EXPECT_EQ(NX_SUCCESS, nx_ds18b20_service(&device, &out));
        EXPECT_EQ(400, out);
        ASSERT_EQ(NX_SUCCESS, nx_ds18b20_stop(&device));
    }
}
TEST_F(Ds18b20, DeadlineAfterScratchReadStillPreventsPublication) {
    initialize();
    start();
    wire.now = 751000U;
    const auto value = scratch(0x0191U);
    exchange(selected(0xbeU), 72U, {value.begin(), value.end()}, NX_SUCCESS,
             [this] { wire.now = budget; });
    int16_t out = 123;
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(123, out);
    EXPECT_FALSE(device.initialized);
}
TEST_F(Ds18b20, LateConvertCommandClosesWaitWithoutClaimingSensorCancellation) {
    initialize();
    sample();
    exchange(selected(0x44U), 0U, {}, NX_SUCCESS,
             [this] { wire.now = budget; });
    EXPECT_EQ(NX_ERROR_TIMEOUT, nx_ds18b20_start(&device, budget));
    EXPECT_FALSE(device.initialized);
    EXPECT_FALSE(device.converting);
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_stop(&device));
}
TEST_F(Ds18b20, AnotherIndependentSensorStopDoesNotCancelThisConversion) {
    initialize();
    nx_ds18b20_t other{};
    auto other_config = config;
    other_config.bus = &wire.second_bus;
    exchange(selected(0xb4U), 1U, {1U}, NX_SUCCESS, {}, &wire.second_bus);
    const auto initial = scratch();
    exchange(selected(0xbeU), 72U, {initial.begin(), initial.end()}, NX_SUCCESS,
             {}, &wire.second_bus);
    ASSERT_EQ(NX_SUCCESS, nx_ds18b20_init(&other, &other_config, budget));
    start();
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_stop(&other));
    EXPECT_TRUE(device.converting);
    wire.now = 751000U;
    sample(scratch(0xfff8U));
    int16_t out = 123;
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(-8, out);
}

class Ds18b20OutputAlias : public Ds18b20,
                           public testing::WithParamInterface<unsigned> {};
TEST_P(Ds18b20OutputAlias, KnownWireImmutableObjectsCannotReceiveTemperature) {
    const auto write = [](void*, uint32_t, uint32_t) { return NX_SUCCESS; };
    const auto read = [](const void*, uint32_t* value) {
        *value = 1U;
        return NX_SUCCESS;
    };
    nx_gpio_ops_t ops{write, read, nullptr};
    nx_gpio_port_t port{&ops, &wire};
    nx_onewire_config_t wire_config{};
    wire_config.port = &port;
    wire_config.mask = 1U;
    wire_config.clock = {[](void*) -> nx_time_us_t { return 1000U; }, &wire};
    wire_config.poll_limit = 128U;
    wire.bus.config = &wire_config;
    wire.bus.initialized = true;
    initialize();
    start();
    wire.now = 751000U;
    void* protected_objects[] = {&wire_config, &port, &ops};
    ON_CALL(wire, Exchange(_, _, _, _, _, _))
        .WillByDefault(testing::Return(NX_ERROR_IO));
    EXPECT_CALL(wire, Exchange(_, _, _, _, _, _)).Times(0);
    std::array<unsigned char, sizeof(wire_config)> before_config{};
    std::array<unsigned char, sizeof(port)> before_port{};
    std::array<unsigned char, sizeof(ops)> before_ops{};
    std::memcpy(before_config.data(), &wire_config, sizeof(wire_config));
    std::memcpy(before_port.data(), &port, sizeof(port));
    std::memcpy(before_ops.data(), &ops, sizeof(ops));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_ds18b20_service(&device, reinterpret_cast<int16_t*>(
                                              protected_objects[GetParam()])));
    EXPECT_EQ(0, std::memcmp(before_config.data(), &wire_config,
                             sizeof(wire_config)));
    EXPECT_EQ(0, std::memcmp(before_port.data(), &port, sizeof(port)));
    EXPECT_EQ(0, std::memcmp(before_ops.data(), &ops, sizeof(ops)));
}
INSTANTIATE_TEST_SUITE_P(ImmutableWireIdentity, Ds18b20OutputAlias,
                         testing::Values(0U, 1U, 2U));

struct Reading {
    uint16_t encoded;
    uint8_t resolution;
    int16_t sixteenths;
};
class Ds18b20Reading : public Ds18b20,
                       public testing::WithParamInterface<Reading> {};
TEST_P(Ds18b20Reading,
       PublishesExactSignedSixteenthsAfterMaskingUndefinedBits) {
    const auto input = GetParam();
    initialize(input.resolution);
    start(input.resolution);
    wire.now = 751000U;
    sample(scratch(input.encoded, input.resolution));
    int16_t out = 123;
    EXPECT_EQ(NX_SUCCESS, nx_ds18b20_service(&device, &out));
    EXPECT_EQ(input.sixteenths, out);
}
INSTANTIATE_TEST_SUITE_P(
    DatasheetAndUndefinedLsb, Ds18b20Reading,
    testing::Values(Reading{0x07d0U, 12U, 2000}, Reading{0x0550U, 12U, 1360},
                    Reading{0x0191U, 12U, 401}, Reading{0x00a2U, 12U, 162},
                    Reading{0x0008U, 12U, 8}, Reading{0x0000U, 12U, 0},
                    Reading{0xfff8U, 12U, -8}, Reading{0xff5eU, 12U, -162},
                    Reading{0xfe6fU, 12U, -401}, Reading{0xfc90U, 12U, -880},
                    Reading{0x0197U, 9U, 400}, Reading{0xfff9U, 9U, -8},
                    Reading{0xff5fU, 10U, -164}, Reading{0xfe6fU, 11U, -402}));
