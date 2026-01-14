/**
 * \file            test_shell_history_properties.cpp
 * \brief           Shell History Manager Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Shell history manager functionality.
 * These tests verify universal properties that should hold for all valid
 * inputs. Each property test runs 100+ iterations with random inputs.
 *
 * Feature: shell-cli-middleware
 * **Validates: Requirements 5.1, 5.2, 5.3, 5.5, 5.6**
 */

#include <algorithm>
#include <deque>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>

extern "C" {
#include "shell/shell_history.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Shell History Property Test Fixture
 */
class HistoryPropertyTest : public ::testing::Test {
  protected:
    static constexpr uint8_t HISTORY_CAPACITY = 16;
    static constexpr uint16_t ENTRY_SIZE = 64;

    std::mt19937 rng;
    history_manager_t history;
    char* entries[HISTORY_CAPACITY];
    char entry_buffers[HISTORY_CAPACITY][ENTRY_SIZE];

    void SetUp() override {
        rng.seed(std::random_device{}());

        /* Set up entry pointers */
        for (int i = 0; i < HISTORY_CAPACITY; i++) {
            entries[i] = entry_buffers[i];
            memset(entry_buffers[i], 0, ENTRY_SIZE);
        }
        history_init(&history, entries, HISTORY_CAPACITY, ENTRY_SIZE);
    }

    void TearDown() override {
        history_deinit(&history);
    }

    /**
     * \brief           Generate a random non-empty command string
     */
    std::string randomCommand(int minLen = 3, int maxLen = 30) {
        std::uniform_int_distribution<int> lenDist(minLen, maxLen);
        std::uniform_int_distribution<int> charDist(0, 61);

        int len = lenDist(rng);
        std::string str;
        str.reserve(len);

        for (int i = 0; i < len; ++i) {
            int c = charDist(rng);
            if (c < 26) {
                str += static_cast<char>('a' + c);
            } else if (c < 52) {
                str += static_cast<char>('A' + (c - 26));
            } else {
                str += static_cast<char>('0' + (c - 52));
            }
        }
        return str;
    }

    /**
     * \brief           Generate a list of unique random commands
     */
    std::vector<std::string> generateUniqueCommands(int count) {
        std::vector<std::string> commands;
        commands.reserve(count);

        while (static_cast<int>(commands.size()) < count) {
            std::string cmd = randomCommand();
            /* Ensure uniqueness */
            bool unique = true;
            for (const auto& existing : commands) {
                if (existing == cmd) {
                    unique = false;
                    break;
                }
            }
            if (unique) {
                commands.push_back(cmd);
            }
        }
        return commands;
    }
};

/*---------------------------------------------------------------------------*/
/* Property 5: History FIFO Order                                            */
/* *For any* sequence of executed commands, the history SHALL maintain FIFO  */
/* order, with the most recent command accessible via single Up arrow press, */
/* and oldest command removed when capacity is exceeded.                     */
/* **Validates: Requirements 5.1, 5.2, 5.3, 5.5**                            */
/*---------------------------------------------------------------------------*/

/**
 * Feature: shell-cli-middleware, Property 5: History FIFO Order
 *
 * *For any* sequence of executed commands, the history SHALL maintain FIFO
 * order, with the most recent command accessible via single Up arrow press,
 * and oldest command removed when capacity is exceeded.
 *
 * **Validates: Requirements 5.1, 5.2, 5.3, 5.5**
 */
TEST_F(HistoryPropertyTest, Property5_HistoryFIFOOrder) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Reset history for each iteration */
        history_clear(&history);

        /* Generate random number of unique commands (1 to 2x capacity) */
        std::uniform_int_distribution<int> countDist(1, HISTORY_CAPACITY * 2);
        int cmdCount = countDist(rng);

        std::vector<std::string> commands = generateUniqueCommands(cmdCount);

        /* Add all commands to history */
        for (const auto& cmd : commands) {
            history_add(&history, cmd.c_str());
        }

        /* Calculate expected history state */
        /* History should contain the most recent min(cmdCount, capacity)
         * commands */
        int expectedCount =
            std::min(cmdCount, static_cast<int>(HISTORY_CAPACITY));

        EXPECT_EQ(expectedCount, history_get_count(&history))
            << "Iter " << iter << ": count mismatch";

        /* Verify FIFO order: index 0 should be most recent */
        for (int i = 0; i < expectedCount; ++i) {
            /* Expected command at index i is commands[cmdCount - 1 - i] */
            int srcIdx = cmdCount - 1 - i;
            const char* entry =
                history_get_entry(&history, static_cast<uint8_t>(i));

            ASSERT_NE(nullptr, entry)
                << "Iter " << iter << ": entry " << i << " is null";
            EXPECT_STREQ(commands[srcIdx].c_str(), entry)
                << "Iter " << iter << ": FIFO order violated at index " << i;
        }

        /* Verify navigation order matches FIFO */
        history_reset_browse(&history);
        for (int i = 0; i < expectedCount; ++i) {
            const char* prev = history_get_prev(&history);
            int srcIdx = cmdCount - 1 - i;

            ASSERT_NE(nullptr, prev)
                << "Iter " << iter << ": get_prev returned null at step " << i;
            EXPECT_STREQ(commands[srcIdx].c_str(), prev)
                << "Iter " << iter << ": navigation order violated at step "
                << i;
        }
    }
}

/**
 * Feature: shell-cli-middleware, Property 5a: Most Recent First
 *
 * *For any* non-empty history, the first call to history_get_prev SHALL
 * return the most recently added command.
 *
 * **Validates: Requirements 5.2**
 */
TEST_F(HistoryPropertyTest, Property5a_MostRecentFirst) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        history_clear(&history);

        /* Add random number of unique commands */
        std::uniform_int_distribution<int> countDist(1, HISTORY_CAPACITY);
        int cmdCount = countDist(rng);

        std::vector<std::string> commands = generateUniqueCommands(cmdCount);

        for (const auto& cmd : commands) {
            history_add(&history, cmd.c_str());
        }

        /* First get_prev should return the last added command */
        const char* first = history_get_prev(&history);

        ASSERT_NE(nullptr, first)
            << "Iter " << iter << ": get_prev returned null";
        EXPECT_STREQ(commands.back().c_str(), first)
            << "Iter " << iter << ": first prev should be most recent";
    }
}

/**
 * Feature: shell-cli-middleware, Property 5b: Capacity Overflow Removes Oldest
 *
 * *For any* sequence of commands exceeding capacity, the oldest commands
 * SHALL be removed to make room for new ones.
 *
 * **Validates: Requirements 5.5**
 */
TEST_F(HistoryPropertyTest, Property5b_CapacityOverflowRemovesOldest) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        history_clear(&history);

        /* Generate more commands than capacity */
        std::uniform_int_distribution<int> overflowDist(1, HISTORY_CAPACITY);
        int overflow = overflowDist(rng);
        int totalCommands = HISTORY_CAPACITY + overflow;

        std::vector<std::string> commands =
            generateUniqueCommands(totalCommands);

        for (const auto& cmd : commands) {
            history_add(&history, cmd.c_str());
        }

        /* History should be at capacity */
        EXPECT_EQ(HISTORY_CAPACITY, history_get_count(&history))
            << "Iter " << iter << ": should be at capacity";

        /* Oldest entries (first 'overflow' commands) should be gone */
        /* Newest entries (last CAPACITY commands) should be present */
        for (int i = 0; i < HISTORY_CAPACITY; ++i) {
            int srcIdx = totalCommands - 1 - i;
            const char* entry =
                history_get_entry(&history, static_cast<uint8_t>(i));

            ASSERT_NE(nullptr, entry)
                << "Iter " << iter << ": entry " << i << " is null";
            EXPECT_STREQ(commands[srcIdx].c_str(), entry)
                << "Iter " << iter << ": wrong entry at index " << i;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Property 6: History Deduplication                                         */
/* *For any* sequence of commands where consecutive commands are identical,  */
/* the history SHALL contain only one entry for the consecutive duplicates.  */
/* **Validates: Requirements 5.6**                                           */
/*---------------------------------------------------------------------------*/

/**
 * Feature: shell-cli-middleware, Property 6: History Deduplication
 *
 * *For any* sequence of commands where consecutive commands are identical,
 * the history SHALL contain only one entry for the consecutive duplicates.
 *
 * **Validates: Requirements 5.6**
 */
TEST_F(HistoryPropertyTest, Property6_HistoryDeduplication) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        history_clear(&history);

        /* Generate a sequence with some consecutive duplicates */
        std::uniform_int_distribution<int> baseCmdDist(3, 8);
        int baseCount = baseCmdDist(rng);

        std::vector<std::string> baseCommands =
            generateUniqueCommands(baseCount);

        /* Build sequence with random consecutive duplicates */
        std::vector<std::string> sequence;
        std::vector<std::string>
            expectedHistory; /* What should end up in history */

        for (const auto& cmd : baseCommands) {
            /* Add 1-4 consecutive copies */
            std::uniform_int_distribution<int> repeatDist(1, 4);
            int repeats = repeatDist(rng);

            for (int r = 0; r < repeats; ++r) {
                sequence.push_back(cmd);
            }

            /* Only one copy should end up in history */
            expectedHistory.push_back(cmd);
        }

        /* Add all commands to history */
        for (const auto& cmd : sequence) {
            history_add(&history, cmd.c_str());
        }

        /* History should have deduplicated consecutive duplicates */
        int expectedCount = std::min(static_cast<int>(expectedHistory.size()),
                                     static_cast<int>(HISTORY_CAPACITY));

        EXPECT_EQ(expectedCount, history_get_count(&history))
            << "Iter " << iter << ": count mismatch after deduplication";

        /* Verify the entries match expected (most recent first) */
        for (int i = 0; i < expectedCount; ++i) {
            int srcIdx = static_cast<int>(expectedHistory.size()) - 1 - i;
            const char* entry =
                history_get_entry(&history, static_cast<uint8_t>(i));

            ASSERT_NE(nullptr, entry)
                << "Iter " << iter << ": entry " << i << " is null";
            EXPECT_STREQ(expectedHistory[srcIdx].c_str(), entry)
                << "Iter " << iter << ": deduplication failed at index " << i;
        }
    }
}

/**
 * Feature: shell-cli-middleware, Property 6a: Non-Consecutive Duplicates
 * Allowed
 *
 * *For any* sequence where the same command appears non-consecutively,
 * all occurrences SHALL be stored in history.
 *
 * **Validates: Requirements 5.6**
 */
TEST_F(HistoryPropertyTest, Property6a_NonConsecutiveDuplicatesAllowed) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        history_clear(&history);

        /* Create a pattern: A, B, A, B, A (non-consecutive duplicates) */
        std::string cmdA = randomCommand();
        std::string cmdB = randomCommand();

        /* Ensure they're different */
        while (cmdA == cmdB) {
            cmdB = randomCommand();
        }

        /* Add alternating pattern */
        std::uniform_int_distribution<int> repeatDist(2, 4);
        int repeats = repeatDist(rng);

        std::vector<std::string> sequence;
        for (int i = 0; i < repeats; ++i) {
            sequence.push_back(cmdA);
            sequence.push_back(cmdB);
        }

        for (const auto& cmd : sequence) {
            history_add(&history, cmd.c_str());
        }

        /* All entries should be stored (non-consecutive duplicates allowed) */
        int expectedCount = std::min(static_cast<int>(sequence.size()),
                                     static_cast<int>(HISTORY_CAPACITY));

        EXPECT_EQ(expectedCount, history_get_count(&history))
            << "Iter " << iter
            << ": non-consecutive duplicates should be stored";
    }
}

/**
 * Feature: shell-cli-middleware, Property 6b: Single Duplicate Rejection
 *
 * *For any* command added immediately after itself, the second add
 * SHALL be rejected and history count SHALL remain unchanged.
 *
 * **Validates: Requirements 5.6**
 */
TEST_F(HistoryPropertyTest, Property6b_SingleDuplicateRejection) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        history_clear(&history);

        std::string cmd = randomCommand();

        /* Add command first time */
        EXPECT_TRUE(history_add(&history, cmd.c_str()))
            << "Iter " << iter << ": first add should succeed";
        EXPECT_EQ(1, history_get_count(&history));

        /* Add same command again */
        EXPECT_FALSE(history_add(&history, cmd.c_str()))
            << "Iter " << iter << ": duplicate add should be rejected";
        EXPECT_EQ(1, history_get_count(&history))
            << "Iter " << iter << ": count should not change after duplicate";
    }
}

/*---------------------------------------------------------------------------*/
/* Additional Property Tests                                                 */
/*---------------------------------------------------------------------------*/

/**
 * Feature: shell-cli-middleware, Property 5c: Navigation Round-Trip
 *
 * *For any* history state, navigating to the oldest entry and back to
 * the newest SHALL return to the same state (current input).
 *
 * **Validates: Requirements 5.2, 5.3**
 */
TEST_F(HistoryPropertyTest, Property5c_NavigationRoundTrip) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        history_clear(&history);

        /* Add random commands */
        std::uniform_int_distribution<int> countDist(1, HISTORY_CAPACITY);
        int cmdCount = countDist(rng);

        std::vector<std::string> commands = generateUniqueCommands(cmdCount);

        for (const auto& cmd : commands) {
            history_add(&history, cmd.c_str());
        }

        /* Navigate all the way back (to oldest) */
        for (int i = 0; i < cmdCount; ++i) {
            history_get_prev(&history);
        }

        EXPECT_TRUE(history_is_browsing(&history))
            << "Iter " << iter << ": should be browsing after prev";

        /* Navigate all the way forward (to current input) */
        for (int i = 0; i < cmdCount; ++i) {
            history_get_next(&history);
        }

        EXPECT_FALSE(history_is_browsing(&history))
            << "Iter " << iter << ": should return to current input";
    }
}

/**
 * Feature: shell-cli-middleware, Property 5d: Empty Command Rejection
 *
 * *For any* empty or whitespace-only string, history_add SHALL return
 * false and not modify the history.
 *
 * **Validates: Requirements 5.7**
 */
TEST_F(HistoryPropertyTest, Property5d_EmptyCommandRejection) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        history_clear(&history);

        /* Add some valid commands first */
        std::uniform_int_distribution<int> countDist(0, 5);
        int initialCount = countDist(rng);

        for (int i = 0; i < initialCount; ++i) {
            std::string cmd = randomCommand();
            history_add(&history, cmd.c_str());
        }

        uint8_t countBefore = history_get_count(&history);

        /* Generate random whitespace-only string */
        std::uniform_int_distribution<int> lenDist(0, 10);
        std::uniform_int_distribution<int> wsDist(0, 3);
        int len = lenDist(rng);

        std::string whitespace;
        for (int i = 0; i < len; ++i) {
            switch (wsDist(rng)) {
                case 0:
                    whitespace += ' ';
                    break;
                case 1:
                    whitespace += '\t';
                    break;
                case 2:
                    whitespace += '\n';
                    break;
                case 3:
                    whitespace += '\r';
                    break;
            }
        }

        /* Try to add whitespace-only command */
        EXPECT_FALSE(history_add(&history, whitespace.c_str()))
            << "Iter " << iter << ": whitespace command should be rejected";
        EXPECT_EQ(countBefore, history_get_count(&history))
            << "Iter " << iter << ": count should not change";
    }
}
