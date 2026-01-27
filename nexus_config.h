/**
 * \file            nexus_config.h
 * \brief           HAL configuration header (auto-generated)
 * \author          Nexus Team
 *
 * This file is auto-generated from Kconfig. Do not edit manually.
 * Generated: 2026-01-27 21:12:23
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
#define NX_CONFIG_ADC0_RESOLUTION_VALUE 2
#define NX_CONFIG_ADC0_SAMPLE_TIME_VALUE 1
#define NX_CONFIG_ADC_BUFFER0_BUFFER_SIZE 256
#define NX_CONFIG_ADC_BUFFER0_CHANNEL_COUNT 4
#define NX_CONFIG_ADC_BUFFER0_TRIGGER_VALUE 0
#define NX_CONFIG_INSTANCE_NX_ADC_0 1
/* #undef NX_CONFIG_INSTANCE_NX_ADC_1 */
/* #undef NX_CONFIG_INSTANCE_NX_ADC_2 */
/* #undef NX_CONFIG_INSTANCE_NX_ADC_3 */
#define NX_CONFIG_INSTANCE_NX_ADC_BUFFER_0 1
/* #undef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_1 */
/* #undef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_2 */
/* #undef NX_CONFIG_INSTANCE_NX_ADC_BUFFER_3 */
#define NX_CONFIG_NATIVE_ADC_BUFFER_ENABLE 1
#define NX_CONFIG_NATIVE_ADC_ENABLE 1
/* #undef NX_CONFIG_NX_ADC0_RESOLUTION_10 */
#define NX_CONFIG_NX_ADC0_RESOLUTION_12 1
/* #undef NX_CONFIG_NX_ADC0_RESOLUTION_16 */
/* #undef NX_CONFIG_NX_ADC0_RESOLUTION_8 */
/* #undef NX_CONFIG_NX_ADC0_SAMPLE_TIME_FAST */
#define NX_CONFIG_NX_ADC0_SAMPLE_TIME_MEDIUM 1
/* #undef NX_CONFIG_NX_ADC0_SAMPLE_TIME_SLOW */
/* #undef NX_CONFIG_NX_ADC1_RESOLUTION_BIT_10 */
/* #undef NX_CONFIG_NX_ADC1_RESOLUTION_BIT_12 */
/* #undef NX_CONFIG_NX_ADC1_RESOLUTION_BIT_16 */
/* #undef NX_CONFIG_NX_ADC1_RESOLUTION_BIT_8 */
/* #undef NX_CONFIG_NX_ADC1_SAMPLE_TIME_FAST */
/* #undef NX_CONFIG_NX_ADC1_SAMPLE_TIME_MEDIUM */
/* #undef NX_CONFIG_NX_ADC1_SAMPLE_TIME_SLOW */
/* #undef NX_CONFIG_NX_ADC2_RESOLUTION_BIT_10 */
/* #undef NX_CONFIG_NX_ADC2_RESOLUTION_BIT_12 */
/* #undef NX_CONFIG_NX_ADC2_RESOLUTION_BIT_16 */
/* #undef NX_CONFIG_NX_ADC2_RESOLUTION_BIT_8 */
/* #undef NX_CONFIG_NX_ADC2_SAMPLE_TIME_FAST */
/* #undef NX_CONFIG_NX_ADC2_SAMPLE_TIME_MEDIUM */
/* #undef NX_CONFIG_NX_ADC2_SAMPLE_TIME_SLOW */
/* #undef NX_CONFIG_NX_ADC3_RESOLUTION_BIT_10 */
/* #undef NX_CONFIG_NX_ADC3_RESOLUTION_BIT_12 */
/* #undef NX_CONFIG_NX_ADC3_RESOLUTION_BIT_16 */
/* #undef NX_CONFIG_NX_ADC3_RESOLUTION_BIT_8 */
/* #undef NX_CONFIG_NX_ADC3_SAMPLE_TIME_FAST */
/* #undef NX_CONFIG_NX_ADC3_SAMPLE_TIME_MEDIUM */
/* #undef NX_CONFIG_NX_ADC3_SAMPLE_TIME_SLOW */
/* #undef NX_CONFIG_NX_ADC_BUFFER0_TRIGGER_EXTERNAL */
#define NX_CONFIG_NX_ADC_BUFFER0_TRIGGER_SOFTWARE 1
/* #undef NX_CONFIG_NX_ADC_BUFFER0_TRIGGER_TIMER */
/* #undef NX_CONFIG_NX_ADC_BUFFER1_TRIGGER_EXTERNAL */
/* #undef NX_CONFIG_NX_ADC_BUFFER1_TRIGGER_SOFTWARE */
/* #undef NX_CONFIG_NX_ADC_BUFFER1_TRIGGER_TIMER */
/* #undef NX_CONFIG_NX_ADC_BUFFER2_TRIGGER_EXTERNAL */
/* #undef NX_CONFIG_NX_ADC_BUFFER2_TRIGGER_SOFTWARE */
/* #undef NX_CONFIG_NX_ADC_BUFFER2_TRIGGER_TIMER */
/* #undef NX_CONFIG_NX_ADC_BUFFER3_TRIGGER_EXTERNAL */
/* #undef NX_CONFIG_NX_ADC_BUFFER3_TRIGGER_SOFTWARE */
/* #undef NX_CONFIG_NX_ADC_BUFFER3_TRIGGER_TIMER */
/* #undef NX_CONFIG_STM32_ADC_ENABLE */

/*---------------------------------------------------------------------------*/
/* Assert Configuration                                                      */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_HAL_ASSERT_ENABLE 1

/*---------------------------------------------------------------------------*/
/* CRC Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_INSTANCE_NX_CRC_0 1
/* #undef NX_CONFIG_INSTANCE_NX_CRC_1 */
#define NX_CONFIG_NATIVE_CRC_ENABLE 1
/* #undef NX_CONFIG_NX_CRC0_ALGORITHM_CRC16 */
#define NX_CONFIG_NX_CRC0_ALGORITHM_CRC32 1
/* #undef NX_CONFIG_NX_CRC0_ALGORITHM_CRC8 */
#define NX_CONFIG_NX_CRC0_ALGORITHM_VALUE 0
#define NX_CONFIG_NX_CRC0_FINAL_XOR 0xFFFFFFFF
#define NX_CONFIG_NX_CRC0_INIT_VALUE 0xFFFFFFFF
#define NX_CONFIG_NX_CRC0_INPUT_BYTE 1
#define NX_CONFIG_NX_CRC0_INPUT_FORMAT_VALUE 0
/* #undef NX_CONFIG_NX_CRC0_INPUT_HALFWORD */
#define NX_CONFIG_NX_CRC0_INPUT_VALUE 0
/* #undef NX_CONFIG_NX_CRC0_INPUT_WORD */
#define NX_CONFIG_NX_CRC0_POLYNOMIAL 0x04C11DB7
/* #undef NX_CONFIG_NX_CRC1_ALGORITHM_CRC16 */
/* #undef NX_CONFIG_NX_CRC1_ALGORITHM_CRC32 */
/* #undef NX_CONFIG_NX_CRC1_ALGORITHM_CRC8 */
/* #undef NX_CONFIG_NX_CRC1_INPUT_BYTE */
/* #undef NX_CONFIG_NX_CRC1_INPUT_HALFWORD */
/* #undef NX_CONFIG_NX_CRC1_INPUT_WORD */

/*---------------------------------------------------------------------------*/
/* DAC Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_DAC0_CHANNEL_COUNT 2
#define NX_CONFIG_DAC0_RESOLUTION 12
#define NX_CONFIG_DAC0_RESOLUTION_VALUE 2
#define NX_CONFIG_DAC0_TRIGGER_VALUE 0
#define NX_CONFIG_DAC0_VREF_MV 3300
#define NX_CONFIG_INSTANCE_NX_DAC_0 1
/* #undef NX_CONFIG_INSTANCE_NX_DAC_1 */
/* #undef NX_CONFIG_INSTANCE_NX_DAC_2 */
/* #undef NX_CONFIG_INSTANCE_NX_DAC_3 */
#define NX_CONFIG_NATIVE_DAC_ENABLE 1
/* #undef NX_CONFIG_NX_DAC0_RESOLUTION_BIT_10 */
#define NX_CONFIG_NX_DAC0_RESOLUTION_BIT_12 1
/* #undef NX_CONFIG_NX_DAC0_RESOLUTION_BIT_16 */
/* #undef NX_CONFIG_NX_DAC0_RESOLUTION_BIT_8 */
/* #undef NX_CONFIG_NX_DAC0_TRIGGER_EXTERNAL */
#define NX_CONFIG_NX_DAC0_TRIGGER_SOFTWARE 1
/* #undef NX_CONFIG_NX_DAC0_TRIGGER_TIMER */
/* #undef NX_CONFIG_NX_DAC1_RESOLUTION_BIT_10 */
/* #undef NX_CONFIG_NX_DAC1_RESOLUTION_BIT_12 */
/* #undef NX_CONFIG_NX_DAC1_RESOLUTION_BIT_16 */
/* #undef NX_CONFIG_NX_DAC1_RESOLUTION_BIT_8 */
/* #undef NX_CONFIG_NX_DAC1_TRIGGER_EXTERNAL */
/* #undef NX_CONFIG_NX_DAC1_TRIGGER_SOFTWARE */
/* #undef NX_CONFIG_NX_DAC1_TRIGGER_TIMER */
/* #undef NX_CONFIG_NX_DAC2_RESOLUTION_BIT_10 */
/* #undef NX_CONFIG_NX_DAC2_RESOLUTION_BIT_12 */
/* #undef NX_CONFIG_NX_DAC2_RESOLUTION_BIT_16 */
/* #undef NX_CONFIG_NX_DAC2_RESOLUTION_BIT_8 */
/* #undef NX_CONFIG_NX_DAC2_TRIGGER_EXTERNAL */
/* #undef NX_CONFIG_NX_DAC2_TRIGGER_SOFTWARE */
/* #undef NX_CONFIG_NX_DAC2_TRIGGER_TIMER */
/* #undef NX_CONFIG_NX_DAC3_RESOLUTION_BIT_10 */
/* #undef NX_CONFIG_NX_DAC3_RESOLUTION_BIT_12 */
/* #undef NX_CONFIG_NX_DAC3_RESOLUTION_BIT_16 */
/* #undef NX_CONFIG_NX_DAC3_RESOLUTION_BIT_8 */
/* #undef NX_CONFIG_NX_DAC3_TRIGGER_EXTERNAL */
/* #undef NX_CONFIG_NX_DAC3_TRIGGER_SOFTWARE */
/* #undef NX_CONFIG_NX_DAC3_TRIGGER_TIMER */
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
/* Flash Configuration                                                       */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_FLASH0_BACKING_FILE "native_flash0.bin"
#define NX_CONFIG_FLASH0_PAGE_SIZE_BYTES 256
#define NX_CONFIG_FLASH0_SECTOR_SIZE_KB 4
#define NX_CONFIG_FLASH0_SIZE_KB 512
#define NX_CONFIG_FLASH0_WP_VALUE 0
#define NX_CONFIG_INSTANCE_NX_INTERNAL_FLASH0 1
#define NX_CONFIG_NATIVE_FLASH_ENABLE 1
#define NX_CONFIG_NX_FLASH0_WP_DISABLED 1
/* #undef NX_CONFIG_NX_FLASH0_WP_ENABLED */

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
#define NX_CONFIG_GPIOA_PIN1_MODE 1
#define NX_CONFIG_GPIOA_PIN1_OUTPUT_VALUE 0
#define NX_CONFIG_GPIOA_PIN1_PULL_VALUE 0
#define NX_CONFIG_GPIOA_PIN1_SPEED_VALUE 1
#define NX_CONFIG_GPIOA_PIN2_MODE 1
#define NX_CONFIG_GPIOA_PIN2_OUTPUT_VALUE 0
#define NX_CONFIG_GPIOA_PIN2_PULL_VALUE 0
#define NX_CONFIG_GPIOA_PIN2_SPEED_VALUE 1
#define NX_CONFIG_GPIOB_PIN0_MODE 1
#define NX_CONFIG_GPIOB_PIN0_OUTPUT_VALUE 0
#define NX_CONFIG_GPIOB_PIN0_PULL_VALUE 0
#define NX_CONFIG_GPIOB_PIN0_SPEED_VALUE 1
#define NX_CONFIG_INSTANCE_NX_GPIOA 1
#define NX_CONFIG_INSTANCE_NX_GPIOA_PIN0 1
#define NX_CONFIG_INSTANCE_NX_GPIOA_PIN1 1
#define NX_CONFIG_INSTANCE_NX_GPIOA_PIN2 1
#define NX_CONFIG_INSTANCE_NX_GPIOB 1
#define NX_CONFIG_INSTANCE_NX_GPIOB_PIN0 1
/* #undef NX_CONFIG_INSTANCE_NX_GPIOC */
/* #undef NX_CONFIG_INSTANCE_NX_GPIOC_PIN0 */
#define NX_CONFIG_NATIVE_GPIO_ENABLE 1
/* #undef NX_CONFIG_NX_GPIOA_PIN0_MODE_AF_OD */
/* #undef NX_CONFIG_NX_GPIOA_PIN0_MODE_AF_PP */
/* #undef NX_CONFIG_NX_GPIOA_PIN0_MODE_ANALOG */
/* #undef NX_CONFIG_NX_GPIOA_PIN0_MODE_INPUT */
/* #undef NX_CONFIG_NX_GPIOA_PIN0_MODE_OUTPUT_OD */
#define NX_CONFIG_NX_GPIOA_PIN0_MODE_OUTPUT_PP 1
/* #undef NX_CONFIG_NX_GPIOA_PIN0_PULL_DOWN */
#define NX_CONFIG_NX_GPIOA_PIN0_PULL_NONE 1
/* #undef NX_CONFIG_NX_GPIOA_PIN0_PULL_UP */
/* #undef NX_CONFIG_NX_GPIOA_PIN0_SPEED_HIGH */
/* #undef NX_CONFIG_NX_GPIOA_PIN0_SPEED_LOW */
#define NX_CONFIG_NX_GPIOA_PIN0_SPEED_MEDIUM 1
/* #undef NX_CONFIG_NX_GPIOA_PIN0_SPEED_VERY_HIGH */
/* #undef NX_CONFIG_NX_GPIOA_PIN1_MODE_AF_OD */
/* #undef NX_CONFIG_NX_GPIOA_PIN1_MODE_AF_PP */
/* #undef NX_CONFIG_NX_GPIOA_PIN1_MODE_ANALOG */
/* #undef NX_CONFIG_NX_GPIOA_PIN1_MODE_INPUT */
/* #undef NX_CONFIG_NX_GPIOA_PIN1_MODE_OUTPUT_OD */
#define NX_CONFIG_NX_GPIOA_PIN1_MODE_OUTPUT_PP 1
/* #undef NX_CONFIG_NX_GPIOA_PIN1_PULL_DOWN */
#define NX_CONFIG_NX_GPIOA_PIN1_PULL_NONE 1
/* #undef NX_CONFIG_NX_GPIOA_PIN1_PULL_UP */
/* #undef NX_CONFIG_NX_GPIOA_PIN1_SPEED_HIGH */
/* #undef NX_CONFIG_NX_GPIOA_PIN1_SPEED_LOW */
#define NX_CONFIG_NX_GPIOA_PIN1_SPEED_MEDIUM 1
/* #undef NX_CONFIG_NX_GPIOA_PIN1_SPEED_VERY_HIGH */
/* #undef NX_CONFIG_NX_GPIOA_PIN2_MODE_AF_OD */
/* #undef NX_CONFIG_NX_GPIOA_PIN2_MODE_AF_PP */
/* #undef NX_CONFIG_NX_GPIOA_PIN2_MODE_ANALOG */
/* #undef NX_CONFIG_NX_GPIOA_PIN2_MODE_INPUT */
/* #undef NX_CONFIG_NX_GPIOA_PIN2_MODE_OUTPUT_OD */
#define NX_CONFIG_NX_GPIOA_PIN2_MODE_OUTPUT_PP 1
/* #undef NX_CONFIG_NX_GPIOA_PIN2_PULL_DOWN */
#define NX_CONFIG_NX_GPIOA_PIN2_PULL_NONE 1
/* #undef NX_CONFIG_NX_GPIOA_PIN2_PULL_UP */
/* #undef NX_CONFIG_NX_GPIOA_PIN2_SPEED_HIGH */
/* #undef NX_CONFIG_NX_GPIOA_PIN2_SPEED_LOW */
#define NX_CONFIG_NX_GPIOA_PIN2_SPEED_MEDIUM 1
/* #undef NX_CONFIG_NX_GPIOA_PIN2_SPEED_VERY_HIGH */
/* #undef NX_CONFIG_NX_GPIOB_PIN0_MODE_AF_OD */
/* #undef NX_CONFIG_NX_GPIOB_PIN0_MODE_AF_PP */
/* #undef NX_CONFIG_NX_GPIOB_PIN0_MODE_ANALOG */
/* #undef NX_CONFIG_NX_GPIOB_PIN0_MODE_INPUT */
/* #undef NX_CONFIG_NX_GPIOB_PIN0_MODE_OUTPUT_OD */
#define NX_CONFIG_NX_GPIOB_PIN0_MODE_OUTPUT_PP 1
/* #undef NX_CONFIG_NX_GPIOB_PIN0_PULL_DOWN */
#define NX_CONFIG_NX_GPIOB_PIN0_PULL_NONE 1
/* #undef NX_CONFIG_NX_GPIOB_PIN0_PULL_UP */
/* #undef NX_CONFIG_NX_GPIOB_PIN0_SPEED_HIGH */
/* #undef NX_CONFIG_NX_GPIOB_PIN0_SPEED_LOW */
#define NX_CONFIG_NX_GPIOB_PIN0_SPEED_MEDIUM 1
/* #undef NX_CONFIG_NX_GPIOB_PIN0_SPEED_VERY_HIGH */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_MODE_AF_OD */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_MODE_AF_PP */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_MODE_ANALOG */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_MODE_INPUT */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_MODE_OUTPUT_OD */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_MODE_OUTPUT_PP */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_PULL_DOWN */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_PULL_NONE */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_PULL_UP */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_SPEED_HIGH */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_SPEED_LOW */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_SPEED_MEDIUM */
/* #undef NX_CONFIG_NX_GPIOC_PIN0_SPEED_VERY_HIGH */
/* #undef NX_CONFIG_STM32_GPIO_ENABLE */

/*---------------------------------------------------------------------------*/
/* HAL Enable                                                                */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_HAL_ENABLE 1

/*---------------------------------------------------------------------------*/
/* I2C Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_I2C0_ADDR_MODE_VALUE 0
#define NX_CONFIG_I2C0_ADDR_VALUE 0
#define NX_CONFIG_I2C0_RX_BUFFER_SIZE 256
#define NX_CONFIG_I2C0_SPEED 100000
#define NX_CONFIG_I2C0_SPEED_VALUE 0
#define NX_CONFIG_I2C0_TX_BUFFER_SIZE 256
#define NX_CONFIG_INSTANCE_NX_I2C_0 1
/* #undef NX_CONFIG_INSTANCE_NX_I2C_1 */
/* #undef NX_CONFIG_INSTANCE_NX_I2C_2 */
/* #undef NX_CONFIG_INSTANCE_NX_I2C_3 */
#define NX_CONFIG_NATIVE_I2C_ENABLE 1
/* #undef NX_CONFIG_NX_I2C0_ADDR_BIT_10 */
#define NX_CONFIG_NX_I2C0_ADDR_BIT_7 1
/* #undef NX_CONFIG_NX_I2C0_SPEED_FAST */
/* #undef NX_CONFIG_NX_I2C0_SPEED_FAST_PLUS */
#define NX_CONFIG_NX_I2C0_SPEED_STANDARD 1
/* #undef NX_CONFIG_NX_I2C1_ADDR_BIT_10 */
/* #undef NX_CONFIG_NX_I2C1_ADDR_BIT_7 */
/* #undef NX_CONFIG_NX_I2C1_SPEED_FAST */
/* #undef NX_CONFIG_NX_I2C1_SPEED_FAST_PLUS */
/* #undef NX_CONFIG_NX_I2C1_SPEED_STANDARD */
/* #undef NX_CONFIG_NX_I2C2_ADDR_BIT_10 */
/* #undef NX_CONFIG_NX_I2C2_ADDR_BIT_7 */
/* #undef NX_CONFIG_NX_I2C2_SPEED_FAST */
/* #undef NX_CONFIG_NX_I2C2_SPEED_FAST_PLUS */
/* #undef NX_CONFIG_NX_I2C2_SPEED_STANDARD */
/* #undef NX_CONFIG_NX_I2C3_ADDR_BIT_10 */
/* #undef NX_CONFIG_NX_I2C3_ADDR_BIT_7 */
/* #undef NX_CONFIG_NX_I2C3_SPEED_FAST */
/* #undef NX_CONFIG_NX_I2C3_SPEED_FAST_PLUS */
/* #undef NX_CONFIG_NX_I2C3_SPEED_STANDARD */
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
#define NX_CONFIG_NATIVE_OPTION_BYTES_ENABLE 1

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
#define NX_CONFIG_INSTANCE_NX_OPTION_BYTES_0 1
#define NX_CONFIG_NX_OPTBYTES0_RDP_LEVEL0 1
/* #undef NX_CONFIG_NX_OPTBYTES0_RDP_LEVEL1 */
/* #undef NX_CONFIG_NX_OPTBYTES0_RDP_LEVEL2 */
#define NX_CONFIG_NX_OPTBYTES0_STDBY_NORST 1
/* #undef NX_CONFIG_NX_OPTBYTES0_STDBY_RST */
#define NX_CONFIG_NX_OPTBYTES0_STOP_NORST 1
/* #undef NX_CONFIG_NX_OPTBYTES0_STOP_RST */
/* #undef NX_CONFIG_NX_OPTBYTES0_WDG_HW */
#define NX_CONFIG_NX_OPTBYTES0_WDG_SW 1
#define NX_CONFIG_OPTBYTES0_RDP_LEVEL_VALUE 0
#define NX_CONFIG_OPTBYTES0_RDP_VALUE 0
#define NX_CONFIG_OPTBYTES0_STDBY_RST_VALUE 0
#define NX_CONFIG_OPTBYTES0_STOP_RST_VALUE 0
#define NX_CONFIG_OPTBYTES0_USER_DATA0 0xFF
#define NX_CONFIG_OPTBYTES0_USER_DATA1 0xFF
#define NX_CONFIG_OPTBYTES0_WDG_VALUE 0
#define NX_CONFIG_OPTBYTES0_WRITE_PROTECT_SECTORS 0x00000000
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
/* RTC Configuration                                                         */
/*---------------------------------------------------------------------------*/

/* #undef NX_CONFIG_INSTANCE_NX_RTC1 */
#define NX_CONFIG_INSTANCE_NX_RTC_0 1
#define NX_CONFIG_NATIVE_RTC_ENABLE 1
/* #undef NX_CONFIG_NX_RTC0_CLOCK_HSE */
/* #undef NX_CONFIG_NX_RTC0_CLOCK_LSE */
/* #undef NX_CONFIG_NX_RTC0_CLOCK_LSI */
/* #undef NX_CONFIG_NX_RTC0_FORMAT_H_12 */
/* #undef NX_CONFIG_NX_RTC0_FORMAT_H_24 */
/* #undef NX_CONFIG_NX_RTC1_CLOCK_HSE */
/* #undef NX_CONFIG_NX_RTC1_CLOCK_LSE */
/* #undef NX_CONFIG_NX_RTC1_CLOCK_LSI */
/* #undef NX_CONFIG_NX_RTC1_FORMAT_H_12 */
/* #undef NX_CONFIG_NX_RTC1_FORMAT_H_24 */
#define NX_CONFIG_RTC0_ALARM_COUNT 2
#define NX_CONFIG_RTC0_ENABLE_ALARM 1
/* #undef NX_CONFIG_RTC1_ENABLE_ALARM */

/*---------------------------------------------------------------------------*/
/* SDIO Configuration                                                        */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_INSTANCE_NX_SDIO_0 1
/* #undef NX_CONFIG_INSTANCE_NX_SDIO_1 */
#define NX_CONFIG_NATIVE_SDIO_ENABLE 1
/* #undef NX_CONFIG_NX_SDIO0_BUS_WIDTH_1BIT */
#define NX_CONFIG_NX_SDIO0_BUS_WIDTH_4BIT 1
/* #undef NX_CONFIG_NX_SDIO0_BUS_WIDTH_8BIT */
#define NX_CONFIG_NX_SDIO0_CARD_SDHC 1
/* #undef NX_CONFIG_NX_SDIO0_CARD_SDSC */
/* #undef NX_CONFIG_NX_SDIO0_CARD_SDXC */
/* #undef NX_CONFIG_NX_SDIO0_CLOCK_KHZ_400 */
/* #undef NX_CONFIG_NX_SDIO0_CLOCK_MHZ_100 */
/* #undef NX_CONFIG_NX_SDIO0_CLOCK_MHZ_200 */
#define NX_CONFIG_NX_SDIO0_CLOCK_MHZ_25 1
/* #undef NX_CONFIG_NX_SDIO0_CLOCK_MHZ_50 */
/* #undef NX_CONFIG_NX_SDIO1_BUS_WIDTH_1BIT */
/* #undef NX_CONFIG_NX_SDIO1_BUS_WIDTH_4BIT */
/* #undef NX_CONFIG_NX_SDIO1_BUS_WIDTH_8BIT */
/* #undef NX_CONFIG_NX_SDIO1_CARD_SDHC */
/* #undef NX_CONFIG_NX_SDIO1_CARD_SDSC */
/* #undef NX_CONFIG_NX_SDIO1_CARD_SDXC */
/* #undef NX_CONFIG_NX_SDIO1_CLOCK_KHZ_400 */
/* #undef NX_CONFIG_NX_SDIO1_CLOCK_MHZ_100 */
/* #undef NX_CONFIG_NX_SDIO1_CLOCK_MHZ_200 */
/* #undef NX_CONFIG_NX_SDIO1_CLOCK_MHZ_25 */
/* #undef NX_CONFIG_NX_SDIO1_CLOCK_MHZ_50 */
#define NX_CONFIG_SDIO0_BLOCK_SIZE 512
#define NX_CONFIG_SDIO0_BUS_WIDTH_VALUE 4
#define NX_CONFIG_SDIO0_CARD_PRESENT 1
#define NX_CONFIG_SDIO0_CARD_TYPE_VALUE 1
#define NX_CONFIG_SDIO0_CARD_VALUE 1
#define NX_CONFIG_SDIO0_CLOCK_SPEED 25000000
#define NX_CONFIG_SDIO0_CLOCK_VALUE 1
#define NX_CONFIG_SDIO0_NUM_BLOCKS 1024
/* #undef NX_CONFIG_SDIO1_CARD_PRESENT */

/*---------------------------------------------------------------------------*/
/* SPI Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_INSTANCE_NX_SPI_0 1
/* #undef NX_CONFIG_INSTANCE_NX_SPI_1 */
/* #undef NX_CONFIG_INSTANCE_NX_SPI_2 */
/* #undef NX_CONFIG_INSTANCE_NX_SPI_3 */
#define NX_CONFIG_NATIVE_SPI_ENABLE 1
#define NX_CONFIG_NX_SPI0_CPHA_EDGE_1 1
/* #undef NX_CONFIG_NX_SPI0_CPHA_EDGE_2 */
/* #undef NX_CONFIG_NX_SPI0_CPOL_HIGH */
#define NX_CONFIG_NX_SPI0_CPOL_LOW 1
/* #undef NX_CONFIG_NX_SPI0_LSB_FIRST */
#define NX_CONFIG_NX_SPI0_MSB_FIRST 1
/* #undef NX_CONFIG_NX_SPI1_CPHA_EDGE_1 */
/* #undef NX_CONFIG_NX_SPI1_CPHA_EDGE_2 */
/* #undef NX_CONFIG_NX_SPI1_CPOL_HIGH */
/* #undef NX_CONFIG_NX_SPI1_CPOL_LOW */
/* #undef NX_CONFIG_NX_SPI1_LSB_FIRST */
/* #undef NX_CONFIG_NX_SPI1_MSB_FIRST */
/* #undef NX_CONFIG_NX_SPI2_CPHA_EDGE_1 */
/* #undef NX_CONFIG_NX_SPI2_CPHA_EDGE_2 */
/* #undef NX_CONFIG_NX_SPI2_CPOL_HIGH */
/* #undef NX_CONFIG_NX_SPI2_CPOL_LOW */
/* #undef NX_CONFIG_NX_SPI2_LSB_FIRST */
/* #undef NX_CONFIG_NX_SPI2_MSB_FIRST */
/* #undef NX_CONFIG_NX_SPI3_CPHA_EDGE_1 */
/* #undef NX_CONFIG_NX_SPI3_CPHA_EDGE_2 */
/* #undef NX_CONFIG_NX_SPI3_CPOL_HIGH */
/* #undef NX_CONFIG_NX_SPI3_CPOL_LOW */
/* #undef NX_CONFIG_NX_SPI3_LSB_FIRST */
/* #undef NX_CONFIG_NX_SPI3_MSB_FIRST */
#define NX_CONFIG_SPI0_BIT_ORDER_VALUE 0
#define NX_CONFIG_SPI0_CPHA_VALUE 0
#define NX_CONFIG_SPI0_CPOL_VALUE 0
#define NX_CONFIG_SPI0_MAX_SPEED 1000000
#define NX_CONFIG_SPI0_MSB_VALUE 0
#define NX_CONFIG_SPI0_RX_BUFFER_SIZE 256
#define NX_CONFIG_SPI0_TX_BUFFER_SIZE 256
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
#define NX_CONFIG_NATIVE_TIMER_ENABLE 1
/* #undef NX_CONFIG_NX_TIMER0_MODE_CENTER */
/* #undef NX_CONFIG_NX_TIMER0_MODE_DOWN */
#define NX_CONFIG_NX_TIMER0_MODE_UP 1
/* #undef NX_CONFIG_NX_TIMER1_MODE_CENTER */
/* #undef NX_CONFIG_NX_TIMER1_MODE_DOWN */
/* #undef NX_CONFIG_NX_TIMER1_MODE_UP */
/* #undef NX_CONFIG_NX_TIMER2_MODE_CENTER */
/* #undef NX_CONFIG_NX_TIMER2_MODE_DOWN */
/* #undef NX_CONFIG_NX_TIMER2_MODE_UP */
/* #undef NX_CONFIG_NX_TIMER3_MODE_CENTER */
/* #undef NX_CONFIG_NX_TIMER3_MODE_DOWN */
/* #undef NX_CONFIG_NX_TIMER3_MODE_UP */
/* #undef NX_CONFIG_STM32_TIMER_ENABLE */
#define NX_CONFIG_TIMER0_CHANNEL_COUNT 4
#define NX_CONFIG_TIMER0_FREQUENCY 1000000
#define NX_CONFIG_TIMER0_MODE_VALUE 0

/*---------------------------------------------------------------------------*/
/* UART Configuration                                                        */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_INSTANCE_NX_UART_0 1
#define NX_CONFIG_INSTANCE_NX_UART_1 1
#define NX_CONFIG_INSTANCE_NX_UART_2 1
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
#define NX_CONFIG_NX_UART1_MODE_INTERRUPT 1
/* #undef NX_CONFIG_NX_UART1_MODE_POLLING */
/* #undef NX_CONFIG_NX_UART1_PARITY_EVEN */
#define NX_CONFIG_NX_UART1_PARITY_NONE 1
/* #undef NX_CONFIG_NX_UART1_PARITY_ODD */
/* #undef NX_CONFIG_NX_UART2_MODE_DMA */
#define NX_CONFIG_NX_UART2_MODE_INTERRUPT 1
/* #undef NX_CONFIG_NX_UART2_MODE_POLLING */
/* #undef NX_CONFIG_NX_UART2_PARITY_EVEN */
#define NX_CONFIG_NX_UART2_PARITY_NONE 1
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
#define NX_CONFIG_UART1_BAUDRATE 115200
#define NX_CONFIG_UART1_DATA_BITS 8
#define NX_CONFIG_UART1_MODE_VALUE 1
#define NX_CONFIG_UART1_PARITY_VALUE 0
#define NX_CONFIG_UART1_RX_BUFFER_SIZE 256
#define NX_CONFIG_UART1_STOP_BITS 1
#define NX_CONFIG_UART1_TX_BUFFER_SIZE 256
#define NX_CONFIG_UART2_BAUDRATE 115200
#define NX_CONFIG_UART2_DATA_BITS 8
#define NX_CONFIG_UART2_MODE_VALUE 1
#define NX_CONFIG_UART2_PARITY_VALUE 0
#define NX_CONFIG_UART2_RX_BUFFER_SIZE 256
#define NX_CONFIG_UART2_STOP_BITS 1
#define NX_CONFIG_UART2_TX_BUFFER_SIZE 256

/*---------------------------------------------------------------------------*/
/* USB Configuration                                                         */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_INSTANCE_NX_USB_0 1
/* #undef NX_CONFIG_INSTANCE_NX_USB_1 */
#define NX_CONFIG_NATIVE_USB_ENABLE 1
#define NX_CONFIG_NX_USB0_POWER_BUS 1
/* #undef NX_CONFIG_NX_USB0_POWER_SELF */
#define NX_CONFIG_NX_USB0_SPEED_FULL 1
/* #undef NX_CONFIG_NX_USB0_SPEED_HIGH */
/* #undef NX_CONFIG_NX_USB0_SPEED_LOW */
/* #undef NX_CONFIG_NX_USB1_POWER_BUS */
/* #undef NX_CONFIG_NX_USB1_POWER_SELF */
/* #undef NX_CONFIG_NX_USB1_SPEED_FULL */
/* #undef NX_CONFIG_NX_USB1_SPEED_HIGH */
/* #undef NX_CONFIG_NX_USB1_SPEED_LOW */
#define NX_CONFIG_USB0_MAX_POWER_MA 500
#define NX_CONFIG_USB0_NUM_ENDPOINTS 8
#define NX_CONFIG_USB0_POWER_VALUE 0
#define NX_CONFIG_USB0_RX_BUFFER_SIZE 1024
#define NX_CONFIG_USB0_SPEED_VALUE 1
#define NX_CONFIG_USB0_TX_BUFFER_SIZE 1024

/*---------------------------------------------------------------------------*/
/* Watchdog Configuration                                                    */
/*---------------------------------------------------------------------------*/

#define NX_CONFIG_INSTANCE_NX_WATCHDOG_0 1
/* #undef NX_CONFIG_INSTANCE_NX_WATCHDOG_1 */
/* #undef NX_CONFIG_INSTANCE_NX_WATCHDOG_2 */
/* #undef NX_CONFIG_INSTANCE_NX_WATCHDOG_3 */
#define NX_CONFIG_NATIVE_WATCHDOG_ENABLE 1
#define NX_CONFIG_WATCHDOG0_DEFAULT_TIMEOUT_MS 5000

/*---------------------------------------------------------------------------*/
/* Peripheral Instance Traversal Macros                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           GPIO instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * GPIO instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_GPIO(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(A, 0) MY_REGISTER_FUNC(A, 1) ...
 */
#define NX_DEFINE_INSTANCE_NX_GPIO(fn) \
    fn(A, 0) \
    fn(A, 1) \
    fn(A, 2) \
    fn(B, 0)

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
#define NX_CONFIG_ADC_BUFFER_0_ENABLED 1
#else
#define _NX_ADC_BUFFER_INSTANCE_0(fn)
#define NX_CONFIG_ADC_BUFFER_0_ENABLED 0
#endif

/**
 * \brief           CRC instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * CRC instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_CRC(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_CRC(fn) \
    _NX_CRC_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_CRC_0
#define _NX_CRC_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_CRC0_ENABLED 1
#else
#define _NX_CRC_INSTANCE_0(fn)
#define NX_CONFIG_CRC0_ENABLED 0
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
 * \brief           INTERNAL_FLASH instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * INTERNAL_FLASH instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_INTERNAL_FLASH(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_INTERNAL_FLASH(fn) \
    _NX_INTERNAL_FLASH_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_INTERNAL_FLASH0
#define _NX_INTERNAL_FLASH_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_INTERNAL_FLASH0_ENABLED 1
#else
#define _NX_INTERNAL_FLASH_INSTANCE_0(fn)
#define NX_CONFIG_INTERNAL_FLASH0_ENABLED 0
#endif

/**
 * \brief           OPTION_BYTES instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * OPTION_BYTES instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_OPTION_BYTES(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_OPTION_BYTES(fn) \
    _NX_OPTION_BYTES_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_OPTION_BYTES_0
#define _NX_OPTION_BYTES_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_OPTION_BYTES_0_ENABLED 1
#else
#define _NX_OPTION_BYTES_INSTANCE_0(fn)
#define NX_CONFIG_OPTION_BYTES_0_ENABLED 0
#endif

/**
 * \brief           RTC instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * RTC instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_RTC(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_RTC(fn) \
    _NX_RTC_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_RTC_0
#define _NX_RTC_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_RTC0_ENABLED 1
#else
#define _NX_RTC_INSTANCE_0(fn)
#define NX_CONFIG_RTC0_ENABLED 0
#endif

/**
 * \brief           SDIO instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * SDIO instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_SDIO(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_SDIO(fn) \
    _NX_SDIO_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_SDIO_0
#define _NX_SDIO_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_SDIO0_ENABLED 1
#else
#define _NX_SDIO_INSTANCE_0(fn)
#define NX_CONFIG_SDIO0_ENABLED 0
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
 *   MY_REGISTER_FUNC(0) MY_REGISTER_FUNC(1) MY_REGISTER_FUNC(2)
 */
#define NX_DEFINE_INSTANCE_NX_UART(fn) \
    _NX_UART_INSTANCE_0(fn) \
    _NX_UART_INSTANCE_1(fn) \
    _NX_UART_INSTANCE_2(fn)

#ifdef NX_CONFIG_INSTANCE_NX_UART_0
#define _NX_UART_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_UART0_ENABLED 1
#else
#define _NX_UART_INSTANCE_0(fn)
#define NX_CONFIG_UART0_ENABLED 0
#endif

#ifdef NX_CONFIG_INSTANCE_NX_UART_1
#define _NX_UART_INSTANCE_1(fn) fn(1)
#define NX_CONFIG_UART1_ENABLED 1
#else
#define _NX_UART_INSTANCE_1(fn)
#define NX_CONFIG_UART1_ENABLED 0
#endif

#ifdef NX_CONFIG_INSTANCE_NX_UART_2
#define _NX_UART_INSTANCE_2(fn) fn(2)
#define NX_CONFIG_UART2_ENABLED 1
#else
#define _NX_UART_INSTANCE_2(fn)
#define NX_CONFIG_UART2_ENABLED 0
#endif

/**
 * \brief           USB instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * USB instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_USB(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_USB(fn) \
    _NX_USB_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_USB_0
#define _NX_USB_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_USB0_ENABLED 1
#else
#define _NX_USB_INSTANCE_0(fn)
#define NX_CONFIG_USB0_ENABLED 0
#endif

/**
 * \brief           WATCHDOG instance traversal macro
 *
 * This macro expands to call the provided function for each enabled
 * WATCHDOG instance. Used by the device registration system.
 *
 * Example:
 *   NX_DEFINE_INSTANCE_NX_WATCHDOG(MY_REGISTER_FUNC)
 *   expands to:
 *   MY_REGISTER_FUNC(0)
 */
#define NX_DEFINE_INSTANCE_NX_WATCHDOG(fn) \
    _NX_WATCHDOG_INSTANCE_0(fn)

#ifdef NX_CONFIG_INSTANCE_NX_WATCHDOG_0
#define _NX_WATCHDOG_INSTANCE_0(fn) fn(0)
#define NX_CONFIG_WATCHDOG0_ENABLED 1
#else
#define _NX_WATCHDOG_INSTANCE_0(fn)
#define NX_CONFIG_WATCHDOG0_ENABLED 0
#endif

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_CONFIG_H */
