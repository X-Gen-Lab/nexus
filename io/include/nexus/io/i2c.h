/**
 * \file            i2c.h
 *
 * \brief           Address endpoints, repeated START and explicit STOP
 *                  settlement.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_H
#define NEXUS_I2C_H

#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_i2c_port nx_i2c_port_t;
typedef struct nx_i2c_endpoint nx_i2c_endpoint_t;
/**
 * \brief           One message; a new message starts with repeated START,
 *                  final with STOP.
 */
typedef struct {
    uint8_t* data;
    size_t length;
    bool read;
} nx_i2c_message_t;

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*recover)(void* context);
} nx_i2c_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_i2c_port {
    const nx_i2c_ops_t* ops;
    void* context;
};

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*transaction)(void* context, nx_i2c_message_t* messages,
                               size_t count, nx_time_us_t deadline,
                               size_t* transferred);
} nx_i2c_endpoint_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_i2c_endpoint {
    const nx_i2c_endpoint_ops_t* ops;
    void* context;
};

/**
 * \brief           Execute a polling 7-bit-address message transaction.
 *
 * \param[in]       endpoint: Fixed address/controller binding.
 *
 * \param[in,out]   messages: Message buffers; write data are not modified.
 *
 * \param[in]       count: Positive finite message count.
 *
 * \param[in]       deadline: Absolute provider-clock deadline including queue.
 *
 * \param[out]      transferred: Successfully transferred byte count.
 *
 * \return          Success, INVALID/BUSY/TIMEOUT/NACK/ARBITRATION/IO. Return
 *                  releases buffers after STOP/abort drain, not bus recovery.
 *
 * \note            Task-only, one executor. No hidden scan or GPIO recovery.
 *                  Failed bus recovery is never reported as transfer success.
 */
nx_result_t nx_i2c_endpoint_transaction(const nx_i2c_endpoint_t* endpoint,
                                        nx_i2c_message_t* messages,
                                        size_t count, nx_time_us_t deadline,
                                        size_t* transferred);
/**
 * \brief           Recover an idle controller after a bus fault.
 *
 * \param[in,out]   port: Single executor, no live transaction/buffer borrow.
 *
 * \return          Success only when hardware bus idle is observed; otherwise
 *                  BUSY/IO/UNSUPPORTED. Does not replay a failed transaction.
 */
nx_result_t nx_i2c_port_recover(const nx_i2c_port_t* port);
#ifdef __cplusplus
}
#endif

#endif
