/**
 * \file            hal_error_handling.h
 * \brief           Unified Error Handling Macros for STM32F4 HAL Adapter
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-01-16
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \note            This header provides unified error checking macros for:
 *                  - Null pointer validation
 *                  - Parameter validation
 *                  - Initialization state checking
 *                  - Timeout handling
 *                  - ST HAL error code mapping
 *
 * \par             Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6
 */

#ifndef HAL_ERROR_HANDLING_H
#define HAL_ERROR_HANDLING_H

#include "hal/nx_status.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* Null Pointer Check Macros                                                  */
/*===========================================================================*/

/**
 * \brief           Check if pointer is NULL and return error if so
 * \param[in]       ptr: Pointer to check
 * \return          NX_ERR_NULL_PTR if ptr is NULL
 * \note            Validates: Requirements 10.1, 10.6
 */
#define NX_CHECK_NULL(ptr)                                                     \
    do {                                                                       \
        if ((ptr) == NULL) {                                                   \
            return NX_ERR_NULL_PTR;                                            \
        }                                                                      \
    } while (0)

/**
 * \brief           Check multiple pointers for NULL
 * \param[in]       ptr1: First pointer to check
 * \param[in]       ptr2: Second pointer to check
 * \return          NX_ERR_NULL_PTR if any pointer is NULL
 */
#define NX_CHECK_NULL2(ptr1, ptr2)                                             \
    do {                                                                       \
        if ((ptr1) == NULL || (ptr2) == NULL) {                                \
            return NX_ERR_NULL_PTR;                                            \
        }                                                                      \
    } while (0)

/**
 * \brief           Check three pointers for NULL
 * \param[in]       ptr1: First pointer to check
 * \param[in]       ptr2: Second pointer to check
 * \param[in]       ptr3: Third pointer to check
 * \return          NX_ERR_NULL_PTR if any pointer is NULL
 */
#define NX_CHECK_NULL3(ptr1, ptr2, ptr3)                                       \
    do {                                                                       \
        if ((ptr1) == NULL || (ptr2) == NULL || (ptr3) == NULL) {              \
            return NX_ERR_NULL_PTR;                                            \
        }                                                                      \
    } while (0)

/*===========================================================================*/
/* Parameter Validation Macros                                                */
/*===========================================================================*/

/**
 * \brief           Check if condition is true, return error if false
 * \param[in]       cond: Condition to check
 * \return          NX_ERR_INVALID_PARAM if condition is false
 * \note            Validates: Requirements 10.2
 */
#define NX_CHECK_PARAM(cond)                                                   \
    do {                                                                       \
        if (!(cond)) {                                                         \
            return NX_ERR_INVALID_PARAM;                                       \
        }                                                                      \
    } while (0)

/**
 * \brief           Check if value is within range [min, max]
 * \param[in]       val: Value to check
 * \param[in]       min: Minimum allowed value (inclusive)
 * \param[in]       max: Maximum allowed value (inclusive)
 * \return          NX_ERR_INVALID_PARAM if value is out of range
 */
#define NX_CHECK_RANGE(val, min, max)                                          \
    do {                                                                       \
        if ((val) < (min) || (val) > (max)) {                                  \
            return NX_ERR_INVALID_PARAM;                                       \
        }                                                                      \
    } while (0)

/**
 * \brief           Check if value is less than maximum
 * \param[in]       val: Value to check
 * \param[in]       max: Maximum allowed value (exclusive)
 * \return          NX_ERR_INVALID_PARAM if value >= max
 */
#define NX_CHECK_LESS_THAN(val, max)                                           \
    do {                                                                       \
        if ((val) >= (max)) {                                                  \
            return NX_ERR_INVALID_PARAM;                                       \
        }                                                                      \
    } while (0)

/**
 * \brief           Check if value is less than or equal to maximum
 * \param[in]       val: Value to check
 * \param[in]       max: Maximum allowed value (inclusive)
 * \return          NX_ERR_INVALID_PARAM if value > max
 */
#define NX_CHECK_LESS_EQUAL(val, max)                                          \
    do {                                                                       \
        if ((val) > (max)) {                                                   \
            return NX_ERR_INVALID_PARAM;                                       \
        }                                                                      \
    } while (0)

/*===========================================================================*/
/* Initialization State Check Macros                                          */
/*===========================================================================*/

/**
 * \brief           Check if peripheral is initialized
 * \param[in]       initialized: Boolean flag indicating initialization state
 * \return          NX_ERR_NOT_INIT if not initialized
 * \note            Validates: Requirements 10.3, 10.6
 */
#define NX_CHECK_INIT(initialized)                                             \
    do {                                                                       \
        if (!(initialized)) {                                                  \
            return NX_ERR_NOT_INIT;                                            \
        }                                                                      \
    } while (0)

/**
 * \brief           Check if peripheral is NOT already initialized
 * \param[in]       initialized: Boolean flag indicating initialization state
 * \return          NX_ERR_ALREADY_INIT if already initialized
 */
#define NX_CHECK_NOT_INIT(initialized)                                         \
    do {                                                                       \
        if (initialized) {                                                     \
            return NX_ERR_ALREADY_INIT;                                        \
        }                                                                      \
    } while (0)

/**
 * \brief           Check peripheral state
 * \param[in]       current_state: Current state of the peripheral
 * \param[in]       expected_state: Expected state for the operation
 * \return          NX_ERR_INVALID_STATE if states don't match
 */
#define NX_CHECK_STATE(current_state, expected_state)                          \
    do {                                                                       \
        if ((current_state) != (expected_state)) {                             \
            return NX_ERR_INVALID_STATE;                                       \
        }                                                                      \
    } while (0)

/*===========================================================================*/
/* Combined Validation Macros                                                 */
/*===========================================================================*/

/**
 * \brief           Validate instance and check initialization
 * \param[in]       instance: Instance index to validate
 * \param[in]       max_instance: Maximum valid instance value (exclusive)
 * \param[in]       initialized: Boolean flag indicating initialization state
 * \return          NX_ERR_INVALID_PARAM or NX_ERR_NOT_INIT
 */
#define NX_VALIDATE_INSTANCE_INIT(instance, max_instance, initialized)         \
    do {                                                                       \
        NX_CHECK_LESS_THAN(instance, max_instance);                            \
        NX_CHECK_INIT(initialized);                                            \
    } while (0)

/**
 * \brief           Validate port and pin parameters
 * \param[in]       port: GPIO port to validate
 * \param[in]       max_port: Maximum valid port value (exclusive)
 * \param[in]       pin: GPIO pin to validate
 * \param[in]       max_pin: Maximum valid pin value (inclusive)
 * \return          NX_ERR_INVALID_PARAM if invalid
 */
#define NX_VALIDATE_PORT_PIN(port, max_port, pin, max_pin)                     \
    do {                                                                       \
        NX_CHECK_LESS_THAN(port, max_port);                                    \
        NX_CHECK_LESS_EQUAL(pin, max_pin);                                     \
    } while (0)

/*===========================================================================*/
/* Timeout Handling                                                           */
/*===========================================================================*/

/**
 * \brief           Wait forever timeout value
 */
#define NX_WAIT_FOREVER 0xFFFFFFFFUL

/**
 * \brief           Timeout wait result
 */
typedef enum {
    NX_WAIT_OK = 0,      /**< Wait completed successfully */
    NX_WAIT_TIMEOUT = 1, /**< Wait timed out */
} nx_wait_result_t;

/**
 * \brief           Get current system tick (to be implemented by platform)
 * \return          Current tick count in milliseconds
 * \note            This is a weak function that should be overridden
 */
extern uint32_t nx_get_tick(void);

/**
 * \brief           Wait for a flag to be set with timeout
 * \param[in]       reg: Pointer to register to check
 * \param[in]       flag: Flag mask to wait for
 * \param[in]       timeout_ms: Timeout in milliseconds (NX_WAIT_FOREVER for
 * infinite) \return          NX_WAIT_OK if flag set, NX_WAIT_TIMEOUT if timeout
 * \note            Validates: Requirements 10.4
 */
static inline nx_wait_result_t
nx_wait_flag_set(volatile uint32_t* reg, uint32_t flag, uint32_t timeout_ms) {
    uint32_t start = nx_get_tick();

    while ((*reg & flag) == 0) {
        if (timeout_ms != NX_WAIT_FOREVER) {
            if ((nx_get_tick() - start) >= timeout_ms) {
                return NX_WAIT_TIMEOUT;
            }
        }
    }
    return NX_WAIT_OK;
}

/**
 * \brief           Wait for a flag to be cleared with timeout
 * \param[in]       reg: Pointer to register to check
 * \param[in]       flag: Flag mask to wait for clear
 * \param[in]       timeout_ms: Timeout in milliseconds (NX_WAIT_FOREVER for
 * infinite) \return          NX_WAIT_OK if flag cleared, NX_WAIT_TIMEOUT if
 * timeout
 */
static inline nx_wait_result_t nx_wait_flag_clear(volatile uint32_t* reg,
                                                  uint32_t flag,
                                                  uint32_t timeout_ms) {
    uint32_t start = nx_get_tick();

    while ((*reg & flag) != 0) {
        if (timeout_ms != NX_WAIT_FOREVER) {
            if ((nx_get_tick() - start) >= timeout_ms) {
                return NX_WAIT_TIMEOUT;
            }
        }
    }
    return NX_WAIT_OK;
}

/**
 * \brief           Check timeout result and return error if timed out
 * \param[in]       result: Wait result to check
 * \return          NX_ERR_TIMEOUT if result is NX_WAIT_TIMEOUT
 */
#define NX_CHECK_TIMEOUT(result)                                               \
    do {                                                                       \
        if ((result) == NX_WAIT_TIMEOUT) {                                     \
            return NX_ERR_TIMEOUT;                                             \
        }                                                                      \
    } while (0)

/*===========================================================================*/
/* ST HAL Error Code Mapping                                                  */
/*===========================================================================*/

/**
 * \brief           ST HAL status codes (for reference)
 * \note            These match the ST HAL library definitions
 */
#define ST_HAL_OK      0x00U
#define ST_HAL_ERROR   0x01U
#define ST_HAL_BUSY    0x02U
#define ST_HAL_TIMEOUT 0x03U

/**
 * \brief           Map ST HAL status to Nexus HAL status
 * \param[in]       st_status: ST HAL status code
 * \return          Corresponding Nexus HAL status code
 * \note            Validates: Requirements 10.5
 */
static inline nx_status_t nx_map_st_status(uint32_t st_status) {
    switch (st_status) {
        case ST_HAL_OK:
            return NX_OK;
        case ST_HAL_ERROR:
            return NX_ERR_GENERIC;
        case ST_HAL_BUSY:
            return NX_ERR_BUSY;
        case ST_HAL_TIMEOUT:
            return NX_ERR_TIMEOUT;
        default:
            return NX_ERR_GENERIC;
    }
}

/**
 * \brief           Check ST HAL status and return mapped error if not OK
 * \param[in]       st_status: ST HAL status code to check
 * \return          Mapped Nexus HAL error code if not HAL_OK
 */
#define NX_CHECK_ST_STATUS(st_status)                                          \
    do {                                                                       \
        uint32_t __st_status = (st_status);                                    \
        if (__st_status != ST_HAL_OK) {                                        \
            return nx_map_st_status(__st_status);                              \
        }                                                                      \
    } while (0)

/*===========================================================================*/
/* UART Error Code Mapping                                                    */
/*===========================================================================*/

/**
 * \brief           ST HAL UART error codes (for reference)
 */
#define ST_HAL_UART_ERROR_NONE 0x00000000U
#define ST_HAL_UART_ERROR_PE   0x00000001U /* Parity error */
#define ST_HAL_UART_ERROR_NE   0x00000002U /* Noise error */
#define ST_HAL_UART_ERROR_FE   0x00000004U /* Frame error */
#define ST_HAL_UART_ERROR_ORE  0x00000008U /* Overrun error */
#define ST_HAL_UART_ERROR_DMA  0x00000010U /* DMA error */

/**
 * \brief           Map ST HAL UART error to Nexus HAL status
 * \param[in]       uart_error: ST HAL UART error code
 * \return          Corresponding Nexus HAL status code
 */
static inline nx_status_t nx_map_uart_error(uint32_t uart_error) {
    if (uart_error == ST_HAL_UART_ERROR_NONE) {
        return NX_OK;
    }
    if (uart_error & ST_HAL_UART_ERROR_PE) {
        return NX_ERR_PARITY;
    }
    if (uart_error & ST_HAL_UART_ERROR_FE) {
        return NX_ERR_FRAMING;
    }
    if (uart_error & ST_HAL_UART_ERROR_ORE) {
        return NX_ERR_OVERRUN;
    }
    if (uart_error & ST_HAL_UART_ERROR_NE) {
        return NX_ERR_NOISE;
    }
    if (uart_error & ST_HAL_UART_ERROR_DMA) {
        return NX_ERR_DMA;
    }
    return NX_ERR_IO;
}

/*===========================================================================*/
/* I2C Error Code Mapping                                                     */
/*===========================================================================*/

/**
 * \brief           ST HAL I2C error codes (for reference)
 */
#define ST_HAL_I2C_ERROR_NONE    0x00000000U
#define ST_HAL_I2C_ERROR_BERR    0x00000001U /* Bus error */
#define ST_HAL_I2C_ERROR_ARLO    0x00000002U /* Arbitration lost */
#define ST_HAL_I2C_ERROR_AF      0x00000004U /* Acknowledge failure (NACK) */
#define ST_HAL_I2C_ERROR_OVR     0x00000008U /* Overrun/Underrun */
#define ST_HAL_I2C_ERROR_DMA     0x00000010U /* DMA error */
#define ST_HAL_I2C_ERROR_TIMEOUT 0x00000020U /* Timeout */

/**
 * \brief           Map ST HAL I2C error to Nexus HAL status
 * \param[in]       i2c_error: ST HAL I2C error code
 * \return          Corresponding Nexus HAL status code
 */
static inline nx_status_t nx_map_i2c_error(uint32_t i2c_error) {
    if (i2c_error == ST_HAL_I2C_ERROR_NONE) {
        return NX_OK;
    }
    if (i2c_error & ST_HAL_I2C_ERROR_TIMEOUT) {
        return NX_ERR_TIMEOUT;
    }
    if (i2c_error & ST_HAL_I2C_ERROR_AF) {
        return NX_ERR_NACK;
    }
    if (i2c_error & ST_HAL_I2C_ERROR_BERR) {
        return NX_ERR_BUS;
    }
    if (i2c_error & ST_HAL_I2C_ERROR_ARLO) {
        return NX_ERR_ARBITRATION;
    }
    if (i2c_error & ST_HAL_I2C_ERROR_OVR) {
        return NX_ERR_OVERRUN;
    }
    if (i2c_error & ST_HAL_I2C_ERROR_DMA) {
        return NX_ERR_DMA;
    }
    return NX_ERR_IO;
}

/*===========================================================================*/
/* SPI Error Code Mapping                                                     */
/*===========================================================================*/

/**
 * \brief           ST HAL SPI error codes (for reference)
 */
#define ST_HAL_SPI_ERROR_NONE  0x00000000U
#define ST_HAL_SPI_ERROR_MODF  0x00000001U /* Mode fault */
#define ST_HAL_SPI_ERROR_CRC   0x00000002U /* CRC error */
#define ST_HAL_SPI_ERROR_OVR   0x00000004U /* Overrun error */
#define ST_HAL_SPI_ERROR_FRE   0x00000008U /* Frame format error */
#define ST_HAL_SPI_ERROR_DMA   0x00000010U /* DMA error */
#define ST_HAL_SPI_ERROR_FLAG  0x00000020U /* Flag error */
#define ST_HAL_SPI_ERROR_ABORT 0x00000040U /* Abort error */

/**
 * \brief           Map ST HAL SPI error to Nexus HAL status
 * \param[in]       spi_error: ST HAL SPI error code
 * \return          Corresponding Nexus HAL status code
 */
static inline nx_status_t nx_map_spi_error(uint32_t spi_error) {
    if (spi_error == ST_HAL_SPI_ERROR_NONE) {
        return NX_OK;
    }
    if (spi_error & ST_HAL_SPI_ERROR_OVR) {
        return NX_ERR_OVERRUN;
    }
    if (spi_error & ST_HAL_SPI_ERROR_FRE) {
        return NX_ERR_FRAMING;
    }
    if (spi_error & ST_HAL_SPI_ERROR_CRC) {
        return NX_ERR_CRC;
    }
    if (spi_error & ST_HAL_SPI_ERROR_DMA) {
        return NX_ERR_DMA;
    }
    return NX_ERR_IO;
}

/*===========================================================================*/
/* ADC Error Code Mapping                                                     */
/*===========================================================================*/

/**
 * \brief           ST HAL ADC error codes (for reference)
 */
#define ST_HAL_ADC_ERROR_NONE     0x00U
#define ST_HAL_ADC_ERROR_INTERNAL 0x01U
#define ST_HAL_ADC_ERROR_OVR      0x02U /* Overrun error */
#define ST_HAL_ADC_ERROR_DMA      0x04U /* DMA error */

/**
 * \brief           Map ST HAL ADC error to Nexus HAL status
 * \param[in]       adc_error: ST HAL ADC error code
 * \return          Corresponding Nexus HAL status code
 */
static inline nx_status_t nx_map_adc_error(uint32_t adc_error) {
    if (adc_error == ST_HAL_ADC_ERROR_NONE) {
        return NX_OK;
    }
    if (adc_error & ST_HAL_ADC_ERROR_OVR) {
        return NX_ERR_OVERRUN;
    }
    if (adc_error & ST_HAL_ADC_ERROR_DMA) {
        return NX_ERR_DMA;
    }
    return NX_ERR_IO;
}

#ifdef __cplusplus
}
#endif

#endif /* HAL_ERROR_HANDLING_H */
