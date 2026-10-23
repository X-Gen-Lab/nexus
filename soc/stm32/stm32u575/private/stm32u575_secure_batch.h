/**
 * \file            stm32u575_secure_batch.h
 * \brief           Copied bounded Secure GPIO jobs with explicit execution
 * \author          Nexus Team
 */
#ifndef NX_STM32U575_SECURE_BATCH_H
#define NX_STM32U575_SECURE_BATCH_H
#include "nexus/io/gpio.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_STM32U575_SECURE_BATCH_MAX 8U
/** \brief Scalar command; no address or callback crosses the gateway. */
typedef struct {
    uint32_t set;
    uint32_t clear;
} nx_stm32u575_secure_gpio_command_t;
typedef enum {
    NX_STM32U575_BATCH_IDLE = 0,
    NX_STM32U575_BATCH_RUNNING,
    NX_STM32U575_BATCH_COMPLETE,
    NX_STM32U575_BATCH_CANCELLED,
    NX_STM32U575_BATCH_FAILED
} nx_stm32u575_secure_batch_phase_t;
/** \brief Copied status, never a request/storage loan. */
typedef struct {
    uint32_t epoch;
    uint32_t completed;
    nx_result_t result;
    nx_stm32u575_secure_batch_phase_t phase;
} nx_stm32u575_secure_batch_status_t;
/**
 * \brief One fixed Secure slot; initialize to zero and retain for its lifetime.
 * \note Single Secure task executor only. The immutable GPIO face and context
 *       outlive this slot. No worker, clock, interrupt or buffer loan is added.
 *       UINT32_MAX exhausts admission; wrap never revalidates an old ticket.
 */
typedef struct {
    const nx_gpio_port_t* port;
    nx_stm32u575_secure_gpio_command_t commands[NX_STM32U575_SECURE_BATCH_MAX];
    nx_stm32u575_secure_batch_status_t status;
    uint32_t count;
    uint32_t allowed;
    bool executing;
} nx_stm32u575_secure_batch_t;
/**
 * \brief Copy and admit a complete trusted Secure command list, without I/O.
 * \note Gateway owns CMSE and NS memory checks before this trusted mechanism.
 *       Rejection leaves slot, ticket and GPIO unchanged. Only a consumed
 *       terminal job can be replaced. Masks must be disjoint and granted.
 */
nx_result_t nx_stm32u575_secure_batch_submit(
    nx_stm32u575_secure_batch_t* slot,
    const nx_stm32u575_secure_gpio_command_t* commands, size_t count,
    uint32_t* ticket);
/** \brief Execute at most one copied write, then latch terminal facts. */
nx_result_t nx_stm32u575_secure_batch_service(nx_stm32u575_secure_batch_t* slot,
                                              uint32_t ticket);
/** \brief Cancel future commands; prior observed writes remain effective. */
nx_result_t nx_stm32u575_secure_batch_cancel(nx_stm32u575_secure_batch_t* slot,
                                             uint32_t ticket);
/** \brief Copy matching status; no retained output and no implicit execution.
 */
nx_result_t
nx_stm32u575_secure_batch_poll(const nx_stm32u575_secure_batch_t* slot,
                               uint32_t ticket,
                               nx_stm32u575_secure_batch_status_t* status);
/** \brief Consume a terminal ticket; RUNNING rejects without revoking work. */
nx_result_t nx_stm32u575_secure_batch_consume(nx_stm32u575_secure_batch_t* slot,
                                              uint32_t ticket);
#define NX_STM32U575_BATCH_SUBMIT  1U
#define NX_STM32U575_BATCH_SERVICE 2U
#define NX_STM32U575_BATCH_POLL    3U
#define NX_STM32U575_BATCH_CANCEL  4U
#define NX_STM32U575_BATCH_CONSUME 5U
/** \brief Fixed scalar-only wire message, copied before any admission. */
typedef struct {
    uint32_t action;
    uint32_t count;
    uint32_t ticket;
    nx_stm32u575_secure_gpio_command_t commands[NX_STM32U575_SECURE_BATCH_MAX];
} nx_stm32u575_secure_batch_message_t;
/** \brief Four scalar wire words, independent of enum ABI. */
typedef struct {
    uint32_t ticket;
    uint32_t completed;
    uint32_t result;
    uint32_t phase;
} nx_stm32u575_secure_batch_reply_t;
/**
 * \brief Dispatch a trusted Secure message/reply copy with no retained input.
 * \note Gateway checks caller, complete NS spans and serialization, then copies
 *       the message before calling. Invalid action/count has no job effects.
 */
nx_result_t nx_stm32u575_secure_batch_dispatch(
    nx_stm32u575_secure_batch_t* slot,
    const nx_stm32u575_secure_batch_message_t* message,
    nx_stm32u575_secure_batch_reply_t* reply);
/** \brief Pure complete-span bounds/alias check; never replaces CMSE checks. */
bool nx_stm32u575_secure_batch_ns_spans(
    uintptr_t ram_base, size_t ram_bytes,
    const nx_stm32u575_secure_batch_message_t* message,
    const nx_stm32u575_secure_batch_reply_t* reply);
#ifdef __cplusplus
}
#endif
#endif
