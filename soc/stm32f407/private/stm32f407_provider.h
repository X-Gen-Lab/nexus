/**
 * \file            stm32f407_provider.h
 * \brief           Private fixed STM32F407 contexts
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F407_PROVIDER_H
#define NX_STM32F407_PROVIDER_H

#include "nexus/io/adc.h"
#include "nexus/io/exti.h"
#include "nexus/io/flash.h"
#include "nexus/io/gpio.h"
#include "nexus/io/i2c.h"
#include "nexus/io/spi.h"
#include "nexus/io/timer.h"
#include "nexus/io/uart.h"
#include "nexus/io/watchdog.h"
#include "stm32f407_system.h"

typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    bool output;
    bool initialized;
} nx_stm32_gpio_state_t;
typedef struct {
    USART_TypeDef* registers;
    const nx_irq_wake_t* wake;
    nx_uart_tx_request_t* active;
    size_t tx_position;
    void* rx_storage;
    size_t rx_capacity;
    size_t rx_head;
    size_t rx_tail;
    size_t rx_count;
    uint32_t rx_loss;
    uint32_t baud;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    nx_uart_rx_profile_t profile;
    IRQn_Type irq;
    bool initialized;
    bool closing;
    bool tx_complete;
} nx_stm32_uart_state_t;
typedef struct {
    SPI_TypeDef* registers;
    RCC_TypeDef* rcc;
    uint32_t clock_hz;
    bool active;
    bool fault;
} nx_stm32_spi_state_t;
typedef struct {
    nx_stm32_spi_state_t* port;
    nx_stm32_gpio_state_t* cs;
    uint32_t cs_mask;
    uint32_t frequency_hz;
    uint8_t mode;
} nx_stm32_spi_endpoint_state_t;
typedef struct {
    I2C_TypeDef* registers;
    const GPIO_TypeDef* line_gpio;
    uint32_t line_mask;
    uint32_t peripheral_mhz;
    uint32_t rate_hz;
    bool active;
    bool fault;
    bool initialized;
} nx_stm32_i2c_state_t;
typedef struct {
    nx_stm32_i2c_state_t* port;
    uint8_t address;
} nx_stm32_i2c_endpoint_state_t;
typedef struct {
    FLASH_TypeDef* registers;
    const nx_flash_geometry_t* geometry;
    volatile uint8_t* memory;
    uint32_t supply_mv;
    bool active;
} nx_stm32_flash_state_t;
typedef struct {
    IWDG_TypeDef* registers;
    FLASH_TypeDef* flash;
    DBGMCU_TypeDef* debug;
    RCC_TypeDef* rcc;
    nx_watchdog_state_t state;
    uint32_t poll_limit;
} nx_stm32_watchdog_state_t;
typedef struct {
    EXTI_TypeDef* registers;
    const nx_irq_wake_t* wake;
    GPIO_TypeDef* gpio;
    nx_exti_event_t* storage;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    uint32_t loss;
    uint8_t line;
    nx_exti_edge_t edge;
    bool initialized;
} nx_stm32_exti_state_t;
typedef struct {
    TIM_TypeDef* registers;
    nx_stm32_gpio_state_t* inactive_gpio;
    uint32_t inactive_mask;
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint16_t prescaler;
    uint8_t channel;
    bool inactive_high;
    bool initialized;
    bool running;
} nx_stm32_pwm_state_t;
typedef struct {
    ADC_TypeDef* registers;
    ADC_Common_TypeDef* common;
    const uint8_t* channels;
    const uint8_t* sample_times;
    size_t channel_count;
    uint32_t reference_mv;
    bool active;
    bool initialized;
} nx_stm32_adc_state_t;

#ifdef NEXUS_STM32_MODEL
extern GPIO_TypeDef g_nx_stm32_gpioa_model;
#undef GPIOA
#define GPIOA (&g_nx_stm32_gpioa_model)
#endif

extern nx_stm32_system_t g_nx_stm32_system;
extern const nx_flash_geometry_t g_nx_stm32_flash_ve;
extern const nx_flash_geometry_t g_nx_stm32_flash_zg;

/** \brief Configure a reviewed pin in a cold control path. */
nx_result_t nx_stm32_pin_configure(GPIO_TypeDef* gpio, uint8_t pin,
                                   uint8_t mode, uint8_t af, uint8_t pull,
                                   bool open_drain);
/** \brief Save only one pin's electrical state for cold-start rollback. */
uint16_t nx_stm32_pin_capture(const GPIO_TypeDef* gpio, uint8_t pin);
/** \brief Restore a captured pin without changing neighboring pin fields. */
void nx_stm32_pin_restore(GPIO_TypeDef* gpio, uint8_t pin, uint16_t state);
/** \brief Enable one GPIOA--GPIOI clock with ordered readback. */
nx_result_t nx_stm32_gpio_clock_enable(unsigned port_index);
/** \brief Configure reviewed USART1 PA9/PA10 AF7 and peripheral clocks. */
nx_result_t nx_stm32_uart1_pins_prepare(void);
/** \brief Invalidate a quiescent GPIO binding at its inactive electrical level.
 */
nx_result_t nx_stm32_gpio_stop(nx_stm32_gpio_state_t* port, uint32_t inactive);
/** \brief Initialize authorized pins, preloading BSRR before output mode. */
nx_result_t nx_stm32_gpio_initialize(nx_stm32_gpio_state_t* port,
                                     uint32_t initial);
/** \brief Initialize one fixed 8N1 UART; storage must outlive IRQ and stop. */
nx_result_t nx_stm32_uart_initialize(nx_stm32_uart_state_t* port,
                                     uint32_t clock_hz);
/** \brief Bounded USART IRQ dispatch for the statically bound instance. */
void nx_stm32_uart_irq(nx_stm32_uart_state_t* port);
/** \brief Advance only TX facts when an independent provider owns RX storage.
 */
bool nx_stm32_uart_tx_irq(nx_stm32_uart_state_t* port, uint32_t status);
/** \brief Initialize standard-mode I2C at a fixed 42 MHz peripheral clock. */
nx_result_t nx_stm32_i2c_initialize(nx_stm32_i2c_state_t* port);
/** \brief Initialize one statically allocated EXTI line and event ring. */
nx_result_t nx_stm32_exti_initialize(nx_stm32_exti_state_t* port,
                                     SYSCFG_TypeDef* mux, uint8_t gpio_index);
/** \brief Bounded IRQ delivery for a statically assembled line array. */
void nx_stm32_exti_dispatch(nx_stm32_exti_state_t* const* ports, size_t count,
                            uint32_t vector_mask);
/** \brief Initialize one TIM3 general-purpose PWM channel. */
nx_result_t nx_stm32_pwm_initialize(nx_stm32_pwm_state_t* port);
/** \brief Initialize fixed ADC1 channels, independent 12-bit software trigger.
 */
nx_result_t nx_stm32_adc_initialize(nx_stm32_adc_state_t* port);

/** \brief Shared provider methods for statically assembled interfaces. */
extern const nx_gpio_ops_t nx_stm32_gpio_ops;
nx_result_t nx_stm32_gpio_write(void* context, uint32_t set_mask,
                                uint32_t reset_mask);
nx_result_t nx_stm32_gpio_read(const void* context, uint32_t* value);
nx_result_t nx_stm32_gpio_toggle(void* context, uint32_t mask);
extern const nx_uart_ops_t nx_stm32_uart_ops;
nx_result_t nx_stm32_uart_submit(void* context, nx_uart_tx_request_t* request);
nx_result_t nx_stm32_uart_start_admitted(void* context,
                                         nx_uart_tx_request_t* request);
nx_result_t nx_stm32_uart_cancel(void* context, nx_uart_tx_request_t* request);
void nx_stm32_uart_service(void* context);
nx_result_t nx_stm32_uart_read_events(void* context, nx_uart_rx_event_t* events,
                                      size_t capacity, size_t* count);
nx_result_t nx_stm32_uart_read_bytes(void* context, uint8_t* bytes,
                                     size_t capacity, size_t* count);
nx_result_t nx_stm32_uart_stop(void* context);
extern const nx_spi_ops_t nx_stm32_spi_ops;
nx_result_t nx_stm32_spi_recover(void* context);
extern const nx_spi_endpoint_ops_t nx_stm32_spi_endpoint_ops;
nx_result_t nx_stm32_spi_endpoint_transfer(void* context, const uint8_t* tx,
                                           uint8_t* rx, size_t length,
                                           nx_time_us_t deadline,
                                           size_t* transferred);
extern const nx_i2c_ops_t nx_stm32_i2c_ops;
nx_result_t nx_stm32_i2c_recover(void* context);
extern const nx_i2c_endpoint_ops_t nx_stm32_i2c_endpoint_ops;
nx_result_t nx_stm32_i2c_endpoint_transaction(void* context,
                                              nx_i2c_message_t* messages,
                                              size_t count,
                                              nx_time_us_t deadline,
                                              size_t* transferred);
extern const nx_flash_ops_t nx_stm32_flash_ops;
const nx_flash_geometry_t* nx_stm32_flash_geometry(const void* context);
nx_result_t nx_stm32_flash_read(const void* context, uint32_t offset,
                                void* data, size_t length);
nx_result_t nx_stm32_flash_program(void* context, uint32_t offset,
                                   const void* data, size_t length,
                                   nx_time_us_t deadline);
nx_result_t nx_stm32_flash_erase(void* context, uint32_t offset, size_t length,
                                 nx_time_us_t deadline);
extern const nx_watchdog_ops_t nx_stm32_watchdog_ops;
nx_result_t nx_stm32_watchdog_enable(void* context, uint32_t timeout_us,
                                     bool debug_freeze,
                                     nx_watchdog_state_t* state);
nx_result_t nx_stm32_watchdog_feed(void* context);
nx_result_t nx_stm32_watchdog_state(const void* context,
                                    nx_watchdog_state_t* state);
extern const nx_exti_ops_t nx_stm32_exti_ops;
nx_result_t nx_stm32_exti_read(void* context, nx_exti_event_t* events,
                               size_t capacity, size_t* count);
nx_result_t nx_stm32_exti_stop(void* context);
extern const nx_pwm_ops_t nx_stm32_pwm_ops;
nx_result_t nx_stm32_pwm_set(void* context, uint32_t period_ticks,
                             uint32_t duty_ticks);
nx_result_t nx_stm32_pwm_start(void* context);
nx_result_t nx_stm32_pwm_stop(void* context);
nx_result_t nx_stm32_pwm_state(const void* context, nx_pwm_state_t* state);
extern const nx_adc_ops_t nx_stm32_adc_ops;
nx_result_t nx_stm32_adc_info(const void* context, nx_adc_info_t* info);
nx_result_t nx_stm32_adc_sample(void* context, uint16_t* samples,
                                size_t capacity, nx_time_us_t deadline,
                                size_t* count);

#endif
