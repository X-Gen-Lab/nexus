/**
 * \file            stm32f103_provider.h
 * \brief           Private static exact-part device bindings
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32F103_PROVIDER_H
#define NX_STM32F103_PROVIDER_H
#include "nexus/io/exti.h"
#include "nexus/io/gpio.h"
#include "nexus/io/spi.h"
#include "nexus/io/uart.h"
#include "nexus/io/watchdog.h"
#include "stm32f103_devices.h"
#include "stm32f103_system.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    bool output;
    bool initialized;
    bool retained;
} nx_stm32f103_gpio_state_t;
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
} nx_stm32f103_uart_state_t;
typedef struct {
    IWDG_TypeDef* registers;
    RCC_TypeDef* rcc;
    FLASH_TypeDef* flash;
    DBGMCU_TypeDef* debug;
    nx_watchdog_state_t state;
    uint32_t poll_limit;
    uint32_t saved_debug;
    bool lsi_owned;
    bool debug_owned;
    bool preparation_retained;
} nx_stm32f103_watchdog_state_t;
extern const nx_watchdog_ops_t nx_stm32f103_watchdog_ops;
/**
 * \brief Restore failed preactivation leases,never disable an active IWDG.
 * \param[in,out] port: Static privileged unmasked owner,consumers joined.
 * \return SUCCESS after readback,IO retains leases,STATE for active watchdog.
 */
nx_result_t nx_stm32f103_watchdog_stop(nx_stm32f103_watchdog_state_t* port);
#ifndef NX_F103_WATCHDOG_POLL
#define NX_F103_WATCHDOG_POLL(port) ((void)(port))
#endif
/** \brief Exact PA0/PA1 line state; storage and wake outlive successful stop.
 */
typedef struct {
    EXTI_TypeDef* registers;
    AFIO_TypeDef* mux;
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
} nx_stm32f103_exti_state_t;
extern const nx_exti_ops_t nx_stm32f103_exti_ops;
/**
 * \brief Acquire PA0 or PA1 input and its independent EXTI vector.
 * \param[in,out] port: Exact static state and nonoverlapping event storage.
 * \return SUCCESS or error; retained means stop must complete before reuse.
 * \note Privileged unmasked task only. Platform owns GPIO/AFIO gates and
 *       original GPIO lease. No pull or debounce policy is inferred.
 */
nx_result_t nx_stm32f103_exti_initialize(nx_stm32f103_exti_state_t* port);
/**
 * \brief Acknowledge one selected line and latch at most one coalesced fact.
 * \param[in,out] port: Live exact vector instance, storage and borrowed wake.
 * \note Pending may reassert during acknowledgement. This bounded handler
 *       never polls; edges can coalesce and must meet the caller's IRQ budget.
 */
void nx_stm32f103_exti_irq(nx_stm32f103_exti_state_t* port);
typedef struct {
    SPI_TypeDef* registers;
    RCC_TypeDef* rcc;
    uint32_t clock_hz;
    bool initialized;
    bool retained;
    bool active;
    bool fault;
} nx_stm32f103_spi_state_t;
typedef struct {
    nx_stm32f103_spi_state_t* port;
    nx_stm32f103_gpio_state_t* cs;
    uint32_t frequency_hz;
    uint32_t cs_mask;
    uint8_t mode;
} nx_stm32f103_spi_endpoint_state_t;
extern const nx_spi_ops_t nx_stm32f103_spi_ops;
extern const nx_spi_endpoint_ops_t nx_stm32f103_spi_endpoint_ops;
/**
 * \brief Initialize exact SPI1 defaultPA5/6/7,privileged cold owner only.
 * \param[in,out] port: Static idle state after platform gate/pin leases.
 * \return SUCCESS or error; retained state requires successful stop.
 * \note Polling only,no DMA or asynchronous buffer access.
 */
nx_result_t nx_stm32f103_spi_initialize(nx_stm32f103_spi_state_t* port);
#ifndef NX_F103_SPI_POLL
#define NX_F103_SPI_POLL(port) ((void)(port))
#endif
#ifndef NX_F103_SPI_WRITE_DATA
#define NX_F103_SPI_WRITE_DATA(regs, byte)                                     \
    (*(volatile uint8_t*)&(regs)->DR = (byte))
#define NX_F103_SPI_READ_DATA(regs) (*(volatile uint8_t*)&(regs)->DR)
#endif
extern const nx_gpio_ops_t nx_stm32f103_gpio_ops;
extern const nx_uart_ops_t nx_stm32f103_uart_ops;
/** \brief Configure leased PA4 or PA5,preloading before its CRL nibble. */
nx_result_t nx_stm32f103_gpio_initialize(nx_stm32f103_gpio_state_t* port,
                                         uint32_t initial);
/** \brief Apply declared inactive output before withdrawing the face. */
nx_result_t nx_stm32f103_gpio_stop(nx_stm32f103_gpio_state_t* port,
                                   uint32_t inactive);
/** \brief Prepare exact default PA2TX/PA3RX after platform pin/gate lease. */
nx_result_t nx_stm32f103_usart2_pins_prepare(void);
/**
 * \brief Configure USART2 8N1,oversample16,byte ring and finite IRQ setup.
 * \param[in,out] port: Exact static context,nonoverlapping ring storage.
 * \param[in] clock_hz: Exactly verified nominal8MHz APB1.
 * \param[in] poll_limit: Positive finite cold setup budget.
 * \return SUCCESS or error; retained means stop must succeed before reuse.
 * \note Privileged unmasked task. Accepted request/ring/wake storage outlive
 *       successful stop; cancel/deadline never release an active borrow.
 */
nx_result_t nx_stm32f103_uart_initialize(nx_stm32f103_uart_state_t* port,
                                         uint32_t clock_hz,
                                         uint32_t poll_limit);
/** \brief Bounded SR/DR read and at most one TX/RX byte; no owner callbacks. */
void nx_stm32f103_uart_irq(nx_stm32f103_uart_state_t* port);
#ifndef NX_F103_IRQ_PRIORITY
#define NX_F103_IRQ_PRIORITY(irq) NVIC_GetPriority((IRQn_Type)(irq))
#endif
#ifndef NX_F103_IRQ_GROUP
#define NX_F103_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif
#ifndef NX_F103_GPIOA
#define NX_F103_GPIOA GPIOA
#endif
#ifndef NX_F103_AFIO
#define NX_F103_AFIO AFIO
#endif
#ifdef __cplusplus
}
#endif
#endif
