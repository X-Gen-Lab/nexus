/**
 * \file            modbus_tcp.h
 * \brief           Bounded Modbus TCP framing with explicit register policy
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_MODBUS_TCP_H
#define NEXUS_COMPONENTS_MODBUS_TCP_H
#include "nexus/components/modbus_registers.h"
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_MODBUS_TCP_ADU_MAX 260u
/** \brief One serialized session; no socket, global map or worker. */
typedef struct {
    nx_modbus_registers_t registers;
    uint8_t rx[NX_MODBUS_TCP_ADU_MAX];
    uint8_t tx[NX_MODBUS_TCP_ADU_MAX];
    uint16_t values[NX_MODBUS_REGISTER_MAX];
    size_t received;
    size_t expected;
    size_t response_size;
    uint8_t unit;
    bool failed;
    bool active;
} nx_modbus_tcp_t;
/**
 * \brief           Initialize an unused, quiescent session.
 * \param[out]      session: Caller storage, private after success.
 * \param[in]       registers: Bounded callbacks, copied; context stays live.
 * \param[in]       unit: Exact accepted unit identity, 1..247; no broadcast.
 * \return          SUCCESS or INVALID, unchanged on failure.
 * \note            Task-only serialized calls; no I/O or allocation. TCP is
 *                  unauthenticated: product callbacks own authorization.
 */
nx_result_t nx_modbus_tcp_init(nx_modbus_tcp_t* session,
                               const nx_modbus_registers_t* registers,
                               uint8_t unit);
/**
 * \brief           Consume bytes through at most one complete MBAP request.
 * \param[in,out]   session: Initialized, serialized session.
 * \param[in]       data: Readable bytes, not retained; NULL only for zero size.
 * \param[in]       size: Available bytes, may include another request.
 * \param[out]      consumed: Distinct count; zero on admission failure.
 * \return          SUCCESS after response production, BUSY for partial/pending,
 *                  INVALID for malformed MBAP (sticky failed session).
 * \note            No socket I/O, deadline or retained input. Caller advances
 *                  socket with its finite deadline. Pending response blocks
 *                  the next request, so writes are never executed twice while
 *                  a response is backpressured. Bad framing requires closing
 *                  the connection; do not search arbitrary payload for MBAP.
 */
nx_result_t nx_modbus_tcp_feed(nx_modbus_tcp_t* session, const uint8_t* data,
                               size_t size, size_t* consumed);
/**
 * \brief           Borrow the complete response until exact release.
 * \param[in]       session: Initialized serialized session.
 * \param[out]      data: Distinct pointer into stable session TX storage.
 * \param[out]      size: Distinct number of response bytes.
 * \return          SUCCESS, EMPTY, INVALID or STATE; failure leaves outputs.
 * \note            Transport must copy or settle its borrow before release.
 */
nx_result_t nx_modbus_tcp_response(const nx_modbus_tcp_t* session,
                                   const uint8_t** data, size_t* size);
/**
 * \brief           Release a fully copied/settled response for next request.
 * \param[in,out]   session: Serialized session, all response borrowers joined.
 * \return          SUCCESS, EMPTY, INVALID or STATE; does not close a socket.
 * \note            Product closes admission and joins socket operations before
 *                  reclaiming or reinitializing session storage.
 */
nx_result_t nx_modbus_tcp_release(nx_modbus_tcp_t* session);
#ifdef __cplusplus
}
#endif
#endif
