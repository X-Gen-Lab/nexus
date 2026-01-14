/**
 * \file            test_shell_line_editor_properties.cpp
 * \brief           Shell Line Editor Property-Based Tests
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * Property-based tests for Shell line editor functionality.
 * These tests verify universal properties that should hold for all valid inputs.
 * Each property test runs 100+ iterations with random inputs.
 *
 * Feature: shell-cli-middleware, Property 4: Line Editor Buffer Consistency
 * **Validates: Requirements 4.1-4.15**
 */

#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vector>
#include <algorithm>

extern "C" {
#include "shell/shell_line_editor.h"
}

/**
 * \brief           Number of iterations for property tests
 */
static constexpr int PROPERTY_TEST_ITERATIONS = 100;

/**
 * \brief           Shell Line Editor Property Test Fixture
 */
class LineEditorPropertyTest : public ::testing::Test {
protected:
    std::mt19937 rng;
    line_editor_t editor;
    char buffer[256];

    void SetUp() override {
        rng.seed(std::random_device{}());
        memset(buffer, 0, sizeof(buffer));
        line_editor_init(&editor, buffer, sizeof(buffer));
    }

    /**
     * \brief           Generate a random printable character
     */
    char randomPrintableChar() {
        std::uniform_int_distribution<int> dist(32, 126);
        return static_cast<char>(dist(rng));
    }

    /**
     * \brief           Generate a random alphanumeric string
     */
    std::string randomAlphanumeric(int minLen, int maxLen) {
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
     * \brief           Generate a random string with spaces
     */
    std::string randomStringWithSpaces(int minLen, int maxLen) {
        std::uniform_int_distribution<int> lenDist(minLen, maxLen);
        std::uniform_int_distribution<int> charDist(0, 63);
        
        int len = lenDist(rng);
        std::string str;
        str.reserve(len);
        
        for (int i = 0; i < len; ++i) {
            int c = charDist(rng);
            if (c < 26) {
                str += static_cast<char>('a' + c);
            } else if (c < 52) {
                str += static_cast<char>('A' + (c - 26));
            } else if (c < 62) {
                str += static_cast<char>('0' + (c - 52));
            } else {
                str += ' ';
            }
        }
        return str;
    }

    /**
     * \brief           Verify buffer consistency invariants
     */
    void verifyInvariants(const std::string& context) {
        /* Cursor must be within valid range [0, length] */
        EXPECT_LE(editor.cursor, editor.length)
            << context << ": cursor (" << editor.cursor 
            << ") exceeds length (" << editor.length << ")";
        
        /* Length must not exceed buffer_size - 1 (for null terminator) */
        EXPECT_LT(editor.length, editor.buffer_size)
            << context << ": length (" << editor.length 
            << ") exceeds buffer capacity (" << editor.buffer_size << ")";
        
        /* Buffer must be null-terminated at length position */
        EXPECT_EQ('\0', editor.buffer[editor.length])
            << context << ": buffer not null-terminated at length";
        
        /* Actual string length must match reported length */
        EXPECT_EQ(editor.length, strlen(editor.buffer))
            << context << ": strlen mismatch with reported length";
    }
};


/*---------------------------------------------------------------------------*/
/* Property 4: Line Editor Buffer Consistency                                */
/* *For any* sequence of line editing operations (insert, delete, cursor     */
/* movement), the buffer content and cursor position SHALL remain consistent,*/
/* with cursor always within valid range [0, length].                        */
/* **Validates: Requirements 4.1-4.15**                                      */
/*---------------------------------------------------------------------------*/

/**
 * Feature: shell-cli-middleware, Property 4: Line Editor Buffer Consistency
 * 
 * *For any* sequence of line editing operations (insert, delete, cursor
 * movement), the buffer content and cursor position SHALL remain consistent,
 * with cursor always within valid range [0, length].
 * 
 * **Validates: Requirements 4.1-4.15**
 */
TEST_F(LineEditorPropertyTest, Property4_BufferConsistencyAfterRandomOperations) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Reset editor for each iteration */
        line_editor_clear(&editor);
        
        /* Generate random sequence of operations (10-50 operations) */
        std::uniform_int_distribution<int> opCountDist(10, 50);
        std::uniform_int_distribution<int> opTypeDist(0, 9);
        int opCount = opCountDist(rng);
        
        for (int op = 0; op < opCount; ++op) {
            int opType = opTypeDist(rng);
            std::string opContext = "Iter " + std::to_string(iter) + 
                                    ", Op " + std::to_string(op);
            
            switch (opType) {
                case 0: /* Insert character */
                case 1: {
                    char c = randomPrintableChar();
                    line_editor_insert_char(&editor, c);
                    break;
                }
                case 2: /* Backspace */
                    line_editor_backspace(&editor);
                    break;
                case 3: /* Delete */
                    line_editor_delete_char(&editor);
                    break;
                case 4: /* Move cursor left */
                    line_editor_move_cursor(&editor, -1);
                    break;
                case 5: /* Move cursor right */
                    line_editor_move_cursor(&editor, 1);
                    break;
                case 6: /* Move to start */
                    line_editor_move_to_start(&editor);
                    break;
                case 7: /* Move to end */
                    line_editor_move_to_end(&editor);
                    break;
                case 8: /* Delete to end */
                    line_editor_delete_to_end(&editor);
                    break;
                case 9: /* Delete to start */
                    line_editor_delete_to_start(&editor);
                    break;
            }
            
            /* Verify invariants after each operation */
            verifyInvariants(opContext);
        }
    }
}

/**
 * Feature: shell-cli-middleware, Property 4a: Insert Preserves Content
 * 
 * *For any* character inserted at any position, the characters before and
 * after the insertion point SHALL remain unchanged.
 * 
 * **Validates: Requirements 4.3**
 */
TEST_F(LineEditorPropertyTest, Property4a_InsertPreservesContent) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Start with random content */
        std::string initial = randomAlphanumeric(5, 50);
        line_editor_set_content(&editor, initial.c_str());
        
        /* Move cursor to random position */
        std::uniform_int_distribution<int> posDist(0, static_cast<int>(initial.length()));
        int insertPos = posDist(rng);
        line_editor_move_to_start(&editor);
        line_editor_move_cursor(&editor, insertPos);
        
        /* Insert a random character */
        char newChar = randomPrintableChar();
        std::string before = initial.substr(0, insertPos);
        std::string after = initial.substr(insertPos);
        
        bool inserted = line_editor_insert_char(&editor, newChar);
        
        if (inserted) {
            /* Build expected result */
            std::string expected = before + newChar + after;
            
            EXPECT_STREQ(expected.c_str(), editor.buffer)
                << "Iter " << iter << ": content mismatch after insert at pos " << insertPos;
            EXPECT_EQ(insertPos + 1, editor.cursor)
                << "Iter " << iter << ": cursor should advance after insert";
        }
        
        verifyInvariants("Iter " + std::to_string(iter));
    }
}

/**
 * Feature: shell-cli-middleware, Property 4b: Backspace Removes Correct Character
 * 
 * *For any* non-empty buffer with cursor not at start, backspace SHALL
 * remove exactly the character before the cursor.
 * 
 * **Validates: Requirements 4.2**
 */
TEST_F(LineEditorPropertyTest, Property4b_BackspaceRemovesCorrectChar) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Start with random content */
        std::string initial = randomAlphanumeric(5, 50);
        line_editor_set_content(&editor, initial.c_str());
        
        /* Move cursor to random position (not at start) */
        std::uniform_int_distribution<int> posDist(1, static_cast<int>(initial.length()));
        int cursorPos = posDist(rng);
        line_editor_move_to_start(&editor);
        line_editor_move_cursor(&editor, cursorPos);
        
        /* Calculate expected result */
        std::string before = initial.substr(0, cursorPos - 1);
        std::string after = initial.substr(cursorPos);
        std::string expected = before + after;
        
        /* Perform backspace */
        bool deleted = line_editor_backspace(&editor);
        
        EXPECT_TRUE(deleted)
            << "Iter " << iter << ": backspace should succeed at pos " << cursorPos;
        EXPECT_STREQ(expected.c_str(), editor.buffer)
            << "Iter " << iter << ": content mismatch after backspace";
        EXPECT_EQ(cursorPos - 1, editor.cursor)
            << "Iter " << iter << ": cursor should move back after backspace";
        
        verifyInvariants("Iter " + std::to_string(iter));
    }
}

/**
 * Feature: shell-cli-middleware, Property 4c: Delete Removes Correct Character
 * 
 * *For any* buffer with cursor not at end, delete SHALL remove exactly
 * the character at the cursor position.
 * 
 * **Validates: Requirements 4.12**
 */
TEST_F(LineEditorPropertyTest, Property4c_DeleteRemovesCorrectChar) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Start with random content */
        std::string initial = randomAlphanumeric(5, 50);
        line_editor_set_content(&editor, initial.c_str());
        
        /* Move cursor to random position (not at end) */
        std::uniform_int_distribution<int> posDist(0, static_cast<int>(initial.length()) - 1);
        int cursorPos = posDist(rng);
        line_editor_move_to_start(&editor);
        line_editor_move_cursor(&editor, cursorPos);
        
        /* Calculate expected result */
        std::string before = initial.substr(0, cursorPos);
        std::string after = initial.substr(cursorPos + 1);
        std::string expected = before + after;
        
        /* Perform delete */
        bool deleted = line_editor_delete_char(&editor);
        
        EXPECT_TRUE(deleted)
            << "Iter " << iter << ": delete should succeed at pos " << cursorPos;
        EXPECT_STREQ(expected.c_str(), editor.buffer)
            << "Iter " << iter << ": content mismatch after delete";
        EXPECT_EQ(cursorPos, editor.cursor)
            << "Iter " << iter << ": cursor should not move after delete";
        
        verifyInvariants("Iter " + std::to_string(iter));
    }
}

/**
 * Feature: shell-cli-middleware, Property 4d: Cursor Movement Clamping
 * 
 * *For any* cursor movement operation, the cursor SHALL always remain
 * within the valid range [0, length].
 * 
 * **Validates: Requirements 4.8, 4.9**
 */
TEST_F(LineEditorPropertyTest, Property4d_CursorMovementClamping) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Start with random content */
        std::string initial = randomAlphanumeric(5, 50);
        line_editor_set_content(&editor, initial.c_str());
        
        /* Try extreme cursor movements */
        std::uniform_int_distribution<int> offsetDist(-1000, 1000);
        int offset = offsetDist(rng);
        
        line_editor_move_cursor(&editor, offset);
        
        /* Cursor must be clamped to valid range */
        EXPECT_GE(editor.cursor, 0)
            << "Iter " << iter << ": cursor went negative with offset " << offset;
        EXPECT_LE(editor.cursor, editor.length)
            << "Iter " << iter << ": cursor exceeded length with offset " << offset;
        
        verifyInvariants("Iter " + std::to_string(iter));
    }
}

/**
 * Feature: shell-cli-middleware, Property 4e: Delete To End Truncates Correctly
 * 
 * *For any* buffer content and cursor position, delete-to-end SHALL
 * preserve content before cursor and remove all content from cursor onwards.
 * 
 * **Validates: Requirements 4.13**
 */
TEST_F(LineEditorPropertyTest, Property4e_DeleteToEndTruncatesCorrectly) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Start with random content */
        std::string initial = randomAlphanumeric(10, 50);
        line_editor_set_content(&editor, initial.c_str());
        
        /* Move cursor to random position */
        std::uniform_int_distribution<int> posDist(0, static_cast<int>(initial.length()));
        int cursorPos = posDist(rng);
        line_editor_move_to_start(&editor);
        line_editor_move_cursor(&editor, cursorPos);
        
        /* Calculate expected result */
        std::string expected = initial.substr(0, cursorPos);
        
        /* Perform delete to end */
        line_editor_delete_to_end(&editor);
        
        EXPECT_STREQ(expected.c_str(), editor.buffer)
            << "Iter " << iter << ": content mismatch after delete-to-end";
        EXPECT_EQ(cursorPos, editor.cursor)
            << "Iter " << iter << ": cursor should remain at same position";
        EXPECT_EQ(cursorPos, editor.length)
            << "Iter " << iter << ": length should equal cursor position";
        
        verifyInvariants("Iter " + std::to_string(iter));
    }
}

/**
 * Feature: shell-cli-middleware, Property 4f: Delete To Start Removes Prefix
 * 
 * *For any* buffer content and cursor position, delete-to-start SHALL
 * remove content before cursor and preserve content from cursor onwards.
 * 
 * **Validates: Requirements 4.14**
 */
TEST_F(LineEditorPropertyTest, Property4f_DeleteToStartRemovesPrefix) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Start with random content */
        std::string initial = randomAlphanumeric(10, 50);
        line_editor_set_content(&editor, initial.c_str());
        
        /* Move cursor to random position */
        std::uniform_int_distribution<int> posDist(0, static_cast<int>(initial.length()));
        int cursorPos = posDist(rng);
        line_editor_move_to_start(&editor);
        line_editor_move_cursor(&editor, cursorPos);
        
        /* Calculate expected result */
        std::string expected = initial.substr(cursorPos);
        
        /* Perform delete to start */
        line_editor_delete_to_start(&editor);
        
        EXPECT_STREQ(expected.c_str(), editor.buffer)
            << "Iter " << iter << ": content mismatch after delete-to-start";
        EXPECT_EQ(0, editor.cursor)
            << "Iter " << iter << ": cursor should be at start";
        
        verifyInvariants("Iter " + std::to_string(iter));
    }
}

/**
 * Feature: shell-cli-middleware, Property 4g: Clear Resets State
 * 
 * *For any* buffer state, clear SHALL reset length to 0, cursor to 0,
 * and buffer to empty string.
 * 
 * **Validates: Requirements 4.1-4.15**
 */
TEST_F(LineEditorPropertyTest, Property4g_ClearResetsState) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        /* Start with random content and cursor position */
        std::string initial = randomAlphanumeric(1, 100);
        line_editor_set_content(&editor, initial.c_str());
        
        std::uniform_int_distribution<int> posDist(0, static_cast<int>(initial.length()));
        line_editor_move_to_start(&editor);
        line_editor_move_cursor(&editor, posDist(rng));
        
        /* Perform clear */
        line_editor_clear(&editor);
        
        EXPECT_EQ(0, editor.length)
            << "Iter " << iter << ": length should be 0 after clear";
        EXPECT_EQ(0, editor.cursor)
            << "Iter " << iter << ": cursor should be 0 after clear";
        EXPECT_STREQ("", editor.buffer)
            << "Iter " << iter << ": buffer should be empty after clear";
        
        verifyInvariants("Iter " + std::to_string(iter));
    }
}

/**
 * Feature: shell-cli-middleware, Property 4h: Set Content Idempotence
 * 
 * *For any* string, setting content twice with the same string SHALL
 * produce identical state.
 * 
 * **Validates: Requirements 4.1-4.15**
 */
TEST_F(LineEditorPropertyTest, Property4h_SetContentIdempotence) {
    for (int iter = 0; iter < PROPERTY_TEST_ITERATIONS; ++iter) {
        std::string content = randomStringWithSpaces(1, 100);
        
        /* Set content first time */
        line_editor_set_content(&editor, content.c_str());
        uint16_t len1 = editor.length;
        uint16_t cursor1 = editor.cursor;
        std::string buf1 = editor.buffer;
        
        /* Set same content again */
        line_editor_set_content(&editor, content.c_str());
        
        EXPECT_EQ(len1, editor.length)
            << "Iter " << iter << ": length should be same after re-setting";
        EXPECT_EQ(cursor1, editor.cursor)
            << "Iter " << iter << ": cursor should be same after re-setting";
        EXPECT_EQ(buf1, std::string(editor.buffer))
            << "Iter " << iter << ": buffer should be same after re-setting";
        
        verifyInvariants("Iter " + std::to_string(iter));
    }
}
