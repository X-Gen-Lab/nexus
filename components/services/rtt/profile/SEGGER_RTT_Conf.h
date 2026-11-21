/**
 * \file            SEGGER_RTT_Conf.h
 * \brief           Private RTT profile with nonblocking exclusion
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_SEGGER_RTT_CONF_H
#define NEXUS_SEGGER_RTT_CONF_H
#include "nexus/arch/arch.h"
#define RTT__DMB()                      nx_arch_dmb()
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS   4
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 4
#define BUFFER_SIZE_UP                  16
#define BUFFER_SIZE_DOWN                16
#define RTT_USE_ASM                     0
/* All upstream target entry points are private and called under the adapter's
 * single try-guard. No BASEPRI overwrite or large masked copy is introduced. */
#define SEGGER_RTT_LOCK()
#define SEGGER_RTT_UNLOCK()
#endif /* NEXUS_SEGGER_RTT_CONF_H */
