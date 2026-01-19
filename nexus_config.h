/**
 * \file            nexus_config.h
 * \brief           HAL configuration header (auto-generated)
 * \author          Nexus Team
 *
 * This file is auto-generated from Kconfig. Do not edit manually.
 * Generated: 2026-01-20 02:28:36
 */

#ifndef NEXUS_CONFIG_H
#define NEXUS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* ADC Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_ADC0_CHANNEL_COUNT 16
#define NX_CONFIG_ADC0_RESOLUTION 12
#define NX_CONFIG_ADC_BUFFER0_BUFFER_SIZE 256
#define NX_CONFIG_ADC_BUFFER0_CHANNEL_COUNT 4
#define NX_CONFIG_ADC_BUFFER_MAX_INSTANCES 4
#define NX_CONFIG_ADC_BUFFER_NATIVE 1
#define NX_CONFIG_ADC_MAX_INSTANCES 4
#define NX_CONFIG_ADC_NATIVE 1
#define NX_CONFIG_INSTANCE_NX_ADC_0 1
/* #undef NX_CONFIG_INSTANCE_NX_ADC_1 */
/* #undef NX_CONFIG_INSTANCE_NX_ADC_2 */
/* #undef NX_CONFIG_INSTANCE_NX_ADC_3 */
#define NX_CONFIG_INSTANCE_NX_ADC_BUFFER_0 1
/* #undef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_1 */
/* #undef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_2 */
/* #undef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_3 */
/* #undef NX_CONFIG_STM32_ADC_ENABLE */

/*---------------------------------------------------------------------------*/
/* Assert Configuration                                                      */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_HAL_ASSERT_ENABLE 1

/*---------------------------------------------------------------------------*/
/* CRC Configuration                                                         */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_CRC0_ALGORITHM_CRC16 */
#define NX_CONFIG_CRC0_ALGORITHM_CRC32 1
#define NX_CONFIG_CRC0_FINAL_XOR 0xFFFFFFFF
#define NX_CONFIG_CRC0_INIT_VALUE 0xFFFFFFFF
#define NX_CONFIG_CRC0_POLYNOMIAL 0x04C11DB7
/* #undef NX_CONFIG_CRC1_ALGORITHM_CRC16 */
/* #undef NX_CONFIG_CRC1_ALGORITHM_CRC32 */
#define NX_CONFIG_CRC_MAX_INSTANCES 1
#define NX_CONFIG_CRC_NATIVE 1
#define NX_CONFIG_INSTANCE_NX_CRC0 1
/* #undef NX_CONFIG_INSTANCE_NX_CRC1 */

/*---------------------------------------------------------------------------*/
/* DAC Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_DAC0_CHANNEL_COUNT 2
#define NX_CONFIG_DAC0_RESOLUTION 12
#define NX_CONFIG_DAC0_VREF_MV 3300
#define NX_CONFIG_DAC_MAX_INSTANCES 4
#define NX_CONFIG_DAC_NATIVE 1
#define NX_CONFIG_INSTANCE_NX_DAC_0 1
/* #undef NX_CONFIG_INSTANCE_NX_DAC_1 */
/* #undef NX_CONFIG_INSTANCE_NX_DAC_2 */
/* #undef NX_CONFIG_INSTANCE_NX_DAC_3 */
/* #undef NX_CONFIG_STM32_DAC_ENABLE */

/*---------------------------------------------------------------------------*/
/* DMA Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_NATIVE_DMA_CHANNELS 8

/*---------------------------------------------------------------------------*/
/* Debug Configuration                                                       */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_HAL_DEBUG_ENABLE */

/*---------------------------------------------------------------------------*/
/* ESP32 Platform                                                            */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_ESP32_PLACEHOLDER */

/*---------------------------------------------------------------------------*/
/* GD32 Platform                                                             */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_GD32_PLACEHOLDER */

/*---------------------------------------------------------------------------*/
/* GPIO Configuration                                                        */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_GPIOA_PIN0_MODE 1
#define NX_CONFIG_GPIOA_PIN0_OUTPUT_VALUE 0
#define NX_CONFIG_GPIOA_PIN0_PULL_VALUE 0
#define NX_CONFIG_GPIOA_PIN0_SPEED_VALUE 1
/* #undef NX_CONFIG_GPIOA_PIN0_WR_MODE_READ */
#define NX_CONFIG_GPIOA_PIN0_WR_MODE_RW 1
/* #undef NX_CONFIG_GPIOA_PIN0_WR_MODE_WRITE */
/* #undef NX_CONFIG_GPIOB_PIN0_WR_MODE_READ */
/* #undef NX_CONFIG_GPIOB_PIN0_WR_MODE_RW */
/* #undef NX_CONFIG_GPIOB_PIN0_WR_MODE_WRITE */
/* #undef NX_CONFIG_GPIOC_PIN0_WR_MODE_READ */
/* #undef NX_CONFIG_GPIOC_PIN0_WR_MODE_RW */
/* #undef NX_CONFIG_GPIOC_PIN0_WR_MODE_WRITE */
#define NX_CONFIG_GPIO_MAX_PINS_PER_PORT 16
#define NX_CONFIG_GPIO_MAX_PORTS 8
#define NX_CONFIG_GPIO_NATIVE 1
#define NX_CONFIG_INSTANCE_NX_GPIOA 1
#define NX_CONFIG_INSTANCE_NX_GPIOA_PIN0 1
/* #undef NX_CONFIG_INSTANCE_NX_GPIOB */
/* #undef NX_CONFIG_INSTANCE_NX_GPIOB_PIN0 */
/* #undef NX_CONFIG_INSTANCE_NX_GPIOC */
/* #undef NX_CONFIG_INSTANCE_NX_GPIOC_PIN0 */
/* #undef NX_CONFIG_STM32_GPIO_ENABLE */

/*---------------------------------------------------------------------------*/
/* HAL Enable                                                                */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_HAL_ENABLE 1

/*---------------------------------------------------------------------------*/
/* I2C Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_I2C0_RX_BUFFER_SIZE 256
#define NX_CONFIG_I2C0_SCL_PIN 5
#define NX_CONFIG_I2C0_SDA_PIN 4
#define NX_CONFIG_I2C0_SPEED 100000
#define NX_CONFIG_I2C0_TX_BUFFER_SIZE 256
#define NX_CONFIG_I2C_MAX_INSTANCES 4
#define NX_CONFIG_I2C_NATIVE 1
#define NX_CONFIG_INSTANCE_NX_I2C_0 1
/* #undef NX_CONFIG_INSTANCE_NX_I2C_1 */
/* #undef NX_CONFIG_INSTANCE_NX_I2C_2 */
/* #undef NX_CONFIG_INSTANCE_NX_I2C_3 */
/* #undef NX_CONFIG_STM32_I2C_ENABLE */

/*---------------------------------------------------------------------------*/
/* Memory Configuration                                                      */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_HAL_MEM_MODE_DYNAMIC */
#define NX_CONFIG_HAL_MEM_MODE_STATIC 1
#define NX_CONFIG_HAL_MEM_POOL_SIZE 4096

/*---------------------------------------------------------------------------*/
/* NRF52 Platform                                                            */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_NRF52_PLACEHOLDER */

/*---------------------------------------------------------------------------*/
/* Native Platform                                                           */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_NATIVE_BUFFER_ALIGNMENT 4
#define NX_CONFIG_NATIVE_ENABLE_LOGGING 1
#define NX_CONFIG_NATIVE_ENABLE_STATISTICS 1
#define NX_CONFIG_NATIVE_ISR_SLOTS 64
#define NX_CONFIG_NATIVE_LOG_LEVEL 3

/*---------------------------------------------------------------------------*/
/* OSAL Configuration                                                        */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_OSAL_BACKEND_NAME "baremetal"
#define NX_CONFIG_OSAL_BAREMETAL 1
/* #undef NX_CONFIG_OSAL_FREERTOS */
#define NX_CONFIG_OSAL_HEAP_SIZE 16384
/* #undef NX_CONFIG_OSAL_LINUX */
#define NX_CONFIG_OSAL_MAIN_STACK_SIZE 4096
/* #undef NX_CONFIG_OSAL_NATIVE */
/* #undef NX_CONFIG_OSAL_RTTHREAD */
#define NX_CONFIG_OSAL_TICK_RATE_HZ 1000
/* #undef NX_CONFIG_OSAL_ZEPHYR */

/*---------------------------------------------------------------------------*/
/* Other Configuration                                                       */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_BAREMETAL_MAX_TASKS 8
#define NX_CONFIG_BAREMETAL_SIMPLE_SCHEDULER 1
/* #undef NX_CONFIG_FREERTOS_USE_PREEMPTION */
/* #undef NX_CONFIG_RTTHREAD_USING_COMPONENTS_INIT */
/* #undef NX_CONFIG_RTTHREAD_USING_USER_MAIN */
/* #undef NX_CONFIG_ZEPHYR_KERNEL_PREEMPT */
/* #undef NX_CONFIG_ZEPHYR_TIMESLICING */

/*---------------------------------------------------------------------------*/
/* Platform Configuration                                                    */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_NATIVE_PLATFORM_NAME "Native Platform (PC Simulation)"
#define NX_CONFIG_NATIVE_PLATFORM_VERSION "1.0.0"
/* #undef NX_CONFIG_PLATFORM_ESP32 */
/* #undef NX_CONFIG_PLATFORM_GD32 */
#define NX_CONFIG_PLATFORM_NAME "native"
#define NX_CONFIG_PLATFORM_NATIVE 1
/* #undef NX_CONFIG_PLATFORM_NRF52 */
/* #undef NX_CONFIG_PLATFORM_STM32 */

/*---------------------------------------------------------------------------*/
/* SPI Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_INSTANCE_NX_SPI_0 1
/* #undef NX_CONFIG_INSTANCE_NX_SPI_1 */
/* #undef NX_CONFIG_INSTANCE_NX_SPI_2 */
/* #undef NX_CONFIG_INSTANCE_NX_SPI_3 */
#define NX_CONFIG_SPI0_MAX_SPEED 1000000
#define NX_CONFIG_SPI0_MISO_PIN 12
#define NX_CONFIG_SPI0_MOSI_PIN 11
#define NX_CONFIG_SPI0_RX_BUFFER_SIZE 256
#define NX_CONFIG_SPI0_SCK_PIN 13
#define NX_CONFIG_SPI0_TX_BUFFER_SIZE 256
#define NX_CONFIG_SPI_MAX_INSTANCES 4
#define NX_CONFIG_SPI_NATIVE 1
/* #undef NX_CONFIG_STM32_SPI_ENABLE */

/*---------------------------------------------------------------------------*/
/* STM32 Platform                                                            */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_STM32F4 */
/* #undef NX_CONFIG_STM32F407 */
/* #undef NX_CONFIG_STM32F429 */
/* #undef NX_CONFIG_STM32F446 */
/* #undef NX_CONFIG_STM32H7 */
/* #undef NX_CONFIG_STM32H743 */
/* #undef NX_CONFIG_STM32H750 */
/* #undef NX_CONFIG_STM32L4 */
/* #undef NX_CONFIG_STM32L432 */
/* #undef NX_CONFIG_STM32L476 */
/* #undef NX_CONFIG_STM32_ENABLE_LOGGING */
/* #undef NX_CONFIG_STM32_ENABLE_STATISTICS */

/*---------------------------------------------------------------------------*/
/* Statistics Configuration                                                  */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_HAL_STATISTICS_BUFFER_SIZE 256
#define NX_CONFIG_HAL_STATISTICS_ENABLE 1

/*---------------------------------------------------------------------------*/
/* Thread Safety                                                             */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_HAL_THREAD_SAFE */

/*---------------------------------------------------------------------------*/
/* Timeout Configuration                                                     */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_HAL_DEFAULT_TIMEOUT_MS 1000
#define NX_CONFIG_HAL_INIT_TIMEOUT_MS 5000

/*---------------------------------------------------------------------------*/
/* Timer Configuration                                                       */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_FREERTOS_USE_TIMERS */
#define NX_CONFIG_INSTANCE_NX_TIMER_0 1
/* #undef NX_CONFIG_INSTANCE_NX_TIMER_1 */
/* #undef NX_CONFIG_INSTANCE_NX_TIMER_2 */
/* #undef NX_CONFIG_INSTANCE_NX_TIMER_3 */
/* #undef NX_CONFIG_INSTANCE_NX_TIMER_4 */
/* #undef NX_CONFIG_INSTANCE_NX_TIMER_5 */
/* #undef NX_CONFIG_INSTANCE_NX_TIMER_6 */
/* #undef NX_CONFIG_INSTANCE_NX_TIMER_7 */
/* #undef NX_CONFIG_STM32_TIMER_ENABLE */
#define NX_CONFIG_TIMER0_CHANNEL_COUNT 4
#define NX_CONFIG_TIMER0_FREQUENCY 1000000
#define NX_CONFIG_TIMER_MAX_INSTANCES 8
#define NX_CONFIG_TIMER_NATIVE 1

/*---------------------------------------------------------------------------*/
/* UART Configuration                                                        */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_INSTANCE_NX_UART_0 1
/* #undef NX_CONFIG_INSTANCE_NX_UART_1 */
/* #undef NX_CONFIG_INSTANCE_NX_UART_2 */
/* #undef NX_CONFIG_INSTANCE_NX_UART_3 */
#define NX_CONFIG_NATIVE_UART_ENABLE 1
#define NX_CONFIG_NATIVE_UART_MAX_INSTANCES 4
/* #undef NX_CONFIG_NX_UART0_MODE_DMA */
#define NX_CONFIG_NX_UART0_MODE_INTERRUPT 1
/* #undef NX_CONFIG_NX_UART0_MODE_POLLING */
/* #undef NX_CONFIG_NX_UART0_PARITY_EVEN */
#define NX_CONFIG_NX_UART0_PARITY_NONE 1
/* #undef NX_CONFIG_NX_UART0_PARITY_ODD */
/* #undef NX_CONFIG_NX_UART1_MODE_DMA */
/* #undef NX_CONFIG_NX_UART1_MODE_INTERRUPT */
/* #undef NX_CONFIG_NX_UART1_MODE_POLLING */
/* #undef NX_CONFIG_NX_UART1_PARITY_EVEN */
/* #undef NX_CONFIG_NX_UART1_PARITY_NONE */
/* #undef NX_CONFIG_NX_UART1_PARITY_ODD */
/* #undef NX_CONFIG_NX_UART2_MODE_DMA */
/* #undef NX_CONFIG_NX_UART2_MODE_INTERRUPT */
/* #undef NX_CONFIG_NX_UART2_MODE_POLLING */
/* #undef NX_CONFIG_NX_UART2_PARITY_EVEN */
/* #undef NX_CONFIG_NX_UART2_PARITY_NONE */
/* #undef NX_CONFIG_NX_UART2_PARITY_ODD */
/* #undef NX_CONFIG_NX_UART3_MODE_DMA */
/* #undef NX_CONFIG_NX_UART3_MODE_INTERRUPT */
/* #undef NX_CONFIG_NX_UART3_MODE_POLLING */
/* #undef NX_CONFIG_NX_UART3_PARITY_EVEN */
/* #undef NX_CONFIG_NX_UART3_PARITY_NONE */
/* #undef NX_CONFIG_NX_UART3_PARITY_ODD */
/* #undef NX_CONFIG_STM32_UART_ENABLE */
#define NX_CONFIG_UART0_BAUDRATE 115200
#define NX_CONFIG_UART0_DATA_BITS 8
#define NX_CONFIG_UART0_MODE_VALUE 1
#define NX_CONFIG_UART0_PARITY_VALUE 0
#define NX_CONFIG_UART0_RX_BUFFER_SIZE 256
#define NX_CONFIG_UART0_STOP_BITS 1
#define NX_CONFIG_UART0_TX_BUFFER_SIZE 256

/*---------------------------------------------------------------------------*/
/* USB Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_NX_USB_AUTO_CONNECT 1
#define NX_CONFIG_NX_USB_ENABLED 1
#define NX_CONFIG_NX_USB_NUM_ENDPOINTS 4
#define NX_CONFIG_NX_USB_RX_BUFFER_SIZE 1024
#define NX_CONFIG_NX_USB_TX_BUFFER_SIZE 1024

/*---------------------------------------------------------------------------*/
/* Peripheral Instance Traversal Macros                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           ADC instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * ADC instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_ADC(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_ADC(fn) \
    _NX_ADC_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_ADC_0
#define _NX_ADC_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_ADC0_ENABLED 1
#else
#define _NX_ADC_INSTANCE_0(fn)
#define NX_CONFIG_ADC0_ENABLED 0
#endif

/**
 * \brief           ADC_BUFFER instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * ADC_BUFFER instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_ADC_BUFFER(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_ADC_BUFFER(fn) \
    _NX_ADC_BUFFER_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_0
#define _NX_ADC_BUFFER_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_ADC_BUFFER0_ENABLED 1
#else
#define _NX_ADC_BUFFER_INSTANCE_0(fn)
#define NX_CONFIG_ADC_BUFFER0_ENABLED 0
#endif

/**
 * \brief           DAC instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * DAC instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_DAC(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_DAC(fn) \
    _NX_DAC_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_DAC_0
#define _NX_DAC_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_DAC0_ENABLED 1
#else
#define _NX_DAC_INSTANCE_0(fn)
#define NX_CONFIG_DAC0_ENABLED 0
#endif

/**
 * \brief           GPIOA instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * GPIOA instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_GPIOA(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(PIN0)
 */
#define NX_DEFINE_INSTANCE_NX_GPIOA(fn) \
    _NX_GPIOA_INSTANCE_PIN0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_GPIOA_PIN0
#define _NX_GPIOA_INSTANCE_PIN0(fn) fn(PIN0)
#define NX_CONFIG_GPIOAPIN0_ENABLED 1
#else
#define _NX_GPIOA_INSTANCE_PIN0(fn)
#define NX_CONFIG_GPIOAPIN0_ENABLED 0
#endif

/**
 * \brief           I2C instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * I2C instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_I2C(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_I2C(fn) \
    _NX_I2C_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_I2C_0
#define _NX_I2C_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_I2C0_ENABLED 1
#else
#define _NX_I2C_INSTANCE_0(fn)
#define NX_CONFIG_I2C0_ENABLED 0
#endif

/**
 * \brief           SPI instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * SPI instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_SPI(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_SPI(fn) \
    _NX_SPI_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_SPI_0
#define _NX_SPI_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_SPI0_ENABLED 1
#else
#define _NX_SPI_INSTANCE_0(fn)
#define NX_CONFIG_SPI0_ENABLED 0
#endif

/**
 * \brief           TIMER instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * TIMER instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_TIMER(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_TIMER(fn) \
    _NX_TIMER_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_TIMER_0
#define _NX_TIMER_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_TIMER0_ENABLED 1
#else
#define _NX_TIMER_INSTANCE_0(fn)
#define NX_CONFIG_TIMER0_ENABLED 0
#endif

/**
 * \brief           UART instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * UART instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_UART(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_UART(fn) \
    _NX_UART_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_UART_0
#define _NX_UART_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_UART0_ENABLED 1
#else
#define _NX_UART_INSTANCE_0(fn)
#define NX_CONFIG_UART0_ENABLED 0
#endif

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_CONFIG_H */
