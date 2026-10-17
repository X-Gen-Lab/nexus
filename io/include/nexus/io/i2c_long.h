/**
 * \file            i2c_long.h
 * \brief           Optional bounded long I2C messages without extra START
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_LONG_H
#define NEXUS_I2C_LONG_H

#include "nexus/io/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NX_I2C_LONG_MAX_MESSAGE_BYTES 65535U

/** \brief Shared read-only optional long-message implementation. */
typedef struct {
    nx_result_t (*transaction)(void* context, nx_i2c_message_t* messages,
                               size_t count, nx_time_us_t deadline,
                               size_t* transferred);
} nx_i2c_long_ops_t;

/** \brief Optional immutable face; controller state is shared with base I2C.
 * \note Face and private context outlive callers. No queue or heap is owned.
 */
typedef struct {
    const nx_i2c_long_ops_t* ops;
    void* context;
} nx_i2c_long_endpoint_t;

/**
 * \brief           Validate complete messages before acquiring any hardware
 * \param[in]       messages: Caller-owned finite descriptors and data
 * \param[in]       count: One through NX_I2C_MAX_MESSAGES
 * \return          SUCCESS, INVALID or UNSUPPORTED; no references retained
 * \note            Each message has 1..65535 bytes. A mutable read cannot
 *                  overlap a write span or descriptor storage. Write data and
 *                  descriptors stay immutable throughout a transaction.
 */
nx_result_t nx_i2c_long_messages_validate(const nx_i2c_message_t* messages,
                                          size_t count);

/**
 * \brief           Execute finite messages without splitting address phases
 * \param[in]       endpoint: Selected optional long-message provider
 * \param[in,out]   messages: Descriptors and buffers borrowed until return
 * \param[in]       count: One through NX_I2C_MAX_MESSAGES
 * \param[in]       deadline: Finite absolute provider-clock deadline
 * \param[out]      transferred: Register byte progress, including failure;
 *                  TX count does not prove those bytes were ACKed on wire
 * \return          SUCCESS or INVALID/UNSUPPORTED/CONTEXT/STATE/BUSY/TIMEOUT/
 *                  NACK/ARBITRATION/IO after local controller drain
 * \note            Task-only, one executor shared with base I2C. Each message
 *                  issues one START; only a following message issues repeated
 *                  START. Hardware reload does not split messages. Final STOP
 *                  and bus-idle observation precede successful return. Failure
 *                  never replays a prefix. Failed abort drain cannot return and
 *                  release borrowed storage. No implicit bus recovery occurs.
 */
nx_result_t
nx_i2c_long_endpoint_transaction(const nx_i2c_long_endpoint_t* endpoint,
                                 nx_i2c_message_t* messages, size_t count,
                                 nx_time_us_t deadline, size_t* transferred);

#ifdef __cplusplus
}
#endif
#endif
