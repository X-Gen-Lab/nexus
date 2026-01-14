/**
 * \file            hal_def.h
 * \brief           HAL Common Definitions
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef HAL_DEF_H
#define HAL_DEF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup        HAL_DEF HAL Definitions
 * \brief           Common definitions for HAL layer
 * \{
 */

/**
 * \brief           HAL status codes
 */
typedef enum {
    /* Success */
    HAL_OK = 0, /**< Operation successful */

    /* General errors (1-99) */
    HAL_ERROR = 1,               /**< Generic error */
    HAL_ERROR_INVALID_PARAM = 2, /**< Invalid parameter */
    HAL_ERROR_NULL_POINTER = 3,  /**< Null pointer */
    HAL_ERROR_NOT_INIT = 4,      /**< Not initialized */
    HAL_ERROR_ALREADY_INIT = 5,  /**< Already initialized */
    HAL_ERROR_NOT_SUPPORTED = 6, /**< Not supported */
    HAL_ERROR_INVALID_STATE = 7, /**< Invalid state */

    /* Resource errors (100-199) */
    HAL_ERROR_NO_MEMORY = 100,   /**< Out of memory */
    HAL_ERROR_NO_RESOURCE = 101, /**< No resource available */
    HAL_ERROR_BUSY = 102,        /**< Resource busy */
    HAL_ERROR_LOCKED = 103,      /**< Resource locked */

    /* Timeout errors (200-299) */
    HAL_ERROR_TIMEOUT = 200, /**< Operation timeout */

    /* IO errors (300-399) */
    HAL_ERROR_IO = 300,       /**< IO error */
    HAL_ERROR_OVERRUN = 301,  /**< Buffer overrun */
    HAL_ERROR_UNDERRUN = 302, /**< Buffer underrun */
    HAL_ERROR_PARITY = 303,   /**< Parity error */
    HAL_ERROR_FRAMING = 304,  /**< Framing error */
    HAL_ERROR_NOISE = 305,    /**< Noise error */

    /* Aliases for compatibility */
    HAL_ERR_PARAM = HAL_ERROR_INVALID_PARAM,
    HAL_ERR_STATE = HAL_ERROR_INVALID_STATE,
    HAL_ERR_NOT_SUPPORTED = HAL_ERROR_NOT_SUPPORTED,
    HAL_ERR_FAIL = HAL_ERROR,

} hal_status_t;

/**
 * \brief           Wait forever timeout value
 */
#define HAL_WAIT_FOREVER 0xFFFFFFFFUL

/**
 * \brief           Check if status is OK
 * \param[in]       status: Status to check
 * \return          true if OK, false otherwise
 */
#define HAL_IS_OK(status) ((status) == HAL_OK)

/**
 * \brief           Check if status is error
 * \param[in]       status: Status to check
 * \return          true if error, false otherwise
 */
#define HAL_IS_ERROR(status) ((status) != HAL_OK)

/**
 * \brief           Return if status is error
 * \param[in]       status: Status to check
 */
#define HAL_RETURN_IF_ERROR(status)                                            \
    do {                                                                       \
        hal_status_t __status = (status);                                      \
        if (HAL_IS_ERROR(__status)) {                                          \
            return __status;                                                   \
        }                                                                      \
    } while (0)

/**
 * \brief           Unused parameter macro
 * \param[in]       x: Parameter to mark as unused
 */
#define HAL_UNUSED(x) ((void)(x))

/**
 * \brief           Array size macro
 * \param[in]       arr: Array
 * \return          Number of elements in array
 */
#define HAL_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * \brief           Minimum of two values
 */
#define HAL_MIN(a, b) (((a) < (b)) ? (a) : (b))

/**
 * \brief           Maximum of two values
 */
#define HAL_MAX(a, b) (((a) > (b)) ? (a) : (b))

/**
 * \brief           Clamp value between min and max
 */
#define HAL_CLAMP(val, min, max) (HAL_MIN(HAL_MAX((val), (min)), (max)))

/**
 * \brief           Bit manipulation macros
 * \{
 */
#define HAL_BIT(n)               (1UL << (n))
#define HAL_BIT_SET(reg, bit)    ((reg) |= (bit))
#define HAL_BIT_CLEAR(reg, bit)  ((reg) &= ~(bit))
#define HAL_BIT_TOGGLE(reg, bit) ((reg) ^= (bit))
#define HAL_BIT_READ(reg, bit)   (((reg) & (bit)) != 0)
/** \} */

/**
 * \}
 */

#ifdef __cplusplus
}
#endif

#endif /* HAL_DEF_H */
