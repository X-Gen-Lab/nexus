/**
 * \file            cold.h
 * \brief           Static GPIO electrical construction and admission
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_DRIVERS_GPIO_COLD_H
#define NEXUS_STM32F4_DRIVERS_GPIO_COLD_H

#include "drivers/gpio/instance.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Cold output slew selection; zero retains the existing pad setting. */
typedef enum nx_stm32_gpio_speed {
    NX_STM32_GPIO_SPEED_PRESERVE = 0,
    NX_STM32_GPIO_SPEED_LOW,
    NX_STM32_GPIO_SPEED_MEDIUM,
    NX_STM32_GPIO_SPEED_HIGH,
    NX_STM32_GPIO_SPEED_VERY_HIGH
} nx_stm32_gpio_speed_t;

/** \brief Immutable exact single-bank GPIO authorization. */
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    uint32_t initial;
    bool output;
    uint8_t pull;
    uint8_t output_type;
    uint8_t speed; /**< nx_stm32_gpio_speed_t; uses existing record padding. */
} nx_stm32_gpio_config_t;

/** \brief Validate exact GPIO binding and context without writes or binding. */
nx_result_t nx_stm32_gpio_admit(const nx_stm32_gpio_state_t* port,
                                const nx_stm32_gpio_config_t* config);

/** \brief Bind and initialize GPIO after Platform has admitted its pads.
 * \param[in,out] port: Serialized cold owner; live rebind is rejected.
 * \param[in] config: Static bank, mask, direction, initial and electrical
 * facts. \return Success or error; malformed configuration changes no hardware.
 * \note Pull is none/up/down (0/1/2); output type is push-pull/open-drain
 *       (0/1). Open-drain applies only to outputs. Platform retains the
 *       original pin snapshots and shared bank clock. Speed applies only to
 *       outputs; zero preserves the prior setting except PC13/14/15, whose
 *       output speed is always LOW. Other explicit restricted speeds reject
 *       before effects. Speed readback failure retains the bound owner.
 */
nx_result_t nx_stm32_gpio_prepare(nx_stm32_gpio_state_t* port,
                                  const nx_stm32_gpio_config_t* config);

#ifdef __cplusplus
}
#endif
#endif
