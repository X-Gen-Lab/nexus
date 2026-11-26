/**
 * \file            crypto_api_test.cpp
 * \brief           Public cryptographic request values and borrow boundaries
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "crypto_validation.h"
#include "nexus/io/cipher.h"
#include "nexus/io/hash.h"
#include <array>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>

namespace {
using ::testing::_;
using ::testing::Return;

class CryptoApi : public ::testing::Test {
  protected:
    alignas(4) std::array<uint8_t, 96> input{};
    alignas(4) std::array<uint8_t, 96> output{};
    std::array<uint8_t, 32> key{};
    std::array<uint8_t, 16> iv{};
    nx_cipher_request_t cipher{};
    nx_hash_request_t hash{};
    nx_cipher_operation_t c{};
    nx_hash_operation_t h{};

    void SetUp() override {
        nx_request_initialize(&cipher.base);
        nx_request_initialize(&hash.base);
        c.algorithm = NX_CIPHER_AES_CBC;
        c.direction = NX_CIPHER_ENCRYPT;
        c.input = input.data();
        c.output = output.data();
        c.bytes = 32U;
        c.key = key.data();
        c.key_bytes = 16U;
        c.iv = iv.data();
        c.iv_bytes = 16U;
        h.algorithm = NX_HASH_SHA1;
        h.input = input.data();
        h.bytes = 67U;
        h.digest = output.data();
        h.digest_capacity = 20U;
    }
};

TEST_F(CryptoApi, CipherCopiesValuesWithoutAdmittingOrReadingInputBytes) {
    c.input = reinterpret_cast<const void*>(uintptr_t{0x1000});
    ASSERT_EQ(nx_cipher_prepare(&cipher, &c, 9U), NX_SUCCESS);
    c.bytes = 64U;
    EXPECT_EQ(cipher.operation.bytes, 32U);
    EXPECT_EQ(cipher.operation.input,
              reinterpret_cast<const void*>(uintptr_t{0x1000}));
    EXPECT_EQ(nx_request_state(&cipher.base), NX_REQUEST_READY);
    EXPECT_EQ(cipher.base.deadline, 9U);
    EXPECT_EQ(cipher.base.transferred, 0U);
}

TEST_F(CryptoApi, CipherAcceptsStandardKeysAndKeepsCtrByteTailsGeneric) {
    for (size_t bytes : {16U, 24U, 32U}) {
        c.key_bytes = bytes;
        ASSERT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_SUCCESS);
    }
    c.algorithm = NX_CIPHER_AES_CTR;
    c.bytes = 17U;
    EXPECT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_SUCCESS);
    c.algorithm = NX_CIPHER_DES_CBC;
    c.key_bytes = 8U;
    c.iv_bytes = 8U;
    c.bytes = 24U;
    EXPECT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_SUCCESS);
    c.algorithm = NX_CIPHER_TDES_CBC;
    for (size_t bytes : {16U, 24U}) {
        c.key_bytes = bytes;
        EXPECT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_SUCCESS);
    }
}

TEST_F(CryptoApi, InvalidAlgorithmKeyIvLengthAndDeadlineLeaveTupleUnchanged) {
    ASSERT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_SUCCESS);
    const auto before = cipher;
    auto reject = [&](const nx_cipher_operation_t& value, nx_time_us_t until) {
        EXPECT_EQ(nx_cipher_prepare(&cipher, &value, until), NX_ERROR_INVALID);
        EXPECT_EQ(std::memcmp(&before, &cipher, sizeof(cipher)), 0);
    };
    auto bad = c;
    bad.algorithm = static_cast<nx_cipher_algorithm_t>(7);
    reject(bad, 7U);
    bad = c;
    bad.key_bytes = 15U;
    reject(bad, 7U);
    bad = c;
    bad.iv_bytes = 15U;
    reject(bad, 7U);
    bad = c;
    bad.bytes = 17U;
    reject(bad, 7U);
    bad.bytes = 0U;
    reject(bad, 7U);
    reject(c, NX_DEADLINE_NEVER);
    bad = c;
    bad.algorithm = NX_CIPHER_AES_ECB;
    reject(bad, 7U);
    bad.iv = nullptr;
    bad.iv_bytes = 0U;
    EXPECT_EQ(nx_cipher_prepare(&cipher, &bad, 7U), NX_SUCCESS);
}

TEST_F(CryptoApi, CipherRejectsEveryReferencedSpanOverTheWholeDescriptor) {
    for (unsigned field = 0U; field < 4U; ++field) {
        auto bad = c;
        void* alias = &cipher.operation;
        if (field == 0U)
            bad.input = alias;
        if (field == 1U)
            bad.output = alias;
        if (field == 2U)
            bad.key = alias;
        if (field == 3U)
            bad.iv = alias;
        EXPECT_EQ(nx_cipher_prepare(&cipher, &bad, 7U), NX_ERROR_INVALID);
    }
    EXPECT_EQ(nx_request_state(&cipher.base), NX_REQUEST_READY);
}

TEST_F(CryptoApi, CipherAllowsExactInPlaceButRejectsPartialOrSecretOverwrite) {
    c.output = const_cast<void*>(c.input);
    EXPECT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_SUCCESS);
    c.output = input.data() + 1U;
    EXPECT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_ERROR_INVALID);
    c.output = key.data();
    EXPECT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_ERROR_INVALID);
    c.output = iv.data();
    EXPECT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_ERROR_INVALID);
}

TEST_F(CryptoApi, WrappedSpansRejectBeforeAnyPayloadAccess) {
    const void* bad = reinterpret_cast<const void*>(UINTPTR_MAX - 7U);
    c.input = bad;
    EXPECT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_ERROR_INVALID);
    h.input = bad;
    EXPECT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_ERROR_INVALID);
}

TEST_F(CryptoApi, BorrowedRequestCannotBePreparedAndSettledCanBeReused) {
    ASSERT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_SUCCESS);
    ASSERT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(&cipher.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(&hash.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    const auto before_c = cipher;
    const auto before_h = hash;
    EXPECT_EQ(nx_cipher_prepare(&cipher, nullptr, 8U), NX_ERROR_STATE);
    EXPECT_EQ(nx_hash_prepare(&hash, nullptr, 8U), NX_ERROR_STATE);
    EXPECT_EQ(std::memcmp(&before_c, &cipher, sizeof(cipher)), 0);
    EXPECT_EQ(std::memcmp(&before_h, &hash, sizeof(hash)), 0);
    nx_request_settle(&cipher.base, NX_SUCCESS, 32U);
    nx_request_settle(&hash.base, NX_SUCCESS, 20U);
    EXPECT_EQ(nx_cipher_prepare(&cipher, &cipher.operation, 8U), NX_SUCCESS);
    EXPECT_EQ(nx_hash_prepare(&hash, &hash.operation, 8U), NX_SUCCESS);
    EXPECT_EQ(cipher.base.transferred, 0U);
    EXPECT_EQ(hash.base.transferred, 0U);
}

TEST_F(CryptoApi, HashAcceptsEmptyAndUnpaddedMessagesWithoutReadingThem) {
    for (size_t bytes : {0U, 1U, 3U, 64U, 65U, 66U, 67U}) {
        h.bytes = bytes;
        h.input =
            bytes == 0U ? nullptr : reinterpret_cast<const void*>(uintptr_t{1});
        ASSERT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_SUCCESS);
        EXPECT_EQ(hash.operation.bytes, bytes);
    }
    EXPECT_EQ(nx_request_state(&hash.base), NX_REQUEST_READY);
}

TEST_F(CryptoApi, HashSeparatesPlainAndHmacKeysAndExactDigestSizes) {
    h.key = key.data();
    h.key_bytes = 16U;
    EXPECT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_ERROR_INVALID);
    h.algorithm = NX_HASH_HMAC_SHA1;
    EXPECT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_SUCCESS);
    h.key = nullptr;
    h.key_bytes = 0U;
    EXPECT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_SUCCESS);
    h.digest_capacity = 19U;
    EXPECT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_ERROR_INVALID);
    h.algorithm = NX_HASH_HMAC_MD5;
    h.digest_capacity = 16U;
    EXPECT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_SUCCESS);
    h.digest_capacity = 15U;
    EXPECT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_ERROR_INVALID);
}

TEST_F(CryptoApi, HashIsolatesWholeDescriptorAndDigestCapacityFromSources) {
    auto bad = h;
    bad.input = &hash.operation;
    EXPECT_EQ(nx_hash_prepare(&hash, &bad, 7U), NX_ERROR_INVALID);
    bad = h;
    bad.algorithm = NX_HASH_HMAC_SHA1;
    bad.key = &hash.operation;
    bad.key_bytes = 16U;
    EXPECT_EQ(nx_hash_prepare(&hash, &bad, 7U), NX_ERROR_INVALID);
    bad = h;
    bad.digest = &hash.operation;
    EXPECT_EQ(nx_hash_prepare(&hash, &bad, 7U), NX_ERROR_INVALID);
    bad = h;
    bad.digest = input.data() + 50U;
    EXPECT_EQ(nx_hash_prepare(&hash, &bad, 7U), NX_ERROR_INVALID);
    bad = h;
    bad.algorithm = NX_HASH_HMAC_SHA1;
    bad.key = output.data() + 24U;
    bad.key_bytes = 16U;
    bad.digest_capacity = 32U;
    EXPECT_EQ(nx_hash_prepare(&hash, &bad, 7U), NX_ERROR_INVALID);
}

TEST_F(CryptoApi, SharedOperationChecksArePureEvenForActiveDescriptors) {
    ASSERT_EQ(nx_request_admit(&cipher.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    ASSERT_EQ(nx_request_admit(&hash.base, NX_REQUEST_ACTIVE), NX_SUCCESS);
    const auto before_c = cipher;
    const auto before_h = hash;
    EXPECT_EQ(nx_cipher_operation_check(&cipher, &c, 7U), NX_SUCCESS);
    EXPECT_EQ(nx_hash_operation_check(&hash, &h, 7U), NX_SUCCESS);
    EXPECT_EQ(std::memcmp(&before_c, &cipher, sizeof(cipher)), 0);
    EXPECT_EQ(std::memcmp(&before_h, &hash, sizeof(hash)), 0);
}

class Provider {
  public:
    MOCK_METHOD(nx_result_t, Submit, (nx_cipher_request_t*));
    MOCK_METHOD(nx_result_t, Query,
                (nx_cipher_algorithm_t, nx_cipher_constraints_t*));
    MOCK_METHOD(nx_result_t, HashSubmit, (nx_hash_request_t*));
    static nx_result_t submit(void* c, nx_cipher_request_t* r) {
        return static_cast<Provider*>(c)->Submit(r);
    }
    static nx_result_t query(void* c, nx_cipher_algorithm_t a,
                             nx_cipher_constraints_t* r) {
        return static_cast<Provider*>(c)->Query(a, r);
    }
    static nx_result_t hash_submit(void* c, nx_hash_request_t* r) {
        return static_cast<Provider*>(c)->HashSubmit(r);
    }
};

TEST_F(CryptoApi, DispatchBindsExactContextAndHasNoPublicBorrowSideEffect) {
    ::testing::StrictMock<Provider> provider;
    nx_cipher_ops_t ops{};
    ops.submit = Provider::submit;
    nx_cipher_port_t port{&ops, &provider};
    ASSERT_EQ(nx_cipher_prepare(&cipher, &c, 7U), NX_SUCCESS);
    EXPECT_CALL(provider, Submit(&cipher)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_cipher_port_submit(&port, &cipher), NX_ERROR_BUSY);
    EXPECT_EQ(nx_request_state(&cipher.base), NX_REQUEST_READY);
    cipher.operation.output = &ops;
    EXPECT_EQ(nx_cipher_port_submit(&port, &cipher), NX_ERROR_INVALID);
}

TEST_F(CryptoApi, QueryRejectsAliasedPublicControlsBeforeProviderAccess) {
    ::testing::StrictMock<Provider> provider;
    nx_cipher_ops_t ops{};
    ops.query = Provider::query;
    nx_cipher_port_t port{&ops, &provider};
    EXPECT_EQ(
        nx_cipher_port_query(&port, NX_CIPHER_AES_CBC,
                             reinterpret_cast<nx_cipher_constraints_t*>(&ops)),
        NX_ERROR_INVALID);
    nx_cipher_constraints_t result{};
    EXPECT_CALL(provider, Query(NX_CIPHER_AES_CBC, &result))
        .WillOnce(Return(NX_ERROR_UNSUPPORTED));
    EXPECT_EQ(nx_cipher_port_query(&port, NX_CIPHER_AES_CBC, &result),
              NX_ERROR_UNSUPPORTED);
}

TEST_F(CryptoApi, HashSubmitRejectsTupleMutationWithoutCallingProvider) {
    ::testing::StrictMock<Provider> provider;
    nx_hash_ops_t ops{};
    ops.submit = Provider::hash_submit;
    nx_hash_port_t port{&ops, &provider};
    ASSERT_EQ(nx_hash_prepare(&hash, &h, 7U), NX_SUCCESS);
    EXPECT_CALL(provider, HashSubmit(&hash)).WillOnce(Return(NX_ERROR_BUSY));
    EXPECT_EQ(nx_hash_port_submit(&port, &hash), NX_ERROR_BUSY);
    hash.operation.digest = &ops;
    EXPECT_EQ(nx_hash_port_submit(&port, &hash), NX_ERROR_INVALID);
}

TEST_F(CryptoApi, MissingOptionalMethodsFailExplicitlyWithoutWork) {
    nx_cipher_ops_t ops{};
    nx_cipher_port_t port{&ops, this};
    EXPECT_EQ(nx_cipher_port_cancel(&port, &cipher), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_cipher_port_service(&port), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_cipher_port_stop(&port), NX_ERROR_UNSUPPORTED);
    EXPECT_EQ(nx_cipher_port_stop(nullptr), NX_ERROR_INVALID);
}
} /* namespace */
