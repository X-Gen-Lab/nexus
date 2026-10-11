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

#include "gd32f4xx.h"
#if !defined(NEXUS_TEST_GD32_CORE_CM4_H)
#include "nexus_config.h"
#endif

/** \brief Check the actual publisher before borrowing any notification target.
 */
static inline nx_result_t nx_gd32_irq_wake_validate(const nx_irq_wake_t* wake,
                                                    int irq,
                                                    uint8_t syscall_ceiling) {
#if defined(NEXUS_TEST_GD32_CORE_CM4_H)
    nx_irq_policy_t policy = {NX_IRQ_KERNEL_BASEPRI, 91U, 4U, 5U};
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
    const nx_irq_source_t source = {(int16_t)irq,
                                    (uint8_t)NVIC_GetPriority((IRQn_Type)irq),
                                    (uint8_t)NVIC_GetPriorityGrouping()};
    return nx_irq_wake_validate(wake, &policy, &source);
}

/** \brief Read-only SoC UART resource facts; Board pin wiring is separate. */
typedef struct {
    uint32_t registers;
    uint32_t clock_hz;
    uint32_t clock;
    uint32_t reset;
    int irq;
} nx_gd32_uart_controller_t;
/** \brief Read-only SoC SPI resource facts with one maintained clock domain. */
typedef struct {
    uint32_t registers;
    uint32_t clock_hz;
    uint32_t clock;
} nx_gd32_spi_controller_t;
/** \brief Read-only SoC I2C resource facts independent of external bus pins. */
typedef struct {
    uint32_t registers;
    uint32_t clock_hz;
    uint32_t clock;
} nx_gd32_i2c_controller_t;
extern const nx_gd32_uart_controller_t nx_gd32_usart0_controller;
extern const nx_gd32_uart_controller_t nx_gd32_usart1_controller;
extern const nx_gd32_spi_controller_t nx_gd32_spi0_controller;
extern const nx_gd32_spi_controller_t nx_gd32_spi4_controller;
extern const nx_gd32_i2c_controller_t nx_gd32_i2c0_controller;
extern const nx_gd32_i2c_controller_t nx_gd32_i2c1_controller;

/** \brief Fixed controller storage; callers do not mutate after assembly. */
typedef struct {
    uint32_t registers;
    uint32_t mask;
    bool output;
} nx_gd32_gpio_state_t;
typedef struct {
    const nx_gd32_uart_controller_t* controller;
    nx_uart_tx_request_t* active;
    const nx_irq_wake_t* wake;
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
} nx_gd32_uart_state_t;
typedef struct {
    const nx_gd32_spi_controller_t* controller;
    bool initialized;
    bool active;
} nx_gd32_spi_state_t;
typedef struct {
    nx_gd32_spi_state_t* port;
    uint32_t control;
    uint32_t cs_gpio;
    uint32_t cs_mask;
} nx_gd32_spi_endpoint_state_t;
typedef struct {
    const nx_gd32_i2c_controller_t* controller;
    uint32_t line_gpio;
    uint32_t line_mask;
    uint32_t bus_hz;
    bool initialized;
    bool active;
    bool faulted;
} nx_gd32_i2c_state_t;
typedef struct {
    nx_gd32_i2c_state_t* port;
    uint8_t address;
} nx_gd32_i2c_endpoint_state_t;
typedef struct {
    bool initialized;
    bool active;
} nx_gd32_flash_state_t;
typedef struct {
    nx_watchdog_state_t state;
    bool initialized;
} nx_gd32_watchdog_state_t;
typedef struct {
    nx_exti_event_t* events;
    const nx_irq_wake_t* wake;
    size_t capacity;
    size_t head;
    size_t count;
    uint32_t lost;
    uint8_t line;
    uint8_t gpio_index;
    nx_exti_edge_t edges;
    bool initialized;
} nx_gd32_exti_state_t;
typedef struct {
    nx_pwm_state_t state;
    bool initialized;
} nx_gd32_pwm_state_t;
typedef struct {
    const uint8_t* channels;
    size_t channel_count;
    uint32_t reference_mv;
    bool initialized;
    bool active;
} nx_gd32_adc_state_t;

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
nx_result_t nx_gd32_gpio_initialize(nx_gd32_gpio_state_t* port,
                                    unsigned gpio_index, uint32_t mask,
                                    uint32_t initial, bool output);
/**
 * \brief           Assemble USART0 PA9/PA10 IRQ 8N1 without hidden storage.
 * \param[out]      port: Fresh storage; one serialized executor.
 * \param[in]       baud: Baud rate between 1526 and 1000000.
 * \param[in]       profile: Byte/event ring or explicit block-only profile.
 * \param[in,out]   storage: Exclusive byte/event array matching profile.
 * \param[in]       capacity: Positive ring count, or NULL/zero for block mode.
 * \param[in]       priority: NVIC preemption priority 0 through 15.
 * \return          Success or INVALID/BUSY/CONTEXT. Block IRQs start
 * separately.
 */
nx_result_t nx_gd32_uart_initialize(nx_gd32_uart_state_t* port, uint32_t baud,
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
nx_result_t nx_gd32_spi_initialize(nx_gd32_spi_state_t* port,
                                   nx_gd32_spi_endpoint_state_t* endpoint,
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
nx_result_t nx_gd32_i2c_initialize(nx_gd32_i2c_state_t* port,
                                   nx_gd32_i2c_endpoint_state_t* endpoint,
                                   uint8_t address, uint32_t bus_hz);
/**
 * \brief           Validate observed physical 1 MiB density and lock FMC.
 * \param[out]      port: Fresh single-executor storage.
 * \return          Success or INVALID/IO/CONTEXT. No product reservation.
 */
nx_result_t nx_gd32_flash_initialize(nx_gd32_flash_state_t* port);
/**
 * \brief           Assemble watchdog storage without implicitly enabling it.
 * \param[out]      port: Fresh unique watchdog storage.
 * \return          Success or INVALID/BUSY; hardware option activation is
 * identified; external software enable must not bypass this unique provider.
 * IRC32K physical bounds remain 0..UINT32_MAX until product characterization.
 */
nx_result_t nx_gd32_watchdog_initialize(nx_gd32_watchdog_state_t* port);
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
nx_result_t nx_gd32_exti_initialize(nx_gd32_exti_state_t* port, unsigned line,
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
nx_result_t nx_gd32_pwm_initialize(nx_gd32_pwm_state_t* port,
                                   uint32_t period_ticks, uint32_t tick_hz);
/**
 * \brief           Release fixed timer ownership after inactive output setup.
 * \param[in,out]   port: Quiesced storage; no concurrent set/start operations.
 * \return          Success or INVALID/CONTEXT; success invalidates the port.
 * \note            Public PWM stop only stops the waveform and permits restart.
 *                  Platform teardown calls this private release before clocks.
 */
nx_result_t nx_gd32_pwm_release(nx_gd32_pwm_state_t* port);
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
 * \return          Success or INVALID/BUSY/TIMEOUT/IO/CONTEXT.
 *
 * \note            Channels and storage remain alive while used. Pin mode and
 *                  external wiring require reviewed routes; no DMA/trigger.
 *                  Startup exclusively owns the shared ADC0/1/2 reset domain;
 *                  all three clocks and external clock-gated owners must be
 *                  idle. An enabled ADC clock returns BUSY before mutation.
 *                  Failed cold startup leaves storage unpublished, powers ADC0
 *                  off and restores selected pin mode/pull, newly enabled GPIO
 *                  clocks and the incoming shared ADC configuration.
 */
nx_result_t nx_gd32_adc_initialize(nx_gd32_adc_state_t* port,
                                   const uint8_t* channels, size_t count,
                                   uint32_t reference_mv,
                                   nx_time_us_t deadline);
/**
 * \brief           Release the controller after all executors are quiescent.
 * \param[in,out]   port: Initialized caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; BUSY retains storage.
 */
nx_result_t nx_gd32_spi_stop(nx_gd32_spi_state_t* port);
/**
 * \brief           Release the controller after all executors are quiescent.
 * \param[in,out]   port: Initialized caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; BUSY retains storage.
 */
nx_result_t nx_gd32_i2c_stop(nx_gd32_i2c_state_t* port);
/**
 * \brief           Release the controller after all executors are quiescent.
 * \param[in,out]   port: Initialized caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; BUSY retains storage.
 */
nx_result_t nx_gd32_adc_stop(nx_gd32_adc_state_t* port);
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
nx_result_t nx_gd32_gpio_stop(nx_gd32_gpio_state_t* port, uint32_t initial);
/**
 * \brief           Release reversible effects after execution is quiescent.
 * \param[in,out]   port: Initialized unique caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; failure retains storage.
 */
nx_result_t nx_gd32_flash_stop(nx_gd32_flash_state_t* port);
/**
 * \brief           Release reversible effects after execution is quiescent.
 * \param[in,out]   port: Initialized unique caller-owned storage.
 * \return          Success or INVALID/BUSY/CONTEXT; failure retains storage.
 * \note            Enabled watchdog returns UNSUPPORTED: its effect cannot be
 *                  undone. Product must retain feed/reset responsibility.
 */
nx_result_t nx_gd32_watchdog_stop(nx_gd32_watchdog_state_t* port);

/** \brief Shared provider methods for statically assembled interfaces. */
extern const nx_gpio_ops_t nx_gd32_gpio_ops;
nx_result_t nx_gd32_gpio_write(void* context, uint32_t set_mask,
                               uint32_t reset_mask);
nx_result_t nx_gd32_gpio_read(const void* context, uint32_t* value);
nx_result_t nx_gd32_gpio_toggle(void* context, uint32_t mask);
extern const nx_uart_ops_t nx_gd32_uart_ops;
nx_result_t nx_gd32_uart_submit(void* context, nx_uart_tx_request_t* request);
nx_result_t nx_gd32_uart_start_admitted(void* context,
                                        nx_uart_tx_request_t* request);
nx_result_t nx_gd32_uart_cancel(void* context, nx_uart_tx_request_t* request);
void nx_gd32_uart_service(void* context);
nx_result_t nx_gd32_uart_read_events(void* context, nx_uart_rx_event_t* events,
                                     size_t capacity, size_t* count);
nx_result_t nx_gd32_uart_read_bytes(void* context, uint8_t* bytes,
                                    size_t capacity, size_t* count);
nx_result_t nx_gd32_uart_stop(void* context);
extern const nx_spi_ops_t nx_gd32_spi_ops;
nx_result_t nx_gd32_spi_recover(void* context);
extern const nx_spi_endpoint_ops_t nx_gd32_spi_endpoint_ops;
nx_result_t nx_gd32_spi_endpoint_transfer(void* context, const uint8_t* tx,
                                          uint8_t* rx, size_t length,
                                          nx_time_us_t deadline,
                                          size_t* transferred);
extern const nx_i2c_ops_t nx_gd32_i2c_ops;
nx_result_t nx_gd32_i2c_recover(void* context);
extern const nx_i2c_endpoint_ops_t nx_gd32_i2c_endpoint_ops;
nx_result_t nx_gd32_i2c_endpoint_transaction(void* context,
                                             nx_i2c_message_t* messages,
                                             size_t count,
                                             nx_time_us_t deadline,
                                             size_t* transferred);
extern const nx_flash_ops_t nx_gd32_flash_ops;
const nx_flash_geometry_t* nx_gd32_flash_geometry(const void* context);
nx_result_t nx_gd32_flash_read(const void* context, uint32_t offset, void* data,
                               size_t length);
nx_result_t nx_gd32_flash_program(void* context, uint32_t offset,
                                  const void* data, size_t length,
                                  nx_time_us_t deadline);
nx_result_t nx_gd32_flash_erase(void* context, uint32_t offset, size_t length,
                                nx_time_us_t deadline);
extern const nx_watchdog_ops_t nx_gd32_watchdog_ops;
nx_result_t nx_gd32_watchdog_enable(void* context, uint32_t timeout_us,
                                    bool debug_freeze,
                                    nx_watchdog_state_t* state);
nx_result_t nx_gd32_watchdog_feed(void* context);
nx_result_t nx_gd32_watchdog_state(const void* context,
                                   nx_watchdog_state_t* state);
extern const nx_exti_ops_t nx_gd32_exti_ops;
nx_result_t nx_gd32_exti_read(void* context, nx_exti_event_t* events,
                              size_t capacity, size_t* count);
nx_result_t nx_gd32_exti_stop(void* context);
extern const nx_pwm_ops_t nx_gd32_pwm_ops;
nx_result_t nx_gd32_pwm_set(void* context, uint32_t period_ticks,
                            uint32_t duty_ticks);
nx_result_t nx_gd32_pwm_start(void* context);
nx_result_t nx_gd32_pwm_stop(void* context);
nx_result_t nx_gd32_pwm_state(const void* context, nx_pwm_state_t* state);
extern const nx_adc_ops_t nx_gd32_adc_ops;
nx_result_t nx_gd32_adc_info(const void* context, nx_adc_info_t* info);
nx_result_t nx_gd32_adc_sample(void* context, uint16_t* samples,
                               size_t capacity, nx_time_us_t deadline,
                               size_t* count);

/** \brief Initialize a selected UART after Board AF wiring is prepared. */
nx_result_t
nx_gd32_uart_initialize_at(nx_gd32_uart_state_t* port,
                           const nx_gd32_uart_controller_t* controller,
                           uint32_t baud, nx_uart_rx_profile_t profile,
                           void* storage, size_t capacity, unsigned priority);
/** \brief Dispatch one exact generated UART binding without a registry. */
void nx_gd32_uart_irq(nx_gd32_uart_state_t* port);
/**
 * \brief           Latch TX/TC facts from one USART status read without RX.
 * \param[in,out]   port: Stable initialized provider with one IRQ writer.
 * \param[in]       status: Status read before any DATA access in this IRQ.
 * \return          True if a terminal fact needs the configured wake hint.
 * \note            No wake callback runs here; the enclosing IRQ signals once
 *                  after all metadata and peripheral work has completed.
 */
bool nx_gd32_uart_tx_irq(nx_gd32_uart_state_t* port, uint32_t status);
/** \brief Initialize a selected SPI after Board AF and inactive CS preparation.
 */
nx_result_t nx_gd32_spi_initialize_at(
    nx_gd32_spi_state_t* port, nx_gd32_spi_endpoint_state_t* endpoint,
    const nx_gd32_spi_controller_t* controller, uint32_t cs_gpio,
    uint32_t cs_mask, uint32_t clock_hz, unsigned mode);
/** \brief Initialize selected I2C after Board open-drain AF wiring preparation.
 */
nx_result_t nx_gd32_i2c_initialize_at(
    nx_gd32_i2c_state_t* port, nx_gd32_i2c_endpoint_state_t* endpoint,
    const nx_gd32_i2c_controller_t* controller, uint8_t address,
    uint32_t bus_hz, uint32_t line_gpio, uint32_t line_mask);

/** \brief Capture a single pin's electrical fields for cold-start rollback. */
uint16_t nx_gd32_pin_capture(uint32_t gpio, unsigned pin);
/** \brief Restore a captured pin without modifying neighboring fields. */
void nx_gd32_pin_restore(uint32_t gpio, unsigned pin, uint16_t state);
/** \brief Configure reviewed GPIO mode/AF/pull/type in a cold task path. */
nx_result_t nx_gd32_pin_configure(uint32_t gpio, unsigned pin, uint8_t mode,
                                  uint8_t af, uint8_t pull, bool open_drain);
/** \brief Enable exact GPIOA--GPIOI clock and prove the ordered readback. */
nx_result_t nx_gd32_gpio_clock_enable(unsigned index);

/** \brief Bind another SPI child in a cold quiescent controller assembly. */
nx_result_t nx_gd32_spi_endpoint_initialize(
    nx_gd32_spi_state_t* port, nx_gd32_spi_endpoint_state_t* endpoint,
    uint32_t cs_gpio, uint32_t cs_mask, uint32_t clock_hz, unsigned mode);

#endif
