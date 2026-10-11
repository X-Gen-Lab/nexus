/**
 * \file            user_io_broker.h
 * \brief           Narrow kernel-invoked user GPIO message contract
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_COMPONENTS_USER_IO_BROKER_H
#define NEXUS_COMPONENTS_USER_IO_BROKER_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Only scalar GPIO operations are admitted by this message ABI. */
enum { NX_USER_IO_ABI = 1U, NX_USER_GPIO_READ = 1U, NX_USER_GPIO_WRITE = 2U };
/**
 * \brief           Untrusted copied message; identity never comes from users
 * \note            No arbitrary port, MMIO, callback, slot or principal
 * pointer. reserved words must be zero. WRITE has read_mask=0 and disjoint
 * set/reset masks. READ has zero set/reset masks and a positive authorized
 * read_mask. Caller keeps storage immutable until the gateway completes or
 * retains its pin.
 */
typedef struct {
    uint32_t abi;
    uint32_t device;
    uint32_t operation;
    uint32_t set_mask;
    uint32_t reset_mask;
    uint32_t read_mask;
    uint32_t reserved[2];
} nx_user_io_message_t;
/** \brief Fully initialized copied result; value is zero for errors/WRITE. */
typedef struct {
    uint32_t result;
    uint32_t value;
} nx_user_io_reply_t;
/** \brief Broker and persistent kernel call storage stay provider private. */
typedef struct nx_user_io_broker nx_user_io_broker_t;
typedef struct nx_user_io_call nx_user_io_call_t;
/**
 * \brief           Distinguish admission, cleanup retention and possible effect
 * \note            accepted means at least one memory pin was acquired. On
 *                  retained=true the exact kernel call slot and user address
 *                  space remain borrowed. device_effect_possible is set before
 *                  a WRITE provider call; an error cannot authorize replay.
 *                  result describes admission/device execution; cleanup_result
 *                  separately describes the most recent release attempt.
 *                  This is software responsibility, not physical pin evidence.
 */
typedef struct {
    nx_result_t result;
    nx_result_t cleanup_result;
    bool accepted;
    bool retained;
    bool device_effect_possible;
} nx_user_io_result_t;
/**
 * \brief           Execute one scalar GPIO message through a trusted gateway
 * \return          REJECTED has accepted=false, no pin/effect. ACCEPTED may
 *                  already be detached or retain cleanup after a failure.
 * \note            Kernel-invoked privileged Thread with incoming masks clear,
 *                  one serialized broker
 *                  executor. Principal comes from the configured trusted kernel
 *                  identity port. Pin both validated memory ranges before any
 *                  GPIO call; snapshot input once, copy an initialized reply.
 *                  Kernel call storage is zero-initialized outside user RAM and
 *                  must outlive retained pins. No OS SVC/MPU/TZ wiring or user
 *                  privilege elevation is provided by this component.
 */
nx_user_io_result_t nx_user_io_broker_gpio(nx_user_io_broker_t* broker,
                                           nx_user_io_call_t* call,
                                           const nx_user_io_message_t* message,
                                           nx_user_io_reply_t* reply);
/**
 * \brief           Retry retained cleanup without reading users or replaying IO
 * \note            Same privileged serialized kernel owner, incoming masks
 * clear. Successful unpin follows actual provider/observer drain; a failed
 * unpin keeps memory, call slot and identity retained. Does not imply that a
 * previously performed GPIO write can be reversed.
 */
nx_user_io_result_t nx_user_io_broker_gpio_finish(nx_user_io_broker_t* broker,
                                                  nx_user_io_call_t* call);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_COMPONENTS_USER_IO_BROKER_H */
