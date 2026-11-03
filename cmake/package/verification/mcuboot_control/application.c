/**
 * \file            application.c
 * \brief           Public control lifecycle through official upstream trailer
 * functions \author          Nexus Team \version         1.0.0 \date 2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "application.h"

nx_result_t control_api_run(bool request_permanent) {
    nx_mcuboot_action_t action = NX_MCUBOOT_ACTION_FAILED;
    nx_result_t result = nx_mcuboot_query(&action);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (action != NX_MCUBOOT_ACTION_NONE ||
        nx_mcuboot_request(NX_MCUBOOT_REQUEST_TEST) != NX_ERROR_UNSUPPORTED) {
        return NX_ERROR_STATE;
    }
    if (!request_permanent) {
        return NX_SUCCESS;
    }
    result = nx_mcuboot_request(NX_MCUBOOT_REQUEST_PERMANENT);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_mcuboot_query(&action);
    return result == NX_SUCCESS && action == NX_MCUBOOT_ACTION_PERMANENT
               ? NX_SUCCESS
               : NX_ERROR_STATE;
}
