/** Management-owner access tests. Config APIs are externally serialized;
 * passing these tests does not establish unguarded multi-thread safety. */
#include "config/config.h"
#include <atomic>
#include <cstdio>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <vector>

class ConfigSerializedAccessTest : public ::testing::Test {
  protected:
    std::mutex owner;
    void SetUp() override {
        if (config_is_initialized()) config_deinit();
        ASSERT_EQ(CONFIG_OK, config_init(nullptr));
    }
    void TearDown() override {
        if (config_is_initialized()) config_deinit();
    }
};

TEST_F(ConfigSerializedAccessTest, SerializedWritersPreserveAllFinalValues) {
    std::atomic<unsigned> failures{0};
    std::vector<std::thread> workers;
    for (unsigned t = 0; t < 4; ++t) {
        workers.emplace_back([&, t] {
            char key[32]; std::snprintf(key, sizeof(key), "worker%u.value", t);
            for (int32_t i = 0; i < 100; ++i) {
                std::lock_guard<std::mutex> request(owner);
                if (config_set_i32(key, i) != CONFIG_OK) ++failures;
            }
        });
    }
    for (auto& worker : workers) worker.join();
    EXPECT_EQ(0u, failures.load());
    for (unsigned t = 0; t < 4; ++t) {
        char key[32]; std::snprintf(key, sizeof(key), "worker%u.value", t);
        int32_t value = -1;
        ASSERT_EQ(CONFIG_OK, config_get_i32(key, &value, -1));
        EXPECT_EQ(99, value);
    }
}

TEST_F(ConfigSerializedAccessTest, ReadsAndWritesUseTheSameManagementOwner) {
    ASSERT_EQ(CONFIG_OK, config_set_i32("shared", 0));
    std::atomic<unsigned> failures{0};
    std::vector<std::thread> workers;
    for (unsigned t = 0; t < 4; ++t) {
        workers.emplace_back([&, t] {
            for (int32_t i = 0; i < 200; ++i) {
                std::lock_guard<std::mutex> request(owner);
                if (t % 2) {
                    int32_t value = -1;
                    if (config_get_i32("shared", &value, -1) != CONFIG_OK ||
                        value < 0 || value >= 200) ++failures;
                } else if (config_set_i32("shared", i) != CONFIG_OK) ++failures;
            }
        });
    }
    for (auto& worker : workers) worker.join();
    EXPECT_EQ(0u, failures.load());
}

TEST_F(ConfigSerializedAccessTest, NamespaceSessionsPreserveIsolation) {
    std::atomic<unsigned> failures{0};
    std::vector<std::thread> workers;
    for (unsigned t = 0; t < 4; ++t) {
        workers.emplace_back([&, t] {
            char name[16]; std::snprintf(name, sizeof(name), "domain%u", t);
            for (int32_t i = 0; i < 50; ++i) {
                std::lock_guard<std::mutex> request(owner);
                config_ns_handle_t ns;
                if (config_open_namespace(name, &ns) != CONFIG_OK) {
                    ++failures; continue;
                }
                if (config_ns_set_i32(ns, "value", i) != CONFIG_OK) ++failures;
                int32_t value = -1;
                if (config_ns_get_i32(ns, "value", &value, -1) != CONFIG_OK ||
                    value != i) ++failures;
                if (config_close_namespace(ns) != CONFIG_OK) ++failures;
            }
        });
    }
    for (auto& worker : workers) worker.join();
    EXPECT_EQ(0u, failures.load());
    for (unsigned t = 0; t < 4; ++t) {
        char name[16]; std::snprintf(name, sizeof(name), "domain%u", t);
        config_ns_handle_t ns;
        ASSERT_EQ(CONFIG_OK, config_open_namespace(name, &ns));
        int32_t value = -1;
        ASSERT_EQ(CONFIG_OK, config_ns_get_i32(ns, "value", &value, -1));
        EXPECT_EQ(49, value);
        EXPECT_EQ(CONFIG_OK, config_close_namespace(ns));
    }
}
