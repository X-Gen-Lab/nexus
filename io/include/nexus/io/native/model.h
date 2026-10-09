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
extern nx_gpio_port_t* const nx_native_gpio;
extern nx_uart_port_t* const nx_native_uart;
extern const nx_spi_endpoint_t* const nx_native_spi;
extern nx_i2c_port_t* const nx_native_i2c_port;
extern const nx_i2c_endpoint_t* const nx_native_i2c;
extern nx_flash_port_t* const nx_native_flash;
extern nx_watchdog_port_t* const nx_native_watchdog;
extern nx_exti_port_t* const nx_native_exti;
extern nx_pwm_port_t* const nx_native_pwm;
extern nx_adc_port_t* const nx_native_adc;
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
#ifdef __cplusplus
}
#endif

#endif
