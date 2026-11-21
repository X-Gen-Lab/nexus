/**
 * \file            at_client.h
 * \brief           Bounded single-owner AT command, URC and binary parsing
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_AT_CLIENT_H
#define NEXUS_COMPONENTS_AT_CLIENT_H
#include "nexus/network/byte_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Callback view, valid only until callback return. */
typedef enum {
    NX_AT_RESPONSE,
    NX_AT_URC,
    NX_AT_BINARY,
    NX_AT_COMPLETE
} nx_at_event_kind_t;
typedef struct {
    nx_at_event_kind_t kind;
    const uint8_t* data;
    size_t size;
    nx_result_t result;
} nx_at_event_t;
/** \brief Explicit caller-owned state; fields private after initialization. */
typedef struct {
    nx_byte_stream_t stream;
    nx_clock_t clock;
    void (*event)(void* context, const nx_at_event_t* event);
    void* context;
    const char* const* urc_prefixes;
    size_t urc_count;
    uint8_t* line;
    size_t line_capacity;
    size_t line_size;
    uint8_t* transmit;
    size_t transmit_capacity;
    size_t transmit_size;
    size_t transmit_offset;
    size_t binary_remaining;
    nx_time_us_t deadline;
    nx_time_us_t last_time;
    nx_result_t result;
    bool command_active;
    bool active;
    bool failed;
} nx_at_client_t;
/** \brief Stable assembly ports; no worker, ISR callbacks or UART ownership. */
typedef struct {
    nx_byte_stream_t stream;
    nx_clock_t clock;
    void (*event)(void* context, const nx_at_event_t* event);
    void* context;
    const char* const* urc_prefixes;
    size_t urc_count;
    uint8_t* line;
    size_t line_capacity;
    uint8_t* transmit;
    size_t transmit_capacity;
} nx_at_config_t;
/**
 * \brief           Bind one caller-owned AT parser to an existing byte stream.
 * \param[out]      client: Unused/quiescent metadata, private after success.
 * \param[in]       config: Disjoint caller buffers and stable
 * callbacks/prefixes. \return          SUCCESS or INVALID, unchanged on
 * admission failure. \note            Task-only serialized calls; no I/O/heap.
 * Prefix strings are immutable, nonempty and retained until all users join.
 */
nx_result_t nx_at_init(nx_at_client_t* client, const nx_at_config_t* config);
/**
 * \brief           Copy one command and append CRLF without starting transport.
 * \param[in,out]   client: Initialized serialized healthy state.
 * \param[in]       command: Nonempty text, no NUL/CR/LF, copied before return.
 * \param[in]       size: Text byte count, excludes terminator.
 * \param[in]       deadline: Finite absolute deadline in the stream clock
 * domain. \return          SUCCESS, INVALID, BUSY, EXHAUSTED, TIMEOUT or STATE.
 * \note            Only one command is active. Standard OK/ERROR/+CME ERROR/
 *                  +CMS ERROR terminate it. Module-specific replies use events.
 */
nx_result_t nx_at_submit(nx_at_client_t* client, const uint8_t* command,
                         size_t size, nx_time_us_t deadline);
/**
 * \brief           Advance immediate copying stream calls with explicit budget.
 * \param[in,out]   client: Initialized state and all retained storage still
 * live. \param[in]       deadline: Finite service deadline, bounded with
 * command time. \param[in]       io_budget: Nonzero maximum individual
 * read/write calls. \return          SUCCESS for progress, BUSY for no
 * progress, or failure. \note            No waits/worker. Callbacks are
 * bounded, cannot reenter any client operation except expect_binary.
 * Timeout/overflow or I/O loss latches desynchronization; recover explicitly
 * after the product has drained/reset the module to a known boundary.
 */
nx_result_t nx_at_service(nx_at_client_t* client, nx_time_us_t deadline,
                          uint32_t io_budget);
/**
 * \brief           Select the next exact bytes as binary, including CR/LF/NUL.
 * \param[in,out]   client: Serialized initialized parser, optionally in event.
 * \param[in]       bytes: Positive exact module-announced binary length.
 * \return          SUCCESS, INVALID, BUSY or STATE; no transport access.
 * \note            Payload is reported in callback-scoped chunks; the caller
 *                  validates its module's length before invoking this method.
 */
nx_result_t nx_at_expect_binary(nx_at_client_t* client, size_t bytes);
/**
 * \brief           Reset software framing after external synchronization.
 * \param[in,out]   client: All event/stream calls have returned; healthy module
 *                  boundary and transport drain are independently established.
 * \return          SUCCESS, BUSY or INVALID; does not reset or probe hardware.
 * \note            Abandons a timed-out command, never silently replays it.
 */
nx_result_t nx_at_recover(nx_at_client_t* client);
#ifdef __cplusplus
}
#endif
#endif
