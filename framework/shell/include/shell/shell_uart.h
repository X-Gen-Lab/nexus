/** Explicit typed UART shell adapter. SPDX-License-Identifier: MIT */
#ifndef SHELL_UART_H
#define SHELL_UART_H
#include "shell_backend.h"
#include "hal/base/nx_device.h"
#ifdef __cplusplus
extern "C" {
#endif
extern const shell_backend_t shell_uart_backend;
/** Single management/task context. The device is opened exclusively by name;
 * no legacy UART pointer or native fake fallback is used. */
shell_status_t shell_uart_backend_init(const char* device_name);
/** Retries close/cancellation. BUSY/error retains the reference and static TX
 * buffer; do not reset/reinitialize a failed teardown to bypass ownership. */
shell_status_t shell_uart_backend_deinit(void);
bool shell_uart_backend_is_initialized(void);
/** Diagnostic status of the last driver operation; RX errors return a negative
 * shell read result and are never reported as ordinary empty input. */
nx_status_t shell_uart_backend_last_status(void);
#ifdef __cplusplus
}
#endif
#endif
