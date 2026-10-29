/**
 * \file            stm32u575_cold.h
 * \brief           Exact immutable resources for concrete U575 cold owners
 * \author          Nexus Team
 * \note            Prepare/release require a serialized privileged unmasked
 *                  Thread owner. Platform first admits and owns the selected
 *                  clocks and sole GPIO snapshots. Static configurations,
 *                  endpoint state and byte/event storage outlive successful
 *                  stop. Failures after effects retain ordinary stop/release
 *                  responsibilities; timeout never revokes outstanding loans.
 */
#ifndef NX_STM32U575_COLD_H
#define NX_STM32U575_COLD_H
#include "stm32u575_provider.h"
#include "stm32u575_uart_fifo.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief An authorized GPIO subset and its explicit initial latch. */
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    uint32_t initial;
    bool output;
} nx_stm32u575_gpio_config_t;
/** \brief A static byte-ring binding; no runtime copy of configuration. */
typedef struct {
    uint8_t* storage;
    size_t capacity;
    uint32_t baud;
    nx_irq_policy_t irq_policy;
    uint8_t controller;
} nx_stm32u575_uart_config_t;
/** \brief Selected leased alternate pin; hardware operations remain in C. */
typedef struct {
    GPIO_TypeDef* registers;
    uint8_t pin;
    uint8_t alternate;
} nx_stm32u575_alternate_config_t;
/** \brief One endpoint uses existing independent GPIO and endpoint state. */
typedef struct {
    nx_stm32u575_gpio_state_t* cs;
    nx_stm32u575_spi_endpoint_state_t* endpoint;
    GPIO_TypeDef* registers;
    uint32_t mask;
    uint32_t frequency_hz;
    uint8_t mode;
} nx_stm32u575_spi_endpoint_config_t;
/** \brief Exact SPI1 finite polling with statically sized endpoints and pads.
 */
typedef struct {
    const nx_stm32u575_spi_endpoint_config_t* endpoints;
    size_t endpoint_count;
    const nx_stm32u575_alternate_config_t* pins;
    size_t pin_count;
} nx_stm32u575_spi_config_t;
/** \brief Exact TIM3 CH1 settings and one already leased inactive GPIO. */
typedef struct {
    nx_stm32u575_gpio_state_t* idle;
    GPIO_TypeDef* gpio;
    uint32_t inactive;
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint8_t pin;
} nx_stm32u575_pwm_config_t;
/** \brief Independent caller-sized EXTI ring and immutable interrupt policy. */
typedef struct {
    nx_exti_event_t* storage;
    size_t capacity;
    nx_irq_policy_t irq_policy;
    nx_exti_edge_t edge;
    uint8_t line;
    uint8_t priority;
} nx_stm32u575_exti_config_t;
/** \brief Prepare only a serialized cold GPIO; retained state rejects. */
nx_result_t nx_stm32u575_gpio_prepare(nx_stm32u575_gpio_state_t* port,
                                      const nx_stm32u575_gpio_config_t* config);
/** \brief Prepare USART1/2 bytes after the platform owns pads and gates. */
nx_result_t nx_stm32u575_uart_prepare(nx_stm32u575_uart_state_t* port,
                                      const nx_stm32u575_uart_config_t* config);
/** \brief Bind an idle byte owner without modifying hardware or live state. */
nx_result_t nx_stm32u575_uart_bind(nx_stm32u575_uart_state_t* port,
                                   const nx_stm32u575_uart_config_t* config);
/** \brief Prepare exact USART1 FIFO state and pads, retaining failed cleanup.
 */
nx_result_t
nx_stm32u575_uart_fifo_prepare(nx_stm32u575_uart_fifo_state_t* port,
                               const nx_stm32u575_uart_config_t* config,
                               const nx_stm32u575_uart_fifo_profile_t* profile);
/** \brief Prepare exact USART1 FIFO flow control with its four leased pads. */
nx_result_t nx_stm32u575_uart_rtscts_prepare(
    nx_stm32u575_uart_fifo_state_t* port,
    const nx_stm32u575_uart_config_t* config,
    const nx_stm32u575_uart_fifo_profile_t* profile);
/** \brief Prepare SPI1 then static CS endpoints and alternate pads. */
nx_result_t nx_stm32u575_spi_prepare(nx_stm32u575_spi_state_t* port,
                                     const nx_stm32u575_spi_config_t* config);
/** \brief Drain SPI before releasing each exact inactive CS owner. */
nx_result_t nx_stm32u575_spi_release(nx_stm32u575_spi_state_t* port,
                                     const nx_stm32u575_spi_config_t* config);
/** \brief Prepare cold TIM3 and its inactive pad without replacing live state.
 */
nx_result_t nx_stm32u575_pwm_prepare(nx_stm32u575_pwm_state_t* port,
                                     const nx_stm32u575_pwm_config_t* config);
/** \brief Bind and initialize a cold exact EXTI0/1 ring. */
nx_result_t nx_stm32u575_exti_prepare(nx_stm32u575_exti_state_t* port,
                                      const nx_stm32u575_exti_config_t* config);
/** \brief Bind physical Flash only when its previous owner is fully cold. */
nx_result_t nx_stm32u575_flash_prepare(nx_stm32u575_flash_state_t* port);
/** \brief Bind inactive watchdog resources without starting the watchdog. */
nx_result_t nx_stm32u575_watchdog_prepare(nx_stm32u575_watchdog_state_t* port);
#ifdef __cplusplus
}
#endif
#endif
