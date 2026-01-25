/**
 * \file            osal_internal.h
 * \brief           OSAL Internal Definitions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         This file contains internal definitions for OSAL
 *                  implementation. It should only be included by OSAL
 *                  adapter implementations, not by application code.
 */

#ifndef OSAL_INTERNAL_H
#define OSAL_INTERNAL_H

#include "osal_config.h"
#include "osal_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Resource Type Identifiers                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Resource type identifiers for handle validation
 */
typedef enum {
    OSAL_TYPE_INVALID = 0x0000, /**< Invalid/freed resource */
    OSAL_TYPE_TASK = 0x0001,    /**< Task resource */
    OSAL_TYPE_MUTEX = 0x0002,   /**< Mutex resource */
    OSAL_TYPE_SEM = 0x0003,     /**< Semaphore resource */
    OSAL_TYPE_QUEUE = 0x0004,   /**< Queue resource */
    OSAL_TYPE_EVENT = 0x0005,   /**< Event flags resource */
    OSAL_TYPE_TIMER = 0x0006    /**< Timer resource */
} osal_resource_type_t;

/*---------------------------------------------------------------------------*/
/* Handle Validation Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Internal handle header for validation
 * \details         This structure must be placed at the beginning of all
 *                  OSAL resource internal structures to enable handle
 *                  validation.
 */
typedef struct {
    uint32_t magic; /**< Magic number for validation */
    uint16_t type;  /**< Resource type identifier */
    uint16_t flags; /**< Status flags */
} osal_handle_header_t;

/**
 * \brief           Handle flag definitions
 */
#define OSAL_HANDLE_FLAG_ACTIVE 0x0001 /**< Resource is active */
#define OSAL_HANDLE_FLAG_STATIC 0x0002 /**< Statically allocated */

/*---------------------------------------------------------------------------*/
/* Handle Validation Macros                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize handle header
 * \param[in]       hdr: Pointer to handle header
 * \param[in]       res_type: Resource type
 */
#define OSAL_HANDLE_INIT(hdr, res_type)                                        \
    do {                                                                       \
        (hdr)->magic = OSAL_HANDLE_MAGIC;                                      \
        (hdr)->type = (uint16_t)(res_type);                                    \
        (hdr)->flags = OSAL_HANDLE_FLAG_ACTIVE;                                \
    } while (0)

/**
 * \brief           Invalidate handle header (on deletion)
 * \param[in]       hdr: Pointer to handle header
 */
#define OSAL_HANDLE_DEINIT(hdr)                                                \
    do {                                                                       \
        (hdr)->magic = OSAL_HANDLE_INVALID;                                    \
        (hdr)->type = (uint16_t)OSAL_TYPE_INVALID;                             \
        (hdr)->flags = 0;                                                      \
    } while (0)

/**
 * \brief           Check if handle header is valid
 * \param[in]       hdr: Pointer to handle header
 * \param[in]       res_type: Expected resource type
 * \return          true if valid, false otherwise
 */
#define OSAL_HANDLE_IS_VALID(hdr, res_type)                                    \
    ((hdr) != NULL && (hdr)->magic == OSAL_HANDLE_MAGIC &&                     \
     (hdr)->type == (uint16_t)(res_type) &&                                    \
     ((hdr)->flags & OSAL_HANDLE_FLAG_ACTIVE) != 0)

/*---------------------------------------------------------------------------*/
/* Extended Error Handling Macros (Internal Use)                             */
/*---------------------------------------------------------------------------*/

/*
 * Note: Basic error handling macros (OSAL_VALIDATE_PTR, OSAL_VALIDATE_PARAM,
 * OSAL_CHECK_NOT_ISR) are defined in osal_def.h for public use.
 * This section provides extended macros for internal adapter use.
 */

/**
 * \brief           Validate handle with magic number
 * \param[in]       handle: Handle to validate
 * \param[in]       res_type: Expected resource type
 * \return          Returns appropriate error if handle is invalid
 *
 * \details         In debug builds (OSAL_HANDLE_VALIDATION enabled),
 *                  performs full validation including magic number and
 *                  type check. In release builds, only performs NULL
 *                  check for performance.
 *
 * \note            Requirements: 3.1, 3.2, 3.3
 */
#if OSAL_HANDLE_VALIDATION
#define OSAL_VALIDATE_HANDLE(handle, res_type)                                 \
    do {                                                                       \
        if ((handle) == NULL) {                                                \
            return OSAL_ERROR_NULL_POINTER;                                    \
        }                                                                      \
        osal_handle_header_t* _hdr = (osal_handle_header_t*)(handle);          \
        if (_hdr->magic != OSAL_HANDLE_MAGIC) {                                \
            return OSAL_ERROR_INVALID_PARAM;                                   \
        }                                                                      \
        if (_hdr->type != (uint16_t)(res_type)) {                              \
            return OSAL_ERROR_INVALID_PARAM;                                   \
        }                                                                      \
        if ((_hdr->flags & OSAL_HANDLE_FLAG_ACTIVE) == 0) {                    \
            return OSAL_ERROR_INVALID_PARAM;                                   \
        }                                                                      \
    } while (0)
#else
#define OSAL_VALIDATE_HANDLE(handle, res_type) OSAL_VALIDATE_PTR(handle)
#endif

/**
 * \brief           Return value if condition is false
 * \param[in]       cond: Condition to check
 * \param[in]       ret: Return value if condition is false
 */
#define OSAL_RETURN_IF_FALSE(cond, ret)                                        \
    do {                                                                       \
        if (!(cond)) {                                                         \
            return (ret);                                                      \
        }                                                                      \
    } while (0)

/**
 * \brief           Return if status is error
 * \param[in]       status: Status to check
 */
#define OSAL_RETURN_IF_ERROR(status)                                           \
    do {                                                                       \
        osal_status_t _status = (status);                                      \
        if (OSAL_IS_ERROR(_status)) {                                          \
            return _status;                                                    \
        }                                                                      \
    } while (0)

/*---------------------------------------------------------------------------*/
/* Statistics Tracking                                                       */
/*---------------------------------------------------------------------------*/

#if OSAL_STATS_ENABLE

/**
 * \brief           Resource statistics tracking structure
 */
typedef struct {
    volatile uint16_t count;     /**< Current count */
    volatile uint16_t watermark; /**< Peak count (high watermark) */
} osal_resource_stats_t;

/**
 * \brief           Increment resource count and update watermark
 * \param[in]       stats: Pointer to statistics structure
 */
#define OSAL_STATS_INC(stats)                                                  \
    do {                                                                       \
        (stats)->count++;                                                      \
        if ((stats)->count > (stats)->watermark) {                             \
            (stats)->watermark = (stats)->count;                               \
        }                                                                      \
    } while (0)

/**
 * \brief           Decrement resource count
 * \param[in]       stats: Pointer to statistics structure
 */
#define OSAL_STATS_DEC(stats)                                                  \
    do {                                                                       \
        if ((stats)->count > 0) {                                              \
            (stats)->count--;                                                  \
        }                                                                      \
    } while (0)

/**
 * \brief           Reset statistics
 * \param[in]       stats: Pointer to statistics structure
 */
#define OSAL_STATS_RESET(stats)                                                \
    do {                                                                       \
        (stats)->watermark = (stats)->count;                                   \
    } while (0)

#else /* !OSAL_STATS_ENABLE */

#define OSAL_STATS_INC(stats)   ((void)0)
#define OSAL_STATS_DEC(stats)   ((void)0)
#define OSAL_STATS_RESET(stats) ((void)0)

#endif /* OSAL_STATS_ENABLE */

/*---------------------------------------------------------------------------*/
/* Debug Utilities                                                           */
/*---------------------------------------------------------------------------*/

#if OSAL_DEBUG

/**
 * \brief           Assert macro for debug builds
 * \param[in]       cond: Condition to assert
 */
#define OSAL_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            osal_assert_failed(__FILE__, __LINE__);                            \
        }                                                                      \
    } while (0)

/**
 * \brief           Assert failure handler (to be implemented by adapter)
 * \param[in]       file: Source file name
 * \param[in]       line: Source line number
 */
void osal_assert_failed(const char* file, uint32_t line);

#else /* !OSAL_DEBUG */

#define OSAL_ASSERT(cond) ((void)0)

#endif /* OSAL_DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* OSAL_INTERNAL_H */
