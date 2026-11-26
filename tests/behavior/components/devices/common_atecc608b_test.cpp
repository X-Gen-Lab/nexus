/**
 * \file            common_atecc608b_test.cpp
 * \brief           Real vendor command execution over a modeled ATECC I2C wire
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/atecc608b.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <new>
#include <vector>

namespace {
const nx_atecc608b_operation_t* active_scope = nullptr;
class Chip {
  public:
    std::array<uint8_t, 128> configuration{};
    std::array<uint8_t, 4> revision{0, 0, 0x60U, 3U};
    std::array<uint8_t, 64> public_key{};
    std::array<uint8_t, 64> signature{};
    std::array<uint8_t, 32> digest{};
    std::vector<uint8_t> response;
    std::vector<uint8_t> opcodes;
    size_t at{0U};
    nx_time_us_t now{1000U};
    unsigned writes{0U};
    unsigned wakes{0U};
    bool fail_sleep{false};
    bool corrupt_key{false};
    bool short_read{false};
    bool fail_random{false};
    bool fail_idle{false};
    bool busy_reply{false};
    bool stalled_clock{false};
    unsigned poll_reads{0U};
    Chip() {
        configuration[0] = 1U;
        configuration[1] = 0x23U;
        configuration[12] = 0xeeU;
        configuration[20] = 0x81U;
        configuration[96] = 0x33U;
        for (size_t index = 0; index < public_key.size(); ++index) {
            public_key[index] = static_cast<uint8_t>(index + 1U);
            signature[index] = static_cast<uint8_t>(0xa0U + index);
        }
    }
    static nx_time_us_t clock(void* context) {
        return static_cast<Chip*>(context)->now;
    }
    static nx_result_t delay(void* context, uint32_t us,
                             nx_time_us_t deadline) {
        auto& self = *static_cast<Chip*>(context);
        if (us > deadline - self.now) {
            return NX_ERROR_TIMEOUT;
        }
        if (!self.stalled_clock) {
            self.now += us;
        }
        return NX_SUCCESS;
    }
    static nx_result_t wake(void* context, nx_time_us_t deadline) {
        auto& self = *static_cast<Chip*>(context);
        if (self.now >= deadline) {
            return NX_ERROR_TIMEOUT;
        }
        ++self.wakes;
        self.response = {4U, 0x11U, 0x33U, 0x43U};
        self.at = 0U;
        return NX_SUCCESS;
    }
    void reply(const uint8_t* data, size_t bytes) {
        response.assign(1U, static_cast<uint8_t>(bytes + 3U));
        response.insert(response.end(), data, data + bytes);
        uint16_t crc = 0U;
        for (const uint8_t byte : response) {
            for (unsigned bit = 0; bit < 8U; ++bit) {
                const bool input = (byte & (1U << bit)) != 0U;
                const bool high = (crc & 0x8000U) != 0U;
                crc = static_cast<uint16_t>(crc << 1U);
                if (input != high) {
                    crc ^= 0x8005U;
                }
            }
        }
        response.push_back(static_cast<uint8_t>(crc));
        response.push_back(static_cast<uint8_t>(crc >> 8U));
        at = 0U;
    }
    nx_result_t command(const uint8_t* bytes, size_t count) {
        ++writes;
        if (count == 1U) {
            if ((bytes[0] == 1U && fail_sleep) ||
                (bytes[0] == 2U && fail_idle)) {
                return NX_ERROR_IO;
            }
            if (bytes[0] == 0U) {
                at = 0U;
            }
            return NX_SUCCESS;
        }
        if (count < 8U || bytes[0] != 3U) {
            return NX_ERROR_IO;
        }
        const uint8_t opcode = bytes[2];
        opcodes.push_back(opcode);
        const uint16_t address =
            static_cast<uint16_t>(bytes[4]) |
            static_cast<uint16_t>(static_cast<uint16_t>(bytes[5]) << 8U);
        if (opcode == 0x30U) {
            reply(revision.data(), revision.size());
        } else if (opcode == 2U && bytes[3] == 0x80U && address <= 24U) {
            reply(configuration.data() + address * 4U, 32U);
        } else if (opcode == 0x40U && bytes[3] == 0U && address == 0U) {
            reply(public_key.data(), public_key.size());
            if (corrupt_key) {
                response.back() ^= 1U;
            }
        } else if (opcode == 0x1bU) {
            if (fail_random) {
                const uint8_t error = 0x0fU;
                reply(&error, 1U);
            } else {
                std::array<uint8_t, 32> random{};
                random.fill(0x5aU);
                reply(random.data(), random.size());
            }
        } else if (opcode == 0x16U && count == 40U) {
            std::copy_n(bytes + 6U, digest.size(), digest.begin());
            const uint8_t okay = 0U;
            reply(&okay, 1U);
        } else if (opcode == 0x41U && address == 0U &&
                   (configuration[20] & 1U) != 0U) {
            reply(signature.data(), signature.size());
        } else {
            const uint8_t unsupported = 3U;
            reply(&unsupported, 1U);
        }
        return NX_SUCCESS;
    }
    static nx_result_t transaction(void* context, nx_i2c_message_t* messages,
                                   size_t count, nx_time_us_t deadline,
                                   size_t* transferred) {
        auto& self = *static_cast<Chip*>(context);
        *transferred = 0U;
        if (self.now >= deadline) {
            return NX_ERROR_TIMEOUT;
        }
        for (size_t index = 0; index < count; ++index) {
            const auto& message = messages[index];
            if (message.read) {
                if (message.length == 1U) {
                    ++self.poll_reads;
                    if (self.busy_reply) {
                        return NX_ERROR_NACK;
                    }
                }
                if (self.at + message.length > self.response.size()) {
                    return NX_ERROR_IO;
                }
                std::memcpy(message.buffer.rx, self.response.data() + self.at,
                            message.length);
                self.at += message.length;
                *transferred += self.short_read ? 0U : message.length;
            } else {
                const auto result =
                    self.command(message.buffer.tx, message.length);
                if (result != NX_SUCCESS) {
                    return result;
                }
                *transferred += message.length;
            }
        }
        return NX_SUCCESS;
    }
    nx_i2c_endpoint_ops_t ops{transaction, nullptr, nullptr, nullptr};
    nx_i2c_endpoint_t endpoint{&ops, this};
};
class Atecc : public testing::Test {
  protected:
    Chip chip;
    nx_atecc608b_t device{};
    nx_atecc608b_config_t config{};
    nx_atecc608b_operation_t operation{};
    std::vector<max_align_t> workspace;
    void SetUp() override {
        config.endpoint = &chip.endpoint;
        config.clock = {Chip::clock, &chip};
        config.wake = Chip::wake;
        config.delay = Chip::delay;
        config.context = &chip;
        config.signing_slot = 0U;
        config.maximum_polls = 32U;
        workspace.resize(
            (nx_atecc608b_workspace_bytes() + sizeof(max_align_t) - 1U) /
            sizeof(max_align_t));
        operation = {&device, 1000000U};
        active_scope = &operation;
    }
    void TearDown() override {
        active_scope = nullptr;
    }
    nx_result_t open() {
        return nx_atecc608b_init(&device, &config, workspace.data(),
                                 workspace.size() * sizeof(max_align_t),
                                 operation.deadline);
    }
};

TEST_F(Atecc, RealInfoAndConfigReadPublishOnlyLockedBRevisionIdentity) {
    ASSERT_EQ(open(), NX_SUCCESS);
    nx_atecc608b_identity_t identity{};
    ASSERT_EQ(nx_atecc608b_read_identity(&device, &identity), NX_SUCCESS);
    EXPECT_EQ(identity.revision[3], 3U);
    EXPECT_EQ(identity.serial[0], 1U);
    EXPECT_EQ(identity.serial[1], 0x23U);
    EXPECT_EQ(identity.serial[8], 0xeeU);
    EXPECT_NE(nx_atecc608b_signer(&device), nullptr);
    EXPECT_EQ(std::count(chip.opcodes.begin(), chip.opcodes.end(), 2U), 4);
}

TEST_F(Atecc, P256PublicAndExternalDigestSignUseVendorBuildersAndExecutor) {
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto* signer = nx_atecc608b_signer(&device);
    nx_p256_public_key_t key{};
    ASSERT_EQ(nx_p256_signer_public_key(signer, &key, operation.deadline),
              NX_SUCCESS);
    EXPECT_EQ(std::memcmp(key.xy, chip.public_key.data(), sizeof(key.xy)), 0);
    nx_p256_digest_t digest{};
    for (size_t index = 0; index < sizeof(digest.bytes); ++index) {
        digest.bytes[index] = static_cast<uint8_t>(index);
    }
    nx_p256_signature_t signature{};
    ASSERT_EQ(nx_p256_signer_sign_digest(signer, &digest, &signature,
                                         operation.deadline),
              NX_SUCCESS);
    EXPECT_EQ(
        std::memcmp(chip.digest.data(), digest.bytes, sizeof(digest.bytes)), 0);
    EXPECT_EQ(
        std::memcmp(signature.rs, chip.signature.data(), sizeof(signature.rs)),
        0);
    EXPECT_EQ(chip.opcodes[chip.opcodes.size() - 2U], 0x16U);
    EXPECT_EQ(chip.opcodes.back(), 0x41U);
}

TEST_F(Atecc, UnlockedDataOrInvalidSlotNeverCreatesASigningFace) {
    chip.configuration[86] = 0x55U;
    EXPECT_EQ(open(), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
    EXPECT_NE(device.state, nullptr);
    EXPECT_EQ(nx_atecc608b_stop(&device, operation.deadline), NX_SUCCESS);
    EXPECT_EQ(device.state, nullptr);
}

TEST_F(Atecc, IndividuallyUnlockedSigningSlotCannotCreateASigner) {
    chip.configuration[88] = 1U;
    EXPECT_EQ(open(), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
    EXPECT_NE(device.state, nullptr);
    EXPECT_EQ(nx_atecc608b_stop(&device, operation.deadline), NX_SUCCESS);
}

TEST_F(Atecc, PrivateSlotWithoutExternalSignPermissionRefusesAdmission) {
    chip.configuration[20] = 0x80U;
    EXPECT_EQ(open(), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
    for (const auto opcode : std::array<uint8_t, 3>{0x1bU, 0x16U, 0x41U}) {
        EXPECT_TRUE(std::find(chip.opcodes.begin(), chip.opcodes.end(),
                              opcode) == chip.opcodes.end());
    }
}

TEST_F(Atecc, ARevisionIsNotAcceptedAsTheSelectedBDevice) {
    chip.revision[3] = 2U;
    EXPECT_EQ(open(), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
}

TEST_F(Atecc, NonSecretOrNonPrivateSlotRefusesPublicSigningCapability) {
    chip.configuration[20] = 0U;
    EXPECT_EQ(open(), NX_ERROR_PERMISSION);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
    EXPECT_TRUE(std::find(chip.opcodes.begin(), chip.opcodes.end(), 0x41U) ==
                chip.opcodes.end());
}

TEST_F(Atecc, CorruptResponseKeepsCallerPublicKeyUnchangedAndClosesAdmission) {
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto* signer = nx_atecc608b_signer(&device);
    nx_p256_public_key_t key{};
    std::memset(key.xy, 0xa5, sizeof(key.xy));
    chip.corrupt_key = true;
    EXPECT_EQ(nx_p256_signer_public_key(signer, &key, operation.deadline),
              NX_ERROR_IO);
    EXPECT_TRUE(std::all_of(std::begin(key.xy), std::end(key.xy),
                            [](uint8_t value) { return value == 0xa5U; }));
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
}

TEST_F(Atecc, MissingOperationScopeRejectsBeforeAnyPhysicalAccess) {
    active_scope = nullptr;
    EXPECT_EQ(open(), NX_ERROR_STATE);
    EXPECT_EQ(chip.wakes, 0U);
    EXPECT_EQ(chip.writes, 0U);
    EXPECT_EQ(device.state, nullptr);
}

TEST_F(Atecc, SleepFailureRetainsTheSameBindingUntilExplicitRetry) {
    ASSERT_EQ(open(), NX_SUCCESS);
    chip.fail_sleep = true;
    EXPECT_EQ(nx_atecc608b_stop(&device, operation.deadline), NX_ERROR_IO);
    EXPECT_NE(device.state, nullptr);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
    chip.fail_sleep = false;
    EXPECT_EQ(nx_atecc608b_stop(&device, operation.deadline), NX_SUCCESS);
    EXPECT_EQ(device.state, nullptr);
}

TEST_F(Atecc, PollingBudgetAndAbsoluteDeadlineBoundExpiredOperations) {
    operation.deadline = chip.now;
    EXPECT_EQ(open(), NX_ERROR_TIMEOUT);
    EXPECT_EQ(chip.writes, 0U);
    EXPECT_EQ(device.state, nullptr);
}

TEST_F(Atecc, RandomSeedUpdateFailureCannotReachNonceOrSign) {
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto* signer = nx_atecc608b_signer(&device);
    chip.fail_random = true;
    nx_p256_digest_t digest{};
    nx_p256_signature_t signature{};
    std::memset(signature.rs, 0xa5, sizeof(signature.rs));
    EXPECT_EQ(nx_p256_signer_sign_digest(signer, &digest, &signature,
                                         operation.deadline),
              NX_ERROR_IO);
    EXPECT_TRUE(std::find(chip.opcodes.begin(), chip.opcodes.end(), 0x16U) ==
                chip.opcodes.end());
    EXPECT_TRUE(std::find(chip.opcodes.begin(), chip.opcodes.end(), 0x41U) ==
                chip.opcodes.end());
    EXPECT_TRUE(std::all_of(std::begin(signature.rs), std::end(signature.rs),
                            [](uint8_t value) { return value == 0xa5U; }));
}

TEST_F(Atecc, ZeroSignatureScalarsNeverBecomeAValidPublishedSignature) {
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto* signer = nx_atecc608b_signer(&device);
    chip.signature.fill(0U);
    nx_p256_digest_t digest{};
    nx_p256_signature_t signature{};
    std::memset(signature.rs, 0xa5, sizeof(signature.rs));
    EXPECT_EQ(nx_p256_signer_sign_digest(signer, &digest, &signature,
                                         operation.deadline),
              NX_ERROR_IO);
    EXPECT_EQ(signature.rs[0], 0xa5U);
}

TEST_F(Atecc, SignatureScalarEqualToP256OrderIsRejected) {
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto* signer = nx_atecc608b_signer(&device);
    const uint8_t order[32] = {0xffU, 0xffU, 0xffU, 0xffU, 0U,    0U,    0U,
                               0U,    0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU,
                               0xffU, 0xffU, 0xbcU, 0xe6U, 0xfaU, 0xadU, 0xa7U,
                               0x17U, 0x9eU, 0x84U, 0xf3U, 0xb9U, 0xcaU, 0xc2U,
                               0xfcU, 0x63U, 0x25U, 0x51U};
    chip.signature.fill(0U);
    std::copy_n(order, 32U, chip.signature.begin());
    chip.signature.back() = 1U;
    nx_p256_digest_t digest{};
    nx_p256_signature_t signature{};
    EXPECT_EQ(nx_p256_signer_sign_digest(signer, &digest, &signature,
                                         operation.deadline),
              NX_ERROR_IO);
}

TEST_F(Atecc, ClockRollbackClosesSigningAndCannotBeRepairedByALargerDeadline) {
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto* signer = nx_atecc608b_signer(&device);
    --chip.now;
    nx_p256_public_key_t key{};
    EXPECT_EQ(nx_p256_signer_public_key(signer, &key, operation.deadline),
              NX_ERROR_STATE);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
    chip.now += 100000U;
    operation.deadline += 100000U;
    EXPECT_EQ(nx_p256_signer_public_key(signer, &key, operation.deadline),
              NX_ERROR_STATE);
}

TEST_F(Atecc, IdleWithdrawalFailureDoesNotPublishASuccessfulPublicKey) {
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto* signer = nx_atecc608b_signer(&device);
    chip.fail_idle = true;
    nx_p256_public_key_t key{};
    std::memset(key.xy, 0xa5, sizeof(key.xy));
    EXPECT_EQ(nx_p256_signer_public_key(signer, &key, operation.deadline),
              NX_ERROR_IO);
    EXPECT_EQ(key.xy[0], 0xa5U);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
}

TEST_F(Atecc, SuccessfulShortI2cReadDoesNotExposeUnverifiedResponseBytes) {
    chip.short_read = true;
    EXPECT_EQ(open(), NX_ERROR_IO);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
}

TEST_F(Atecc, AliasedDigestAndSignatureRejectBeforeAnyPhysicalCommand) {
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto* signer = nx_atecc608b_signer(&device);
    nx_p256_signature_t signature{};
    const auto previous = chip.writes;
    const auto* digest = reinterpret_cast<const nx_p256_digest_t*>(&signature);
    EXPECT_EQ(nx_p256_signer_sign_digest(signer, digest, &signature,
                                         operation.deadline),
              NX_ERROR_INVALID);
    EXPECT_EQ(chip.writes, previous);
}

TEST_F(Atecc, ReceiveBudgetBoundsBusyDeviceEvenWhenClockDoesNotAdvance) {
    config.maximum_polls = 3U;
    chip.busy_reply = true;
    chip.stalled_clock = true;
    EXPECT_EQ(open(), NX_ERROR_TIMEOUT);
    EXPECT_EQ(chip.poll_reads, 3U);
    EXPECT_EQ(nx_atecc608b_signer(&device), nullptr);
    EXPECT_EQ(nx_atecc608b_stop(&device, operation.deadline), NX_SUCCESS);
}

TEST_F(Atecc, EndpointInsideWorkspaceRejectsBeforeBindingOrWake) {
    const size_t bytes = nx_atecc608b_workspace_bytes();
    auto* const storage = reinterpret_cast<uint8_t*>(workspace.data());
    const size_t offset = (bytes - sizeof(nx_i2c_endpoint_t)) &
                          ~(alignof(nx_i2c_endpoint_t) - 1U);
    auto* endpoint = new (storage + offset) nx_i2c_endpoint_t(chip.endpoint);
    config.endpoint = endpoint;
    EXPECT_EQ(open(), NX_ERROR_INVALID);
    EXPECT_EQ(device.state, nullptr);
    EXPECT_EQ(chip.wakes, 0U);
    EXPECT_EQ(chip.writes, 0U);
}

TEST_F(Atecc, PublicKeyOutputCannotOverwriteRetainedEndpointBinding) {
    alignas(max_align_t) std::array<uint8_t, 64> storage{};
    auto* endpoint = new (storage.data()) nx_i2c_endpoint_t(chip.endpoint);
    config.endpoint = endpoint;
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto original = storage;
    const auto wakes = chip.wakes;
    auto* key = reinterpret_cast<nx_p256_public_key_t*>(storage.data());
    EXPECT_EQ(nx_p256_signer_public_key(nx_atecc608b_signer(&device), key,
                                        operation.deadline),
              NX_ERROR_INVALID);
    EXPECT_EQ(chip.wakes, wakes);
    EXPECT_EQ(storage, original);
}

TEST_F(Atecc, AnotherDeviceOperationScopeCannotUseThisSigner) {
    ASSERT_EQ(open(), NX_SUCCESS);
    const auto* signer = nx_atecc608b_signer(&device);
    nx_atecc608b_t unrelated{};
    operation.device = &unrelated;
    const unsigned previous = chip.writes;
    nx_p256_public_key_t key{};
    EXPECT_EQ(nx_p256_signer_public_key(signer, &key, operation.deadline),
              NX_ERROR_STATE);
    EXPECT_EQ(chip.writes, previous);
    operation.device = &device;
}
} /* namespace */
extern "C" const nx_atecc608b_operation_t* nx_atecc608b_operation_get(void) {
    return active_scope;
}
