/**
 * \file            FreeRTOSConfig.h
 * \brief           Test-only eight-bit Mainline IRQ priority contract
 * \author          Nexus Team
 */
#ifndef NEXUS_FREERTOS_PRIORITY8_CONFIG_H
#define NEXUS_FREERTOS_PRIORITY8_CONFIG_H

#include "../../contracts/os_freertos_runtime/FreeRTOSConfig.h"

#undef configPRIO_BITS
#undef configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
#define configPRIO_BITS                              8u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 10u

#endif /* NEXUS_FREERTOS_PRIORITY8_CONFIG_H */
