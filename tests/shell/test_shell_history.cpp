/**
 * \file            test_shell_history.cpp
 * \brief           Shell History Manager Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for Shell history manager functionality.
 * Requirements: 5.1, 5.2, 5.3, 5.5, 5.6
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "shell/shell_history.h"
}

/**
 * \brief           History Manager Test Fixture
 */
class HistoryTest : public ::testing::Test {
  protected:
    static constexpr uint8_t HISTORY_CAPACITY = 8;
    static constexpr uint16_t ENTRY_SIZE = 64;

    history_manager_t history;
    char* entries[HISTORY_CAPACITY];
    char entry_buffers[HISTORY_CAPACITY][ENTRY_SIZE];

    void SetUp() override {
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
};

/*---------------------------------------------------------------------------*/
/* Initialization Tests                                                      */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, InitSetsCorrectState) {
    EXPECT_EQ(0, history_get_count(&history));
    EXPECT_FALSE(history_is_browsing(&history));
}

TEST_F(HistoryTest, InitWithNullHistory) {
    /* Should not crash */
    history_init(nullptr, entries, HISTORY_CAPACITY, ENTRY_SIZE);
}

TEST_F(HistoryTest, InitWithNullEntries) {
    history_manager_t hist;
    history_init(&hist, nullptr, HISTORY_CAPACITY, ENTRY_SIZE);
    EXPECT_EQ(0, history_get_count(&hist));
}

TEST_F(HistoryTest, DeinitResetsState) {
    history_add(&history, "test");
    history_deinit(&history);
    EXPECT_EQ(0, history_get_count(&history));
}

TEST_F(HistoryTest, DeinitWithNullHistory) {
    /* Should not crash */
    history_deinit(nullptr);
}

/*---------------------------------------------------------------------------*/
/* Add Command Tests - Requirements 5.1, 5.6, 5.7                            */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, AddSingleCommand) {
    EXPECT_TRUE(history_add(&history, "help"));
    EXPECT_EQ(1, history_get_count(&history));
    EXPECT_STREQ("help", history_get_entry(&history, 0));
}

TEST_F(HistoryTest, AddMultipleCommands) {
    EXPECT_TRUE(history_add(&history, "cmd1"));
    EXPECT_TRUE(history_add(&history, "cmd2"));
    EXPECT_TRUE(history_add(&history, "cmd3"));

    EXPECT_EQ(3, history_get_count(&history));
    /* Index 0 is most recent */
    EXPECT_STREQ("cmd3", history_get_entry(&history, 0));
    EXPECT_STREQ("cmd2", history_get_entry(&history, 1));
    EXPECT_STREQ("cmd1", history_get_entry(&history, 2));
}

TEST_F(HistoryTest, AddEmptyCommandRejected) {
    EXPECT_FALSE(history_add(&history, ""));
    EXPECT_EQ(0, history_get_count(&history));
}

TEST_F(HistoryTest, AddWhitespaceOnlyCommandRejected) {
    EXPECT_FALSE(history_add(&history, "   "));
    EXPECT_FALSE(history_add(&history, "\t\t"));
    EXPECT_FALSE(history_add(&history, " \t \n "));
    EXPECT_EQ(0, history_get_count(&history));
}

TEST_F(HistoryTest, AddNullCommandRejected) {
    EXPECT_FALSE(history_add(&history, nullptr));
    EXPECT_EQ(0, history_get_count(&history));
}

TEST_F(HistoryTest, AddDuplicateConsecutiveRejected) {
    EXPECT_TRUE(history_add(&history, "help"));
    EXPECT_FALSE(history_add(&history, "help")); /* Duplicate */
    EXPECT_EQ(1, history_get_count(&history));
}

TEST_F(HistoryTest, AddDuplicateNonConsecutiveAllowed) {
    EXPECT_TRUE(history_add(&history, "help"));
    EXPECT_TRUE(history_add(&history, "version"));
    EXPECT_TRUE(history_add(&history, "help")); /* Not consecutive duplicate */
    EXPECT_EQ(3, history_get_count(&history));
}

TEST_F(HistoryTest, AddWithNullHistory) {
    EXPECT_FALSE(history_add(nullptr, "test"));
}

/*---------------------------------------------------------------------------*/
/* Capacity and FIFO Tests - Requirements 5.4, 5.5                           */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, FillToCapacity) {
    for (int i = 0; i < HISTORY_CAPACITY; i++) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "cmd%d", i);
        EXPECT_TRUE(history_add(&history, cmd));
    }

    EXPECT_EQ(HISTORY_CAPACITY, history_get_count(&history));
}

TEST_F(HistoryTest, FIFORemovesOldest) {
    /* Fill history */
    for (int i = 0; i < HISTORY_CAPACITY; i++) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "cmd%d", i);
        history_add(&history, cmd);
    }

    /* Add one more - should remove oldest (cmd0) */
    EXPECT_TRUE(history_add(&history, "new_cmd"));
    EXPECT_EQ(HISTORY_CAPACITY, history_get_count(&history));

    /* Most recent should be new_cmd */
    EXPECT_STREQ("new_cmd", history_get_entry(&history, 0));

    /* Oldest should now be cmd1 (cmd0 was removed) */
    EXPECT_STREQ("cmd1", history_get_entry(&history, HISTORY_CAPACITY - 1));
}

TEST_F(HistoryTest, FIFOWrapsAround) {
    /* Fill and overflow multiple times */
    for (int i = 0; i < HISTORY_CAPACITY * 2; i++) {
        char cmd[16];
        snprintf(cmd, sizeof(cmd), "cmd%d", i);
        history_add(&history, cmd);
    }

    EXPECT_EQ(HISTORY_CAPACITY, history_get_count(&history));

    /* Most recent should be the last added */
    char expected[16];
    snprintf(expected, sizeof(expected), "cmd%d", HISTORY_CAPACITY * 2 - 1);
    EXPECT_STREQ(expected, history_get_entry(&history, 0));
}

/*---------------------------------------------------------------------------*/
/* Browse Navigation Tests - Requirements 5.2, 5.3                           */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, GetPrevFromEmpty) {
    EXPECT_EQ(nullptr, history_get_prev(&history));
}

TEST_F(HistoryTest, GetPrevSingleEntry) {
    history_add(&history, "cmd1");

    EXPECT_STREQ("cmd1", history_get_prev(&history));
    EXPECT_TRUE(history_is_browsing(&history));

    /* Second call should return same (oldest) */
    EXPECT_STREQ("cmd1", history_get_prev(&history));
}

TEST_F(HistoryTest, GetPrevMultipleEntries) {
    history_add(&history, "cmd1");
    history_add(&history, "cmd2");
    history_add(&history, "cmd3");

    /* Navigate backward through history */
    EXPECT_STREQ("cmd3", history_get_prev(&history)); /* Most recent */
    EXPECT_STREQ("cmd2", history_get_prev(&history));
    EXPECT_STREQ("cmd1", history_get_prev(&history)); /* Oldest */

    /* At oldest, should stay there */
    EXPECT_STREQ("cmd1", history_get_prev(&history));
}

TEST_F(HistoryTest, GetNextFromNotBrowsing) {
    history_add(&history, "cmd1");

    /* Not browsing, should return NULL */
    EXPECT_EQ(nullptr, history_get_next(&history));
}

TEST_F(HistoryTest, GetNextAfterPrev) {
    history_add(&history, "cmd1");
    history_add(&history, "cmd2");
    history_add(&history, "cmd3");

    /* Go back to oldest */
    history_get_prev(&history); /* cmd3 */
    history_get_prev(&history); /* cmd2 */
    history_get_prev(&history); /* cmd1 */

    /* Navigate forward */
    EXPECT_STREQ("cmd2", history_get_next(&history));
    EXPECT_STREQ("cmd3", history_get_next(&history));

    /* At newest, next returns NULL (back to current input) */
    EXPECT_EQ(nullptr, history_get_next(&history));
    EXPECT_FALSE(history_is_browsing(&history));
}

TEST_F(HistoryTest, GetPrevWithNullHistory) {
    EXPECT_EQ(nullptr, history_get_prev(nullptr));
}

TEST_F(HistoryTest, GetNextWithNullHistory) {
    EXPECT_EQ(nullptr, history_get_next(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Reset Browse Tests                                                        */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, ResetBrowseWhileBrowsing) {
    history_add(&history, "cmd1");
    history_add(&history, "cmd2");

    history_get_prev(&history);
    EXPECT_TRUE(history_is_browsing(&history));

    history_reset_browse(&history);
    EXPECT_FALSE(history_is_browsing(&history));
}

TEST_F(HistoryTest, ResetBrowseWhenNotBrowsing) {
    history_add(&history, "cmd1");

    EXPECT_FALSE(history_is_browsing(&history));
    history_reset_browse(&history);
    EXPECT_FALSE(history_is_browsing(&history));
}

TEST_F(HistoryTest, ResetBrowseWithNullHistory) {
    /* Should not crash */
    history_reset_browse(nullptr);
}

/*---------------------------------------------------------------------------*/
/* Get Entry Tests                                                           */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, GetEntryValidIndex) {
    history_add(&history, "cmd1");
    history_add(&history, "cmd2");

    EXPECT_STREQ("cmd2", history_get_entry(&history, 0));
    EXPECT_STREQ("cmd1", history_get_entry(&history, 1));
}

TEST_F(HistoryTest, GetEntryInvalidIndex) {
    history_add(&history, "cmd1");

    EXPECT_EQ(nullptr, history_get_entry(&history, 1));
    EXPECT_EQ(nullptr, history_get_entry(&history, 100));
}

TEST_F(HistoryTest, GetEntryFromEmpty) {
    EXPECT_EQ(nullptr, history_get_entry(&history, 0));
}

TEST_F(HistoryTest, GetEntryWithNullHistory) {
    EXPECT_EQ(nullptr, history_get_entry(nullptr, 0));
}

/*---------------------------------------------------------------------------*/
/* Clear Tests                                                               */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, ClearRemovesAllEntries) {
    history_add(&history, "cmd1");
    history_add(&history, "cmd2");
    history_add(&history, "cmd3");

    history_clear(&history);

    EXPECT_EQ(0, history_get_count(&history));
    EXPECT_EQ(nullptr, history_get_entry(&history, 0));
}

TEST_F(HistoryTest, ClearResetsBrowse) {
    history_add(&history, "cmd1");
    history_get_prev(&history);
    EXPECT_TRUE(history_is_browsing(&history));

    history_clear(&history);
    EXPECT_FALSE(history_is_browsing(&history));
}

TEST_F(HistoryTest, ClearWithNullHistory) {
    /* Should not crash */
    history_clear(nullptr);
}

/*---------------------------------------------------------------------------*/
/* Get Count Tests                                                           */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, GetCountEmpty) {
    EXPECT_EQ(0, history_get_count(&history));
}

TEST_F(HistoryTest, GetCountAfterAdds) {
    history_add(&history, "cmd1");
    EXPECT_EQ(1, history_get_count(&history));

    history_add(&history, "cmd2");
    EXPECT_EQ(2, history_get_count(&history));
}

TEST_F(HistoryTest, GetCountWithNullHistory) {
    EXPECT_EQ(0, history_get_count(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Is Browsing Tests                                                         */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, IsBrowsingInitiallyFalse) {
    EXPECT_FALSE(history_is_browsing(&history));
}

TEST_F(HistoryTest, IsBrowsingAfterPrev) {
    history_add(&history, "cmd1");
    history_get_prev(&history);
    EXPECT_TRUE(history_is_browsing(&history));
}

TEST_F(HistoryTest, IsBrowsingWithNullHistory) {
    EXPECT_FALSE(history_is_browsing(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Long Command Tests                                                        */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, AddLongCommandTruncated) {
    /* Create a command longer than ENTRY_SIZE */
    std::string long_cmd(ENTRY_SIZE + 10, 'x');

    EXPECT_TRUE(history_add(&history, long_cmd.c_str()));

    const char* stored = history_get_entry(&history, 0);
    EXPECT_NE(nullptr, stored);
    EXPECT_EQ(ENTRY_SIZE - 1, strlen(stored));
}

/*---------------------------------------------------------------------------*/
/* Browse After Add Tests                                                    */
/*---------------------------------------------------------------------------*/

TEST_F(HistoryTest, AddResetsBrowse) {
    history_add(&history, "cmd1");
    history_add(&history, "cmd2");

    /* Start browsing */
    history_get_prev(&history);
    EXPECT_TRUE(history_is_browsing(&history));

    /* Add new command should reset browse */
    history_add(&history, "cmd3");
    EXPECT_FALSE(history_is_browsing(&history));
}
