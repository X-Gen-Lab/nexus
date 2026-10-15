/**
 * \file            tx_user.h
 * \brief           Explicit static single-core ThreadX adapter requirements
 * \author          Nexus Team
 */
#ifndef NEXUS_THREADX_USER_H
#define NEXUS_THREADX_USER_H
#ifndef TX_TIMER_TICKS_PER_SECOND
#define TX_TIMER_TICKS_PER_SECOND 1000U
#endif
#define TX_TIMER_PROCESS_IN_ISR

#if defined(__linux__) && !defined(__arm__) && !defined(__thumb__)
/* The Linux interrupt simulator changes these values from another pthread.
 * Port observers share its recursive mutex after the single bootstrap has
 * initialized it. Primitive ThreadX types are not available at this include. */
#ifdef __cplusplus
extern "C" {
#endif
struct TX_THREAD_STRUCT;
unsigned int nx_threadx_linux_system_state(void);
unsigned int nx_threadx_linux_return_flags(void);
struct TX_THREAD_STRUCT* nx_threadx_linux_current(void);
#ifdef __cplusplus
}
#endif
#define TX_THREAD_GET_SYSTEM_STATE() ((ULONG)nx_threadx_linux_system_state())
#define TX_THREAD_GET_CURRENT(destination)                                     \
    (destination) = nx_threadx_linux_current();
#define TX_THREAD_SYSTEM_RETURN_CHECK(destination)                             \
    (destination) = (ULONG)nx_threadx_linux_return_flags();
#endif
#endif /* NEXUS_THREADX_USER_H */
