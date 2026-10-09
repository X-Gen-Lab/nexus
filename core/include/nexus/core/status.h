/**
 * \file            status.h
 *
 * \brief           Small vendor-independent result vocabulary.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STATUS_H
#define NEXUS_STATUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Operation result; success at submission means admission
 *                  only.
 */
typedef enum {
    NX_SUCCESS = 0,
    NX_ERROR_INVALID,
    NX_ERROR_BUSY,
    NX_ERROR_STATE,
    NX_ERROR_TIMEOUT,
    NX_ERROR_CANCELLED,
    NX_ERROR_IO,
    NX_ERROR_UNSUPPORTED,
    NX_ERROR_OVERFLOW,
    NX_ERROR_EMPTY,
    NX_ERROR_PERMISSION,
    NX_ERROR_NACK,
    NX_ERROR_ARBITRATION,
    NX_ERROR_EXHAUSTED,
    NX_ERROR_CONTEXT
} nx_result_t;
#ifdef __cplusplus
}
#endif

#endif
