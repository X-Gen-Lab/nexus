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
#include "nexus/io/flash_maintenance.h"
#include "nexus/io/gpio.h"
#include "nexus/io/i2c.h"
#include "nexus/io/spi.h"
#include "nexus/io/spi_dma_owner_binding.h"
#include "nexus/io/spi_transaction.h"
#include "nexus/io/timer.h"
#include "nexus/io/watchdog.h"
#include "stm32f407_system.h"
#include "stm32f4_uart.h"

#include "stm32f407_timer_hardware.h"
#include "stm32f4_irq_wake.h"
#include "stm32f4xx.h"
#if !defined(NEXUS_STM32_MODEL)
#include "nexus_config.h"
#endif

typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    bool output;
    bool initialized;
} nx_stm32_gpio_state_t;
typedef struct nx_stm32_spi_endpoint_state nx_stm32_spi_endpoint_state_t;
typedef struct {
    SPI_TypeDef* registers;
    RCC_TypeDef* rcc;
    uint32_t clock_hz;
    bool active;
    bool fault;
    const nx_stm32_spi_endpoint_state_t* held_endpoint;
} nx_stm32_spi_state_t;
struct nx_stm32_spi_endpoint_state {
    nx_stm32_spi_state_t* port;
    nx_stm32_gpio_state_t* cs;
    uint32_t cs_mask;
    uint32_t frequency_hz;
    uint8_t mode;
    bool lsb_first;
};

/** \brief Check the selected CS output latch, retaining failed withdrawal.
 * \note Endpoint identity is static; this never retains a payload pointer.
 */
nx_result_t nx_stm32_spi_select(const nx_stm32_spi_endpoint_state_t* endpoint,
                                bool selected);
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
    uint16_t address;
    uint8_t address_bits;
} nx_stm32_i2c_endpoint_state_t;
typedef struct {
    FLASH_TypeDef* registers;
    const nx_flash_geometry_t* geometry;
    volatile uint8_t* memory;
    uint32_t supply_mv;
    bool active;
    bool power_on_reset_required;
    bool options_owned;
#if defined(STM32F479xx)
    uint8_t cache_flags;
    bool cache_owned;
#endif
} nx_stm32_flash_state_t;
/** \brief Explicitly assembled optional face shares the physical pulse owner.
 */
extern const nx_flash_maintenance_ops_t nx_stm32_flash_maintenance_ops;
typedef struct {
    IWDG_TypeDef* registers;
    FLASH_TypeDef* flash;
    DBGMCU_TypeDef* debug;
    RCC_TypeDef* rcc;
    nx_watchdog_state_t state;
    uint32_t poll_limit;
    uint32_t saved_debug;
    bool lsi_owned;
    bool debug_owned;
    bool preparation_retained;
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
    bool cold_owned;
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
    uint8_t cold_effects;
} nx_stm32_pwm_state_t;

enum {
    NX_STM32_ADC_CLOCK = 1U,
    NX_STM32_ADC_TIMER = 2U,
    NX_STM32_ADC_CONVERTER = 4U,
    NX_STM32_ADC_PROBE1 = 8U,
    NX_STM32_ADC_PROBE2 = 16U,
    NX_STM32_ADC_PROBE3 = 32U,
    NX_STM32_ADC_PROBES = 56U,
    NX_STM32_ADC_IRQ = 64U,
    NX_STM32_ADC_INTERNAL = 128U
};
typedef struct {
    ADC_TypeDef* registers;
    ADC_Common_TypeDef* common;
    const uint8_t* channels;
    const uint8_t* sample_times;
    size_t channel_count;
    uint32_t reference_mv;
    bool active;
    bool initialized;
    uint8_t cold_effects;
    uint8_t resolution; /**< ADC_CR1 RES encoding: 0/1/2/3 = 12/10/8/6 bits. */
} nx_stm32_adc_state_t;

/** \brief Include one microsecond of quantization in the fixed TIM2 domain. */
static inline uint32_t
nx_stm32_adc_startup_delay_us(const nx_stm32_adc_state_t* port) {
    return (port->cold_effects & NX_STM32_ADC_INTERNAL) != 0U ? 11U : 4U;
}

/** \brief Validate raw ranks before any converter or analog path is changed. */
nx_result_t nx_stm32_adc_sequence_validate(ADC_TypeDef* registers,
                                           const uint8_t* channels,
                                           const uint8_t* sample_times,
                                           size_t count);
/** \brief Derive F407 ADC1 internal paths from the retained static sequence. */
uint32_t nx_stm32_adc_internal_mask(const nx_stm32_adc_state_t* port);
/** \brief Acquire previously disabled internal paths, retaining failed effects.
 */
nx_result_t nx_stm32_adc_internal_prepare(nx_stm32_adc_state_t* port);
/** \brief Release only acquired analog paths before removing the ADC clock. */
nx_result_t nx_stm32_adc_internal_close(nx_stm32_adc_state_t* port);

#ifdef NEXUS_STM32_MODEL
extern GPIO_TypeDef g_nx_stm32_gpioa_model;
#undef GPIOA
#define GPIOA (&g_nx_stm32_gpioa_model)
#endif

extern nx_stm32_system_t g_nx_stm32_system;

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
/** \brief Invalidate a quiescent GPIO binding at its inactive electrical level.
 */
nx_result_t nx_stm32_gpio_stop(nx_stm32_gpio_state_t* port, uint32_t inactive);
/** \brief Initialize authorized pins, preloading BSRR before output mode. */
nx_result_t nx_stm32_gpio_initialize(nx_stm32_gpio_state_t* port,
                                     uint32_t initial);
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
/** \brief Initialize independent ADC1/2/3 ranks and selected raw resolution.
 */
nx_result_t nx_stm32_adc_initialize(nx_stm32_adc_state_t* port);
nx_result_t nx_stm32_adc_common_prepare(nx_stm32_adc_state_t* port);
nx_result_t nx_stm32_adc_probe_close(nx_stm32_adc_state_t* port);
nx_result_t nx_stm32_adc_quiesce(nx_stm32_adc_state_t* port);

/** \brief Shared provider methods for statically assembled interfaces. */
extern const nx_gpio_ops_t nx_stm32_gpio_ops;
nx_result_t nx_stm32_gpio_write(void* context, uint32_t set_mask,
                                uint32_t reset_mask);
nx_result_t nx_stm32_gpio_read(const void* context, uint32_t* value);
nx_result_t nx_stm32_gpio_toggle(void* context, uint32_t mask);
extern const nx_spi_ops_t nx_stm32_spi_ops;
nx_result_t nx_stm32_spi_recover(void* context);
extern const nx_spi_endpoint_ops_t nx_stm32_spi_endpoint_ops;
extern const nx_spi_transaction_ops_t nx_stm32_spi_transaction_ops;
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
/** \brief Retry owned preparation cleanup; enabled IWDG stays irreversible. */
nx_result_t nx_stm32_watchdog_stop(nx_stm32_watchdog_state_t* port);
extern const nx_exti_ops_t nx_stm32_exti_ops;
nx_result_t nx_stm32_exti_read(void* context, nx_exti_event_t* events,
                               size_t capacity, size_t* count);
nx_result_t nx_stm32_exti_stop(void* context);
nx_result_t nx_stm32_exti_attach_wake(void* context, const nx_irq_wake_t* wake,
                                      uint8_t syscall_ceiling);
extern const nx_pwm_ops_t nx_stm32_pwm_ops;
nx_result_t nx_stm32_pwm_set(void* context, uint32_t period_ticks,
                             uint32_t duty_ticks);
nx_result_t nx_stm32_pwm_start(void* context);
nx_result_t nx_stm32_pwm_stop(void* context);
nx_result_t nx_stm32_pwm_state(const void* context, nx_pwm_state_t* state);
extern const nx_adc_ops_t nx_stm32_adc_ops;
extern const nx_adc_ops_t nx_stm32_adc_injected_ops;
nx_result_t nx_stm32_adc_info(const void* context, nx_adc_info_t* info);
nx_result_t nx_stm32_adc_sample(void* context, uint16_t* samples,
                                size_t capacity, nx_time_us_t deadline,
                                size_t* count);

/** \brief Exact enhanced sequence and underlying controller membership. */
bool nx_stm32_spi_dma_owner_contains(
    const nx_spi_dma_transaction_endpoint_t* endpoint,
    const nx_spi_dma_transaction_port_t* port);
/** \brief Bind one borrowed wake sink to the exact enhanced controller. */
nx_result_t
nx_stm32_spi_dma_owner_attach_wake(const nx_spi_dma_transaction_port_t* port,
                                   const nx_irq_wake_t* wake,
                                   uint8_t syscall_ceiling);

#endif
