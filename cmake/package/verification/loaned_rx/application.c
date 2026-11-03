/**
 * \file            application.c
 * \brief           Public loaned RX lifecycle without concrete storage
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"

nx_result_t loaned_api_run(nx_lwip_adapter_t* adapter,
                           const nx_lwip_loaned_config_t* config) {
    nx_result_t result = nx_lwip_loaned_adapter_start(adapter, config);
    if (result == NX_SUCCESS) {
        result = nx_lwip_adapter_service(adapter, 1U);
        nx_result_t stopped = nx_lwip_adapter_stop(adapter);
        if (stopped != NX_SUCCESS) {
            return stopped;
        }
    }
    return result;
}
