/**
 * \file            notify.h
 * \brief           Portable single-waiter latched notifications
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_NOTIFY_H
#define NEXUS_OS_NOTIFY_H
#include "nexus/os/wait.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Caller-owned notification with one waiter and multiple publishers. */
typedef struct nx_notify nx_notify_t;
/**
 * \brief           Initialize unused zeroed notification storage
 * \param[in,out]   notification: Object retained through all port users
 * \return          SUCCESS or admission/initialization error; BUSY can retain
 *                  resources that require destroy before retry
 * \note            Task/bootstrap only; one external lifecycle owner.
 */
nx_result_t nx_notify_init(nx_notify_t* notification);
/**
 * \brief           Bind the existing sequence-hint wait protocol
 * \param[in,out]   notification: Live object, retained through all publishers
 * \return          Wait port; an empty port for invalid/non-live storage
 * \note            arm/wait have one owner. port.wake accepts only the
 *                  backend's admitted task/bootstrap or hardware IRQ domain.
 *                  Read authoritative predicates after hints. A timeout never
 *                  releases request buffers. Fewer than 2^32 wakes may occur
 *                  between arm and recheck; see wait.h for the full contract.
 */
nx_wait_port_t nx_notify_port(nx_notify_t* notification);
/**
 * \brief           Publish a task/bootstrap notification hint
 * \param[in,out]   notification: Live object with stable storage
 * \return          SUCCESS or an admission/kernel error
 * \note            Admission rejects before publication. A kernel error may
 *                  follow a published hint and cannot revoke it. No transfer
 *                  of business data ownership or completion proof is implied.
 */
nx_result_t nx_notify_wake(nx_notify_t* notification);
/**
 * \brief           Publish from a validated configurable hardware IRQ
 * \param[in,out]   notification: Live object retained until IRQ quiescence
 * \return          SUCCESS or INVALID/CONTEXT/error; Native returns CONTEXT
 * \note            No blocking. Validate masks, actual IRQ bounds and kernel
 *                  syscall priority before mutation. NMI/faults are
 * unsupported.
 */
nx_result_t nx_notify_wake_from_isr(nx_notify_t* notification);
/**
 * \brief           Destroy only after every publisher and waiter has exited
 * \param[in,out]   notification: Live object with no future users
 * \return          SUCCESS or BUSY/error retaining outstanding resources
 * \note            Task lifecycle owner only. Busy checks cannot prevent a
 *                  future caller racing destruction. After partial failure,
 *                  retain storage and retry destroy, never reinitialize it.
 */
nx_result_t nx_notify_destroy(nx_notify_t* notification);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_OS_NOTIFY_H */
