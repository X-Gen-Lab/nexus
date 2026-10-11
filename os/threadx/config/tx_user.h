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
#endif /* NEXUS_THREADX_USER_H */
