#ifndef SHELL_MOCK_BACKEND_H
#define SHELL_MOCK_BACKEND_H
#include "shell/shell_backend.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \defgroup        SHELL_MOCK_BACKEND Mock Backend (Testing)
 * \brief           Mock backend for testing
 * \{
 */

/**
 * \brief           Mock backend instance
 */
extern const shell_backend_t shell_mock_backend;

/**
 * \brief           Initialize mock backend
 * \return          SHELL_OK on success
 */
shell_status_t shell_mock_backend_init(void);

/**
 * \brief           Deinitialize mock backend
 * \return          SHELL_OK on success
 */
shell_status_t shell_mock_backend_deinit(void);

/**
 * \brief           Reset mock backend buffers
 */
void shell_mock_backend_reset(void);

/**
 * \brief           Inject input data into mock backend
 * \param[in]       data: Data to inject
 * \param[in]       len: Length of data
 * \return          Number of bytes injected
 */
int shell_mock_backend_inject_input(const uint8_t* data, size_t len);

/**
 * \brief           Inject input string into mock backend
 * \param[in]       str: Null-terminated string to inject
 * \return          Number of bytes injected
 */
int shell_mock_backend_inject_string(const char* str);

/**
 * \brief           Get captured output data
 * \param[out]      data: Buffer to store output data
 * \param[in]       max_len: Maximum buffer size
 * \return          Number of bytes copied
 */
int shell_mock_backend_get_output(uint8_t* data, size_t max_len);

/**
 * \brief           Get captured output as string
 * \param[out]      str: Buffer to store output string
 * \param[in]       max_len: Maximum buffer size (including null terminator)
 * \return          Number of characters copied (excluding null terminator)
 */
int shell_mock_backend_get_output_string(char* str, size_t max_len);

/**
 * \brief           Get current output length
 * \return          Number of bytes in output buffer
 */
size_t shell_mock_backend_get_output_length(void);

/**
 * \brief           Clear output buffer
 */
void shell_mock_backend_clear_output(void);

/**
 * \brief           Get remaining input length
 * \return          Number of bytes remaining in input buffer
 */
size_t shell_mock_backend_get_remaining_input(void);

/**
 * \brief           Check if mock backend is initialized
 * \return          true if initialized, false otherwise
 */
bool shell_mock_backend_is_initialized(void);

/**
 * \}
 */


#ifdef __cplusplus
}
#endif
#endif
