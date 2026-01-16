/**
 * \file            nx_firmware_info.c
 * \brief           Nexus firmware information implementation
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file implements firmware information retrieval
 *                  functions. The actual firmware info structure is
 *                  defined by the application using NX_FIRMWARE_INFO_DEFINE.
 */

#include "nx_firmware_info.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* External References                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Weak reference to firmware info structure
 *
 * This symbol is defined by the application using NX_FIRMWARE_INFO_DEFINE.
 * If not defined, the weak symbol resolves to NULL.
 */
#if defined(_MSC_VER)
/* MSVC doesn't support weak symbols, provide a test helper */
static const nx_firmware_info_t* _nx_firmware_info_ptr = NULL;

/**
 * \brief           Test helper to get firmware info pointer (MSVC only)
 */
const nx_firmware_info_t* _nx_get_firmware_info_test(void) {
    return _nx_firmware_info_ptr;
}

/**
 * \brief           Test helper to set firmware info pointer (MSVC only)
 */
void _nx_set_firmware_info_test(const nx_firmware_info_t* info) {
    _nx_firmware_info_ptr = info;
}
#else
NX_WEAK const nx_firmware_info_t nx_firmware_info;
#endif

/*---------------------------------------------------------------------------*/
/* Public API Implementation                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get firmware information pointer
 */
const nx_firmware_info_t* nx_get_firmware_info(void) {
#if defined(_MSC_VER)
    /* For MSVC testing, return the pointer if defined */
    extern const nx_firmware_info_t* _nx_get_firmware_info_test(void);
    return _nx_get_firmware_info_test();
#else
    /* Check if firmware info was defined (weak symbol check) */
    /* If the weak symbol is not overridden, it will be zero/null */
    if (&nx_firmware_info != NULL && nx_firmware_info.product[0] != '\0') {
        return &nx_firmware_info;
    }
    return NULL;
#endif
}

/**
 * \brief           Get firmware version as formatted string
 */
size_t nx_get_version_string(char* buf, size_t size) {
    const nx_firmware_info_t* info;
    uint8_t major, minor, patch, build;
    size_t written = 0;
    char temp[16];
    size_t temp_len;
    size_t i;

    if (buf == NULL || size == 0) {
        return 0;
    }

    info = nx_get_firmware_info();
    if (info == NULL) {
        return 0;
    }

    /* Extract version components */
    major = NX_VERSION_MAJOR(info->version);
    minor = NX_VERSION_MINOR(info->version);
    patch = NX_VERSION_PATCH(info->version);
    build = NX_VERSION_BUILD(info->version);

    /* Format version string manually (avoid snprintf for embedded) */
    temp_len = 0;

    /* Major version */
    if (major >= 100) {
        temp[temp_len++] = '0' + (major / 100);
        major %= 100;
        temp[temp_len++] = '0' + (major / 10);
        temp[temp_len++] = '0' + (major % 10);
    } else if (major >= 10) {
        temp[temp_len++] = '0' + (major / 10);
        temp[temp_len++] = '0' + (major % 10);
    } else {
        temp[temp_len++] = '0' + major;
    }
    temp[temp_len++] = '.';

    /* Minor version */
    if (minor >= 100) {
        temp[temp_len++] = '0' + (minor / 100);
        minor %= 100;
        temp[temp_len++] = '0' + (minor / 10);
        temp[temp_len++] = '0' + (minor % 10);
    } else if (minor >= 10) {
        temp[temp_len++] = '0' + (minor / 10);
        temp[temp_len++] = '0' + (minor % 10);
    } else {
        temp[temp_len++] = '0' + minor;
    }
    temp[temp_len++] = '.';

    /* Patch version */
    if (patch >= 100) {
        temp[temp_len++] = '0' + (patch / 100);
        patch %= 100;
        temp[temp_len++] = '0' + (patch / 10);
        temp[temp_len++] = '0' + (patch % 10);
    } else if (patch >= 10) {
        temp[temp_len++] = '0' + (patch / 10);
        temp[temp_len++] = '0' + (patch % 10);
    } else {
        temp[temp_len++] = '0' + patch;
    }
    temp[temp_len++] = '.';

    /* Build number */
    if (build >= 100) {
        temp[temp_len++] = '0' + (build / 100);
        build %= 100;
        temp[temp_len++] = '0' + (build / 10);
        temp[temp_len++] = '0' + (build % 10);
    } else if (build >= 10) {
        temp[temp_len++] = '0' + (build / 10);
        temp[temp_len++] = '0' + (build % 10);
    } else {
        temp[temp_len++] = '0' + build;
    }

    /* Copy to output buffer */
    for (i = 0; i < temp_len && i < size - 1; i++) {
        buf[i] = temp[i];
        written++;
    }
    buf[written] = '\0';

    return written;
}
