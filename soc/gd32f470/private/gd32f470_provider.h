/**
 * \file            gd32f470_provider.h
 * \brief           Explicit caller-owned GD32F470ZGT6 provider assembly storage
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_PROVIDER_H
#define NEXUS_GD32F470_PROVIDER_H
#include "nexus/io/adc.h"
#include "nexus/io/exti.h"
#include "nexus/io/flash.h"
#include "nexus/io/gpio.h"
#include "nexus/io/i2c.h"
#include "nexus/io/spi.h"
#include "nexus/io/timer.h"
#include "nexus/io/uart.h"
#include "nexus/io/watchdog.h"

/** \brief Fixed controller storage; callers do not mutate after assembly. */
struct nx_gpio_port {
    uint32_t registers;
    uint32_t mask;
    bool output;
};
struct nx_uart_port {
    nx_uart_tx_request_t* active;
    void* rx_storage;
    size_t rx_capacity;
    size_t rx_head;
    size_t rx_count;
    size_t tx_position;
    uint32_t baud;
    uint32_t losses;
    nx_time_us_t loss_timestamp;
    nx_uart_rx_profile_t profile;
    bool initialized;
    bool stopping;
    nx_result_t terminal;
    bool tc;
};
struct nx_spi_port {
    bool initialized;
    bool active;
};
struct nx_spi_endpoint {
    nx_spi_port_t* port;
    uint32_t control;
};
struct nx_i2c_port {
    uint32_t bus_hz;
    bool initialized;
    bool active;
    bool faulted;
};
struct nx_i2c_endpoint {
    nx_i2c_port_t* port;
    uint8_t address;
};
struct nx_flash_port {
    bool initialized;
    bool active;
};
struct nx_watchdog_port {
    nx_watchdog_state_t state;
    bool initialized;
};
struct nx_exti_port {
    nx_exti_event_t* events;
    size_t capacity;
    size_t head;
    size_t count;
    uint32_t lost;
    uint8_t line;
    uint8_t gpio_index;
    nx_exti_edge_t edges;
    bool initialized;
};
struct nx_pwm_port {
    nx_pwm_state_t state;
    bool initialized;
};
struct nx_adc_port {
    const uint8_t* channels;
    size_t channel_count;
    uint32_t reference_mv;
    bool initialized;
    bool active;
};

/**
 * \brief           Start fixed clocks and TIMER1 before provider assembly.
 * \return          Zero on success; negative retains diagnosed effects.
 */
int nx_gd32_soc_start(void);
/**
 * \brief           Stop clocks only after external owners/IRQs have quiesced.
 * \return          Zero on successful release; negative retains ownership.
 */
int nx_gd32_soc_stop(void);
/**
 * \brief           Bind a GPIO port index A=0 through I=8 and authorized mask.
 * \param[out]      port: Fresh caller storage kept alive while used.
 * \param[in]       gpio_index: Exact configured port index.
 * \param[in]       mask: Nonzero 16-bit authorized pin mask.
 * \param[in]       initial: Initial output bits, subset of mask.
 * \param[in]       output: Push-pull output or input with no pull.
 * \return          Success or INVALID/CONTEXT, with no bad-input writes.
 * \note            Startup-only, after SoC start; quiesce direct writers before
 *                  changing modes. Initial latch is loaded before output mode.
 */
nx_result_t nx_gd32_gpio_initialize(nx_gpio_port_t* port, unsigned gpio_index,
                                    uint32_t mask, uint32_t initial,
                                    bool output);
/**
 * \brief           Assemble USART0 PA9/PA10 IRQ 8N1 without hidden storage.
 * \param[out]      port: Fresh storage; one serialized executor.
 * \param[in]       baud: Baud rate between 1526 and 1000000.
 * \param[in]       profile: Byte or timestamped IRQ-observation event profile.
 * \param[in,out]   storage: Exclusive byte/event array matching profile.
 * \param[in]       capacity: Positive element count, kept until stopped.
 * \param[in]       priority: NVIC preemption priority 0 through 15.
 * \return          Success or INVALID/BUSY/CONTEXT. No DMA/block mode.
 */
nx_result_t nx_gd32_uart_initialize(nx_uart_port_t* port, uint32_t baud,
                                    nx_uart_rx_profile_t profile, void* storage,
                                    size_t capacity, unsigned priority);
/**
 * \brief           Bind SPI4 PF7/PF8/PF9 AF5 and active-low PF6 CS endpoint.
 * \param[out]      port: Fresh single-executor storage.
 * \param[out]      endpoint: Stable CS configuration storage.
 * \param[in]       clock_hz: Maximum requested SCK, realizable divided APB2.
 * \param[in]       mode: SPI mode 0 through 3, 8-bit MSB first.
 * \return          Success or INVALID/CONTEXT; actual SCK never exceeds
 * request.
 */
nx_result_t nx_gd32_spi_initialize(nx_spi_port_t* port,
                                   nx_spi_endpoint_t* endpoint,
                                   uint32_t clock_hz, unsigned mode);
/**
 * \brief           Bind I2C0 PB6/PB7 AF4 open-drain with external pullups.
 * \param[out]      port: Fresh single-executor storage.
 * \param[out]      endpoint: Fixed seven-bit address storage.
 * \param[in]       address: Seven-bit nonreserved address 8 through 119.
 * \param[in]       bus_hz: Maintained standard mode, 100000 Hz only.
 * \return          Success or INVALID/UNSUPPORTED/CONTEXT.
 * \note            Explicit external Board wiring required; no default Board
 *                  binding. Read messages are final, length 1 or 2; up to 4
 *                  messages support write/repeated START/final read.
 */
nx_result_t nx_gd32_i2c_initialize(nx_i2c_port_t* port,
                                   nx_i2c_endpoint_t* endpoint, uint8_t address,
                                   uint32_t bus_hz);
/**
 * \brief           Validate observed physical 1 MiB density and lock FMC.
 * \param[out]      port: Fresh single-executor storage.
 * \return          Success or INVALID/IO/CONTEXT. No product reservation.
 */
nx_result_t nx_gd32_flash_initialize(nx_flash_port_t* port);
/**
 * \brief           Assemble watchdog storage without implicitly enabling it.
 * \param[out]      port: Fresh unique watchdog storage.
 * \return          Success or INVALID/BUSY; hardware option activation is
 * identified; external software enable must not bypass this unique provider.
 * IRC32K physical bounds remain 0..UINT32_MAX until product characterization.
 */
nx_result_t nx_gd32_watchdog_initialize(nx_watchdog_port_t* port);
/**
 * \brief           Bind a unique EXTI line and caller-budgeted event queue.
 * \param[out]      port: Fresh storage, kept alive until successful stop.
 * \param[in]       line: Reviewed GPIO/EXTI line 0 through 15.
 * \param[in]       gpio_index: GPIO port A=0 through I=8.
 * \param[in]       edges: Rising, falling or both edges.
 * \param[in,out]   events: Exclusive event array.
 * \param[in]       capacity: Positive element count.
 * \param[in]       priority: Reviewed shared-vector priority 0 through 15.
 * \return          Success or INVALID/BUSY/CONTEXT. Shared priorities must
 * match.
 */
nx_result_t nx_gd32_exti_initialize(nx_exti_port_t* port, unsigned line,
                                    unsigned gpio_index, nx_exti_edge_t edges,
                                    nx_exti_event_t* events, size_t capacity,
                                    unsigned priority);
/**
 * \brief           Bind TIMER2 channel0 PA6 AF2, fixed up-counting PWM base.
 * \param[out]      port: Fresh single-writer storage.
 * \param[in]       period_ticks: Positive 16-bit period count.
 * \param[in]       tick_hz: Exact divider of the fixed 100 MHz timer clock.
 * \return          Success or INVALID/BUSY/CONTEXT. Channel starts inactive
 * low.
 *
 * \note            No capture, break, dead-time, DMA or runtime base switching.
 */
nx_result_t nx_gd32_pwm_initialize(nx_pwm_port_t* port, uint32_t period_ticks,
                                   uint32_t tick_hz);
/**
 * \brief           Release fixed timer ownership after inactive output setup.
 * \param[in,out]   port: Quiesced storage; no concurrent set/start operations.
 * \return          Success or INVALID/CONTEXT; success invalidates the port.
 * \note            Public PWM stop only stops the waveform and permits restart.
 *                  Platform teardown calls this private release before clocks.
 */
nx_result_t nx_gd32_pwm_release(nx_pwm_port_t* port);
/**
 * \brief           Calibrate ADC0 for fixed single or low-rate polling
 * sequence.
 *
 * \param[out]      port: Fresh single-executor storage.
 *
 * \param[in]       channels: Immutable reviewed external channels 0 through 15.
 *
 * \param[in]       count: Positive sequence length, at most 16.
 *
 * \param[in]       reference_mv: Explicit nominal positive Vref value.
 *
 * \param[in]       deadline: Absolute startup calibration deadline.
 *
 * \return          Success or INVALID/BUSY/TIMEOUT/CONTEXT.
 *
 * \note            Channels and storage remain alive while used. Pin mode and
 *                  external wiring require reviewed routes; no DMA/trigger.
 */
nx_result_t nx_gd32_adc_initialize(nx_adc_port_t* port, const uint8_t* channels,
                                   size_t count, uint32_t reference_mv,
                                   nx_time_us_t deadline);
/**
 * \brief           Release the controller after all executors are quiescent.
 * \param[in,out]   port: Initialized caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; BUSY retains storage.
 */
nx_result_t nx_gd32_spi_stop(nx_spi_port_t* port);
/**
 * \brief           Release the controller after all executors are quiescent.
 * \param[in,out]   port: Initialized caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; BUSY retains storage.
 */
nx_result_t nx_gd32_i2c_stop(nx_i2c_port_t* port);
/**
 * \brief           Release the controller after all executors are quiescent.
 * \param[in,out]   port: Initialized caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; BUSY retains storage.
 */
nx_result_t nx_gd32_adc_stop(nx_adc_port_t* port);
/** \brief Fixed USART0 vector; bounded payload moves and RX facts only. */
void USART0_IRQHandler(void);
/** \brief Dedicated EXTI0 vector. */
void EXTI0_IRQHandler(void);
/** \brief Dedicated EXTI1 vector. */
void EXTI1_IRQHandler(void);
/** \brief Dedicated EXTI2 vector. */
void EXTI2_IRQHandler(void);
/** \brief Dedicated EXTI3 vector. */
void EXTI3_IRQHandler(void);
/** \brief Dedicated EXTI4 vector. */
void EXTI4_IRQHandler(void);
/** \brief Bounded five-line shared vector. */
void EXTI5_9_IRQHandler(void);
/** \brief Bounded six-line shared vector. */
void EXTI10_15_IRQHandler(void);
/**
 * \brief           Release reversible effects after execution is quiescent.
 * \param[in,out]   port: Initialized unique caller-owned storage.
 * \param[in]       initial: Reviewed safe latch value, subset of binding mask.
 * \return          Success or INVALID/BUSY/CONTEXT; failure retains storage.
 */
nx_result_t nx_gd32_gpio_stop(nx_gpio_port_t* port, uint32_t initial);
/**
 * \brief           Release reversible effects after execution is quiescent.
 * \param[in,out]   port: Initialized unique caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; failure retains storage.
 */
nx_result_t nx_gd32_flash_stop(nx_flash_port_t* port);
/**
 * \brief           Release reversible effects after execution is quiescent.
 * \param[in,out]   port: Initialized unique caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; failure retains storage.
 * \note            Enabled watchdog returns UNSUPPORTED: its effect cannot be
 *                  undone. Product must retain feed/reset responsibility.
 */
nx_result_t nx_gd32_watchdog_stop(nx_watchdog_port_t* port);
#endif
