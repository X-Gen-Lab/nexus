/**
 * \file            test_shell_line_editor.cpp
 * \brief           Shell Line Editor Unit Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Unit tests for Shell line editor functionality.
 * Requirements: 4.1-4.15
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "shell/shell_line_editor.h"
}

/**
 * \brief           Line Editor Test Fixture
 */
class LineEditorTest : public ::testing::Test {
protected:
    line_editor_t editor;
    char buffer[128];

    void SetUp() override {
        memset(buffer, 0, sizeof(buffer));
        line_editor_init(&editor, buffer, sizeof(buffer));
    }
};

/*---------------------------------------------------------------------------*/
/* Initialization Tests                                                      */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, InitSetsCorrectState) {
    EXPECT_EQ(0, editor.length);
    EXPECT_EQ(0, editor.cursor);
    EXPECT_EQ(sizeof(buffer), editor.buffer_size);
    EXPECT_TRUE(editor.insert_mode);
    EXPECT_STREQ("", editor.buffer);
}

TEST_F(LineEditorTest, InitWithNullEditor) {
    /* Should not crash */
    line_editor_init(nullptr, buffer, sizeof(buffer));
}

TEST_F(LineEditorTest, InitWithNullBuffer) {
    line_editor_t ed;
    line_editor_init(&ed, nullptr, 128);
    /* Should not crash, editor state undefined */
}

TEST_F(LineEditorTest, InitWithZeroSize) {
    line_editor_t ed;
    line_editor_init(&ed, buffer, 0);
    /* Should not crash */
}

/*---------------------------------------------------------------------------*/
/* Character Insertion Tests - Requirements 4.3                              */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, InsertSingleChar) {
    EXPECT_TRUE(line_editor_insert_char(&editor, 'a'));
    EXPECT_STREQ("a", buffer);
    EXPECT_EQ(1, editor.length);
    EXPECT_EQ(1, editor.cursor);
}

TEST_F(LineEditorTest, InsertMultipleChars) {
    line_editor_insert_char(&editor, 'h');
    line_editor_insert_char(&editor, 'e');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'o');
    
    EXPECT_STREQ("hello", buffer);
    EXPECT_EQ(5, editor.length);
    EXPECT_EQ(5, editor.cursor);
}

TEST_F(LineEditorTest, InsertAtMiddle) {
    /* Type "hllo" */
    line_editor_insert_char(&editor, 'h');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'l');
    line_editor_insert_char(&editor, 'o');
    
    /* Move cursor back 3 positions */
    line_editor_move_cursor(&editor, -3);
    EXPECT_EQ(1, editor.cursor);
    
    /* Insert 'e' at position 1 */
    line_editor_insert_char(&editor, 'e');
    
    EXPECT_STREQ("hello", buffer);
    EXPECT_EQ(5, editor.length);
    EXPECT_EQ(2, editor.cursor);
}

TEST_F(LineEditorTest, InsertWhenBufferFull) {
    char small_buffer[4];
    line_editor_t small_editor;
    line_editor_init(&small_editor, small_buffer, sizeof(small_buffer));
    
    /* Fill buffer (3 chars + null terminator) */
    EXPECT_TRUE(line_editor_insert_char(&small_editor, 'a'));
    EXPECT_TRUE(line_editor_insert_char(&small_editor, 'b'));
    EXPECT_TRUE(line_editor_insert_char(&small_editor, 'c'));
    
    /* Buffer should be full now */
    EXPECT_FALSE(line_editor_insert_char(&small_editor, 'd'));
    EXPECT_STREQ("abc", small_buffer);
}

TEST_F(LineEditorTest, InsertWithNullEditor) {
    EXPECT_FALSE(line_editor_insert_char(nullptr, 'a'));
}


/*---------------------------------------------------------------------------*/
/* Backspace Tests - Requirements 4.2                                        */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, BackspaceAtEnd) {
    line_editor_set_content(&editor, "hello");
    
    EXPECT_TRUE(line_editor_backspace(&editor));
    EXPECT_STREQ("hell", buffer);
    EXPECT_EQ(4, editor.length);
    EXPECT_EQ(4, editor.cursor);
}

TEST_F(LineEditorTest, BackspaceAtMiddle) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_cursor(&editor, -2);  /* cursor at 'l' */
    
    EXPECT_TRUE(line_editor_backspace(&editor));
    EXPECT_STREQ("helo", buffer);
    EXPECT_EQ(4, editor.length);
    EXPECT_EQ(2, editor.cursor);
}

TEST_F(LineEditorTest, BackspaceAtStart) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_to_start(&editor);
    
    EXPECT_FALSE(line_editor_backspace(&editor));
    EXPECT_STREQ("hello", buffer);
    EXPECT_EQ(5, editor.length);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, BackspaceEmptyBuffer) {
    EXPECT_FALSE(line_editor_backspace(&editor));
    EXPECT_EQ(0, editor.length);
}

TEST_F(LineEditorTest, BackspaceWithNullEditor) {
    EXPECT_FALSE(line_editor_backspace(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Delete Tests - Requirements 4.12                                          */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, DeleteAtStart) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_to_start(&editor);
    
    EXPECT_TRUE(line_editor_delete_char(&editor));
    EXPECT_STREQ("ello", buffer);
    EXPECT_EQ(4, editor.length);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, DeleteAtMiddle) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_cursor(&editor, -3);  /* cursor at second 'l' */
    
    EXPECT_TRUE(line_editor_delete_char(&editor));
    EXPECT_STREQ("helo", buffer);
    EXPECT_EQ(4, editor.length);
    EXPECT_EQ(2, editor.cursor);
}

TEST_F(LineEditorTest, DeleteAtEnd) {
    line_editor_set_content(&editor, "hello");
    
    EXPECT_FALSE(line_editor_delete_char(&editor));
    EXPECT_STREQ("hello", buffer);
    EXPECT_EQ(5, editor.length);
}

TEST_F(LineEditorTest, DeleteWithNullEditor) {
    EXPECT_FALSE(line_editor_delete_char(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Cursor Movement Tests - Requirements 4.8, 4.9                             */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, MoveCursorLeft) {
    line_editor_set_content(&editor, "hello");
    
    line_editor_move_cursor(&editor, -1);
    EXPECT_EQ(4, editor.cursor);
    
    line_editor_move_cursor(&editor, -2);
    EXPECT_EQ(2, editor.cursor);
}

TEST_F(LineEditorTest, MoveCursorRight) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_to_start(&editor);
    
    line_editor_move_cursor(&editor, 1);
    EXPECT_EQ(1, editor.cursor);
    
    line_editor_move_cursor(&editor, 2);
    EXPECT_EQ(3, editor.cursor);
}

TEST_F(LineEditorTest, MoveCursorBeyondStart) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_cursor(&editor, -10);
    
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, MoveCursorBeyondEnd) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_cursor(&editor, 10);
    
    EXPECT_EQ(5, editor.cursor);
}

TEST_F(LineEditorTest, MoveCursorWithNullEditor) {
    /* Should not crash */
    line_editor_move_cursor(nullptr, 1);
}

/*---------------------------------------------------------------------------*/
/* Home/End Tests - Requirements 4.10, 4.11                                  */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, MoveToStart) {
    line_editor_set_content(&editor, "hello world");
    
    line_editor_move_to_start(&editor);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, MoveToEnd) {
    line_editor_set_content(&editor, "hello world");
    line_editor_move_to_start(&editor);
    
    line_editor_move_to_end(&editor);
    EXPECT_EQ(11, editor.cursor);
}

TEST_F(LineEditorTest, MoveToStartWithNullEditor) {
    /* Should not crash */
    line_editor_move_to_start(nullptr);
}

TEST_F(LineEditorTest, MoveToEndWithNullEditor) {
    /* Should not crash */
    line_editor_move_to_end(nullptr);
}

/*---------------------------------------------------------------------------*/
/* Delete to End Tests - Requirements 4.13                                   */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, DeleteToEndFromMiddle) {
    line_editor_set_content(&editor, "hello world");
    line_editor_move_cursor(&editor, -6);  /* cursor at 'w' */
    
    line_editor_delete_to_end(&editor);
    EXPECT_STREQ("hello", buffer);
    EXPECT_EQ(5, editor.length);
    EXPECT_EQ(5, editor.cursor);
}

TEST_F(LineEditorTest, DeleteToEndFromStart) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_to_start(&editor);
    
    line_editor_delete_to_end(&editor);
    EXPECT_STREQ("", buffer);
    EXPECT_EQ(0, editor.length);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, DeleteToEndAtEnd) {
    line_editor_set_content(&editor, "hello");
    
    line_editor_delete_to_end(&editor);
    EXPECT_STREQ("hello", buffer);
    EXPECT_EQ(5, editor.length);
}

TEST_F(LineEditorTest, DeleteToEndWithNullEditor) {
    /* Should not crash */
    line_editor_delete_to_end(nullptr);
}

/*---------------------------------------------------------------------------*/
/* Delete to Start Tests - Requirements 4.14                                 */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, DeleteToStartFromMiddle) {
    line_editor_set_content(&editor, "hello world");
    line_editor_move_cursor(&editor, -6);  /* cursor at ' ' before 'w' */
    
    line_editor_delete_to_start(&editor);
    EXPECT_STREQ(" world", buffer);  /* Space before 'world' remains */
    EXPECT_EQ(6, editor.length);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, DeleteToStartFromEnd) {
    line_editor_set_content(&editor, "hello");
    
    line_editor_delete_to_start(&editor);
    EXPECT_STREQ("", buffer);
    EXPECT_EQ(0, editor.length);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, DeleteToStartAtStart) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_to_start(&editor);
    
    line_editor_delete_to_start(&editor);
    EXPECT_STREQ("hello", buffer);
    EXPECT_EQ(5, editor.length);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, DeleteToStartWithNullEditor) {
    /* Should not crash */
    line_editor_delete_to_start(nullptr);
}

/*---------------------------------------------------------------------------*/
/* Delete Word Tests - Requirements 4.15                                     */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, DeleteWordAtEnd) {
    line_editor_set_content(&editor, "hello world");
    
    line_editor_delete_word(&editor);
    EXPECT_STREQ("hello ", buffer);
    EXPECT_EQ(6, editor.length);
    EXPECT_EQ(6, editor.cursor);
}

TEST_F(LineEditorTest, DeleteWordWithTrailingSpaces) {
    line_editor_set_content(&editor, "hello world   ");
    
    line_editor_delete_word(&editor);
    EXPECT_STREQ("hello ", buffer);
    EXPECT_EQ(6, editor.length);
}

TEST_F(LineEditorTest, DeleteWordAtMiddle) {
    line_editor_set_content(&editor, "hello world test");
    line_editor_move_cursor(&editor, -5);  /* cursor at ' ' before 'test' */
    
    line_editor_delete_word(&editor);
    EXPECT_STREQ("hello  test", buffer);  /* Two spaces: one before 'world' and one before 'test' */
    EXPECT_EQ(11, editor.length);
}

TEST_F(LineEditorTest, DeleteWordAtStart) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_to_start(&editor);
    
    line_editor_delete_word(&editor);
    EXPECT_STREQ("hello", buffer);
    EXPECT_EQ(5, editor.length);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, DeleteWordSingleWord) {
    line_editor_set_content(&editor, "hello");
    
    line_editor_delete_word(&editor);
    EXPECT_STREQ("", buffer);
    EXPECT_EQ(0, editor.length);
}

TEST_F(LineEditorTest, DeleteWordWithNullEditor) {
    /* Should not crash */
    line_editor_delete_word(nullptr);
}

/*---------------------------------------------------------------------------*/
/* Clear Tests                                                               */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, ClearBuffer) {
    line_editor_set_content(&editor, "hello world");
    
    line_editor_clear(&editor);
    EXPECT_STREQ("", buffer);
    EXPECT_EQ(0, editor.length);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, ClearEmptyBuffer) {
    line_editor_clear(&editor);
    EXPECT_STREQ("", buffer);
    EXPECT_EQ(0, editor.length);
    EXPECT_EQ(0, editor.cursor);
}

TEST_F(LineEditorTest, ClearWithNullEditor) {
    /* Should not crash */
    line_editor_clear(nullptr);
}

/*---------------------------------------------------------------------------*/
/* Accessor Function Tests                                                   */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, GetBuffer) {
    line_editor_set_content(&editor, "hello");
    EXPECT_STREQ("hello", line_editor_get_buffer(&editor));
}

TEST_F(LineEditorTest, GetBufferWithNullEditor) {
    EXPECT_STREQ("", line_editor_get_buffer(nullptr));
}

TEST_F(LineEditorTest, GetLength) {
    line_editor_set_content(&editor, "hello");
    EXPECT_EQ(5, line_editor_get_length(&editor));
}

TEST_F(LineEditorTest, GetLengthWithNullEditor) {
    EXPECT_EQ(0, line_editor_get_length(nullptr));
}

TEST_F(LineEditorTest, GetCursor) {
    line_editor_set_content(&editor, "hello");
    line_editor_move_cursor(&editor, -2);
    EXPECT_EQ(3, line_editor_get_cursor(&editor));
}

TEST_F(LineEditorTest, GetCursorWithNullEditor) {
    EXPECT_EQ(0, line_editor_get_cursor(nullptr));
}

/*---------------------------------------------------------------------------*/
/* Set Content Tests                                                         */
/*---------------------------------------------------------------------------*/

TEST_F(LineEditorTest, SetContent) {
    line_editor_set_content(&editor, "hello world");
    EXPECT_STREQ("hello world", buffer);
    EXPECT_EQ(11, editor.length);
    EXPECT_EQ(11, editor.cursor);
}

TEST_F(LineEditorTest, SetContentNull) {
    line_editor_set_content(&editor, "hello");
    line_editor_set_content(&editor, nullptr);
    EXPECT_STREQ("", buffer);
    EXPECT_EQ(0, editor.length);
}

TEST_F(LineEditorTest, SetContentTruncation) {
    char small_buffer[8];
    line_editor_t small_editor;
    line_editor_init(&small_editor, small_buffer, sizeof(small_buffer));
    
    line_editor_set_content(&small_editor, "hello world");
    EXPECT_EQ(7, small_editor.length);  /* Truncated to fit */
    EXPECT_STREQ("hello w", small_buffer);
}

TEST_F(LineEditorTest, SetContentWithNullEditor) {
    /* Should not crash */
    line_editor_set_content(nullptr, "hello");
}
