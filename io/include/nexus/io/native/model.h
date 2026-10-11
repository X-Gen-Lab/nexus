/**
 * \file            model.h
 *
 * \brief           Explicit host-model fixtures; no physical MCU
 *                  qualification.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_MODEL_H
#define NEXUS_MODEL_H

#include "nexus/io/adc.h"
#include "nexus/io/exti.h"
#include "nexus/io/flash.h"
#include "nexus/io/gpio.h"
#include "nexus/io/i2c.h"
#include "nexus/io/spi.h"
#include "nexus/io/timer.h"
#include "nexus/io/uart.h"
#include "nexus/io/watchdog.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief           Fixed model bindings; no allocation, lookup, probe or
 *                  construction.
 */
extern const nx_gpio_port_t* const nx_native_gpio;
extern const nx_uart_port_t* const nx_native_uart;
extern const nx_spi_endpoint_t* const nx_native_spi;
extern const nx_i2c_port_t* const nx_native_i2c_port;
extern const nx_i2c_endpoint_t* const nx_native_i2c;
extern const nx_flash_port_t* const nx_native_flash;
extern const nx_watchdog_port_t* const nx_native_watchdog;
extern const nx_exti_port_t* const nx_native_exti;
extern const nx_pwm_port_t* const nx_native_pwm;
extern const nx_adc_port_t* const nx_native_adc;
/**
 * \brief           Switch host model to a deterministic clock domain.
 *
 * \param[in]       enabled: True selects manual clock, false real monotonic.
 *
 * \param[in]       now: Initial manual microsecond value, below UINT64_MAX.
 *
 * \return          Success or INVALID. Caller must first settle every request;
 *                  switching creates a new domain and invalidates old
 *                  deadlines.
 */
nx_result_t nx_native_clock_configure(bool enabled, nx_time_us_t now);
/**
 * \brief           Advance deterministic time without an implicit hardware
 *                  IRQ.
 *
 * \param[in]       duration_us: Positive or zero microsecond increment.
 *
 * \return          Success, STATE outside manual mode or EXHAUSTED on
 *                  overflow.
 */
nx_result_t nx_native_clock_advance(uint64_t duration_us);
/**
 * \brief           Configure the fixed GPIO model after writer quiescence.
 *
 * \param[in]       mask: Authorized input/output bit mask.
 *
 * \param[in]       initial: Initial output snapshot within mask.
 *
 * \return          Success or INVALID; no fake Board/electrical qualification.
 */
nx_result_t nx_native_gpio_configure(uint32_t mask, uint32_t initial);
/**
 * \brief           Inject one GPIO input snapshot in the deterministic model.
 *
 * \param[in]       value: Physical-port snapshot; read masks unauthorized
 *                  bits.
 */
void nx_native_gpio_input(uint32_t value);
/**
 * \brief           Observe output state for a host behavioral assertion.
 *
 * \return          Model output snapshot; no measured waveform semantics.
 */
uint32_t nx_native_gpio_output(void);
/**
 * \brief           UART storage belongs to the fixture until port stop and IRQ
 *                  quiescence.
 */
typedef struct {
    nx_uart_rx_profile_t profile;
    void* rx_storage;
    size_t rx_capacity;
    uint8_t* tx_log;
    size_t tx_capacity;
    bool automatic_irq;
} nx_native_uart_config_t;
/**
 * \brief           Configure exact RX/TX fixture capacities on the fixed UART.
 *
 * \param[in]       config: Caller-owned selected-profile storage description.
 *
 * \return          Success, INVALID or BUSY if an active request remains.
 */
nx_result_t nx_native_uart_configure(const nx_native_uart_config_t* config);
/**
 * \brief           Copy explicit IRQ facts into a quiescent host UART model.
 *
 * \param[in]       binding: Configured Native UART face with no IRQ users.
 *
 * \param[in]       policy: Immutable CPU/kernel facts copied during this call.
 *
 * \param[in]       source: Actual modeled publisher facts copied during call.
 *
 * \return          Success, INVALID, CONTEXT, STATE or BUSY before side
 * effects.
 *
 * \note            Task-only cold configuration; caller has stopped publishers.
 *                  No active request, stream or attached wake may remain.
 *                  Default preparation explicitly models BASEPRI/four-bit IRQs;
 *                  this entry can model Baseline without inventing a SoC/Board.
 */
nx_result_t nx_native_uart_model_irq_configure(const nx_uart_port_t* binding,
                                               const nx_irq_policy_t* policy,
                                               const nx_irq_source_t* source);

/**
 * \brief           Execute one deterministic TX IRQ byte/TC event.
 *
 * \note            No actual hardware IRQ, latency or wire timing is
 *                  simulated.
 */
void nx_native_uart_irq_step(void);
/**
 * \brief           Inject one RX IRQ fact into bounded selected-profile
 *                  storage.
 *
 * \param[in]       byte: Received byte; ignored for NO_BYTE status facts.
 *
 * \param[in]       flags: NX_UART_EVENT_* error facts; zero for normal
 *                  reception.
 *
 * \return          Success, OVERFLOW or STATE if disabled.
 */
nx_result_t nx_native_uart_receive(uint8_t byte, uint32_t flags);
/**
 * \brief           Configure start failure or inability to prove hardware
 *                  drain.
 *
 * \param[in]       start_failure: Fail after direct admission, not as
 *                  rejection.
 *
 * \param[in]       hold_drain: Retain QUARANTINED request until explicitly
 *                  released.
 */
void nx_native_uart_fault(bool start_failure, bool hold_drain);
/**
 * \brief           Observe captured TX bytes.
 *
 * \return          Number captured in caller-owned tx_log.
 */
size_t nx_native_uart_transmitted(void);
/**
 * \brief           Configure a fixed SPI echo or register-file model.
 *
 * \param[in,out]   registers: Optional caller-owned register bytes; NULL is
 *                  echo.
 *
 * \param[in]       length: Register capacity; zero only for echo mode.
 *
 * \return          Success or INVALID; fixture has one serialized executor.
 */
nx_result_t nx_native_spi_configure(uint8_t* registers, size_t length);
/**
 * \brief           Inject bounded SPI partial failure or controller occupancy.
 *
 * \param[in]       fail_after: Byte index to fail at, SIZE_MAX disables fault.
 *
 * \param[in]       busy: True rejects transactions without CS/buffer access.
 */
void nx_native_spi_fault(size_t fail_after, bool busy);
/**
 * \brief           Check the modeled CS level after a transaction.
 *
 * \return          True while model transaction owns active CS.
 */
bool nx_native_spi_cs_active(void);
/**
 * \brief           Configure one 7-bit-address register-file I2C model.
 *
 * \param[in,out]   memory: Caller-owned register bytes retained by fixture.
 *
 * \param[in]       length: Positive register capacity, at most 256.
 *
 * \param[in]       address: Valid nonreserved 7-bit address.
 *
 * \return          Success or INVALID.
 */
nx_result_t nx_native_i2c_configure(uint8_t* memory, size_t length,
                                    uint8_t address);
/**
 * \brief           Inject NACK, arbitration loss or a stuck bus.
 *
 * \param[in]       result: Success disables fault; NACK/ARBITRATION/IO
 *                  supported.
 *
 * \param[in]       stuck: True makes explicit recovery fail until released.
 */
void nx_native_i2c_fault(nx_result_t result, bool stuck);
/**
 * \brief           Configure physical-geometry RAM model, clearly
 *                  nonpersistent.
 *
 * \param[in,out]   memory: Caller-owned geometry-sized bytes; erased to 0xff.
 *
 * \param[in]       geometry: Caller-owned immutable contiguous sector
 *                  geometry.
 *
 * \return          Success or INVALID; no heap, product reserve or
 *                  persistence.
 */
nx_result_t nx_native_flash_configure(uint8_t* memory,
                                      const nx_flash_geometry_t* geometry);
/**
 * \brief           Inject program/erase failure after complete modeled pulses.
 *
 * \param[in]       pulse_count: Complete pulses allowed; SIZE_MAX disables
 *                  fault.
 */
void nx_native_flash_fault(size_t pulse_count);
/**
 * \brief           Reset the watchdog fixture as a simulated new boot.
 *
 * \param[in]       causes: Latched reset cause mask.
 *
 * \note            Test-only; production IWDG cannot be disabled by this
 *                  action.
 */
void nx_native_watchdog_boot(uint32_t causes);
/**
 * \brief           Observe expiration of the enabled deterministic IWDG.
 *
 * \return          True once its nominal model deadline expires; latches IWDG.
 */
bool nx_native_watchdog_expired(void);
/**
 * \brief           Configure exact storage for one static EXTI line.
 *
 * \param[in]       line: Zero through fifteen.
 *
 * \param[in]       edge: RISING/FALLING/BOTH selection.
 *
 * \param[in,out]   events: Fixture-owned queue storage.
 *
 * \param[in]       capacity: Positive event capacity.
 *
 * \return          Success or INVALID; quiesce producer/consumer before reset.
 */
nx_result_t nx_native_exti_configure(uint8_t line, nx_exti_edge_t edge,
                                     nx_exti_event_t* events, size_t capacity);
/**
 * \brief           Inject a timestamped selected edge fact.
 *
 * \param[in]       line: Configured static line.
 *
 * \param[in]       edge: RISING or FALLING occurrence.
 *
 * \return          Success, INVALID/STATE/OVERFLOW; loss remains observable.
 */
nx_result_t nx_native_exti_emit(uint8_t line, nx_exti_edge_t edge);
/**
 * \brief           Configure fixed PWM model timer facts after writer
 *                  quiescence.
 *
 * \param[in]       tick_hz: Positive post-prescaler frequency.
 *
 * \param[in]       period_ticks: Fixed shared-base positive period.
 *
 * \return          Success or INVALID.
 */
nx_result_t nx_native_pwm_configure(uint32_t tick_hz, uint32_t period_ticks);
/**
 * \brief           Configure fixed ADC raw-count sequence and format.
 *
 * \param[in]       samples: Fixture-owned nominal sample sequence.
 *
 * \param[in]       count: Positive channel count.
 *
 * \param[in]       resolution_bits: One through sixteen.
 *
 * \param[in]       reference_mv: Positive nominal reference voltage.
 *
 * \return          Success or INVALID.
 */
nx_result_t nx_native_adc_configure(const uint16_t* samples, size_t count,
                                    uint8_t resolution_bits,
                                    uint32_t reference_mv);
/**
 * \brief           Inject ADC failure after a prefix of complete samples.
 *
 * \param[in]       sample_count: Complete samples allowed; SIZE_MAX disables.
 */
void nx_native_adc_fault(size_t sample_count);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
void nx_native_gpio_model_input(const nx_gpio_port_t* binding, uint32_t value);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
uint32_t nx_native_gpio_model_output(const nx_gpio_port_t* binding);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
nx_result_t
nx_native_uart_model_configure(const nx_uart_port_t* binding,
                               const nx_native_uart_config_t* config);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
void nx_native_uart_model_irq_step(const nx_uart_port_t* binding);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
nx_result_t nx_native_uart_model_receive(const nx_uart_port_t* binding,
                                         uint8_t byte, uint32_t flags);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
void nx_native_uart_model_fault(const nx_uart_port_t* binding,
                                bool start_failure, bool hold_drain);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
size_t nx_native_uart_model_transmitted(const nx_uart_port_t* binding);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
nx_result_t nx_native_spi_model_configure(const nx_spi_endpoint_t* binding,
                                          uint8_t* registers, size_t length);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
void nx_native_spi_model_fault(const nx_spi_port_t* binding, size_t fail_after,
                               bool busy);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
bool nx_native_spi_model_cs_active(const nx_spi_endpoint_t* binding);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
nx_result_t nx_native_i2c_model_configure(const nx_i2c_endpoint_t* binding,
                                          uint8_t* memory, size_t length,
                                          uint8_t address);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
void nx_native_i2c_model_fault(const nx_i2c_port_t* binding, nx_result_t result,
                               bool stuck);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
nx_result_t
nx_native_flash_model_configure(const nx_flash_port_t* binding, uint8_t* memory,
                                const nx_flash_geometry_t* geometry);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
void nx_native_flash_model_fault(const nx_flash_port_t* binding,
                                 size_t pulse_count);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
void nx_native_watchdog_model_boot(const nx_watchdog_port_t* binding,
                                   uint32_t causes);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
bool nx_native_watchdog_model_expired(const nx_watchdog_port_t* binding);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
nx_result_t nx_native_exti_model_configure(const nx_exti_port_t* binding,
                                           uint8_t line, nx_exti_edge_t edge,
                                           nx_exti_event_t* events,
                                           size_t capacity);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
nx_result_t nx_native_exti_model_emit(const nx_exti_port_t* binding,
                                      uint8_t line, nx_exti_edge_t edge);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
nx_result_t nx_native_pwm_model_configure(const nx_pwm_port_t* binding,
                                          uint32_t tick_hz,
                                          uint32_t period_ticks);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
nx_result_t nx_native_adc_model_configure(const nx_adc_port_t* binding,
                                          const uint16_t* samples, size_t count,
                                          uint8_t resolution_bits,
                                          uint32_t reference_mv);
/**
 * \brief           Operate on the selected Native model face only.
 *
 * \note            Null or foreign-provider faces return INVALID for status
 *                  operations, zero/false for observations, and have no effect
 *                  for injections. Referenced fixture storage remains owned
 *                  by the caller; reconfiguration requires writer and IRQ
 *                  quiescence and never revokes a live UART borrow.
 */
void nx_native_adc_model_fault(const nx_adc_port_t* binding,
                               size_t sample_count);

/**
 * \brief           Choose explicit or automatic modeled SPI IRQ stepping.
 *
 * \note            Quiesce the controller first. Automatic service advances
 *                  at most one byte or a separate final idle fact per call;
 *                  this does not model physical DMA or wire timing.
 */
void nx_native_spi_model_async_configure(const nx_spi_port_t* binding,
                                         bool automatic_irq);

/** \brief Inject one bounded SPI byte/idle event on the exact Native bus. */
void nx_native_spi_model_irq_step(const nx_spi_port_t* binding);

/**
 * \brief           Inject inability to prove SPI hardware drain.
 *
 * \note            Retained requests become QUARANTINED until hold is released
 *                  and the executor services the bus again. No loan is revoked.
 */
void nx_native_spi_model_drain_hold(const nx_spi_port_t* binding, bool hold);

/**
 * \brief           Inject UART IDLE and publish its current valid byte prefix.
 *
 * \return          Success, EMPTY without a prefix, STATE outside a running
 *                  block stream, BUSY while its modeled writer is active, or
 *                  INVALID for a foreign provider. No producer loan is revoked.
 */
nx_result_t nx_native_uart_model_idle(const nx_uart_port_t* binding);

/**
 * \brief           Inject one ADC timer-trigger scan into a free selected
 * block.
 *
 * \return          Success, OVERFLOW while a required block remains held, IO
 *                  for a failed scan, STATE after stop, or INVALID for a
 * foreign provider. No hardware cadence or analog accuracy is claimed.
 *
 * \note            Each success publishes one interleaved raw-count scan;
 *                  partial failed scans are detached and never published.
 */
nx_result_t nx_native_adc_model_trigger(const nx_adc_port_t* binding);
#ifdef __cplusplus
}
#endif

#endif
