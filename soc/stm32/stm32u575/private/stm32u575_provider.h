/**
 * \file            stm32u575_provider.h
 * \brief           Private allocation-free GPIO and USART1 IRQ-byte bindings
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32U575_PROVIDER_H
#define NX_STM32U575_PROVIDER_H
#include "../../../../drivers/i2c/i2c_v2/i2c_v2.h"
#include "nexus/io/adc.h"
#include "nexus/io/exti.h"
#include "nexus/io/flash.h"
#include "nexus/io/gpio.h"
#include "nexus/io/spi.h"
#include "nexus/io/timer.h"
#include "nexus/io/uart.h"
#include "nexus/io/watchdog.h"
#include "stm32u575_system.h"
#ifndef NX_U575_GPIOG
#define NX_U575_GPIOG GPIOG
#endif
#ifndef NX_U575_PWR
#define NX_U575_PWR PWR
#endif
#ifndef NX_U575_RCC
#define NX_U575_RCC RCC
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    GPIO_TypeDef* registers;
    uint32_t mask;
    bool output;
    bool initialized;
    bool retained;
} nx_stm32u575_gpio_state_t;
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
} nx_stm32u575_uart_state_t;
typedef struct {
    IWDG_TypeDef* registers;
    RCC_TypeDef* rcc;
    FLASH_TypeDef* flash;
    DBGMCU_TypeDef* debug;
    uint32_t poll_limit;
    nx_watchdog_state_t state;
    bool lsi_added;
    bool debug_pending;
    uint32_t previous_debug;
} nx_stm32u575_watchdog_state_t;
/** \brief Fixed TIM3 CH1 PA6/AF2; one exclusive timer and safe idle pin. */
typedef struct {
    TIM_TypeDef* registers;
    nx_stm32u575_gpio_state_t* idle;
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
} nx_stm32u575_pwm_state_t;
extern const nx_pwm_ops_t nx_stm32u575_pwm_ops;
/** \brief Acquire the admitted fixed TIM3 base with a safe GPIO output. */
nx_result_t nx_stm32u575_pwm_initialize(nx_stm32u575_pwm_state_t* port);
/** \brief Observe reset and safe GPIO before releasing partial ownership. */
nx_result_t nx_stm32u575_pwm_close(nx_stm32u575_pwm_state_t* port);
/** \brief Configure a leased exact-package alternate pin without a registry. */
nx_result_t nx_stm32u575_pin_alternate(GPIO_TypeDef* gpio, unsigned pin,
                                       unsigned af);
#ifndef NX_U575_PWM_POLL
#define NX_U575_PWM_POLL(port) ((void)(port))
#endif
typedef nx_i2c_v2_state_t nx_stm32u575_i2c_state_t;
typedef nx_i2c_v2_endpoint_state_t nx_stm32u575_i2c_endpoint_state_t;
/** \brief Separate cold clock ownership keeps the actual V2 hot context. */
typedef struct {
    nx_i2c_v2_state_t* port;
    bool retained;
    bool source_added;
    bool mux_owned;
} nx_stm32u575_i2c_clock_t;
/** \brief Acquire unused HSI16 and I2C1SEL without changing SYSCLK or TIM2.
 * \note Failed acquisition retains cleanup ownership until clock_stop.
 */
nx_result_t nx_stm32u575_i2c_clock_start(nx_stm32u575_i2c_clock_t* clock,
                                         nx_i2c_v2_state_t* port);
/** \brief Restore selector and source only after the V2 port has joined. */
nx_result_t nx_stm32u575_i2c_clock_stop(nx_stm32u575_i2c_clock_t* clock);
/** \brief Admit I2C1/HSI16, finite polling, reviewed conservative timing. */
nx_result_t nx_stm32u575_i2c_initialize(nx_stm32u575_i2c_state_t* port);
/** \brief Initialize with an assembly-validated AF-on/DNF0 timing value.
 * \note Caller has already reviewed HSI tolerance, pin electrical bounds and
 *       exact-part errata. This private helper does not synthesize timing.
 */
nx_result_t nx_stm32u575_i2c_initialize_timing(nx_stm32u575_i2c_state_t* port,
                                               uint32_t timing);
/** \brief Prepare leased PB8/PB9 AF4, open drain; pull-ups remain external. */
nx_result_t nx_stm32u575_i2c1_pins_prepare(void);
/** \brief Bind cold resources, prepare pads, then acquire HSI16 and I2C1.
 * \param[in,out] port: Runtime; live or retained owners reject rebinding.
 * \param[in,out] clock: Independent cold HSI16/selector cleanup owner.
 * \param[in] config: Immutable exact resources, timing and errata policy.
 * \return Success/error; partial acquisition requires ordinary close.
 */
nx_result_t nx_stm32u575_i2c_prepare(nx_i2c_v2_state_t* port,
                                     nx_stm32u575_i2c_clock_t* clock,
                                     const nx_i2c_v2_config_t* config);
/** \brief Prove peripheral withdrawal before releasing its HSI16 owner.
 * \param[in,out] port: Serialized runtime with all transfers joined.
 * \param[in,out] clock: Same retained independent clock owner.
 * \return Success/error; failed steps preserve responsibility for retry.
 */
nx_result_t nx_stm32u575_i2c_close(nx_i2c_v2_state_t* port,
                                   nx_stm32u575_i2c_clock_t* clock);
/** \brief SPI1 V3 finite polling, byte FIFO and explicit TSIZE/CSTART. */
typedef struct {
    SPI_TypeDef* registers;
    nx_stm32u575_gpio_state_t* held_cs;
    uint32_t held_cs_mask;
    volatile uint32_t* reset;
    uint32_t reset_mask;
    uint32_t clock_hz;
    uint32_t poll_limit;
    bool initialized;
    bool retained;
    bool active;
    bool fault;
    /* Cold CS acquisition is distinct from the drained SPI wire. */
    uint16_t cold_acquired;
    bool cold_owned;
} nx_stm32u575_spi_state_t;
typedef struct {
    nx_stm32u575_spi_state_t* port;
    nx_stm32u575_gpio_state_t* cs;
    uint32_t cs_mask;
    uint32_t frequency_hz;
    uint8_t mode;
} nx_stm32u575_spi_endpoint_state_t;
extern const nx_spi_ops_t nx_stm32u575_spi_ops;
extern const nx_spi_endpoint_ops_t nx_stm32u575_spi_endpoint_ops;
/** \brief Acquire cold SPI1/PCLK2 at4MHz, no DMA/interrupt ownership. */
nx_result_t nx_stm32u575_spi_initialize(nx_stm32u575_spi_state_t* port);
#ifndef NX_U575_SPI_POLL
#define NX_U575_SPI_POLL(port) ((void)(port))
#endif
#ifndef NX_U575_SPI_READ8
#define NX_U575_SPI_READ8(reg) (*(volatile uint8_t*)(reg))
#endif
#ifndef NX_U575_SPI_WRITE8
#define NX_U575_SPI_WRITE8(reg, value) (*(volatile uint8_t*)(reg) = (value))
#endif
/** \brief Exact PA0/PA1 IRQ rings; U5 owns its EXTI mux and two pending banks.
 */
typedef struct {
    EXTI_TypeDef* registers;
    EXTI_TypeDef* mux;
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
} nx_stm32u575_exti_state_t;
extern const nx_exti_ops_t nx_stm32u575_exti_ops;
/** \brief Admit only unowned GPIOA EXTI0/1; ring lives until successful stop.
 */
nx_result_t nx_stm32u575_exti_initialize(nx_stm32u575_exti_state_t* port);
/** \brief Observe/ack both W1C pending banks once; no edge history promise. */
void nx_stm32u575_exti_irq(nx_stm32u575_exti_state_t* port);
/** \brief ADC4 channel1 PC0, fixed12bit software shot, no DMA or trigger. */
typedef struct {
    ADC_TypeDef* registers;
    ADC_Common_TypeDef* common;
    uint32_t reference_mv;
    uint32_t poll_limit;
    bool initialized;
    bool retained;
    bool active;
    bool fault;
} nx_stm32u575_adc_state_t;
/** \brief Immutable selected ADC4 resources; no runtime config loan. */
typedef struct {
    ADC_TypeDef* registers;
    ADC_Common_TypeDef* common;
    uint32_t reference_mv;
    uint32_t poll_limit;
} nx_stm32u575_adc_config_t;
/** \brief Bind cold ADC4 state and configure PC0 before calibration.
 * \param[in,out] port: Exclusive static state; retained rebinding rejects.
 * \param[in] config: Immutable exact resources; its pointer is not retained.
 * \return Status; failures retain close and platform pin-restore duties.
 * \note The platform owns GPIOC/ADC clocks, VDDA and the PC0 snapshot.
 */
nx_result_t nx_stm32u575_adc_prepare(nx_stm32u575_adc_state_t* port,
                                     const nx_stm32u575_adc_config_t* config);
extern const nx_adc_ops_t nx_stm32u575_adc_ops;
/** \brief Require externally prepared VDDA/ASV, then LDO/calibration/ready. */
nx_result_t nx_stm32u575_adc_initialize(nx_stm32u575_adc_state_t* port);
/** \brief Observe exclusive ADC4 reset before releasing retained ownership. */
nx_result_t nx_stm32u575_adc_close(nx_stm32u575_adc_state_t* port);
#ifndef NX_U575_ADC_POLL
#define NX_U575_ADC_POLL(port) ((void)(port))
#endif
/** \brief Physical2MiB Flash,256x8KiB pages,128bit programming, TZEN0 only. */
typedef struct {
    FLASH_TypeDef* registers;
    volatile uint8_t* memory;
    bool initialized;
    bool retained;
    bool active;
} nx_stm32u575_flash_state_t;
extern const nx_flash_geometry_t g_nx_stm32u575_flash_geometry;
extern const nx_flash_ops_t nx_stm32u575_flash_ops;
/** \brief Observe exact density and locked idle control without unlocking. */
nx_result_t nx_stm32u575_flash_initialize(nx_stm32u575_flash_state_t* port);
/** \brief Retry idle mode withdrawal/relock, never revoke an active pulse. */
nx_result_t nx_stm32u575_flash_close(nx_stm32u575_flash_state_t* port);
#ifndef NX_U575_FLASH_POLL
#define NX_U575_FLASH_POLL(port) ((void)(port))
#endif
#ifndef NX_U575_FLASH_PROGRAM32
#define NX_U575_FLASH_PROGRAM32(address, value)                                \
    (*(volatile uint32_t*)(address) = (value))
#endif
extern const nx_gpio_ops_t nx_stm32u575_gpio_ops;
extern const nx_uart_ops_t nx_stm32u575_uart_ops;
extern const nx_watchdog_ops_t nx_stm32u575_watchdog_ops;
/** \brief Retry preactivation cleanup; an active IWDG remains irreversible. */
nx_result_t nx_stm32u575_watchdog_stop(nx_stm32u575_watchdog_state_t* port);
/** \brief Cold owner configures authorized pins after acquiring clocks. */
nx_result_t nx_stm32u575_gpio_initialize(nx_stm32u575_gpio_state_t* port,
                                         uint32_t initial);
/** \brief Preserve inactive output levels before withdrawing the face. */
nx_result_t nx_stm32u575_gpio_stop(nx_stm32u575_gpio_state_t* port,
                                   uint32_t inactive);
/** \brief Acquire a reviewed GPIO port clock with readback and barrier. */
nx_result_t nx_stm32u575_gpio_clock_enable(unsigned port_index);
/** \brief Source-reviewed PA9/PA10 AF7; caller owns Board bridge choice. */
nx_result_t nx_stm32u575_usart1_pins_prepare(void);
/** \brief Fixed PA2/PA3 AF7, APB1 USART2; external wiring stays Board-owned. */
nx_result_t nx_stm32u575_usart2_pins_prepare(void);
/**
 * \brief Configure explicit byte ring,8N1,PCLK4MHz/PRESC1; no FIFO/DMA.
 * \param[in,out] port: Exact static instance with unused hardware and ring.
 * \param[in] clock_hz: Verified4MHz APB2 fixed profile.
 * \param[in] poll_limit: Nonzero bounded transmitter/receiver ACK budget.
 * \return Success or error; retained/closing expose failed hardware cleanup.
 * \note Privileged single task owner; IRQ only moves one TX and one RX byte.
 *       Ring and wake live until stop succeeds and external consumers join.
 */
nx_result_t nx_stm32u575_uart_initialize(nx_stm32u575_uart_state_t* port,
                                         uint32_t clock_hz,
                                         uint32_t poll_limit);
/** \brief Move at most one byte each direction and latch TC/error facts. */
void nx_stm32u575_uart_irq(nx_stm32u575_uart_state_t* port);
#ifndef NX_U575_UART_POLL
#define NX_U575_UART_POLL(port) ((void)(port))
#endif
#ifndef NX_U575_IRQ_PRIORITY
#define NX_U575_IRQ_PRIORITY(irq) NVIC_GetPriority((IRQn_Type)(irq))
#endif
#ifndef NX_U575_IRQ_GROUP
#define NX_U575_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif
#ifdef __cplusplus
}
#endif
#endif
