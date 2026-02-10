/**
 * \file            stm32f4_it.c
 * \brief           STM32F4 interrupt handlers
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-02-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Interrupt service routines for STM32F4 series.
 *                  Uses STM32_ISR_HANDLER macros for automatic dispatch.
 */

/*
 * Copyright (c) 2026 Nexus Team
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of Nexus framework.
 *
 * Author:          Nexus Team
 */

#if defined(STM32F407xx) || defined(STM32F429xx) || defined(STM32F446xx) ||    \
    defined(STM32F4)

/*---------------------------------------------------------------------------*/
/* Includes                                                                  */
/*---------------------------------------------------------------------------*/

#include "interrupt/stm32_interrupt.h"
#include "stm32f4xx_hal.h"


/*---------------------------------------------------------------------------*/
/* Cortex-M4 Processor Exceptions                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Non-maskable interrupt handler
 */
void NMI_Handler(void) {
    while (1) {
    }
}

/**
 * \brief           Hard fault interrupt handler
 */
void HardFault_Handler(void) {
    while (1) {
    }
}

/**
 * \brief           Memory management fault handler
 */
void MemManage_Handler(void) {
    while (1) {
    }
}

/**
 * \brief           Bus fault handler
 */
void BusFault_Handler(void) {
    while (1) {
    }
}

/**
 * \brief           Usage fault handler
 */
void UsageFault_Handler(void) {
    while (1) {
    }
}

/**
 * \brief           SVC handler
 */
void SVC_Handler(void) {
}

/**
 * \brief           Debug monitor handler
 */
void DebugMon_Handler(void) {
}

/**
 * \brief           PendSV handler
 */
void PendSV_Handler(void) {
}

/**
 * \brief           SysTick timer interrupt handler
 */
void SysTick_Handler(void) {
    HAL_IncTick();
}

/*---------------------------------------------------------------------------*/
/* STM32F4 Peripheral Interrupts                                             */
/*---------------------------------------------------------------------------*/

/* Window Watchdog */
STM32_ISR_HANDLER(WWDG_IRQHandler, WWDG_IRQn)

/* PVD through EXTI Line detection */
STM32_ISR_HANDLER(PVD_IRQHandler, PVD_IRQn)

/* Tamper and TimeStamp through EXTI */
STM32_ISR_HANDLER(TAMP_STAMP_IRQHandler, TAMP_STAMP_IRQn)

/* RTC Wakeup through EXTI */
STM32_ISR_HANDLER(RTC_WKUP_IRQHandler, RTC_WKUP_IRQn)

/* Flash global interrupt */
STM32_ISR_HANDLER(FLASH_IRQHandler, FLASH_IRQn)

/* RCC global interrupt */
STM32_ISR_HANDLER(RCC_IRQHandler, RCC_IRQn)

/* EXTI Line0 */
STM32_ISR_HANDLER(EXTI0_IRQHandler, EXTI0_IRQn)

/* EXTI Line1 */
STM32_ISR_HANDLER(EXTI1_IRQHandler, EXTI1_IRQn)

/* EXTI Line2 */
STM32_ISR_HANDLER(EXTI2_IRQHandler, EXTI2_IRQn)

/* EXTI Line3 */
STM32_ISR_HANDLER(EXTI3_IRQHandler, EXTI3_IRQn)

/* EXTI Line4 */
STM32_ISR_HANDLER(EXTI4_IRQHandler, EXTI4_IRQn)

/* DMA1 Stream 0 */
STM32_ISR_HANDLER(DMA1_Stream0_IRQHandler, DMA1_Stream0_IRQn)

/* DMA1 Stream 1 */
STM32_ISR_HANDLER(DMA1_Stream1_IRQHandler, DMA1_Stream1_IRQn)

/* DMA1 Stream 2 */
STM32_ISR_HANDLER(DMA1_Stream2_IRQHandler, DMA1_Stream2_IRQn)

/* DMA1 Stream 3 */
STM32_ISR_HANDLER(DMA1_Stream3_IRQHandler, DMA1_Stream3_IRQn)

/* DMA1 Stream 4 */
STM32_ISR_HANDLER(DMA1_Stream4_IRQHandler, DMA1_Stream4_IRQn)

/* DMA1 Stream 5 */
STM32_ISR_HANDLER(DMA1_Stream5_IRQHandler, DMA1_Stream5_IRQn)

/* DMA1 Stream 6 */
STM32_ISR_HANDLER(DMA1_Stream6_IRQHandler, DMA1_Stream6_IRQn)

/* ADC1, ADC2 and ADC3 */
STM32_ISR_HANDLER(ADC_IRQHandler, ADC_IRQn)

/* CAN1 TX */
STM32_ISR_HANDLER(CAN1_TX_IRQHandler, CAN1_TX_IRQn)

/* CAN1 RX0 */
STM32_ISR_HANDLER(CAN1_RX0_IRQHandler, CAN1_RX0_IRQn)

/* CAN1 RX1 */
STM32_ISR_HANDLER(CAN1_RX1_IRQHandler, CAN1_RX1_IRQn)

/* CAN1 SCE */
STM32_ISR_HANDLER(CAN1_SCE_IRQHandler, CAN1_SCE_IRQn)

/* EXTI Line[9:5] */
STM32_ISR_HANDLER(EXTI9_5_IRQHandler, EXTI9_5_IRQn)

/* TIM1 Break and TIM9 */
STM32_ISR_HANDLER(TIM1_BRK_TIM9_IRQHandler, TIM1_BRK_TIM9_IRQn)

/* TIM1 Update and TIM10 */
STM32_ISR_HANDLER(TIM1_UP_TIM10_IRQHandler, TIM1_UP_TIM10_IRQn)

/* TIM1 Trigger and Commutation and TIM11 */
STM32_ISR_HANDLER(TIM1_TRG_COM_TIM11_IRQHandler, TIM1_TRG_COM_TIM11_IRQn)

/* TIM1 Capture Compare */
STM32_ISR_HANDLER(TIM1_CC_IRQHandler, TIM1_CC_IRQn)

/* TIM2 */
STM32_ISR_HANDLER(TIM2_IRQHandler, TIM2_IRQn)

/* TIM3 */
STM32_ISR_HANDLER(TIM3_IRQHandler, TIM3_IRQn)

/* TIM4 */
STM32_ISR_HANDLER(TIM4_IRQHandler, TIM4_IRQn)

/* I2C1 Event */
STM32_ISR_HANDLER(I2C1_EV_IRQHandler, I2C1_EV_IRQn)

/* I2C1 Error */
STM32_ISR_HANDLER(I2C1_ER_IRQHandler, I2C1_ER_IRQn)

/* I2C2 Event */
STM32_ISR_HANDLER(I2C2_EV_IRQHandler, I2C2_EV_IRQn)

/* I2C2 Error */
STM32_ISR_HANDLER(I2C2_ER_IRQHandler, I2C2_ER_IRQn)

/* SPI1 */
STM32_ISR_HANDLER(SPI1_IRQHandler, SPI1_IRQn)

/* SPI2 */
STM32_ISR_HANDLER(SPI2_IRQHandler, SPI2_IRQn)

/* USART1 */
STM32_ISR_HANDLER(USART1_IRQHandler, USART1_IRQn)

/* USART2 */
STM32_ISR_HANDLER(USART2_IRQHandler, USART2_IRQn)

/* USART3 */
STM32_ISR_HANDLER(USART3_IRQHandler, USART3_IRQn)

/* EXTI Line[15:10] */
STM32_ISR_HANDLER(EXTI15_10_IRQHandler, EXTI15_10_IRQn)

/* RTC Alarm through EXTI */
STM32_ISR_HANDLER(RTC_Alarm_IRQHandler, RTC_Alarm_IRQn)

/* USB OTG FS Wakeup through EXTI */
STM32_ISR_HANDLER(OTG_FS_WKUP_IRQHandler, OTG_FS_WKUP_IRQn)

/* TIM8 Break and TIM12 */
STM32_ISR_HANDLER(TIM8_BRK_TIM12_IRQHandler, TIM8_BRK_TIM12_IRQn)

/* TIM8 Update and TIM13 */
STM32_ISR_HANDLER(TIM8_UP_TIM13_IRQHandler, TIM8_UP_TIM13_IRQn)

/* TIM8 Trigger and Commutation and TIM14 */
STM32_ISR_HANDLER(TIM8_TRG_COM_TIM14_IRQHandler, TIM8_TRG_COM_TIM14_IRQn)

/* TIM8 Capture Compare */
STM32_ISR_HANDLER(TIM8_CC_IRQHandler, TIM8_CC_IRQn)

/* DMA1 Stream7 */
STM32_ISR_HANDLER(DMA1_Stream7_IRQHandler, DMA1_Stream7_IRQn)

/* FSMC */
STM32_ISR_HANDLER(FSMC_IRQHandler, FSMC_IRQn)

/* SDIO */
STM32_ISR_HANDLER(SDIO_IRQHandler, SDIO_IRQn)

/* TIM5 */
STM32_ISR_HANDLER(TIM5_IRQHandler, TIM5_IRQn)

/* SPI3 */
STM32_ISR_HANDLER(SPI3_IRQHandler, SPI3_IRQn)

/* UART4 */
STM32_ISR_HANDLER(UART4_IRQHandler, UART4_IRQn)

/* UART5 */
STM32_ISR_HANDLER(UART5_IRQHandler, UART5_IRQn)

/* TIM6 and DAC1&2 underrun errors */
STM32_ISR_HANDLER(TIM6_DAC_IRQHandler, TIM6_DAC_IRQn)

/* TIM7 */
STM32_ISR_HANDLER(TIM7_IRQHandler, TIM7_IRQn)

/* DMA2 Stream 0 */
STM32_ISR_HANDLER(DMA2_Stream0_IRQHandler, DMA2_Stream0_IRQn)

/* DMA2 Stream 1 */
STM32_ISR_HANDLER(DMA2_Stream1_IRQHandler, DMA2_Stream1_IRQn)

/* DMA2 Stream 2 */
STM32_ISR_HANDLER(DMA2_Stream2_IRQHandler, DMA2_Stream2_IRQn)

/* DMA2 Stream 3 */
STM32_ISR_HANDLER(DMA2_Stream3_IRQHandler, DMA2_Stream3_IRQn)

/* DMA2 Stream 4 */
STM32_ISR_HANDLER(DMA2_Stream4_IRQHandler, DMA2_Stream4_IRQn)

/* Ethernet */
STM32_ISR_HANDLER(ETH_IRQHandler, ETH_IRQn)

/* Ethernet Wakeup through EXTI */
STM32_ISR_HANDLER(ETH_WKUP_IRQHandler, ETH_WKUP_IRQn)

/* CAN2 TX */
STM32_ISR_HANDLER(CAN2_TX_IRQHandler, CAN2_TX_IRQn)

/* CAN2 RX0 */
STM32_ISR_HANDLER(CAN2_RX0_IRQHandler, CAN2_RX0_IRQn)

/* CAN2 RX1 */
STM32_ISR_HANDLER(CAN2_RX1_IRQHandler, CAN2_RX1_IRQn)

/* CAN2 SCE */
STM32_ISR_HANDLER(CAN2_SCE_IRQHandler, CAN2_SCE_IRQn)

/* USB OTG FS */
STM32_ISR_HANDLER(OTG_FS_IRQHandler, OTG_FS_IRQn)

/* DMA2 Stream 5 */
STM32_ISR_HANDLER(DMA2_Stream5_IRQHandler, DMA2_Stream5_IRQn)

/* DMA2 Stream 6 */
STM32_ISR_HANDLER(DMA2_Stream6_IRQHandler, DMA2_Stream6_IRQn)

/* DMA2 Stream 7 */
STM32_ISR_HANDLER(DMA2_Stream7_IRQHandler, DMA2_Stream7_IRQn)

/* USART6 */
STM32_ISR_HANDLER(USART6_IRQHandler, USART6_IRQn)

/* I2C3 event */
STM32_ISR_HANDLER(I2C3_EV_IRQHandler, I2C3_EV_IRQn)

/* I2C3 error */
STM32_ISR_HANDLER(I2C3_ER_IRQHandler, I2C3_ER_IRQn)

/* USB OTG HS End Point 1 Out */
STM32_ISR_HANDLER(OTG_HS_EP1_OUT_IRQHandler, OTG_HS_EP1_OUT_IRQn)

/* USB OTG HS End Point 1 In */
STM32_ISR_HANDLER(OTG_HS_EP1_IN_IRQHandler, OTG_HS_EP1_IN_IRQn)

/* USB OTG HS Wakeup through EXTI */
STM32_ISR_HANDLER(OTG_HS_WKUP_IRQHandler, OTG_HS_WKUP_IRQn)

/* USB OTG HS */
STM32_ISR_HANDLER(OTG_HS_IRQHandler, OTG_HS_IRQn)

/* DCMI */
STM32_ISR_HANDLER(DCMI_IRQHandler, DCMI_IRQn)

/* Hash and Rng */
STM32_ISR_HANDLER(HASH_RNG_IRQHandler, HASH_RNG_IRQn)

/* FPU */
STM32_ISR_HANDLER(FPU_IRQHandler, FPU_IRQn)

#endif /* STM32F4 */
