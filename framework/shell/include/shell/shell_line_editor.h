/**
 * \file            shell_line_editor.h
 * \brief           Shell line editor interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This file defines the line editor data structures and functions
 * for handling interactive command line editing.
 */

#ifndef SHELL_LINE_EDITOR_H
#define SHELL_LINE_EDITOR_H

#include "shell_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        SHELL_LINE_EDITOR Line Editor
 * \brief           Interactive line editing functionality
 * \{
 */

/**
 * \brief           Line editor state structure
 */
typedef struct {
    char*       buffer;         /**< Input buffer pointer */
    uint16_t    buffer_size;    /**< Buffer size (capacity) */
    uint16_t    length;         /**< Current content length */
    uint16_t    cursor;         /**< Cursor position (0 to length) */
    bool        insert_mode;    /**< Insert mode (true) or overwrite mode */
} line_editor_t;

/**
 * \brief           Initialize line editor
 * \param[in,out]   editor: Pointer to line editor structure
 * \param[in]       buffer: Pointer to character buffer
 * \param[in]       size: Size of the buffer
 */
void line_editor_init(line_editor_t* editor, char* buffer, uint16_t size);

/**
 * \brief           Insert a character at cursor position
 * \param[in,out]   editor: Pointer to line editor
 * \param[in]       c: Character to insert
 * \return          true if character was inserted, false if buffer full
 */
bool line_editor_insert_char(line_editor_t* editor, char c);

/**
 * \brief           Delete character at cursor position (Delete key)
 * \param[in,out]   editor: Pointer to line editor
 * \return          true if character was deleted, false if at end
 */
bool line_editor_delete_char(line_editor_t* editor);

/**
 * \brief           Delete character before cursor (Backspace)
 * \param[in,out]   editor: Pointer to line editor
 * \return          true if character was deleted, false if at start
 */
bool line_editor_backspace(line_editor_t* editor);

/**
 * \brief           Move cursor by offset
 * \param[in,out]   editor: Pointer to line editor
 * \param[in]       offset: Number of positions to move (can be negative)
 */
void line_editor_move_cursor(line_editor_t* editor, int offset);

/**
 * \brief           Move cursor to start of line (Home/Ctrl+A)
 * \param[in,out]   editor: Pointer to line editor
 */
void line_editor_move_to_start(line_editor_t* editor);

/**
 * \brief           Move cursor to end of line (End/Ctrl+E)
 * \param[in,out]   editor: Pointer to line editor
 */
void line_editor_move_to_end(line_editor_t* editor);

/**
 * \brief           Delete from cursor to end of line (Ctrl+K)
 * \param[in,out]   editor: Pointer to line editor
 */
void line_editor_delete_to_end(line_editor_t* editor);

/**
 * \brief           Delete from start to cursor (Ctrl+U)
 * \param[in,out]   editor: Pointer to line editor
 */
void line_editor_delete_to_start(line_editor_t* editor);

/**
 * \brief           Delete word before cursor (Ctrl+W)
 * \param[in,out]   editor: Pointer to line editor
 */
void line_editor_delete_word(line_editor_t* editor);

/**
 * \brief           Clear the entire line
 * \param[in,out]   editor: Pointer to line editor
 */
void line_editor_clear(line_editor_t* editor);

/**
 * \brief           Get current buffer content
 * \param[in]       editor: Pointer to line editor
 * \return          Pointer to null-terminated buffer content
 */
const char* line_editor_get_buffer(const line_editor_t* editor);

/**
 * \brief           Get current content length
 * \param[in]       editor: Pointer to line editor
 * \return          Current content length
 */
uint16_t line_editor_get_length(const line_editor_t* editor);

/**
 * \brief           Get current cursor position
 * \param[in]       editor: Pointer to line editor
 * \return          Current cursor position
 */
uint16_t line_editor_get_cursor(const line_editor_t* editor);

/**
 * \brief           Set buffer content
 * \param[in,out]   editor: Pointer to line editor
 * \param[in]       content: String to set as buffer content
 */
void line_editor_set_content(line_editor_t* editor, const char* content);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* SHELL_LINE_EDITOR_H */
