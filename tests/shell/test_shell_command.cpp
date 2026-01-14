/**
 * \file            test_shell_command.cpp
 * \brief           Shell Command Registration Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for Shell command registration functionality.
 * Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7
 */

#include <cstring>
#include <gtest/gtest.h>

extern "C" {
#include "shell/shell_command.h"
}

/*---------------------------------------------------------------------------*/
/* Test Helpers                                                              */
/*---------------------------------------------------------------------------*/

/** Simple test command handler */
static int test_handler(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return 0;
}

/** Another test command handler */
static int test_handler2(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return 1;
}

/** Test completion callback */
static void test_completion(const char* partial, char* completions[],
                            int* count) {
    (void)partial;
    (void)completions;
    *count = 0;
}

/**
 * \brief           Command Registration Test Fixture
 */
class CommandTest : public ::testing::Test {
  protected:
    void SetUp() override {
        shell_clear_commands();
    }

    void TearDown() override {
        shell_clear_commands();
    }
};

/*---------------------------------------------------------------------------*/
/* Registration Tests - Requirements 2.1, 2.2                                */
/*---------------------------------------------------------------------------*/

TEST_F(CommandTest, RegisterValidCommand) {
    shell_command_t cmd = {.name = "test",
                           .handler = test_handler,
                           .help = "Test command",
                           .usage = "test [args]",
                           .completion = nullptr};

    EXPECT_EQ(SHELL_OK, shell_register_command(&cmd));
    EXPECT_EQ(1, shell_get_command_count());
}

TEST_F(CommandTest, RegisterCommandWithMinimalFields) {
    shell_command_t cmd = {.name = "minimal",
                           .handler = test_handler,
                           .help = nullptr,
                           .usage = nullptr,
                           .completion = nullptr};

    EXPECT_EQ(SHELL_OK, shell_register_command(&cmd));
    EXPECT_EQ(1, shell_get_command_count());
}

TEST_F(CommandTest, RegisterCommandWithCompletion) {
    shell_command_t cmd = {.name = "complete",
                           .handler = test_handler,
                           .help = "Command with completion",
                           .usage = "complete <arg>",
                           .completion = test_completion};

    EXPECT_EQ(SHELL_OK, shell_register_command(&cmd));

    const shell_command_t* retrieved = shell_get_command("complete");
    ASSERT_NE(nullptr, retrieved);
    EXPECT_EQ(test_completion, retrieved->completion);
}

TEST_F(CommandTest, RegisterNullCommand) {
    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_register_command(nullptr));
    EXPECT_EQ(0, shell_get_command_count());
}

TEST_F(CommandTest, RegisterCommandWithNullName) {
    shell_command_t cmd = {.name = nullptr,
                           .handler = test_handler,
                           .help = "Test",
                           .usage = nullptr,
                           .completion = nullptr};

    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_register_command(&cmd));
    EXPECT_EQ(0, shell_get_command_count());
}

TEST_F(CommandTest, RegisterCommandWithNullHandler) {
    shell_command_t cmd = {.name = "test",
                           .handler = nullptr,
                           .help = "Test",
                           .usage = nullptr,
                           .completion = nullptr};

    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_register_command(&cmd));
    EXPECT_EQ(0, shell_get_command_count());
}

/*---------------------------------------------------------------------------*/
/* Duplicate Registration Tests - Requirement 2.3                            */
/*---------------------------------------------------------------------------*/

TEST_F(CommandTest, RegisterDuplicateNameRejected) {
    shell_command_t cmd1 = {.name = "test",
                            .handler = test_handler,
                            .help = "First command",
                            .usage = nullptr,
                            .completion = nullptr};

    shell_command_t cmd2 = {.name = "test",
                            .handler = test_handler2,
                            .help = "Second command",
                            .usage = nullptr,
                            .completion = nullptr};

    EXPECT_EQ(SHELL_OK, shell_register_command(&cmd1));
    EXPECT_EQ(SHELL_ERROR_ALREADY_EXISTS, shell_register_command(&cmd2));
    EXPECT_EQ(1, shell_get_command_count());
}

TEST_F(CommandTest, RegisterDifferentNamesAllowed) {
    shell_command_t cmd1 = {.name = "cmd1",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};

    shell_command_t cmd2 = {.name = "cmd2",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};

    EXPECT_EQ(SHELL_OK, shell_register_command(&cmd1));
    EXPECT_EQ(SHELL_OK, shell_register_command(&cmd2));
    EXPECT_EQ(2, shell_get_command_count());
}

/*---------------------------------------------------------------------------*/
/* Capacity Tests - Requirement 2.4                                          */
/*---------------------------------------------------------------------------*/

TEST_F(CommandTest, RegisterUpToMaxCommands) {
    shell_command_t cmds[SHELL_MAX_COMMANDS];
    char names[SHELL_MAX_COMMANDS][16];

    for (int i = 0; i < SHELL_MAX_COMMANDS; i++) {
        snprintf(names[i], sizeof(names[i]), "cmd%d", i);
        cmds[i].name = names[i];
        cmds[i].handler = test_handler;
        cmds[i].help = nullptr;
        cmds[i].usage = nullptr;
        cmds[i].completion = nullptr;

        EXPECT_EQ(SHELL_OK, shell_register_command(&cmds[i]));
    }

    EXPECT_EQ(SHELL_MAX_COMMANDS, shell_get_command_count());
}

TEST_F(CommandTest, RegisterBeyondCapacityRejected) {
    shell_command_t cmds[SHELL_MAX_COMMANDS + 1];
    char names[SHELL_MAX_COMMANDS + 1][16];

    /* Fill to capacity */
    for (int i = 0; i < SHELL_MAX_COMMANDS; i++) {
        snprintf(names[i], sizeof(names[i]), "cmd%d", i);
        cmds[i].name = names[i];
        cmds[i].handler = test_handler;
        cmds[i].help = nullptr;
        cmds[i].usage = nullptr;
        cmds[i].completion = nullptr;

        EXPECT_EQ(SHELL_OK, shell_register_command(&cmds[i]));
    }

    /* Try to add one more */
    snprintf(names[SHELL_MAX_COMMANDS], sizeof(names[SHELL_MAX_COMMANDS]),
             "overflow");
    cmds[SHELL_MAX_COMMANDS].name = names[SHELL_MAX_COMMANDS];
    cmds[SHELL_MAX_COMMANDS].handler = test_handler;
    cmds[SHELL_MAX_COMMANDS].help = nullptr;
    cmds[SHELL_MAX_COMMANDS].usage = nullptr;
    cmds[SHELL_MAX_COMMANDS].completion = nullptr;

    EXPECT_EQ(SHELL_ERROR_NO_MEMORY,
              shell_register_command(&cmds[SHELL_MAX_COMMANDS]));
    EXPECT_EQ(SHELL_MAX_COMMANDS, shell_get_command_count());
}

/*---------------------------------------------------------------------------*/
/* Unregister Tests - Requirements 2.5, 2.6                                  */
/*---------------------------------------------------------------------------*/

TEST_F(CommandTest, UnregisterValidCommand) {
    shell_command_t cmd = {.name = "test",
                           .handler = test_handler,
                           .help = nullptr,
                           .usage = nullptr,
                           .completion = nullptr};

    shell_register_command(&cmd);
    EXPECT_EQ(1, shell_get_command_count());

    EXPECT_EQ(SHELL_OK, shell_unregister_command("test"));
    EXPECT_EQ(0, shell_get_command_count());
    EXPECT_EQ(nullptr, shell_get_command("test"));
}

TEST_F(CommandTest, UnregisterMiddleCommand) {
    shell_command_t cmd1 = {.name = "cmd1",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};
    shell_command_t cmd2 = {.name = "cmd2",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};
    shell_command_t cmd3 = {.name = "cmd3",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};

    shell_register_command(&cmd1);
    shell_register_command(&cmd2);
    shell_register_command(&cmd3);

    EXPECT_EQ(SHELL_OK, shell_unregister_command("cmd2"));
    EXPECT_EQ(2, shell_get_command_count());

    /* Verify remaining commands are still accessible */
    EXPECT_NE(nullptr, shell_get_command("cmd1"));
    EXPECT_EQ(nullptr, shell_get_command("cmd2"));
    EXPECT_NE(nullptr, shell_get_command("cmd3"));
}

TEST_F(CommandTest, UnregisterNonExistentCommand) {
    EXPECT_EQ(SHELL_ERROR_NOT_FOUND, shell_unregister_command("nonexistent"));
}

TEST_F(CommandTest, UnregisterNullName) {
    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_unregister_command(nullptr));
}

TEST_F(CommandTest, UnregisterFromEmpty) {
    EXPECT_EQ(SHELL_ERROR_NOT_FOUND, shell_unregister_command("test"));
}

TEST_F(CommandTest, ReregisterAfterUnregister) {
    shell_command_t cmd = {.name = "test",
                           .handler = test_handler,
                           .help = nullptr,
                           .usage = nullptr,
                           .completion = nullptr};

    shell_register_command(&cmd);
    shell_unregister_command("test");

    /* Should be able to register again */
    EXPECT_EQ(SHELL_OK, shell_register_command(&cmd));
    EXPECT_EQ(1, shell_get_command_count());
}

/*---------------------------------------------------------------------------*/
/* Get Command Tests - Requirement 2.7                                       */
/*---------------------------------------------------------------------------*/

TEST_F(CommandTest, GetCommandByName) {
    shell_command_t cmd = {.name = "mycommand",
                           .handler = test_handler,
                           .help = "My help text",
                           .usage = "mycommand <arg>",
                           .completion = test_completion};

    shell_register_command(&cmd);

    const shell_command_t* retrieved = shell_get_command("mycommand");
    ASSERT_NE(nullptr, retrieved);
    EXPECT_STREQ("mycommand", retrieved->name);
    EXPECT_EQ(test_handler, retrieved->handler);
    EXPECT_STREQ("My help text", retrieved->help);
    EXPECT_STREQ("mycommand <arg>", retrieved->usage);
    EXPECT_EQ(test_completion, retrieved->completion);
}

TEST_F(CommandTest, GetCommandNotFound) {
    EXPECT_EQ(nullptr, shell_get_command("nonexistent"));
}

TEST_F(CommandTest, GetCommandNullName) {
    EXPECT_EQ(nullptr, shell_get_command(nullptr));
}

TEST_F(CommandTest, GetCommandFromMultiple) {
    shell_command_t cmd1 = {.name = "alpha",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};
    shell_command_t cmd2 = {.name = "beta",
                            .handler = test_handler2,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};
    shell_command_t cmd3 = {.name = "gamma",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};

    shell_register_command(&cmd1);
    shell_register_command(&cmd2);
    shell_register_command(&cmd3);

    const shell_command_t* retrieved = shell_get_command("beta");
    ASSERT_NE(nullptr, retrieved);
    EXPECT_STREQ("beta", retrieved->name);
    EXPECT_EQ(test_handler2, retrieved->handler);
}

/*---------------------------------------------------------------------------*/
/* Get Commands List Tests                                                   */
/*---------------------------------------------------------------------------*/

TEST_F(CommandTest, GetCommandsList) {
    shell_command_t cmd1 = {.name = "cmd1",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};
    shell_command_t cmd2 = {.name = "cmd2",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};

    shell_register_command(&cmd1);
    shell_register_command(&cmd2);

    const shell_command_t** cmds = nullptr;
    int count = 0;

    EXPECT_EQ(SHELL_OK, shell_get_commands(&cmds, &count));
    EXPECT_NE(nullptr, cmds);
    EXPECT_EQ(2, count);
}

TEST_F(CommandTest, GetCommandsNullParams) {
    int count = 0;
    const shell_command_t** cmds = nullptr;

    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_get_commands(nullptr, &count));
    EXPECT_EQ(SHELL_ERROR_INVALID_PARAM, shell_get_commands(&cmds, nullptr));
}

/*---------------------------------------------------------------------------*/
/* Get Command Count Tests                                                   */
/*---------------------------------------------------------------------------*/

TEST_F(CommandTest, GetCommandCountEmpty) {
    EXPECT_EQ(0, shell_get_command_count());
}

TEST_F(CommandTest, GetCommandCountAfterRegistrations) {
    shell_command_t cmd1 = {.name = "cmd1",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};
    shell_command_t cmd2 = {.name = "cmd2",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};

    shell_register_command(&cmd1);
    EXPECT_EQ(1, shell_get_command_count());

    shell_register_command(&cmd2);
    EXPECT_EQ(2, shell_get_command_count());
}

/*---------------------------------------------------------------------------*/
/* Completion Callback Tests                                                 */
/*---------------------------------------------------------------------------*/

TEST_F(CommandTest, SetCompletionCallback) {
    EXPECT_EQ(SHELL_OK, shell_set_completion_callback(test_completion));
    EXPECT_EQ(test_completion, shell_get_completion_callback());
}

TEST_F(CommandTest, SetCompletionCallbackNull) {
    shell_set_completion_callback(test_completion);
    EXPECT_EQ(SHELL_OK, shell_set_completion_callback(nullptr));
    EXPECT_EQ(nullptr, shell_get_completion_callback());
}

TEST_F(CommandTest, GetCompletionCallbackInitiallyNull) {
    EXPECT_EQ(nullptr, shell_get_completion_callback());
}

/*---------------------------------------------------------------------------*/
/* Clear Commands Tests                                                      */
/*---------------------------------------------------------------------------*/

TEST_F(CommandTest, ClearCommandsRemovesAll) {
    shell_command_t cmd1 = {.name = "cmd1",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};
    shell_command_t cmd2 = {.name = "cmd2",
                            .handler = test_handler,
                            .help = nullptr,
                            .usage = nullptr,
                            .completion = nullptr};

    shell_register_command(&cmd1);
    shell_register_command(&cmd2);
    shell_set_completion_callback(test_completion);

    shell_clear_commands();

    EXPECT_EQ(0, shell_get_command_count());
    EXPECT_EQ(nullptr, shell_get_command("cmd1"));
    EXPECT_EQ(nullptr, shell_get_command("cmd2"));
    EXPECT_EQ(nullptr, shell_get_completion_callback());
}
