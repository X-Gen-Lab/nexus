/**
 * \file            baremetal.h
 * \brief           Bare-metal poll notification with caller-owned storage
 * \author          Nexus Team
 */
#ifndef NEXUS_OS_BAREMETAL_H
#define NEXUS_OS_BAREMETAL_H
#include "nexus/os/wait.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Explicit nonblocking bare-metal notification storage. */
typedef struct {
    uint32_t sequence;
    uint64_t (*now_us)(void*);
    void* clock_context;
} nx_baremetal_notify_t;

/**
 * \brief           Initialize storage before exposing it to any ISR
 * \param[out]      notification: Unused caller-owned storage
 * \param[in]       now_us: Monotonic microsecond source, continuing while
 * waiting \param[in]       clock_context: Clock source context \return
 * NX_SUCCESS or NX_ERROR_INVALID \note            Startup context; not a
 * task/queue emulation. Word publication uses the reviewed inline compiler/CPU
 * port.
 */
nx_result_t nx_baremetal_notify_init(nx_baremetal_notify_t* notification,
                                     uint64_t (*now_us)(void*),
                                     void* clock_context);
/**
 * \brief           Bind the explicit polling notification
 * \param[in]       notification: Initialized storage
 * \return          Port whose wait returns BUSY while unchanged before
 * deadline; NX_DEADLINE_NEVER disables expiry, never implies blocking \note
 * wake uses compiler RMW or, on M0/M0+, a bounded saved PRIMASK guard. That
 * guard requires one CPU and privileged Thread/configurable-IRQ callers;
 * NMI/HardFault, unprivileged, DMA and other security-domain access to its
 *                  metadata are unsupported. The
 *                  owner loop continues to service controller/drain after BUSY.
 *                  No WFI/WFE is inserted without a reviewed
 *                  architecture-specific atomic sleep contract.
 */
nx_wait_port_t nx_baremetal_notify_port(nx_baremetal_notify_t* notification);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_OS_BAREMETAL_H */
