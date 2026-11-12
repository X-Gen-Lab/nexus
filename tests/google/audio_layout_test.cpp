/**
 * \file            audio_layout_test.cpp
 * \brief           Public read-only audio storage and exact provider dispatch
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/audio.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

/* Genuine feature-absence RED can link before this new public C entry exists.
 * This does not simulate provider or serializer behavior. */
extern "C" nx_result_t nx_audio_port_layout(const nx_audio_port_t*,
                                            nx_audio_layout_t*)
    __attribute__((weak));

namespace {
using ::testing::_;
using ::testing::Return;

class Provider {
  public:
    MOCK_METHOD(nx_result_t, Layout, (nx_audio_layout_t*));
    static nx_result_t layout(void* context, nx_audio_layout_t* result) {
        return static_cast<Provider*>(context)->Layout(result);
    }
};
class AudioLayout : public testing::Test {
  protected:
    testing::StrictMock<Provider> provider;
    nx_audio_ops_t ops{};
    nx_audio_port_t port{&ops, &provider};
    nx_audio_layout_t output{};
    void SetUp() override {
        auto* const entry = &nx_audio_port_layout;
        ASSERT_NE(nullptr, entry);
        ops.layout = Provider::layout;
    }
};

TEST_F(AudioLayout, DispatchPreservesDifferentSampleStorageAndExactContext) {
    EXPECT_CALL(provider, Layout(&output))
        .WillOnce([](nx_audio_layout_t* result) {
            *result = {NX_AUDIO_STORAGE_HIGH_HALFWORD_FIRST,
                       NX_FRAME_SAMPLE_MSB,
                       3U,
                       24U,
                       32U,
                       2U};
            return NX_SUCCESS;
        })
        .WillOnce([](nx_audio_layout_t* result) {
            *result = {NX_AUDIO_STORAGE_LITTLE_ENDIAN,
                       NX_FRAME_SAMPLE_LSB,
                       3U,
                       24U,
                       32U,
                       2U};
            return NX_SUCCESS;
        });
    ASSERT_EQ(NX_SUCCESS, nx_audio_port_layout(&port, &output));
    EXPECT_EQ(NX_AUDIO_STORAGE_HIGH_HALFWORD_FIRST, output.storage_order);
    EXPECT_EQ(NX_FRAME_SAMPLE_MSB, output.sample_alignment);
    ASSERT_EQ(NX_SUCCESS, nx_audio_port_layout(&port, &output));
    EXPECT_EQ(NX_AUDIO_STORAGE_LITTLE_ENDIAN, output.storage_order);
    EXPECT_EQ(NX_FRAME_SAMPLE_LSB, output.sample_alignment);
    EXPECT_EQ(24U, output.sample_bits);
    EXPECT_EQ(32U, output.container_bits);
    EXPECT_EQ(2U, output.slots);
    EXPECT_EQ(3U, output.active_slots);
}

TEST_F(AudioLayout, MalformedOrAliasedPublicStorageNeverReachesTheProvider) {
    EXPECT_EQ(NX_ERROR_INVALID, nx_audio_port_layout(nullptr, &output));
    EXPECT_EQ(NX_ERROR_INVALID, nx_audio_port_layout(&port, nullptr));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_audio_port_layout(&port, reinterpret_cast<nx_audio_layout_t*>(
                                              UINTPTR_MAX - 3U)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_audio_port_layout(
                  &port, reinterpret_cast<nx_audio_layout_t*>(&port)));
    EXPECT_EQ(NX_ERROR_INVALID,
              nx_audio_port_layout(&port,
                                   reinterpret_cast<nx_audio_layout_t*>(&ops)));
    EXPECT_EQ(&ops, port.ops);
    EXPECT_EQ(&provider, port.context);
}

TEST_F(AudioLayout, OriginalOutputIsNotHiddenFromPrivateAdmission) {
    /* The private owner must see this original alias, rather than a temporary
     * that could bypass its own state/config/active-buffer checks. */
    auto* alias = reinterpret_cast<nx_audio_layout_t*>(&provider);
    EXPECT_CALL(provider, Layout(alias)).WillOnce(Return(NX_ERROR_INVALID));
    EXPECT_EQ(NX_ERROR_INVALID, nx_audio_port_layout(&port, alias));
}

TEST_F(AudioLayout, MissingMethodAndProviderRejectionLeaveTheOutputUnchanged) {
    std::memset(&output, 0x5a, sizeof(output));
    const nx_audio_layout_t before = output;
    ops.layout = nullptr;
    EXPECT_EQ(NX_ERROR_UNSUPPORTED, nx_audio_port_layout(&port, &output));
    EXPECT_EQ(0, std::memcmp(&before, &output, sizeof(output)));
    ops.layout = Provider::layout;
    EXPECT_CALL(provider, Layout(&output)).WillOnce(Return(NX_ERROR_STATE));
    EXPECT_EQ(NX_ERROR_STATE, nx_audio_port_layout(&port, &output));
    EXPECT_EQ(0, std::memcmp(&before, &output, sizeof(output)));
}
} /* namespace */
