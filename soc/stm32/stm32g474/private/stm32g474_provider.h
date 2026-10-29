/**
 * \file            stm32g474_provider.h
 * \brief           Private static GPIO, LPUART1 bytes and exact EXTI bindings
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32G474_PROVIDER_H
#define NX_STM32G474_PROVIDER_H
#include "../../../../drivers/i2c/i2c_v2/i2c_v2.h"
#include "nexus/io/exti.h"
#include "nexus/io/gpio.h"
#include "nexus/io/spi.h"
#include "nexus/io/timer.h"
#include "nexus/io/uart.h"
#include "nexus/io/watchdog.h"
#include "stm32g474_system.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Configure reviewed PB8/PB9 AF4 open drain; Board owns pull-ups. */
nx_result_t nx_stm32g474_i2c_pins_prepare(void);
/** \brief Bind a cold I2C1 owner, prepare its pads, then acquire the engine.
 * \param[in,out] port: Serialized runtime; retained owners reject rebinding.
 * \param[in] config: Static immutable I2C1 resources and reviewed timing.
 * \return Success/error; platform cleanup retains any partial pad effects.
 */
nx_result_t nx_stm32g474_i2c_prepare(nx_i2c_v2_state_t* port,
                                     const nx_i2c_v2_config_t* config);
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    bool output;
    bool initialized;
    bool retained;
} nx_stm32g474_gpio_state_t;
typedef struct {
    USART_TypeDef* registers;
    const nx_irq_wake_t* wake;
    nx_uart_tx_request_t* active;
    void* rx_storage;
    size_t rx_capacity;
    size_t rx_head;
    size_t rx_tail;
    size_t rx_count;
    size_t tx_position;
    uint32_t rx_loss;
    uint32_t baud;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    nx_irq_policy_t irq_policy;
    IRQn_Type irq;
    nx_uart_rx_profile_t profile;
    uint16_t wake_publishers;
    bool tx_complete;
    bool closing;
    bool initialized;
    bool retained;
} nx_stm32g474_uart_state_t;
/** \brief Exact PA0/PA1 line state; storage and wake outlive successful stop.
 */
typedef struct {
    EXTI_TypeDef* registers;
    SYSCFG_TypeDef* mux;
    GPIO_TypeDef* gpio;
    nx_exti_event_t* storage;
    const nx_irq_wake_t* wake;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    nx_irq_policy_t irq_policy;
    nx_exti_edge_t edge;
    uint16_t wake_publishers;
    uint8_t line;
    uint8_t priority;
    uint8_t saved_mux;
    bool initialized;
    bool retained;
    bool closing;
    bool loss;
} nx_stm32g474_exti_state_t;
/** \brief Single-executor G4 FIFO SPI, no DMA/IRQ or request borrowing. */
typedef struct {
    SPI_TypeDef* registers;
    nx_stm32g474_gpio_state_t* held_cs;
    uint32_t held_cs_mask;
    volatile uint32_t* reset;
    uint32_t reset_mask;
    uint32_t clock_hz;
    uint32_t poll_limit;
    bool initialized;
    bool retained;
    bool active;
    bool fault;
} nx_stm32g474_spi_state_t;
typedef struct {
    nx_stm32g474_spi_state_t* port;
    nx_stm32g474_gpio_state_t* cs;
    uint32_t cs_mask;
    uint32_t frequency_hz;
    uint8_t mode;
} nx_stm32g474_spi_endpoint_state_t;
extern const nx_spi_ops_t nx_stm32g474_spi_ops;
extern const nx_spi_endpoint_ops_t nx_stm32g474_spi_endpoint_ops;
/** \brief Acquire a cold FIFO-empty G4 SPI instance with byte RX threshold. */
nx_result_t nx_stm32g474_spi_initialize(nx_stm32g474_spi_state_t* port);
/** \brief Configure a leased push-pull AF pin with no pull and readbacks. */
nx_result_t nx_stm32g474_pin_alternate(GPIO_TypeDef* gpio, unsigned pin,
                                       unsigned af);
#ifndef NX_G474_SPI_POLL
#define NX_G474_SPI_POLL(port) ((void)(port))
#endif
#ifndef NX_G474_SPI_READ8
#define NX_G474_SPI_READ8(reg) (*(volatile uint8_t*)(reg))
#endif
#ifndef NX_G474_SPI_WRITE8
#define NX_G474_SPI_WRITE8(reg, value) (*(volatile uint8_t*)(reg) = (value))
#endif
/** \brief One statically selected CH1 on exclusive TIM3 or TIM4. */
typedef struct {
    TIM_TypeDef* registers;
    nx_stm32g474_gpio_state_t* idle;
    volatile uint32_t* reset;
    uint32_t reset_mask;
    uint32_t inactive;
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint32_t poll_limit;
    uint8_t pin;
    bool initialized;
    bool retained;
    bool running;
    bool fault;
} nx_stm32g474_pwm_state_t;
extern const nx_pwm_ops_t nx_stm32g474_pwm_ops;
/** \brief Acquire a cold timer and safe GPIO; preserve partial cleanup leases.
 */
nx_result_t nx_stm32g474_pwm_initialize(nx_stm32g474_pwm_state_t* port);
/** \brief Prove controller reset and Board idle before releasing static state.
 */
nx_result_t nx_stm32g474_pwm_close(nx_stm32g474_pwm_state_t* port);
#ifndef NX_G474_PWM_POLL
#define NX_G474_PWM_POLL(port) ((void)(port))
#endif
/** \brief Irreversible IWDG state; image owns the feed policy externally. */
typedef struct {
    IWDG_TypeDef* registers;
    RCC_TypeDef* rcc;
    FLASH_TypeDef* flash;
    DBGMCU_TypeDef* debug;
    uint32_t poll_limit;
    uint32_t saved_debug;
    nx_watchdog_state_t state;
    bool lsi_added;
    bool debug_pending;
} nx_stm32g474_watchdog_state_t;
extern const nx_watchdog_ops_t nx_stm32g474_watchdog_ops;
/** \brief Retry preactivation leases; enabled watchdogs remain irreversible. */
nx_result_t nx_stm32g474_watchdog_stop(nx_stm32g474_watchdog_state_t* port);
#ifndef NX_G474_WATCHDOG_POLL
#define NX_G474_WATCHDOG_POLL(port) ((void)(port))
#endif
extern const nx_exti_ops_t nx_stm32g474_exti_ops;
/**
 * \brief Acquire PA0 or PA1 input and its independent EXTI vector.
 * \param[in,out] port: Exact static state and nonoverlapping event storage.
 * \return SUCCESS or error; retained means stop must complete before reuse.
 * \note Privileged unmasked task only. Platform owns GPIO/SYSCFG gates and
 *       original GPIO lease. No pull or debounce policy is inferred.
 */
nx_result_t nx_stm32g474_exti_initialize(nx_stm32g474_exti_state_t* port);
/**
 * \brief Acknowledge one selected line and latch at most one coalesced fact.
 * \param[in,out] port: Live exact vector instance, storage and borrowed wake.
 * \note Pending may reassert during acknowledgement. This bounded handler
 *       never polls; edges can coalesce and must meet the caller's IRQ budget.
 */
void nx_stm32g474_exti_irq(nx_stm32g474_exti_state_t* port);
extern const nx_gpio_ops_t nx_stm32g474_gpio_ops;
extern const nx_uart_ops_t nx_stm32g474_uart_ops;
/** \brief Cold owner configures authorized pins after acquiring clocks. */
nx_result_t nx_stm32g474_gpio_initialize(nx_stm32g474_gpio_state_t* port,
                                         uint32_t initial);
/** \brief Preserve inactive output levels before withdrawing the face. */
nx_result_t nx_stm32g474_gpio_stop(nx_stm32g474_gpio_state_t* port,
                                   uint32_t inactive);
/** \brief Acquire a reviewed GPIO port clock with readback and barrier. */
nx_result_t nx_stm32g474_gpio_clock_enable(unsigned port_index);
/** \brief Prepare exact reviewed TX/RX pins after their Platform lease. */
nx_result_t nx_stm32g474_uart_pins_prepare(GPIO_TypeDef* tx, unsigned tx_pin,
                                           GPIO_TypeDef* rx, unsigned rx_pin,
                                           unsigned af);
/** \brief Source-reviewed PA2/PA3 AF12; caller owns Board bridge choice. */
nx_result_t nx_stm32g474_lpuart1_pins_prepare(void);
/**
 * \brief Configure explicit byte ring,8N1,APB170; no FIFO/DMA.
 * \param[in,out] port: Exact static instance with unused hardware and ring.
 * \param[in] clock_hz: Verified170MHz APB1 fixed profile.
 * \param[in] poll_limit: Nonzero bounded transmitter/receiver ACK budget.
 * \return Success or error; retained/closing expose failed hardware cleanup.
 * \note Privileged single task owner; IRQ only moves one TX and one RX byte.
 *       Ring and wake live until stop succeeds and external consumers join.
 *       LPUART1 uses PRESC16; USART1/2/3 and UART4/5 use PRESC1/OVER16.
 */
nx_result_t nx_stm32g474_uart_initialize(nx_stm32g474_uart_state_t* port,
                                         uint32_t clock_hz,
                                         uint32_t poll_limit);
/** \brief Move at most one byte each direction and latch TC/error facts. */
void nx_stm32g474_uart_irq(nx_stm32g474_uart_state_t* port);
#ifndef NX_G474_UART_POLL
#define NX_G474_UART_POLL(port) ((void)(port))
#endif
#ifndef NX_G474_IRQ_PRIORITY
#define NX_G474_IRQ_PRIORITY(irq) NVIC_GetPriority((IRQn_Type)(irq))
#endif
#ifndef NX_G474_IRQ_GROUP
#define NX_G474_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif
/** \brief Immutable GPIO binding; Platform owns its original pad lease. */
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    uint32_t initial;
    bool output;
} nx_stm32g474_gpio_config_t;
/** \brief Bind a fresh GPIO face without changing a retained epoch.
 * \param[in,out] port: Static zeroed or successfully stopped state.
 * \param[in] config: Reviewed static port, mask, direction and initial latch.
 * \return Success or error; failed effects remain for Platform cleanup.
 */
nx_result_t nx_stm32g474_gpio_prepare(nx_stm32g474_gpio_state_t* port,
                                      const nx_stm32g474_gpio_config_t* config);
/** \brief Immutable UART/ring and exact reviewed TX/RX connection. */
typedef struct {
    USART_TypeDef* registers;
    void* rx_storage;
    size_t rx_capacity;
    uint32_t baud;
    IRQn_Type irq;
    nx_irq_policy_t irq_policy;
    GPIO_TypeDef* tx_gpio;
    GPIO_TypeDef* rx_gpio;
    uint8_t tx_pin;
    uint8_t rx_pin;
    uint8_t af;
} nx_stm32g474_uart_config_t;
/** \brief Bind one static byte ring, prepare pads and acquire its UART.
 * \param[in,out] port: Unretained state; ring/wake observers have joined.
 * \param[in] config: Immutable ring and physical route; no overlapping state.
 * \return Success or error, preserving any partial effects for stop/retry.
 */
nx_result_t nx_stm32g474_uart_prepare(nx_stm32g474_uart_state_t* port,
                                      const nx_stm32g474_uart_config_t* config);
/** \brief Immutable AF connection, never a register action stream. */
typedef struct {
    GPIO_TypeDef* gpio;
    uint8_t pin;
    uint8_t af;
} nx_stm32g474_alternate_t;
/** \brief Static endpoint storage and its reviewed active-low CS/rate. */
typedef struct {
    nx_stm32g474_spi_endpoint_state_t* endpoint;
    nx_stm32g474_gpio_state_t* cs;
    GPIO_TypeDef* gpio;
    uint32_t mask;
    uint32_t frequency_hz;
    uint8_t mode;
} nx_stm32g474_spi_device_config_t;
/** \brief One exact SPI instance and its statically selected endpoints. */
typedef struct {
    SPI_TypeDef* registers;
    volatile uint32_t* reset;
    uint32_t reset_mask;
    const nx_stm32g474_alternate_t* pins;
    size_t pin_count;
    const nx_stm32g474_spi_device_config_t* devices;
    size_t device_count;
} nx_stm32g474_spi_config_t;
/** \brief Acquire the cold SPI and prepare its endpoints and AF connections.
 * \param[in,out] port: Static unretained physical state.
 * \param[in] config: Static immutable endpoint/pad records, all storage live.
 * \return Success or error; Platform cleanup owns partial pads and endpoints.
 */
nx_result_t nx_stm32g474_spi_prepare(nx_stm32g474_spi_state_t* port,
                                     const nx_stm32g474_spi_config_t* config);
/** \brief Drain the SPI before withdrawing every acquired endpoint CS.
 * \param[in,out] port: Same serialized physical state.
 * \param[in] config: Original static config and endpoint state.
 * \return Success only after all selected CS sources are withdrawn.
 */
nx_result_t nx_stm32g474_spi_close(nx_stm32g474_spi_state_t* port,
                                   const nx_stm32g474_spi_config_t* config);
/** \brief Immutable fixed PWM base and external inactive-level owner. */
typedef struct {
    TIM_TypeDef* registers;
    nx_stm32g474_gpio_state_t* idle;
    GPIO_TypeDef* gpio;
    volatile uint32_t* reset;
    uint32_t reset_mask;
    uint32_t inactive;
    uint32_t period_ticks;
    uint32_t duty_ticks;
    uint32_t tick_hz;
    uint8_t pin;
} nx_stm32g474_pwm_config_t;
/** \brief Bind the cold timer and its Board-defined inactive GPIO lease.
 * \param[in,out] port: Static unretained timer state.
 * \param[in] config: Immutable timer, pad and exact initial tick geometry.
 * \return Success/error; close retains responsibility for partial effects.
 */
nx_result_t nx_stm32g474_pwm_prepare(nx_stm32g474_pwm_state_t* port,
                                     const nx_stm32g474_pwm_config_t* config);
/** \brief Immutable EXTI storage, line, edge and kernel IRQ policy. */
typedef struct {
    nx_exti_event_t* storage;
    size_t capacity;
    nx_irq_policy_t irq_policy;
    nx_exti_edge_t edge;
    uint8_t line;
    uint8_t priority;
} nx_stm32g474_exti_config_t;
/** \brief Bind fresh event storage before acquiring the selected EXTI source.
 * \param[in,out] port: Static state with no wake/publisher/retained borrow.
 * \param[in] config: Immutable reviewed PA0/PA1 connection and event ring.
 * \return Success/error; public stop drains any retained partial effects.
 */
nx_result_t nx_stm32g474_exti_prepare(nx_stm32g474_exti_state_t* port,
                                      const nx_stm32g474_exti_config_t* config);
/** \brief Bind a fresh, still disabled irreversible IWDG owner.
 * \param[in,out] port: Static state; successful stop joined prior consumers.
 * \return Success or busy/invalid; no watchdog activation occurs here.
 */
nx_result_t nx_stm32g474_watchdog_prepare(nx_stm32g474_watchdog_state_t* port);
#ifdef __cplusplus
}
#endif
#endif
