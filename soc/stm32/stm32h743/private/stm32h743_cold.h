/**
 * \file            stm32h743_cold.h
 * \brief           Immutable H743 controller construction and cold admission
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32H743_COLD_H
#define NEXUS_STM32H743_COLD_H
#include "stm32h743_provider.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief GPIO authorization; Platform retains the one pad snapshot. */
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    uint32_t initial;
    bool output;
} nx_stm32h743_gpio_config_t;
/** \brief One explicit byte ring or block profile and immutable IRQ policy. */
typedef struct {
    uint8_t* storage;
    size_t capacity;
    uint32_t baud;
    uint32_t clock_hz;
    nx_irq_policy_t irq_policy;
    nx_uart_rx_profile_t profile;
} nx_stm32h743_uart_config_t;
/** \brief A selected SPI endpoint and its independently retained CS state. */
typedef struct {
    nx_stm32h743_spi_endpoint_state_t* endpoint;
    nx_stm32h743_gpio_state_t* cs;
    GPIO_TypeDef* gpio;
    uint32_t mask;
    uint32_t frequency_hz;
    uint8_t mode;
} nx_stm32h743_spi_endpoint_config_t;
/** \brief Finite endpoint list; no runtime discovery or maximum pool. */
typedef struct {
    const nx_stm32h743_spi_endpoint_config_t* endpoints;
    size_t count;
    uint32_t clock_hz;
} nx_stm32h743_spi_config_t;
/** \brief Exact TIM3 PA6 PWM and Board-approved inactive output. */
typedef struct {
    nx_stm32h743_gpio_state_t* idle;
    GPIO_TypeDef* gpio;
    uint32_t inactive;
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint8_t pin;
} nx_stm32h743_pwm_config_t;
/** \brief One EXTI line with caller-explicit bounded event storage. */
typedef struct {
    nx_exti_event_t* storage;
    size_t capacity;
    nx_irq_policy_t irq_policy;
    nx_exti_edge_t edge;
    uint8_t line;
    uint8_t priority;
} nx_stm32h743_exti_config_t;
/** \brief Configure an initially cold GPIO; effects remain Platform-owned.
 * \note Privileged unmasked Thread context, serialized through successful stop.
 *       Failure after effects retains state and the same Platform pad lease.
 */
nx_result_t nx_stm32h743_gpio_prepare(nx_stm32h743_gpio_state_t* port,
                                      const nx_stm32h743_gpio_config_t* config);
/** \brief Bind cold UART state without enabling hardware or publishers.
 * \note The concrete mode prepares pins and initializes immediately afterward.
 *       Static configuration and storage outlive all retained lifecycle
 * effects.
 */
nx_result_t
nx_stm32h743_uart_configure(nx_stm32h743_uart_state_t* port,
                            const nx_stm32h743_uart_config_t* config);
/** \brief Prepare exact IRQ-byte USART3 after Platform acquires its pads.
 * \note Privileged unmasked Thread context; stop joins publishers before the
 *       Platform restores its one GPIO/RCC receipt. Failure retains that duty.
 */
nx_result_t nx_stm32h743_uart_prepare(nx_stm32h743_uart_state_t* port,
                                      const nx_stm32h743_uart_config_t* config);
/** \brief Prepare SPI4 and every exact static CS; no runtime registration.
 * \note Privileged unmasked Thread context, no concurrent transfer. Partial
 *       effects remain under Platform until successful release and pad restore.
 */
nx_result_t nx_stm32h743_spi_prepare(nx_stm32h743_spi_state_t* port,
                                     const nx_stm32h743_spi_config_t* config);
/** \brief Drain SPI before withdrawing every selected CS output.
 * \note Failure preserves state and the immutable configuration for retry.
 */
nx_result_t nx_stm32h743_spi_release(nx_stm32h743_spi_state_t* port,
                                     const nx_stm32h743_spi_config_t* config);
/** \brief Prepare TIM3 only from a cold owner with a Board-approved idle.
 * \note Task-only. Platform owns snapshots through successful pwm_close.
 */
nx_result_t nx_stm32h743_pwm_prepare(nx_stm32h743_pwm_state_t* port,
                                     const nx_stm32h743_pwm_config_t* config);
/** \brief Bind an inactive IWDG descriptor without starting the watchdog.
 * \note Task-only; enabled watchdogs and pending cleanup reject replacement.
 */
nx_result_t nx_stm32h743_watchdog_prepare(nx_stm32h743_watchdog_state_t* port);
/** \brief Bind exact EXTI state before enabling its bounded publisher.
 * \note Task-only; state, storage and Platform pad leases outlive failed stop.
 */
nx_result_t nx_stm32h743_exti_prepare(nx_stm32h743_exti_state_t* port,
                                      const nx_stm32h743_exti_config_t* config);
/** \brief Read exact selected controller prerequisites without effects.
 * \note Privileged unmasked Thread context before System clock acquisition;
 *       BUSY preserves every foreign gate, reset, IRQ and mux observation.
 */
nx_result_t nx_stm32h743_uart_preflight(void);
nx_result_t nx_stm32h743_spi_preflight(void);
nx_result_t nx_stm32h743_pwm_preflight(void);
nx_result_t nx_stm32h743_i2c_preflight(void);
nx_result_t nx_stm32h743_adc_preflight(void);
nx_result_t nx_stm32h743_flash_preflight(void);
nx_result_t nx_stm32h743_exti_preflight(uint8_t line);

#ifdef __cplusplus
}
#endif
#endif
