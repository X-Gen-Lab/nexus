/**
 * \file            mcuboot_swap_control.c
 * \brief           Official scratch profile trial, observation and confirmation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/components/mcuboot_swap_control.h"
#include "bootutil/bootutil_public.h"

nx_result_t nx_mcuboot_swap_query(nx_mcuboot_action_t* action) {
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

nx_result_t nx_mcuboot_swap_request(nx_mcuboot_request_t request) {
    if (request != NX_MCUBOOT_REQUEST_TEST &&
        request != NX_MCUBOOT_REQUEST_PERMANENT) {
        return NX_ERROR_INVALID;
    }
    if (boot_set_pending_multi(0, request == NX_MCUBOOT_REQUEST_PERMANENT) !=
        0) {
        return NX_ERROR_IO;
    }
    nx_mcuboot_action_t action;
    const nx_result_t result = nx_mcuboot_swap_query(&action);
    if (result != NX_SUCCESS) {
        return result;
    }
    return action == (request == NX_MCUBOOT_REQUEST_TEST
                          ? NX_MCUBOOT_ACTION_TEST
                          : NX_MCUBOOT_ACTION_PERMANENT)
               ? NX_SUCCESS
               : NX_ERROR_STATE;
}

nx_result_t nx_mcuboot_swap_confirmed(bool* confirmed) {
    if (confirmed == NULL) {
        return NX_ERROR_INVALID;
    }
    const struct flash_area* primary = NULL;
    if (flash_area_open(NX_MCUBOOT_PRIMARY_AREA_ID, &primary) != 0) {
        return NX_ERROR_IO;
    }
    uint8_t value = BOOT_FLAG_BAD;
    const int result = boot_read_image_ok(primary, &value);
    flash_area_close(primary);
    if (result != 0) {
        return NX_ERROR_IO;
    }
    if (value != BOOT_FLAG_SET && value != BOOT_FLAG_UNSET) {
        return NX_ERROR_STATE;
    }
    *confirmed = value == BOOT_FLAG_SET;
    return NX_SUCCESS;
}

nx_result_t nx_mcuboot_swap_confirm(void) {
    if (boot_set_confirmed_multi(0) != 0) {
        return NX_ERROR_IO;
    }
    bool confirmed = false;
    const nx_result_t result = nx_mcuboot_swap_confirmed(&confirmed);
    return result != NX_SUCCESS ? result
                                : (confirmed ? NX_SUCCESS : NX_ERROR_STATE);
}
