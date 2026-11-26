/**
 * \file            common_shell_test.cpp
 * \brief           Actual letter-shell parser exposes only read-only
 * diagnostics \author          Nexus Team \version         1.0.0 \date
 * 2026-11-21 \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/shell_storage.h"
#include <array>
#include <deque>
#include <gtest/gtest.h>
#include <string>

namespace {
struct Terminal {
    std::deque<uint8_t> input;
    std::string output;
    static nx_result_t read(void* context, uint8_t* bytes, size_t capacity,
                            size_t* count) {
        auto& self = *static_cast<Terminal*>(context);
        *count = 0;
        if (self.input.empty() || capacity == 0)
            return NX_ERROR_EMPTY;
        bytes[0] = self.input.front();
        self.input.pop_front();
        *count = 1;
        return NX_SUCCESS;
    }
    static nx_result_t write(void* context, const void* bytes, size_t count,
                             nx_time_us_t) {
        static_cast<Terminal*>(context)->output.append(
            static_cast<const char*>(bytes), count);
        return NX_SUCCESS;
    }
    nx_shell_io_t port() {
        return {this, read, write};
    }
    void queue(const std::string& text) {
        input.insert(input.end(), text.begin(), text.end());
    }
};

TEST(CommonShell, RealParserRunsReadOnlyCommandsAndRejectsMaintenanceNames) {
    nx_shell_t shell{};
    Terminal terminal;
    std::array<char, 384> buffer{};
    nx_health_snapshot_t health{};
    nx_reset_record_t reset{};
    reset.reset_causes = 0x48;
    nx_shell_diagnostics_t diagnostics{"firmware-a613", &health, &reset,
                                       nullptr};
    ASSERT_EQ(nx_shell_init(&shell, terminal.port(), buffer.data(),
                            buffer.size(), &diagnostics, 500),
              NX_SUCCESS);
    terminal.queue(
        "version\nhealth\nfault\nreset\nresources\nsetVar x 1\nexec 123\n");
    while (!terminal.input.empty())
        ASSERT_EQ(nx_shell_step(&shell, 500), NX_SUCCESS);
    EXPECT_NE(terminal.output.find("firmware-a613"), std::string::npos);
    EXPECT_NE(terminal.output.find("unknown"), std::string::npos);
    EXPECT_NE(terminal.output.find("resources=unknown"), std::string::npos);
    EXPECT_NE(terminal.output.find("reset=0x00000048"), std::string::npos);
    EXPECT_NE(terminal.output.find("Command not Found"), std::string::npos);
    terminal.input = {0x80U, 0xFFU, 0x08U, 0x08U, '\n'};
    while (!terminal.input.empty())
        ASSERT_EQ(nx_shell_step(&shell, 500), NX_SUCCESS);
    EXPECT_EQ(nx_shell_step(&shell, 500), NX_ERROR_EMPTY);
    EXPECT_EQ(nx_shell_stop(&shell), NX_SUCCESS);
    EXPECT_EQ(nx_shell_step(&shell, 500), NX_ERROR_STATE);
}

TEST(CommonShell, OneLiveBackendIsExplicitAndCanBeReusedOnlyAfterStop) {
    nx_shell_t first{};
    nx_shell_t second{};
    Terminal terminal;
    std::array<char, 192> first_bytes{};
    std::array<char, 192> second_bytes{};
    nx_shell_diagnostics_t diagnostics{"test", nullptr, nullptr, nullptr};
    ASSERT_EQ(nx_shell_init(&first, terminal.port(), first_bytes.data(),
                            first_bytes.size(), &diagnostics, 100),
              NX_SUCCESS);
    EXPECT_EQ(nx_shell_init(&second, terminal.port(), second_bytes.data(),
                            second_bytes.size(), &diagnostics, 100),
              NX_ERROR_BUSY);
    ASSERT_EQ(nx_shell_stop(&first), NX_SUCCESS);
    ASSERT_EQ(nx_shell_init(&second, terminal.port(), second_bytes.data(),
                            second_bytes.size(), &diagnostics, 100),
              NX_SUCCESS);
    EXPECT_EQ(nx_shell_stop(&second), NX_SUCCESS);
}
} /* namespace */
