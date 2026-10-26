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
#include "nexus/io/spi_dma_owner_binding.h"
#include "nexus/io/spi_transaction.h"
#include "nexus/io/timer.h"
#include "nexus/io/uart.h"
#include "nexus/io/uart_format.h"
#include "nexus/io/watchdog.h"
#include "stm32f407_system.h"

#include "stm32f407xx.h"
#if !defined(NEXUS_STM32_MODEL)
#include "nexus_config.h"
#endif

/** \brief Check the actual publisher before borrowing any notification target.
 */
static inline nx_result_t nx_stm32_irq_wake_validate(const nx_irq_wake_t* wake,
                                                     int irq,
                                                     uint8_t syscall_ceiling) {
#if defined(NEXUS_STM32_MODEL)
    nx_irq_policy_t policy = {NX_IRQ_KERNEL_BASEPRI, 82U, 4U, 5U};
#else
    nx_irq_policy_t policy = {
        (nx_irq_kernel_policy_t)NEXUS_IRQ_KERNEL_POLICY,
        NEXUS_CPU_EXTERNAL_IRQ_COUNT,
        NEXUS_IRQ_PRIORITY_BITS,
#if NEXUS_IRQ_KERNEL_POLICY == 2
        NEXUS_IRQ_SYSCALL_PRIORITY
#else
        0U
#endif
    };
#endif
    if (irq < 0 || (unsigned)irq >= policy.external_irq_count) {
        return NX_ERROR_INVALID;
    }
    if (wake != NULL && wake->calls_kernel) {
        if (policy.kernel == NX_IRQ_KERNEL_PRIMASK && syscall_ceiling != 0U) {
            return NX_ERROR_INVALID;
        }
        if (policy.kernel == NX_IRQ_KERNEL_BASEPRI &&
            syscall_ceiling > policy.syscall_ceiling) {
            policy.syscall_ceiling = syscall_ceiling;
        }
    }
    const nx_irq_source_t source = {
        (int16_t)irq,
#if defined(NEXUS_STM32_MODEL)
        5U,
        0U
#else
        (uint8_t)NVIC_GetPriority((IRQn_Type)irq),
        (uint8_t)NVIC_GetPriorityGrouping()
#endif
    };
    return nx_irq_wake_validate(wake, &policy, &source);
}

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
/** \brief Initialize one fixed 8N1 UART; storage outlives IRQ and stop.
 * \param[in,out] port: Fresh zeroed state with selected config filled in.
 * \note Live or retained ownership rejects reinitialization. Reuse after
 *       successful stop; never zero state to revoke a live borrow.
 */
nx_result_t nx_stm32_uart_initialize(nx_stm32_uart_state_t* port,
                                     uint32_t clock_hz);
/** \brief Cold selected frame is installed before UE and IRQ publication. */
nx_result_t nx_stm32_uart_initialize_format(nx_stm32_uart_state_t* port,
                                            uint32_t clock_hz,
                                            nx_uart_format_t format);
/** \brief Apply a selected byte format only under quiescent cold ownership.
 * \param[in,out] state: Initialized byte UART; no TX/RX loan or queued RX.
 * \param[in] format: 7E/O or 8N/E/O with one/two stop bits and reviewed flow.
 * \return Success or INVALID/UNSUPPORTED/CONTEXT/STATE/BUSY before writes;
 *         IO attempts rollback after failed register readback. If rollback
 *         cannot be proven, initialized remains true and closing rejects new
 *         work. Retain pins, clock and storage until stop proves withdrawal.
 * \note Startup task must quiesce the peer. Seven-bit receive uses the separate
 *         format_irq entry. Format selection adds no default state or ops slot.
 */
nx_result_t nx_stm32_uart_format_configure(nx_stm32_uart_state_t* state,
                                           nx_uart_format_t format);
/** \brief IRQ entry for explicitly selected seven/eight-bit parity formats.
 * \param[in,out] port: Exact live binding; preserves RX parity/error facts.
 */
void nx_stm32_uart_format_irq(nx_stm32_uart_state_t* port);

/**
 * \brief           Select independent RTS/CTS only on an idle live controller.
 * \param[in,out]   state: Startup-controlled UART, no active TX or RX DMA.
 * \param[in]       rts: Actual hardware RTS output enable.
 * \param[in]       cts: Actual hardware CTS transmit gate enable.
 * \return          Success or INVALID/STATE/CONTEXT/BUSY before any write.
 * \note            Board owns AF pins/wiring. RTS covers hardware receive
 *                  register capacity, not application stream/ring backpressure.
 *                  Default state has no extra RAM; other CR3 bits are
 * preserved.
 */
nx_result_t nx_stm32_uart_flow_configure(nx_stm32_uart_state_t* state, bool rts,
                                         bool cts);

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

/**
 * \brief           Side-effect-free checks shared with this exact TX start.
 * \param[in]       context: Exact immutable face's selected provider state.
 * \param[in]       request: Live descriptor, not admitted or retained here.
 * \return          Same payload/domain/context/state/deadline rejection.
 * \note            Task-only; reads facts but performs no MMIO store or borrow.
 *                  RS485 binding must use this provider's matching validator.
 */
nx_result_t nx_stm32_uart_tx_validate(void* context,
                                      const nx_uart_tx_request_t* request);

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
