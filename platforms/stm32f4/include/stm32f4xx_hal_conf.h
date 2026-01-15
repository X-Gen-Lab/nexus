/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           STM32F4xx HAL Configuration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */

#ifndef __STM32F4xx_HAL_CONF_H
#define __STM32F4xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Module Selection ############################## */
/**
 * @brief This is the list of modules to be used in the HAL driver
 */
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_SPI_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED

/* ########################## HSE/HSI Values adaptation ##################### */
/**
 * @brief Adjust the value of External High Speed oscillator (HSE) used in your
 * application. This value is used by the RCC HAL module to compute the system
 * frequency (when HSE is used as system clock source, directly or through the
 * PLL).
 */
#if !defined(HSE_VALUE)
#define HSE_VALUE 8000000U /*!< Value of the External oscillator in Hz */
#endif                     /* HSE_VALUE */

#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT 100U /*!< Time out for HSE start up, in ms */
#endif                           /* HSE_STARTUP_TIMEOUT */

/**
 * @brief Internal High Speed oscillator (HSI) value.
 *        This value is used by the RCC HAL module to compute the system
 * frequency (when HSI is used as system clock source, directly or through the
 * PLL).
 */
#if !defined(HSI_VALUE)
#define HSI_VALUE 16000000U /*!< Value of the Internal oscillator in Hz */
#endif                      /* HSI_VALUE */

/**
 * @brief Internal Low Speed oscillator (LSI) value.
 */
#if !defined(LSI_VALUE)
#define LSI_VALUE 32000U /*!< LSI Typical Value in Hz */
#endif                   /* LSI_VALUE */

/**
 * @brief External Low Speed oscillator (LSE) value.
 */
#if !defined(LSE_VALUE)
#define LSE_VALUE                                                              \
    32768U /*!< Value of the External Low Speed oscillator in Hz               \
            */
#endif     /* LSE_VALUE */

#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT 5000U /*!< Time out for LSE start up, in ms */
#endif                            /* LSE_STARTUP_TIMEOUT */

/**
 * @brief External clock source for I2S peripheral
 */
#if !defined(EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE                                                   \
    12288000U /*!< Value of the External oscillator in Hz*/
#endif        /* EXTERNAL_CLOCK_VALUE */

/* ########################### System Configuration ######################### */
/**
 * @brief This is the HAL system configuration section
 */
#define VDD_VALUE                3300U /*!< Value of VDD in mv */
#define TICK_INT_PRIORITY        0x0FU /*!< tick interrupt priority */
#define USE_RTOS                 0U
#define PREFETCH_ENABLE          1U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE        1U

/* ########################## Assert Selection ############################## */
/* #define USE_FULL_ASSERT    1U */

/* Includes ------------------------------------------------------------------*/
/**
 * @brief Include module's header file
 */

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f4xx_hal_rcc.h"
#endif /* HAL_RCC_MODULE_ENABLED */

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f4xx_hal_gpio.h"
#endif /* HAL_GPIO_MODULE_ENABLED */

#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32f4xx_hal_exti.h"
#endif /* HAL_EXTI_MODULE_ENABLED */

#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32f4xx_hal_dma.h"
#endif /* HAL_DMA_MODULE_ENABLED */

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f4xx_hal_cortex.h"
#endif /* HAL_CORTEX_MODULE_ENABLED */

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f4xx_hal_flash.h"
#endif /* HAL_FLASH_MODULE_ENABLED */

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32f4xx_hal_pwr.h"
#endif /* HAL_PWR_MODULE_ENABLED */

#ifdef HAL_UART_MODULE_ENABLED
#include "stm32f4xx_hal_uart.h"
#endif /* HAL_UART_MODULE_ENABLED */

#ifdef HAL_SPI_MODULE_ENABLED
#include "stm32f4xx_hal_spi.h"
#endif /* HAL_SPI_MODULE_ENABLED */

#ifdef HAL_I2C_MODULE_ENABLED
#include "stm32f4xx_hal_i2c.h"
#endif /* HAL_I2C_MODULE_ENABLED */

#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32f4xx_hal_tim.h"
#endif /* HAL_TIM_MODULE_ENABLED */

#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32f4xx_hal_adc.h"
#endif /* HAL_ADC_MODULE_ENABLED */

/* Exported macro ------------------------------------------------------------*/
#ifdef USE_FULL_ASSERT
#define assert_param(expr)                                                     \
    ((expr) ? (void)0U : assert_failed((uint8_t*)__FILE__, __LINE__))
void assert_failed(uint8_t* file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif /* USE_FULL_ASSERT */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_HAL_CONF_H */
