/**
 * \file            freertos.h
 * \brief           Explicit FreeRTOS notification-slot and IRQ extensions
 * \author          Nexus Team
 * \note            Ordinary tasks, queues and notifications use public OS
 *                  headers. Extensions retain native receiver identity and
 *                  require privileged, current-security-domain callers.
 */
#ifndef NEXUS_OS_FREERTOS_H
#define NEXUS_OS_FREERTOS_H
#include "FreeRTOS.h"
#include "nexus/core/time.h"
#include "nexus/os/wait.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Fixed receiver and exclusive notification slot; no semaphore. */
typedef struct {
    TaskHandle_t receiver;
    UBaseType_t index;
    uint32_t sequence;
    uint32_t waiting;
} nx_freertos_direct_notify_t;

/**
 * \brief           Check whether the actual IRQ may call kernel ISR APIs
 * \return          True only for an admitted privileged external IRQ
 * \note            Validate actual bounds, masks and selected port policy.
 *                  Mainline also checks grouping and syscall ceiling.
 *                  Native, NMI, faults and kernel exceptions are rejected.
 */
bool nx_freertos_isr_allowed(void);
/**
 * \brief           Bind an exclusive slot of one live receiver task
 * \param[out]      notification: Zeroed caller storage
 * \param[in]       receiver: Task retained until all publishers quiesce
 * \param[in]       index: Exclusively owned kernel notification slot
 * \return          SUCCESS, INVALID, CONTEXT or UNSUPPORTED by OS profile
 * \note            Caller ensures slot exclusivity. No global slot registry,
 *                  allocation, slot discovery or cross-world synchronization.
 */
nx_result_t
nx_freertos_direct_notify_init(nx_freertos_direct_notify_t* notification,
                               TaskHandle_t receiver, UBaseType_t index);
/**
 * \brief           Get a direct notification wait port
 * \param[in]       notification: Live explicitly bound storage
 * \return          Port using nx_time_now_us or an invalid zero port
 * \note            Only receiver may wait. Task/validated IRQ publishers use
 *                  the exclusive slot; notification remains a predicate hint.
 */
nx_wait_port_t
nx_freertos_direct_notify_port(nx_freertos_direct_notify_t* notification);
/**
 * \brief           Unbind after all publishers stop and join
 * \param[in,out]   notification: Live storage with no future users
 * \return          SUCCESS, BUSY if waiting, or validation/context error
 * \note            Does not delete receiver or clear its kernel slot. Receiver
 *                  and storage must outlive every retained port and publisher.
 */
nx_result_t
nx_freertos_direct_notify_destroy(nx_freertos_direct_notify_t* notification);

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_FREERTOS_H */
