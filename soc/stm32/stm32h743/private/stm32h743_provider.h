/**
 * \file            stm32h743_provider.h
 * \brief           Private GPIO and shared USART3 IRQ transmission bindings
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32H743_PROVIDER_H
#define NX_STM32H743_PROVIDER_H
#include "../../../../drivers/i2c/i2c_v2/i2c_v2.h"
#include "nexus/io/adc.h"
#include "nexus/io/exti.h"
#include "nexus/io/flash.h"
#include "nexus/io/gpio.h"
#include "nexus/io/spi.h"
#include "nexus/io/timer.h"
#include "nexus/io/uart.h"
#include "nexus/io/watchdog.h"
#include "stm32h743_system.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief DMA reachability also requires the boot-owned selected SRAM clock.
 * \note Range structure is validated by nx_dma_buffer_validate first. Default
 *       images omit this branch when SRAM1 has not been selected.
 */
static inline bool nx_stm32h743_dma_memory_powered(const void* data,
                                                   size_t bytes) {
#if (defined(NEXUS_MEMORY_SRAM1) && NEXUS_MEMORY_SRAM1 == 1) ||                \
    defined(NX_H743_MODEL)
    uintptr_t begin = (uintptr_t)data;
    if (begin < 0x30020000U && 0x30000000U < begin + bytes)
        return g_nx_stm32h743_system.rcc != NULL &&
               (NX_H743_READ(&g_nx_stm32h743_system.rcc->AHB2ENR) &
                RCC_AHB2ENR_SRAM1EN) != 0U;
#else
    (void)data;
    (void)bytes;
#endif
    return true;
}
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    bool output;
    bool initialized;
    bool retained;
} nx_stm32h743_gpio_state_t;
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
} nx_stm32h743_uart_state_t;
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
} nx_stm32h743_exti_state_t;
/** \brief Single-executor H7 v3 FIFO SPI, no DMA/IRQ or request borrowing. */
typedef struct {
    SPI_TypeDef* registers;
    nx_stm32h743_gpio_state_t* held_cs;
    uint32_t held_cs_mask;
    volatile uint32_t* reset;
    uint32_t reset_mask;
    uint32_t clock_hz;
    uint32_t poll_limit;
    bool initialized;
    bool retained;
    bool active;
    bool fault;
} nx_stm32h743_spi_state_t;
typedef struct {
    nx_stm32h743_spi_state_t* port;
    nx_stm32h743_gpio_state_t* cs;
    uint32_t cs_mask;
    uint32_t frequency_hz;
    uint8_t mode;
} nx_stm32h743_spi_endpoint_state_t;
extern const nx_spi_ops_t nx_stm32h743_spi_ops;
extern const nx_spi_endpoint_ops_t nx_stm32h743_spi_endpoint_ops;
/**
 * \brief Acquire a cold H743 V3 SPI instance with byte RX threshold.
 * \note The trusted static constructor already owns the exclusive GPIOE
 *       PE12/13/14 AF5 route and its clock through complete stop. Hand-built
 *       private callers owe the same lifetime and serial-executor contract;
 *       matching readable registers cannot grant a foreign lease. Selected
 *       CS remains driven high before GPIO SCK idle takeover and SPI reset.
 *       Transfer drain counts/copies complete late RX only within this call's
 *       submitted prefix; unsent TXFIFO contents are flushed without counting.
 *       An unproved active-frame drain cannot return its caller buffers.
 */
nx_result_t nx_stm32h743_spi_initialize(nx_stm32h743_spi_state_t* port);
/** \brief Configure a leased push-pull AF pin with no pull and readbacks. */
nx_result_t nx_stm32h743_pin_alternate(GPIO_TypeDef* gpio, unsigned pin,
                                       unsigned af);
#ifndef NX_H743_SPI_POLL
#define NX_H743_SPI_POLL(port) ((void)(port))
#endif
#ifndef NX_H743_SPI_READ8
#define NX_H743_SPI_READ8(reg) (*(volatile uint8_t*)(reg))
#endif
#ifndef NX_H743_SPI_WRITE8
#define NX_H743_SPI_WRITE8(reg, value) (*(volatile uint8_t*)(reg) = (value))
#endif
/** \brief One statically selected CH1 on exclusive TIM3 or TIM4. */
typedef struct {
    TIM_TypeDef* registers;
    nx_stm32h743_gpio_state_t* idle;
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
} nx_stm32h743_pwm_state_t;
extern const nx_pwm_ops_t nx_stm32h743_pwm_ops;
/** \brief Acquire a cold timer and safe GPIO; preserve partial cleanup leases.
 */
nx_result_t nx_stm32h743_pwm_initialize(nx_stm32h743_pwm_state_t* port);
/** \brief Prove controller reset and Board idle before releasing static state.
 */
nx_result_t nx_stm32h743_pwm_close(nx_stm32h743_pwm_state_t* port);
#ifndef NX_H743_PWM_POLL
#define NX_H743_PWM_POLL(port) ((void)(port))
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
} nx_stm32h743_watchdog_state_t;
extern const nx_watchdog_ops_t nx_stm32h743_watchdog_ops;
/** \brief Retry preactivation leases; enabled watchdogs remain irreversible. */
nx_result_t nx_stm32h743_watchdog_stop(nx_stm32h743_watchdog_state_t* port);
#ifndef NX_H743_WATCHDOG_POLL
#define NX_H743_WATCHDOG_POLL(port) ((void)(port))
#endif
extern const nx_exti_ops_t nx_stm32h743_exti_ops;
/**
 * \brief Acquire PA0 or PA1 input and its independent EXTI vector.
 * \param[in,out] port: Exact static state and nonoverlapping event storage.
 * \return SUCCESS or error; retained means stop must complete before reuse.
 * \note Privileged unmasked task only. Platform owns GPIO/SYSCFG gates and
 *       original GPIO lease. No pull or debounce policy is inferred.
 */
nx_result_t nx_stm32h743_exti_initialize(nx_stm32h743_exti_state_t* port);
/**
 * \brief Acknowledge one selected line and latch at most one coalesced fact.
 * \param[in,out] port: Live exact vector instance, storage and borrowed wake.
 * \note Pending may reassert during acknowledgement. This bounded handler
 *       never polls; edges can coalesce and must meet the caller's IRQ budget.
 */
void nx_stm32h743_exti_irq(nx_stm32h743_exti_state_t* port);
/** \brief H743 V2 layout is independently asserted in its actual port. */
typedef nx_i2c_v2_state_t nx_stm32h743_i2c_state_t;
/** \brief Prepare selected PB8/PB9 AF4 open drain after Platform pin lease. */
nx_result_t nx_stm32h743_i2c_pins_prepare(void);
/** \brief Bind a cold I2C1 owner, prepare its pads, then acquire the engine.
 * \param[in,out] port: Serialized runtime; retained owners reject rebinding.
 * \param[in] config: Static immutable I2C1 resources and reviewed timing.
 * \return Success/error; platform cleanup retains any partial pad effects.
 */
nx_result_t nx_stm32h743_i2c_prepare(nx_i2c_v2_state_t* port,
                                     const nx_i2c_v2_config_t* config);
extern const nx_gpio_ops_t nx_stm32h743_gpio_ops;
/** \brief One ADC1 PA3/INP15 conversion, no injected/continuous/DMA mode. */
typedef struct {
    ADC_TypeDef* registers;
    ADC_TypeDef* peer;
    ADC_Common_TypeDef* common;
    GPIO_TypeDef* gpio;
    uint32_t saved_common;
    uint32_t reference_mv;
    uint32_t supply_mv;
    uint32_t poll_limit;
    bool initialized;
    bool retained;
    bool active;
    bool fault;
} nx_stm32h743_adc_state_t;
extern const nx_adc_ops_t nx_stm32h743_adc_ops;
/** \brief Acquire cold ADC12 domain, regulator and offset/linearity
 * calibration. \note Privileged unmasked task; Board owns VDDA and analog
 * wiring. The returned counts do not establish calibrated physical voltage.
 */
nx_result_t nx_stm32h743_adc_initialize(nx_stm32h743_adc_state_t* port);
/** \brief Prove regular/injected stop and restore the acquired common domain.
 */
nx_result_t nx_stm32h743_adc_stop(nx_stm32h743_adc_state_t* port);
#ifndef NX_H743_ADC_POLL
#define NX_H743_ADC_POLL(port) ((void)(port))
#endif
/** \brief Exact two-bank physical Flash; no product reservations or partitions.
 */
typedef struct nx_stm32h743_flash_ram_workspace
    nx_stm32h743_flash_ram_workspace_t;
typedef struct {
    FLASH_TypeDef* registers;
    volatile uint8_t* memory;
    uint32_t supply_mv;
    uint32_t poll_limit;
    bool active;
    nx_stm32h743_flash_ram_workspace_t* execution;
} nx_stm32h743_flash_state_t;
extern const nx_flash_geometry_t g_nx_stm32h743_flash_geometry;
extern const nx_flash_ops_t nx_stm32h743_flash_ops;
/** \brief Optional cold owner of the shared Flash programming-delay field. */
typedef struct {
    uint32_t saved_acr;
    bool retained;
} nx_stm32h743_flash_timing_state_t;
/**
 * \brief Acquire programming timing for the exact fixed HSI32/HSI64 profile.
 * \param[in,out] state: Zeroed, exclusive static state selected with Flash.
 * \return SUCCESS or error; retained requires explicit successful stop.
 * \note Privileged unmasked task after system start and before consumers.
 *       Only ready LDO/VOS3, Cache-off, reset LATENCY7/WRHIGHFREQ0 and both
 *       idle banks are admitted. HSI64 acquires WRHIGHFREQ1; HSI32 needs no
 *       write or lease. A failed readback retains cleanup responsibility.
 */
nx_result_t
nx_stm32h743_flash_timing_initialize(nx_stm32h743_flash_timing_state_t* state);
/**
 * \brief Restore owned programming delay after all Flash consumers join.
 * \param[in,out] state: Same exclusive owner; live system remains available.
 * \return SUCCESS after release, or error with retained responsibility.
 * \note Both banks and pulse controls must be quiet. Preserve LATENCY7 and
 *       unrelated ACR fields; do not restore a saved unsafe read latency.
 *       Failed restoration is retryable. An already released owner returns
 *       without MMIO and cannot modify a later owner's hardware state.
 */
nx_result_t
nx_stm32h743_flash_timing_stop(nx_stm32h743_flash_timing_state_t* state);
/** \brief Stop succeeds only after both physical banks release all pulses. */
nx_result_t nx_stm32h743_flash_stop(nx_stm32h743_flash_state_t* port);
#ifndef NX_H743_FLASH_POLL
#define NX_H743_FLASH_POLL(port) ((void)(port))
#endif
#ifndef NX_H743_FLASH_READ8
#define NX_H743_FLASH_READ8(address) (*(const volatile uint8_t*)(address))
#endif
#ifndef NX_H743_FLASH_WRITE32
#define NX_H743_FLASH_WRITE32(address, value) (*(address) = (value))
#endif
extern const nx_uart_ops_t nx_stm32h743_uart_ops;
/** \brief Cold owner configures authorized pins after acquiring clocks. */
nx_result_t nx_stm32h743_gpio_initialize(nx_stm32h743_gpio_state_t* port,
                                         uint32_t initial);
/** \brief Preserve inactive output levels before withdrawing the face. */
nx_result_t nx_stm32h743_gpio_stop(nx_stm32h743_gpio_state_t* port,
                                   uint32_t inactive);
/** \brief Acquire a reviewed GPIO port clock with readback and barrier. */
nx_result_t nx_stm32h743_gpio_clock_enable(unsigned port_index);
/** \brief Source-reviewed PD8/PD9 AF7; caller owns Board bridge choice. */
nx_result_t nx_stm32h743_usart3_pins_prepare(void);
/**
 * \brief Configure bytes or ring-free blocks,8N1,PCLK/PRESC1; no FIFO/DMA.
 * \param[in,out] port: Exact static instance with unused hardware and ring.
 * \param[in] clock_hz: Verified 32MHz or 64MHz APB1 fixed profile.
 * \param[in] poll_limit: Nonzero bounded transmitter/receiver ACK budget.
 * \return Success or error; retained/closing expose failed hardware cleanup.
 * \note Privileged single task owner; byte mode IRQ moves at most one TX/RX
 *       byte. Block mode leaves RDR and RX sources to the selected provider.
 *       Ring and wake live until stop succeeds and external consumers join.
 */
nx_result_t nx_stm32h743_uart_initialize(nx_stm32h743_uart_state_t* port,
                                         uint32_t clock_hz,
                                         uint32_t poll_limit);
/** \brief Move at most one byte each direction and latch TC/error facts. */
void nx_stm32h743_uart_irq(nx_stm32h743_uart_state_t* port);
/** \brief Pure shared TX argument/alias validation; no admission or MMIO. */
nx_result_t nx_stm32h743_uart_tx_validate(nx_stm32h743_uart_state_t* port,
                                          nx_uart_tx_request_t* request);
/** \brief Shared admission; validated arguments and saved interrupt mask held.
 */
nx_result_t nx_stm32h743_uart_tx_start_locked(nx_stm32h743_uart_state_t* port,
                                              nx_uart_tx_request_t* request,
                                              bool admitted);
/** \brief Shared cancellation; non-NULL validated port/request and saved mask
 * held. */
nx_result_t nx_stm32h743_uart_tx_cancel_locked(nx_stm32h743_uart_state_t* port,
                                               nx_uart_tx_request_t* request);
/** \brief Prove wire and UART-source drain without releasing the request.
 * Caller holds its saved mask. True is valid only in this critical section,
 * with no intervening UART mutation. Modes recheck their domain before settle.
 */
bool nx_stm32h743_uart_tx_prepare_settle_locked(
    nx_stm32h743_uart_state_t* port);
/** \brief Final request access after successful preparation in the same mask.
 * Caller has proved its mode lifetime. Ordinary UART adds no dynamic guard. */
void nx_stm32h743_uart_tx_settle_locked(nx_stm32h743_uart_state_t* port);

/**
 * \brief Shared real-TC IRQ transmission; never reads RDR or RX status flags.
 * \note Selected provider holds the saved interrupt mask and initialized port.
 */
void nx_stm32h743_uart_transmit_irq(nx_stm32h743_uart_state_t* port,
                                    uint32_t status);
/** \brief Private bounded RX path shared by IRQ-byte and DMA TX providers. */
void nx_stm32h743_uart_receive_irq(nx_stm32h743_uart_state_t* port,
                                   uint32_t status);
/** \brief Publish an IRQ hint while accounting for a retained wake observer. */
void nx_stm32h743_uart_signal(nx_stm32h743_uart_state_t* port);
#ifndef NX_H743_UART_POLL
#define NX_H743_UART_POLL(port) ((void)(port))
#endif
#ifndef NX_H743_IRQ_PRIORITY
#define NX_H743_IRQ_PRIORITY(irq) NVIC_GetPriority((IRQn_Type)(irq))
#endif
#ifndef NX_H743_IRQ_GROUP
#define NX_H743_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif
#ifdef __cplusplus
}
#endif
#endif
