/**
 * \file            settings_test.cpp
 * \brief           Bounded settings staging and durable recovery contracts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/settings.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <string>

namespace {
class Settings : public testing::Test {
  protected:
    static constexpr uint32_t schema = 7;
    std::array<uint8_t, 1024> flash{};
    std::array<uint8_t, 32> scratch{};
    std::array<uint8_t, 256> image{};
    nx_storage_t storage{};
    nx_settings_t settings{};
    size_t mutations = 0;
    size_t fail_at = 0;
    bool partial_failure = false;
    bool fail_read = false;
    nx_storage_port_t port{this, flash.size(), 512,   4,
                           read, program,      erase, sync};

    static nx_storage_status_t read(void* ctx, size_t offset, void* output,
                                    size_t count) {
        auto& self = *static_cast<Settings*>(ctx);
        if (self.fail_read)
            return NX_STORAGE_IO;
        if (offset > self.flash.size() || count > self.flash.size() - offset)
            return NX_STORAGE_INVALID;
        std::memcpy(output, self.flash.data() + offset, count);
        return NX_STORAGE_OK;
    }

    static nx_storage_status_t program(void* ctx, size_t offset,
                                       const void* input, size_t count) {
        auto& self = *static_cast<Settings*>(ctx);
        if (offset % 4 != 0 || count % 4 != 0 || offset > self.flash.size() ||
            count > self.flash.size() - offset)
            return NX_STORAGE_INVALID;
        bool fail = ++self.mutations == self.fail_at;
        if (fail && !self.partial_failure)
            return NX_STORAGE_IO;
        const auto* bytes = static_cast<const uint8_t*>(input);
        size_t applied = fail ? count / 2 : count;
        for (size_t i = 0; i < applied; ++i) {
            if ((self.flash[offset + i] & bytes[i]) != bytes[i])
                return NX_STORAGE_IO;
            self.flash[offset + i] &= bytes[i];
        }
        return fail ? NX_STORAGE_IO : NX_STORAGE_OK;
    }

    static nx_storage_status_t erase(void* ctx, size_t offset, size_t count) {
        auto& self = *static_cast<Settings*>(ctx);
        if (offset % 512 != 0 || count % 512 != 0 ||
            offset > self.flash.size() || count > self.flash.size() - offset)
            return NX_STORAGE_INVALID;
        bool fail = ++self.mutations == self.fail_at;
        if (fail && !self.partial_failure)
            return NX_STORAGE_IO;
        size_t applied = fail ? count / 2 : count;
        std::memset(self.flash.data() + offset, 0xff, applied);
        return fail ? NX_STORAGE_IO : NX_STORAGE_OK;
    }

    static nx_storage_status_t sync(void* ctx) {
        auto& self = *static_cast<Settings*>(ctx);
        return ++self.mutations == self.fail_at ? NX_STORAGE_IO : NX_STORAGE_OK;
    }

    void SetUp() override {
        flash.fill(0xff);
        ASSERT_EQ(nx_storage_open(&storage, &port, 0, 512, scratch.data(),
                                  scratch.size()),
                  NX_STORAGE_OK);
    }

    void open(size_t bytes = 256, uint32_t entries = 8) {
        ASSERT_EQ(nx_settings_open(&settings, &storage, schema, image.data(),
                                   bytes, entries),
                  NX_STORAGE_OK);
    }

    void reopen() {
        nx_settings_close(&settings);
        ASSERT_EQ(nx_storage_open(&storage, &port, 0, 512, scratch.data(),
                                  scratch.size()),
                  NX_STORAGE_OK);
        open();
    }

    void expect_value(uint32_t key, const char* expected) {
        std::array<char, 64> output{};
        size_t count = output.size();
        ASSERT_EQ(nx_settings_get(&settings, key, output.data(), &count),
                  NX_STORAGE_OK);
        EXPECT_EQ(count, std::strlen(expected));
        EXPECT_EQ(std::memcmp(output.data(), expected, count), 0);
    }

    static void put32(uint8_t* output, uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            output[i] = static_cast<uint8_t>(value >> (i * 8U));
    }
};

TEST_F(Settings, EmptyStoreNeedsExplicitCommitAndNoImplicitDefaults) {
    open();
    EXPECT_TRUE(settings.dirty);
    EXPECT_EQ(storage.active_bank, -1);
    size_t count = 99;
    EXPECT_EQ(nx_settings_get(&settings, 1, nullptr, &count),
              NX_STORAGE_NOT_FOUND);
    EXPECT_EQ(count, 0U);
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    EXPECT_FALSE(settings.dirty);
    EXPECT_NE(storage.active_bank, -1);
    const auto before = mutations;
    EXPECT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    EXPECT_EQ(mutations, before);
    reopen();
    EXPECT_EQ(settings.count, 0U);
    EXPECT_FALSE(settings.dirty);
}

TEST_F(Settings, StagingCommitsOrderedKeysAndCopiesValues) {
    open();
    std::array<char, 3> value{'t', 'w', 'o'};
    ASSERT_EQ(nx_settings_set(&settings, 20, value.data(), value.size()),
              NX_STORAGE_OK);
    value.fill('x');
    ASSERT_EQ(nx_settings_set(&settings, 2, "first", 5), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&settings, 10, "middle", 6), NX_STORAGE_OK);
    EXPECT_EQ(storage.active_bank, -1);
    expect_value(20, "two");
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    reopen();
    expect_value(2, "first");
    expect_value(10, "middle");
    expect_value(20, "two");
    EXPECT_EQ(settings.count, 3U);
}

TEST_F(Settings, ReplaceGrowShrinkRemoveAndEmptyValuesSurviveReopen) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "a", 1), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&settings, 3, "tail", 4), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&settings, 1, "expanded", 8), NX_STORAGE_OK);
    expect_value(3, "tail");
    ASSERT_EQ(nx_settings_set(&settings, 1, "b", 1), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&settings, 2, nullptr, 0), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_remove(&settings, 1), NX_STORAGE_OK);
    EXPECT_EQ(nx_settings_remove(&settings, 1), NX_STORAGE_NOT_FOUND);
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    reopen();
    expect_value(3, "tail");
    size_t count = 1;
    EXPECT_EQ(nx_settings_get(&settings, 2, nullptr, &count), NX_STORAGE_OK);
    EXPECT_EQ(count, 0U);
    EXPECT_EQ(settings.count, 2U);
}

TEST_F(Settings, QueryAndShortOutputDoNotMutateImage) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "value", 5), NX_STORAGE_OK);
    const auto before = image;
    size_t count = 0;
    EXPECT_EQ(nx_settings_get(&settings, 1, nullptr, &count), NX_STORAGE_OK);
    EXPECT_EQ(count, 5U);
    std::array<char, 4> output{'x', 'x', 'x', 'x'};
    count = output.size();
    EXPECT_EQ(nx_settings_get(&settings, 1, output.data(), &count),
              NX_STORAGE_NO_SPACE);
    EXPECT_EQ(count, 5U);
    EXPECT_EQ(output, (std::array<char, 4>{'x', 'x', 'x', 'x'}));
    EXPECT_EQ(image, before);
}

TEST_F(Settings, ReapplyingIdenticalValueDoesNotWearFlashOrClearOtherChanges) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "value", 5), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&settings, 2, nullptr, 0), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    const auto before = mutations;
    EXPECT_EQ(nx_settings_set(&settings, 1, "value", 5), NX_STORAGE_OK);
    EXPECT_FALSE(settings.dirty);
    EXPECT_EQ(nx_settings_set(&settings, 2, nullptr, 0), NX_STORAGE_OK);
    EXPECT_FALSE(settings.dirty);
    EXPECT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    EXPECT_EQ(mutations, before);
    ASSERT_EQ(nx_settings_set(&settings, 3, "new", 3), NX_STORAGE_OK);
    EXPECT_EQ(nx_settings_set(&settings, 1, "value", 5), NX_STORAGE_OK);
    EXPECT_TRUE(settings.dirty);
}

TEST_F(Settings, EntryAndByteLimitsRejectWithoutChangingStagedValues) {
    open(40, 2);
    ASSERT_EQ(nx_settings_set(&settings, 1, "data", 4), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&settings, 2, "more", 4), NX_STORAGE_OK);
    const auto before = image;
    EXPECT_EQ(nx_settings_set(&settings, 3, nullptr, 0), NX_STORAGE_NO_SPACE);
    EXPECT_EQ(nx_settings_set(&settings, 1, "longer", 6), NX_STORAGE_NO_SPACE);
    EXPECT_EQ(image, before);
    EXPECT_EQ(settings.count, 2U);
    expect_value(1, "data");
    EXPECT_EQ(mutations, 0U);
}

TEST_F(Settings, RejectedAliasesNullAndOverflowHaveNoEffects) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "value", 5), NX_STORAGE_OK);
    const auto before = image;
    EXPECT_EQ(nx_settings_set(&settings, 2, nullptr, 1), NX_STORAGE_INVALID);
    EXPECT_EQ(nx_settings_set(&settings, 2, image.data(), 1),
              NX_STORAGE_INVALID);
    EXPECT_EQ(nx_settings_set(&settings, 2, &settings, 1), NX_STORAGE_INVALID);
    EXPECT_EQ(nx_settings_set(&settings, 2, "x", SIZE_MAX),
              NX_STORAGE_NO_SPACE);
    size_t count = image.size();
    EXPECT_EQ(nx_settings_get(&settings, 1, image.data(), &count),
              NX_STORAGE_INVALID);
    EXPECT_EQ(nx_settings_get(&settings, 1, nullptr, nullptr),
              NX_STORAGE_INVALID);
    EXPECT_EQ(image, before);
    EXPECT_EQ(mutations, 0U);
}

TEST_F(Settings, SchemaMismatchNeverRewritesExistingSettings) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "old", 3), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    nx_settings_close(&settings);
    const auto before = flash;
    EXPECT_EQ(nx_settings_open(&settings, &storage, schema + 1, image.data(),
                               image.size(), 8),
              NX_STORAGE_UNSUPPORTED);
    EXPECT_FALSE(settings.opened);
    EXPECT_EQ(flash, before);
    open();
    expect_value(1, "old");
}

TEST_F(Settings, ReadFailureDoesNotBecomeAnEmptyWritableStore) {
    open();
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    nx_settings_close(&settings);
    fail_read = true;
    EXPECT_EQ(nx_settings_open(&settings, &storage, schema, image.data(),
                               image.size(), 8),
              NX_STORAGE_IO);
    EXPECT_FALSE(settings.opened);
    EXPECT_EQ(nx_settings_commit(&settings), NX_STORAGE_INVALID);
}

TEST_F(Settings, DurablePayloadDamageNeverBecomesAnEmptyStore) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "saved", 5), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    nx_settings_close(&settings);
    const size_t payload =
        static_cast<size_t>(storage.active_bank) * 512 + storage.payload_offset;
    flash[payload] ^= 1;
    const auto damaged = flash;
    EXPECT_EQ(nx_settings_open(&settings, &storage, schema, image.data(),
                               image.size(), 8),
              NX_STORAGE_CORRUPT);
    EXPECT_FALSE(settings.opened);
    EXPECT_EQ(nx_settings_commit(&settings), NX_STORAGE_INVALID);
    EXPECT_EQ(flash, damaged);
}

TEST_F(Settings, BoundedMutationsMatchIndependentKeyValueModel) {
    open(96, 4);
    std::map<uint32_t, std::string> expected;
    const std::array<uint32_t, 6> keys{0, UINT32_MAX, 2, 3, 4, 5};
    for (size_t step = 0; step < 64; ++step) {
        SCOPED_TRACE(step);
        uint32_t key = keys[step % keys.size()];
        if (step % 7 == 0) {
            bool existed = expected.erase(key) != 0;
            EXPECT_EQ(nx_settings_remove(&settings, key),
                      existed ? NX_STORAGE_OK : NX_STORAGE_NOT_FOUND);
        } else {
            std::string value(step % 13, static_cast<char>('a' + step % 26));
            auto next = expected;
            next[key] = value;
            size_t required = NX_SETTINGS_HEADER_SIZE;
            for (const auto& entry : next)
                required += 8 + entry.second.size();
            bool fits = next.size() <= 4 && required <= 96;
            const auto before = image;
            EXPECT_EQ(
                nx_settings_set(&settings, key, value.data(), value.size()),
                fits ? NX_STORAGE_OK : NX_STORAGE_NO_SPACE);
            if (fits)
                expected = next;
            else
                EXPECT_EQ(image, before);
        }
        if (step % 5 == 0) {
            ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
            nx_settings_close(&settings);
            ASSERT_EQ(nx_storage_open(&storage, &port, 0, 512, scratch.data(),
                                      scratch.size()),
                      NX_STORAGE_OK);
            open(96, 4);
        }
        EXPECT_EQ(settings.count, expected.size());
        for (uint32_t candidate : keys) {
            const auto entry = expected.find(candidate);
            if (entry != expected.end()) {
                expect_value(candidate, entry->second.c_str());
            } else {
                size_t count = 0;
                EXPECT_EQ(
                    nx_settings_get(&settings, candidate, nullptr, &count),
                    NX_STORAGE_NOT_FOUND);
            }
        }
    }
}

TEST_F(Settings, UnknownFormatAndMalformedEntriesFailClosed) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "a", 1), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&settings, 3, "b", 1), NX_STORAGE_OK);
    const auto valid = image;
    const auto bytes = settings.size;
    nx_settings_close(&settings);
    std::array<std::array<uint8_t, 256>, 6> malformed{};
    malformed.fill(valid);
    malformed[0][0] ^= 1;
    put32(malformed[1].data() + 4, 2);
    put32(malformed[2].data() + 12, 3);
    put32(malformed[3].data() + 20, UINT32_MAX);
    put32(malformed[4].data() + 25, 1);
    put32(malformed[5].data() + 25, 0);
    for (size_t i = 0; i < malformed.size(); ++i) {
        SCOPED_TRACE(i);
        ASSERT_EQ(nx_storage_save(&storage, malformed[i].data(), bytes),
                  NX_STORAGE_OK);
        EXPECT_EQ(nx_settings_open(&settings, &storage, schema, image.data(),
                                   image.size(), 8),
                  i == 1 ? NX_STORAGE_UNSUPPORTED : NX_STORAGE_CORRUPT);
        EXPECT_FALSE(settings.opened);
    }
}

TEST_F(Settings, LoadedEntryLimitAndImageCapacityAreExplicit) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "a", 1), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&settings, 2, "b", 1), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    nx_settings_close(&settings);
    EXPECT_EQ(nx_settings_open(&settings, &storage, schema, image.data(),
                               image.size(), 1),
              NX_STORAGE_NO_SPACE);
    EXPECT_FALSE(settings.opened);
    EXPECT_EQ(
        nx_settings_open(&settings, &storage, schema, image.data(), 16, 8),
        NX_STORAGE_NO_SPACE);
    EXPECT_FALSE(settings.opened);
}

TEST_F(Settings, InvalidOpenDoesNotTouchFlashOrOtherStorage) {
    EXPECT_EQ(nx_settings_open(nullptr, &storage, schema, image.data(),
                               image.size(), 8),
              NX_STORAGE_INVALID);
    EXPECT_EQ(nx_settings_open(&settings, &storage, schema, scratch.data(),
                               scratch.size(), 8),
              NX_STORAGE_INVALID);
    EXPECT_EQ(
        nx_settings_open(&settings, &storage, schema, image.data(), 15, 8),
        NX_STORAGE_INVALID);
    EXPECT_EQ(nx_settings_open(&settings, &storage, schema, image.data(),
                               image.size(), 0),
              NX_STORAGE_INVALID);
    EXPECT_EQ(mutations, 0U);
}

TEST_F(Settings, FailedCommitRequiresRecoveryAndEveryCutKeepsWholeSnapshot) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "old", 3), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    const auto committed = flash;
    ASSERT_EQ(nx_settings_set(&settings, 1, "new-value", 9), NX_STORAGE_OK);
    mutations = 0;
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    const auto commit_steps = mutations;
    ASSERT_GT(commit_steps, 4U);
    for (bool partial : {false, true}) {
        for (size_t step = 1; step <= commit_steps; ++step) {
            SCOPED_TRACE(step);
            SCOPED_TRACE(partial);
            fail_at = 0;
            flash = committed;
            reopen();
            ASSERT_EQ(nx_settings_set(&settings, 1, "new-value", 9),
                      NX_STORAGE_OK);
            mutations = 0;
            fail_at = step;
            partial_failure = partial;
            EXPECT_EQ(nx_settings_commit(&settings), NX_STORAGE_IO);
            EXPECT_FALSE(settings.opened);
            EXPECT_TRUE(settings.dirty);
            EXPECT_EQ(nx_settings_set(&settings, 2, "bad", 3),
                      NX_STORAGE_INVALID);
            EXPECT_EQ(nx_settings_commit(&settings), NX_STORAGE_INVALID);
            fail_at = 0;
            reopen();
            std::array<char, 16> output{};
            size_t count = output.size();
            ASSERT_EQ(nx_settings_get(&settings, 1, output.data(), &count),
                      NX_STORAGE_OK);
            bool old = count == 3 && std::memcmp(output.data(), "old", 3) == 0;
            bool next =
                count == 9 && std::memcmp(output.data(), "new-value", 9) == 0;
            EXPECT_TRUE(old || next);
        }
    }
}

TEST_F(Settings, CloseDiscardsUncommittedChangesAndInstancesStayIndependent) {
    open();
    ASSERT_EQ(nx_settings_set(&settings, 1, "saved", 5), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_commit(&settings), NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&settings, 1, "pending", 7), NX_STORAGE_OK);
    nx_settings_close(&settings);
    EXPECT_FALSE(settings.opened);
    EXPECT_EQ(settings.image, nullptr);
    EXPECT_EQ(nx_settings_remove(&settings, 1), NX_STORAGE_INVALID);
    open();
    expect_value(1, "saved");
    nx_settings_t other{};
    std::array<uint8_t, 256> other_image{};
    ASSERT_EQ(nx_settings_open(&other, &storage, schema, other_image.data(),
                               other_image.size(), 8),
              NX_STORAGE_OK);
    ASSERT_EQ(nx_settings_set(&other, 2, "second", 6), NX_STORAGE_OK);
    size_t count = 0;
    EXPECT_EQ(nx_settings_get(&settings, 2, nullptr, &count),
              NX_STORAGE_NOT_FOUND);
    nx_settings_close(&other);
}
} /* namespace */
