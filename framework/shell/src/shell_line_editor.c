/**
 * @file shell_line_editor.c
 * @brief Shell line editor implementation
 * 
 * Implements line editing functionality including character insertion,
 * deletion, cursor movement, and advanced editing operations.
 * 
 * Requirements: 4.1, 4.2, 4.3, 4.8, 4.9, 4.10, 4.11, 4.12, 4.13, 4.14, 4.15
 */

#include "shell/shell_line_editor.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

void line_editor_init(line_editor_t* editor, char* buffer, uint16_t size)
{
    if (editor == NULL || buffer == NULL || size == 0) {
        return;
    }
    
    editor->buffer = buffer;
    editor->buffer_size = size;
    editor->length = 0;
    editor->cursor = 0;
    editor->insert_mode = true;
    
    /* Ensure buffer is null-terminated */
    editor->buffer[0] = '\0';
}

/*---------------------------------------------------------------------------*/
/* Core Editing Operations - Requirements 4.2, 4.3                           */
/*---------------------------------------------------------------------------*/

bool line_editor_insert_char(line_editor_t* editor, char c)
{
    if (editor == NULL || editor->buffer == NULL) {
        return false;
    }
    
    /* Check if buffer is full (need space for char + null terminator) */
    if (editor->length >= editor->buffer_size - 1) {
        return false;
    }
    
    if (editor->insert_mode) {
        /* Insert mode: shift characters right to make room */
        if (editor->cursor < editor->length) {
            memmove(&editor->buffer[editor->cursor + 1],
                    &editor->buffer[editor->cursor],
                    editor->length - editor->cursor);
        }
        editor->buffer[editor->cursor] = c;
        editor->length++;
        editor->cursor++;
    } else {
        /* Overwrite mode: replace character at cursor */
        if (editor->cursor < editor->length) {
            editor->buffer[editor->cursor] = c;
            editor->cursor++;
        } else {
            /* At end, append character */
            editor->buffer[editor->cursor] = c;
            editor->length++;
            editor->cursor++;
        }
    }
    
    /* Ensure null termination */
    editor->buffer[editor->length] = '\0';
    
    return true;
}


bool line_editor_delete_char(line_editor_t* editor)
{
    if (editor == NULL || editor->buffer == NULL) {
        return false;
    }
    
    /* Cannot delete if cursor is at end */
    if (editor->cursor >= editor->length) {
        return false;
    }
    
    /* Shift characters left to fill the gap */
    memmove(&editor->buffer[editor->cursor],
            &editor->buffer[editor->cursor + 1],
            editor->length - editor->cursor);
    
    editor->length--;
    editor->buffer[editor->length] = '\0';
    
    return true;
}

bool line_editor_backspace(line_editor_t* editor)
{
    if (editor == NULL || editor->buffer == NULL) {
        return false;
    }
    
    /* Cannot backspace if cursor is at start */
    if (editor->cursor == 0) {
        return false;
    }
    
    /* Move cursor back and delete character at that position */
    editor->cursor--;
    
    /* Shift characters left to fill the gap */
    memmove(&editor->buffer[editor->cursor],
            &editor->buffer[editor->cursor + 1],
            editor->length - editor->cursor);
    
    editor->length--;
    editor->buffer[editor->length] = '\0';
    
    return true;
}

/*---------------------------------------------------------------------------*/
/* Cursor Movement - Requirements 4.8, 4.9                                   */
/*---------------------------------------------------------------------------*/

void line_editor_move_cursor(line_editor_t* editor, int offset)
{
    if (editor == NULL) {
        return;
    }
    
    int new_pos = (int)editor->cursor + offset;
    
    /* Clamp to valid range [0, length] */
    if (new_pos < 0) {
        new_pos = 0;
    } else if (new_pos > (int)editor->length) {
        new_pos = (int)editor->length;
    }
    
    editor->cursor = (uint16_t)new_pos;
}

void line_editor_move_to_start(line_editor_t* editor)
{
    if (editor == NULL) {
        return;
    }
    
    editor->cursor = 0;
}

void line_editor_move_to_end(line_editor_t* editor)
{
    if (editor == NULL) {
        return;
    }
    
    editor->cursor = editor->length;
}

/*---------------------------------------------------------------------------*/
/* Advanced Editing - Requirements 4.10, 4.11, 4.12, 4.13, 4.14, 4.15        */
/*---------------------------------------------------------------------------*/

void line_editor_delete_to_end(line_editor_t* editor)
{
    if (editor == NULL || editor->buffer == NULL) {
        return;
    }
    
    /* Truncate at cursor position */
    editor->length = editor->cursor;
    editor->buffer[editor->length] = '\0';
}

void line_editor_delete_to_start(line_editor_t* editor)
{
    if (editor == NULL || editor->buffer == NULL) {
        return;
    }
    
    if (editor->cursor == 0) {
        return;
    }
    
    /* Shift remaining content to start */
    uint16_t remaining = editor->length - editor->cursor;
    memmove(editor->buffer, &editor->buffer[editor->cursor], remaining);
    
    editor->length = remaining;
    editor->cursor = 0;
    editor->buffer[editor->length] = '\0';
}

void line_editor_delete_word(line_editor_t* editor)
{
    if (editor == NULL || editor->buffer == NULL) {
        return;
    }
    
    if (editor->cursor == 0) {
        return;
    }
    
    uint16_t start = editor->cursor;
    
    /* Skip trailing whitespace before cursor */
    while (start > 0 && (editor->buffer[start - 1] == ' ' || 
                          editor->buffer[start - 1] == '\t')) {
        start--;
    }
    
    /* Find start of word (non-whitespace characters) */
    while (start > 0 && editor->buffer[start - 1] != ' ' && 
                        editor->buffer[start - 1] != '\t') {
        start--;
    }
    
    if (start == editor->cursor) {
        return;
    }
    
    /* Calculate how many characters to delete */
    uint16_t delete_count = editor->cursor - start;
    
    /* Shift remaining content left */
    uint16_t remaining = editor->length - editor->cursor;
    memmove(&editor->buffer[start], &editor->buffer[editor->cursor], remaining);
    
    editor->length -= delete_count;
    editor->cursor = start;
    editor->buffer[editor->length] = '\0';
}

void line_editor_clear(line_editor_t* editor)
{
    if (editor == NULL || editor->buffer == NULL) {
        return;
    }
    
    editor->length = 0;
    editor->cursor = 0;
    editor->buffer[0] = '\0';
}

/*---------------------------------------------------------------------------*/
/* Accessor Functions                                                        */
/*---------------------------------------------------------------------------*/

const char* line_editor_get_buffer(const line_editor_t* editor)
{
    if (editor == NULL || editor->buffer == NULL) {
        return "";
    }
    
    return editor->buffer;
}

uint16_t line_editor_get_length(const line_editor_t* editor)
{
    if (editor == NULL) {
        return 0;
    }
    
    return editor->length;
}

uint16_t line_editor_get_cursor(const line_editor_t* editor)
{
    if (editor == NULL) {
        return 0;
    }
    
    return editor->cursor;
}

void line_editor_set_content(line_editor_t* editor, const char* content)
{
    if (editor == NULL || editor->buffer == NULL) {
        return;
    }
    
    if (content == NULL) {
        line_editor_clear(editor);
        return;
    }
    
    size_t len = strlen(content);
    
    /* Truncate if content is too long */
    if (len >= editor->buffer_size) {
        len = editor->buffer_size - 1;
    }
    
    memcpy(editor->buffer, content, len);
    editor->buffer[len] = '\0';
    editor->length = (uint16_t)len;
    editor->cursor = (uint16_t)len;
}
