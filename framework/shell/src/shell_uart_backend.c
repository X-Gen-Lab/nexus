/** Typed UART adapter with retained teardown ownership. SPDX-License-Identifier: MIT */
#include "shell/shell_uart.h"
#include "osal/osal.h"
#include <string.h>
#define SHELL_UART_TX_CAPACITY 256u
#define SHELL_UART_TX_TIMEOUT_MS 1000u
static nx_device_ref_t g_uart;
static nx_uart_ticket_t g_ticket;
static uint8_t g_tx[SHELL_UART_TX_CAPACITY];
static bool g_opened, g_busy, g_active, g_unknown_lease;
static nx_status_t g_last_status;
static shell_status_t translated(nx_status_t s) {
    if (s == NX_OK) return SHELL_OK;
    if (s == NX_ERR_BUSY) return SHELL_ERROR_BUSY;
    if (s == NX_ERR_TIMEOUT) return SHELL_ERROR_TIMEOUT;
    if (s == NX_ERR_CONTEXT) return SHELL_ERROR_ISR;
    if (s == NX_ERR_NOT_SUPPORTED) return SHELL_ERROR_UNSUPPORTED;
    return SHELL_ERROR;
}
static bool claim(void) {
    if (osal_is_isr()) return false;
    osal_enter_critical(); bool ok = !g_busy;
    if (ok) g_busy = true;
    osal_exit_critical(); return ok;
}
static void release(void) {
    osal_enter_critical(); g_busy = false; osal_exit_critical();
}
static void remember(nx_status_t status) {
    osal_enter_critical(); g_last_status = status; osal_exit_critical();
}
static int uart_backend_read(uint8_t* data, int max_len) {
    if (!data || max_len <= 0 || !claim()) return 0;
    if (!g_opened || g_active || g_unknown_lease) { release(); return 0; }
    int n = 0;
    while (n < max_len) {
        nx_uart_rx_event_t event;
        nx_status_t status = nx_device_uart_receive_event(g_uart, &event);
        if (status == NX_ERR_NO_DATA) break;
        if (status != NX_OK || event.status != NX_OK || event.raw_error || !event.has_data) {
            remember(status == NX_OK ? (event.status == NX_OK ? NX_ERR_IO : event.status) : status);
            release(); return n ? n : -1;
        }
        data[n++] = event.data;
    }
    release(); return n;
}
static int uart_backend_write(const uint8_t* data, int len) {
    if (!data || len <= 0 || !claim()) return 0;
    if (!g_opened || g_active || g_unknown_lease) { release(); return 0; }
    int accepted = 0;
    uint32_t started;
    if (osal_get_time_ms(&started) != OSAL_OK) { release(); return 0; }
    while (accepted < len) {
        size_t chunk = (size_t)(len - accepted);
        if (chunk > sizeof(g_tx)) chunk = sizeof(g_tx);
        memcpy(g_tx, data + accepted, chunk);
        uint32_t now;
        if (osal_get_time_ms(&now) != OSAL_OK) break;
        uint32_t elapsed = now - started;
        if (elapsed >= SHELL_UART_TX_TIMEOUT_MS) { remember(NX_ERR_TIMEOUT); break; }
        nx_status_t status = nx_device_uart_submit(g_uart, g_tx, chunk,
                SHELL_UART_TX_TIMEOUT_MS - elapsed, &g_ticket);
        remember(status);
        if (status != NX_OK) {
            g_unknown_lease = status == NX_ERR_INVALID_STATE;
            break;
        }
        g_active = true;
        bool success = false;
        for (;;) {
            nx_uart_result_t result;
            status = nx_device_uart_poll(g_uart, g_ticket, &result);
            if (status != NX_OK) { remember(status); break; }
            if (result.settled && result.wire_idle) {
                g_active = false; g_ticket.sequence = 0; remember(result.status);
                success = result.status == NX_OK; break;
            }
            if (osal_get_time_ms(&now) != OSAL_OK || now - started >= SHELL_UART_TX_TIMEOUT_MS) {
                remember(NX_ERR_TIMEOUT); break;
            }
            if (osal_task_delay(1) != OSAL_OK) { remember(NX_ERR_BUSY); break; }
        }
        if (g_active) {
            status = nx_device_uart_cancel(g_uart, g_ticket);
            if (status == NX_OK) { g_active = false; g_ticket.sequence = 0; }
            else remember(status); /* g_tx remains borrowed and cannot be overwritten. */
        }
        if (!success) break;
        accepted += (int)chunk;
    }
    release(); return accepted;
}
const shell_backend_t shell_uart_backend = {.read = uart_backend_read, .write = uart_backend_write};
shell_status_t shell_uart_backend_init(const char* name) {
    if (!name || !*name) return SHELL_ERROR_INVALID_PARAM;
    if (osal_is_isr()) return SHELL_ERROR_ISR;
    if (!claim()) return SHELL_ERROR_BUSY;
    if (g_opened) { release(); return SHELL_ERROR_ALREADY_INIT; }
    nx_status_t status = nx_device_open(name, NX_DEVICE_CLASS_UART, (uintptr_t)&g_uart, &g_uart);
    if (status == NX_OK) {
        osal_enter_critical(); g_opened = true; osal_exit_critical();
        nx_device_caps_t caps;
        status = nx_device_query(g_uart, &caps);
        if (status == NX_OK && ((caps.flags & (NX_DEVICE_CAP_UART_OPERATIONS | NX_DEVICE_CAP_UART_RX_EVENTS)) !=
                    (NX_DEVICE_CAP_UART_OPERATIONS | NX_DEVICE_CAP_UART_RX_EVENTS))) status = NX_ERR_NOT_SUPPORTED;
        if (status != NX_OK && nx_device_close(g_uart) == NX_OK) {
            osal_enter_critical(); g_opened = false; osal_exit_critical();
            memset(&g_uart, 0, sizeof(g_uart));
        }
    }
    remember(status); release(); return translated(status);
}
shell_status_t shell_uart_backend_deinit(void) {
    if (osal_is_isr()) return SHELL_ERROR_ISR;
    if (!claim()) return SHELL_ERROR_BUSY;
    if (!g_opened) { release(); return SHELL_OK; }
    nx_status_t status = NX_OK;
    if (g_unknown_lease) status = nx_device_uart_recover(g_uart);
    else {
        if (g_active) {
            status = nx_device_uart_cancel(g_uart, g_ticket);
            if (status == NX_OK) { g_active = false; g_ticket.sequence = 0; }
        }
        if (status == NX_OK) status = nx_device_close(g_uart);
    }
    if (status == NX_OK) {
        osal_enter_critical(); g_opened = g_active = g_unknown_lease = false; osal_exit_critical();
        memset(&g_uart, 0, sizeof(g_uart)); g_ticket.sequence = 0;
    }
    /* Failed close/cancel/recover leaves the typed owner and TX bytes intact. */
    remember(status); release(); return translated(status);
}
bool shell_uart_backend_is_initialized(void) {
    osal_enter_critical(); bool open = g_opened; osal_exit_critical(); return open;
}
nx_status_t shell_uart_backend_last_status(void) {
    osal_enter_critical(); nx_status_t status = g_last_status; osal_exit_critical(); return status;
}
