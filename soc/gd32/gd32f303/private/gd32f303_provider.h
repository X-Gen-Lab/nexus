/**
 * \file            gd32f303_provider.h
 * \brief           Private GD32F303 static GPIO lease and USART0 IRQ bytes
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_GD32F303_PROVIDER_H
#define NX_GD32F303_PROVIDER_H
#include "gd32f303_devices.h"
#include "gd32f303_exti.h"
#include "gd32f303_system.h"
#include "nexus/io/gpio.h"
#include "nexus/io/spi.h"
#include "nexus/io/uart.h"
#include "nexus/io/watchdog.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Exact authorized static GPIO context; shared readonly ops. */
typedef struct {
    uint32_t mask;
    bool output;
    bool initialized;
    bool retained;
} nx_gd32f303_gpio_state_t;
/** \brief Official legacy USART register offsets remain private. */
typedef struct {
    volatile uint32_t STAT0;
    volatile uint32_t DATA;
    volatile uint32_t BAUD;
    volatile uint32_t CTL0;
    volatile uint32_t CTL1;
    volatile uint32_t CTL2;
    volatile uint32_t GP;
    volatile uint32_t reserved[25];
    volatile uint32_t CTL3;
    volatile uint32_t RT;
    volatile uint32_t STAT1;
} nx_gd32f303_uart_registers_t;
/** \brief Caller-bounded RX ring and TX borrow retained until SETTLED. */
typedef struct {
    nx_gd32f303_uart_registers_t* registers;
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
} nx_gd32f303_uart_state_t;
/** \brief Readonly selected USART0 byte-ring connections and IRQ policy. */
typedef struct {
    nx_gd32f303_uart_registers_t* registers;
    uint8_t* rx_storage;
    size_t rx_capacity;
    uint32_t baud;
    nx_irq_policy_t irq_policy;
} nx_gd32f303_uart_config_t;
/** \brief Exact v1 SPI0 registers, no FIFO or DMA contract inferred. */
typedef struct {
    volatile uint32_t CTL0;
    volatile uint32_t CTL1;
    volatile uint32_t STAT;
    volatile uint32_t DATA;
    volatile uint32_t CRCPOLY;
    volatile uint32_t RCRC;
    volatile uint32_t TCRC;
    volatile uint32_t I2SCTL;
    volatile uint32_t I2SPSC;
} nx_gd32f303_spi_registers_t;
typedef struct {
    nx_gd32f303_spi_registers_t* registers;
    nx_gd32f303_gpio_state_t* held_cs;
    uint32_t held_cs_mask;
    uint32_t clock_hz;
    uint32_t poll_limit;
    bool initialized;
    bool retained;
    bool active;
    bool fault;
} nx_gd32f303_spi_state_t;
typedef struct {
    nx_gd32f303_spi_state_t* port;
    nx_gd32f303_gpio_state_t* cs;
    uint32_t cs_mask;
    uint32_t frequency_hz;
    unsigned mode;
} nx_gd32f303_spi_endpoint_state_t;
extern const nx_spi_ops_t nx_gd32f303_spi_ops;
extern const nx_spi_endpoint_ops_t nx_gd32f303_spi_endpoint_ops;
/** \brief Bind reset-like SPI0 under Platform's exact clock/pin lease.
 * \param[in,out] port: Unique privileged unmasked task owner, bounded polls.
 * \return SUCCESS/error; retained hardware/CS effects require stop/recovery.
 */
nx_result_t nx_gd32f303_spi_initialize(nx_gd32f303_spi_state_t* port);
/**
 * \brief Bind cold SPI0 and its single maintained explicit PA4 CS owner.
 * \param[in,out] port: Unique cold controller with no retained execution.
 * \param[in,out] chip_select: Distinct cold static GPIO state under Platform.
 * \return SUCCESS/error; partial CS/controller effects require close retry.
 * \note Privileged unmasked task; Platform owns original pad/gate snapshots.
 */
nx_result_t nx_gd32f303_spi_prepare(nx_gd32f303_spi_state_t* port,
                                    nx_gd32f303_gpio_state_t* chip_select);
/**
 * \brief Join SPI0 before returning its sole selected CS owner inactive.
 * \param[in,out] port: Same controller after all transaction consumers join.
 * \param[in,out] chip_select: Same static selected PA4 owner.
 * \return SUCCESS after all withdrawals; failure retains unfinished owners.
 */
nx_result_t nx_gd32f303_spi_close(nx_gd32f303_spi_state_t* port,
                                  nx_gd32f303_gpio_state_t* chip_select);
/** \brief Prepare PA5/6/7 default SPI0 route after complete pin admission. */
nx_result_t nx_gd32f303_spi0_pins_prepare(void);
#ifndef NX_GD303_SPI_POLL
#define NX_GD303_SPI_POLL() ((void)0)
#endif
/** \brief Exclusive physical FWDGT owner; activation has no disable path. */
typedef struct {
    nx_watchdog_state_t state;
    uint32_t poll_limit;
    uint32_t saved_debug;
    bool initialized;
    bool preparation_retained;
    bool lsi_owned;
    bool debug_owned;
} nx_gd32f303_watchdog_state_t;
extern const nx_watchdog_ops_t nx_gd32f303_watchdog_ops;
/** \brief Observe hardware option effects without feeding or activating.
 * \param[in,out] port: Unique static cold owner with explicit poll budget.
 * \return SUCCESS or error; actual IRC40K physical bounds remain unknown.
 */
nx_result_t nx_gd32f303_watchdog_initialize(nx_gd32f303_watchdog_state_t* port);
/** \brief Bind an idle cold watchdog without reusing irreversible effects. */
nx_result_t nx_gd32f303_watchdog_prepare(nx_gd32f303_watchdog_state_t* port,
                                         uint32_t poll_limit);
/** \brief Undo only preactivation leases; never disable an enabled watchdog.
 * \param[in,out] port: Serialized privileged unmasked owner, consumers joined.
 * \return STATE while irreversible; IO retains failed debug/IRC40K cleanup.
 */
nx_result_t nx_gd32f303_watchdog_stop(nx_gd32f303_watchdog_state_t* port);
#ifndef NX_GD303_WATCHDOG_POLL
#define NX_GD303_WATCHDOG_POLL() ((void)0)
#endif
extern const nx_gpio_ops_t nx_gd32f303_gpio_ops;
extern const nx_uart_ops_t nx_gd32f303_uart_ops;
/** \brief Initialize PA4 or PA5 under an already-held Platform pin lease. */
nx_result_t nx_gd32f303_gpio_initialize(nx_gd32f303_gpio_state_t* port,
                                        uint32_t initial);
/** \brief Bind GPIOA selection only after retained cold ownership is clear. */
nx_result_t nx_gd32f303_gpio_prepare(nx_gd32f303_gpio_state_t* port,
                                     uint32_t mask, bool output,
                                     uint32_t initial);
/** \brief Apply explicit inactive latch; Platform later restores the lease. */
nx_result_t nx_gd32f303_gpio_stop(nx_gd32f303_gpio_state_t* port,
                                  uint32_t inactive);
/** \brief Set default USART0 PA9 AF push-pull/PA10 floating input route.
 * \return Task SUCCESS/error; Platform lease owns rollback and gate lifetime.
 */
nx_result_t nx_gd32f303_usart0_pins_prepare(void);
/** \brief Bind cold USART0 state and prepare its reviewed default pin route. */
nx_result_t nx_gd32f303_uart_prepare(nx_gd32f303_uart_state_t* port,
                                     const nx_gd32f303_uart_config_t* config);
/** \brief Configure exact USART0 8N1 at IRC8/APB2 8MHz, no DMA.
 * \param[in,out] port: Static context plus nonoverlapping explicit byte ring.
 * \param[in] clock_hz: Must equal verified 8000000Hz live system profile.
 * \param[in] poll_limit: Positive finite admission budget, no hidden wait.
 * \return SUCCESS/error; retained storage and failed cleanup remain owned.
 * \note Privileged unmasked task only. Accepted TX borrows survive timeout,
 *       cancel and failed drain. Consumers and callbacks join before stop.
 */
nx_result_t nx_gd32f303_uart_initialize(nx_gd32f303_uart_state_t* port,
                                        uint32_t clock_hz, uint32_t poll_limit);
/** \brief Move at most one TX/RX byte; notification only hints at predicates.
 */
void nx_gd32f303_uart_irq(nx_gd32f303_uart_state_t* port);
#ifdef __cplusplus
}
#endif
#endif
