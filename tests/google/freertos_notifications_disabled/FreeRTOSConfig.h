/**
 * \file            FreeRTOSConfig.h
 * \brief           Test-only notification-disabled capability profile
 * \author          Nexus Team
 */
#ifndef NEXUS_FREERTOS_NOTIFICATIONS_DISABLED_CONFIG_H
#define NEXUS_FREERTOS_NOTIFICATIONS_DISABLED_CONFIG_H
#include "../../contracts/os_freertos_runtime/FreeRTOSConfig.h"
#undef configUSE_TASK_NOTIFICATIONS
#define configUSE_TASK_NOTIFICATIONS 0
#endif /* NEXUS_FREERTOS_NOTIFICATIONS_DISABLED_CONFIG_H */
