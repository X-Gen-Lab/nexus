/**
 * \file            mcuboot_control.c
 * \brief           Fixed overwrite-only control through the public boot API.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/mcuboot_control.h"
#include "bootutil/bootutil_public.h"

nx_result_t nx_mcuboot_query(nx_mcuboot_action_t* action) {
    if (action == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_mcuboot_action_t observed;
    switch (boot_swap_type_multi(0)) {
        case BOOT_SWAP_TYPE_NONE:
            observed = NX_MCUBOOT_ACTION_NONE;
            break;
        case BOOT_SWAP_TYPE_TEST:
            observed = NX_MCUBOOT_ACTION_TEST;
            break;
        case BOOT_SWAP_TYPE_PERM:
            observed = NX_MCUBOOT_ACTION_PERMANENT;
            break;
        case BOOT_SWAP_TYPE_REVERT:
            observed = NX_MCUBOOT_ACTION_REVERT;
            break;
        case BOOT_SWAP_TYPE_FAIL:
            observed = NX_MCUBOOT_ACTION_FAILED;
            break;
        default:
            return NX_ERROR_IO;
    }
    *action = observed;
    return NX_SUCCESS;
}

nx_result_t nx_mcuboot_request(nx_mcuboot_request_t request) {
    if (request == NX_MCUBOOT_REQUEST_TEST) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (request != NX_MCUBOOT_REQUEST_PERMANENT) {
        return NX_ERROR_INVALID;
    }
    if (boot_set_pending_multi(0, 1) != 0) {
        return NX_ERROR_IO;
    }
    nx_mcuboot_action_t observed;
    nx_result_t result = nx_mcuboot_query(&observed);
    if (result != NX_SUCCESS) {
        return result;
    }
    return observed == NX_MCUBOOT_ACTION_PERMANENT ? NX_SUCCESS
                                                   : NX_ERROR_STATE;
}
