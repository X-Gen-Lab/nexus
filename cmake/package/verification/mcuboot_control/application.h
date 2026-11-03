/**
 * \file            application.h
 * \brief           Generic MCUboot control caller with no supplier storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef SDK_MCUBOOT_APPLICATION_H
#define SDK_MCUBOOT_APPLICATION_H
#include "nexus/components/mcuboot_control.h"
#include <stdbool.h>
/** \brief Observe an empty model, reject TEST, optionally request PERMANENT.
 * \note Caller provides serialized finite Flash operation lifetime.
 */
nx_result_t control_api_run(bool request_permanent);
#endif
