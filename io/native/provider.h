/**
 * \file            provider.h
 *
 * \brief           Private caller-owned Native state and shared operations.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-10
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_NATIVE_PROVIDER_H
#define NEXUS_NATIVE_PROVIDER_H

#include "nexus/io/native/model.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Apply an additional caller ceiling without weakening model facts. */
static inline nx_result_t nx_native_irq_wake_validate(
    const nx_irq_wake_t* wake, const nx_irq_policy_t* policy,
    const nx_irq_source_t* source, uint8_t syscall_ceiling) {
    if (policy == NULL) {
        return NX_ERROR_INVALID;
    }
    nx_irq_policy_t effective = *policy;
    if (wake != NULL && wake->calls_kernel) {
        if (effective.kernel == NX_IRQ_KERNEL_PRIMASK &&
            syscall_ceiling != 0U) {
            return NX_ERROR_INVALID;
        }
        if (effective.kernel == NX_IRQ_KERNEL_BASEPRI &&
            syscall_ceiling > effective.syscall_ceiling) {
            effective.syscall_ceiling = syscall_ceiling;
        }
    }
    return nx_irq_wake_validate(wake, &effective, source);
}

typedef struct nx_native_gpio_state {
    uint32_t mask;
    uint32_t input;
    uint32_t output;
    bool output_enabled;
} nx_native_gpio_state_t;

typedef struct nx_native_uart_state {
    nx_uart_tx_request_t* active;
    const nx_irq_wake_t* wake;
    nx_native_uart_config_t config;
    nx_irq_policy_t irq_policy;
    nx_irq_source_t irq_source;
    size_t tx_index;
    size_t logged;
    size_t rx_head;
    size_t rx_count;
    bool loss;
    nx_time_us_t loss_timestamp;
    bool terminal;
    bool start_failure;
    bool hold_drain;
    bool opened;
    bool stopping;
    nx_result_t terminal_result;
    nx_stream_t* rx_stream;
    nx_stream_fill_t rx_fill;
    size_t rx_block_length;
    bool rx_filling;
    bool rx_producing;
} nx_native_uart_state_t;

typedef struct nx_native_flash_state {
    uint8_t* memory;
    const nx_flash_geometry_t* geometry;
    size_t pulses_left;
    bool active;
    bool formatted;
} nx_native_flash_state_t;

typedef struct nx_native_watchdog_state {
    nx_watchdog_state_t state;
    uint32_t nominal_timeout_us;
    nx_time_us_t expiration;
    uint32_t causes;
    bool expired;
    bool initialized;
} nx_native_watchdog_state_t;

typedef struct nx_native_exti_state {
    nx_exti_event_t* events;
    const nx_irq_wake_t* wake;
    size_t capacity;
    size_t head;
    size_t count;
    uint8_t line;
    nx_exti_edge_t edge;
    bool opened;
    bool loss;
    nx_time_us_t loss_timestamp;
} nx_native_exti_state_t;

typedef struct nx_native_pwm_state {
    nx_pwm_state_t state;
    uint32_t base_period;
} nx_native_pwm_state_t;

typedef struct nx_native_adc_state {
    const uint16_t* samples;
    nx_adc_info_t info;
    size_t fail_after;
    bool active;
    nx_stream_t* stream;
    uint32_t trigger_hz;
} nx_native_adc_state_t;

/** \brief Controller owns arbitration and injected faults, not device memory.
 */
typedef struct nx_native_spi_state {
    const nx_irq_wake_t* wake;
    size_t fail_after;
    bool busy;
    bool active;
    bool opened;
    bool stopping;
    bool automatic_irq;
    bool terminal;
    bool hold_drain;
    bool register_read;
    uint8_t register_address;
    nx_result_t terminal_result;
    size_t transferred;
    nx_spi_request_t* active_request;
    struct nx_native_spi_endpoint_state* active_endpoint;
} nx_native_spi_state_t;

/** \brief Each CS endpoint owns independent memory and transaction facts. */
typedef struct nx_native_spi_endpoint_state {
    nx_native_spi_state_t* port;
    uint8_t* registers;
    size_t register_count;
    bool cs;
} nx_native_spi_endpoint_state_t;

/** \brief One I2C controller owns bus faults and serialized execution. */
typedef struct nx_native_i2c_state {
    nx_result_t fault;
    bool stuck;
    bool active;
    bool opened;
} nx_native_i2c_state_t;

/** \brief Register-file memory and cursor belong to the addressed device. */
typedef struct nx_native_i2c_endpoint_state {
    nx_native_i2c_state_t* port;
    uint8_t* memory;
    size_t size;
    uint32_t cursor;
    uint8_t address;
} nx_native_i2c_endpoint_state_t;

extern const nx_gpio_ops_t nx_native_gpio_ops;
extern const nx_uart_ops_t nx_native_uart_ops;
extern const nx_spi_ops_t nx_native_spi_ops;
extern const nx_i2c_ops_t nx_native_i2c_ops;
extern const nx_flash_ops_t nx_native_flash_ops;
extern const nx_watchdog_ops_t nx_native_watchdog_ops;
extern const nx_exti_ops_t nx_native_exti_ops;
extern const nx_pwm_ops_t nx_native_pwm_ops;
extern const nx_adc_ops_t nx_native_adc_ops;
extern const nx_spi_endpoint_ops_t nx_native_spi_endpoint_ops;
extern const nx_i2c_endpoint_ops_t nx_native_i2c_endpoint_ops;

/** \brief Explicit default fixtures; generated assemblies use their own state.
 */
extern const nx_gpio_port_t g_nx_native_gpio;
extern const nx_uart_port_t g_nx_native_uart;
extern const nx_flash_port_t g_nx_native_flash;
extern const nx_watchdog_port_t g_nx_native_watchdog;
extern const nx_exti_port_t g_nx_native_exti;
extern const nx_pwm_port_t g_nx_native_pwm;
extern const nx_adc_port_t g_nx_native_adc;
extern const nx_spi_port_t g_nx_native_spi_port;
extern const nx_spi_endpoint_t g_nx_native_spi;
extern const nx_i2c_port_t g_nx_native_i2c_port;
extern const nx_i2c_endpoint_t g_nx_native_i2c;

/**
 * \brief           Initialize exactly one zero-initialized static state.
 *
 * \note            Caller owns state and referenced storage for its lifetime.
 *                  Reconfiguration requires writer and IRQ quiescence. Live
 *                  asynchronous borrows reject UART reconfiguration as BUSY.
 *                  No constructor allocates or registers an instance.
 */
nx_result_t nx_native_gpio_configure_instance(nx_native_gpio_state_t* state,
                                              uint32_t mask, uint32_t initial,
                                              bool output_enabled);
/** \brief Restore inactive output and withdraw access after writer quiescence.
 */
nx_result_t nx_native_gpio_stop_instance(nx_native_gpio_state_t* state,
                                         uint32_t inactive);
void nx_native_gpio_input_instance(nx_native_gpio_state_t* state,
                                   uint32_t value);
uint32_t nx_native_gpio_output_instance(const nx_native_gpio_state_t* state);
nx_result_t
nx_native_uart_configure_instance(nx_native_uart_state_t* state,
                                  const nx_native_uart_config_t* config);
void nx_native_uart_irq_step_instance(nx_native_uart_state_t* state);
nx_result_t nx_native_uart_receive_instance(nx_native_uart_state_t* state,
                                            uint8_t byte, uint32_t flags);
void nx_native_uart_fault_instance(nx_native_uart_state_t* state,
                                   bool start_failure, bool hold_drain);
size_t nx_native_uart_transmitted_instance(const nx_native_uart_state_t* state);
nx_result_t nx_native_spi_port_configure_instance(nx_native_spi_state_t* state);
/** \brief Close an idle bus; active controller occupancy retains its state. */
nx_result_t nx_native_spi_stop_instance(nx_native_spi_state_t* state);
nx_result_t
nx_native_spi_configure_instance(nx_native_spi_endpoint_state_t* state,
                                 nx_native_spi_state_t* controller,
                                 uint8_t* registers, size_t length);
void nx_native_spi_fault_instance(nx_native_spi_state_t* state,
                                  size_t fail_after, bool busy);
bool nx_native_spi_cs_active_instance(
    const nx_native_spi_endpoint_state_t* state);
nx_result_t nx_native_i2c_port_configure_instance(nx_native_i2c_state_t* state);
/** \brief Close an idle bus without claiming physical stuck-bus recovery. */
nx_result_t nx_native_i2c_stop_instance(nx_native_i2c_state_t* state);
nx_result_t nx_native_i2c_configure_instance(
    nx_native_i2c_endpoint_state_t* state, nx_native_i2c_state_t* controller,
    uint8_t* memory, size_t length, uint8_t address);
void nx_native_i2c_fault_instance(nx_native_i2c_state_t* state,
                                  nx_result_t result, bool stuck);
nx_result_t
nx_native_flash_configure_instance(nx_native_flash_state_t* state,
                                   uint8_t* memory,
                                   const nx_flash_geometry_t* geometry);
/** \brief Bind static model memory, preserving bytes on platform restart. */
nx_result_t
nx_native_flash_initialize_instance(nx_native_flash_state_t* state,
                                    uint8_t* memory,
                                    const nx_flash_geometry_t* geometry);
void nx_native_flash_fault_instance(nx_native_flash_state_t* state,
                                    size_t pulse_count);
/** \brief Withdraw idle Flash geometry without erasing model memory. */
nx_result_t nx_native_flash_stop_instance(nx_native_flash_state_t* state);
void nx_native_watchdog_boot_instance(nx_native_watchdog_state_t* state,
                                      uint32_t causes);
/** \brief Initialize assembly access without simulating a new hardware boot. */
nx_result_t
nx_native_watchdog_initialize_instance(nx_native_watchdog_state_t* state);
bool nx_native_watchdog_expired_instance(nx_native_watchdog_state_t* state);
nx_result_t nx_native_exti_configure_instance(nx_native_exti_state_t* state,
                                              uint8_t line, nx_exti_edge_t edge,
                                              nx_exti_event_t* events,
                                              size_t capacity);
nx_result_t nx_native_exti_emit_instance(nx_native_exti_state_t* state,
                                         uint8_t line, nx_exti_edge_t edge);
nx_result_t nx_native_pwm_configure_instance(nx_native_pwm_state_t* state,
                                             uint32_t tick_hz,
                                             uint32_t period_ticks);
/** \brief Drive inactive output and withdraw the platform timer binding. */
nx_result_t nx_native_pwm_stop_instance(nx_native_pwm_state_t* state);
nx_result_t nx_native_adc_configure_instance(nx_native_adc_state_t* state,
                                             const uint16_t* samples,
                                             size_t count,
                                             uint8_t resolution_bits,
                                             uint32_t reference_mv);
void nx_native_adc_fault_instance(nx_native_adc_state_t* state,
                                  size_t sample_count);
/** \brief Release block producers and withdraw ADC admission after drain. */
nx_result_t nx_native_adc_stop_instance(nx_native_adc_state_t* state);

/** \brief Native finite async callbacks share the same explicit bus state. */
nx_result_t nx_native_spi_submit(void* context, nx_spi_request_t* request);
nx_result_t nx_native_spi_start_admitted(void* context,
                                         nx_spi_request_t* request);
nx_result_t nx_native_spi_cancel(void* context, nx_spi_request_t* request);
void nx_native_spi_service(void* context);
nx_result_t nx_native_spi_stop(void* context);
bool nx_native_spi_on_port(const void* context, const nx_spi_port_t* port);
nx_result_t nx_native_spi_attach_wake(void* context, const nx_irq_wake_t* wake,
                                      uint8_t syscall_ceiling);

/** \brief Native block callbacks retain exact caller-owned stream control. */
nx_result_t nx_native_uart_rx_start(void* context, nx_stream_t* stream);
nx_result_t nx_native_uart_rx_stop(void* context);
nx_result_t nx_native_uart_block_receive(nx_native_uart_state_t* state,
                                         uint8_t byte, uint32_t flags);
nx_result_t nx_native_adc_stream_start(void* context, nx_stream_t* stream,
                                       uint32_t trigger_hz);
nx_result_t nx_native_adc_stream_stop(void* context);
nx_result_t nx_native_adc_stream_service(void* context);

#ifdef __cplusplus
}
#endif
#endif
