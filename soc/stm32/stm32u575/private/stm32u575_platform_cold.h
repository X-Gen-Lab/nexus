/**
 * \file            stm32u575_platform_cold.h
 * \brief           Static selected resources and one U575 platform transaction
 * \author          Nexus Team
 */
#ifndef NX_STM32U575_PLATFORM_COLD_H
#define NX_STM32U575_PLATFORM_COLD_H
#include "stm32u575_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Physical selections, never a runtime register instruction stream. */
#define NX_U575_SELECT_USART1 (1U << 0U)
#define NX_U575_SELECT_USART2 (1U << 1U)
#define NX_U575_SELECT_SPI1   (1U << 2U)
#define NX_U575_SELECT_TIM3   (1U << 3U)
#define NX_U575_SELECT_I2C1   (1U << 4U)
#define NX_U575_SELECT_ADC4   (1U << 5U)
#define NX_U575_SELECT_FLASH  (1U << 6U)
#define NX_U575_SELECT_EXTI0  (1U << 7U)
#define NX_U575_SELECT_EXTI1  (1U << 8U)
#define NX_U575_SELECT_LPTIM2 (1U << 9U)
#define NX_U575_SELECT_GPDMA  (1U << 10U)
/** \brief One exact selected bank snapshot, retained through failed restore. */
typedef struct {
    uint32_t mode;
    uint32_t type;
    uint32_t speed;
    uint32_t pull;
    uint32_t af0;
    uint32_t af1;
    uint32_t odr;
    bool captured;
} nx_stm32u575_gpio_receipt_t;
/** \brief Static bank authorization and its sole mutable snapshot. */
typedef struct {
    GPIO_TypeDef* registers;
    nx_stm32u575_gpio_receipt_t* receipt;
    uint16_t pins;
    uint8_t bank;
} nx_stm32u575_gpio_lease_t;
/** \brief Only selected gates receive mutable storage from static assembly. */
typedef struct {
    const nx_stm32u575_gpio_lease_t* leases;
    size_t lease_count;
    uint32_t selected;
    uint32_t* gpio_added;
    uint32_t* apb1_added;
    uint32_t* apb2_added;
    uint32_t* ahb3_added;
} nx_stm32u575_platform_config_t;
/** \brief Read-only exact controller admission before clock effects. */
nx_result_t nx_stm32u575_uart_preflight(uint8_t controller, bool dma);
nx_result_t nx_stm32u575_spi_preflight(void);
nx_result_t nx_stm32u575_pwm_preflight(void);
nx_result_t nx_stm32u575_i2c_preflight(void);
nx_result_t nx_stm32u575_adc_preflight(void);
nx_result_t nx_stm32u575_flash_preflight(void);
nx_result_t nx_stm32u575_exti_preflight(uint8_t line);
nx_result_t nx_stm32u575_lptim_preflight(void);
/** \brief Read selected cold peripheral and powered-pad facts, without effects.
 */
nx_result_t
nx_stm32u575_platform_preflight(const nx_stm32u575_platform_config_t* config);
/** \brief Acquire selected gates and capture all admitted banks exactly once.
 * \note A serialized privileged unmasked Thread owner first passes each
 *       selected controller preflight and platform_preflight. Partial failures
 *       retain gate/snapshot duties for platform_release; an owner with any
 *       pending snapshot or added gate cannot be rebound by another acquire.
 */
nx_result_t
nx_stm32u575_platform_acquire(const nx_stm32u575_platform_config_t* config);
/** \brief Restore only captured pins before releasing added clock gates.
 * \note Call only after all selected controllers and publishers have joined.
 *       Failed readback retains the same snapshot and supports retry.
 */
nx_result_t
nx_stm32u575_platform_release(const nx_stm32u575_platform_config_t* config);
#ifdef __cplusplus
}
#endif
#endif
