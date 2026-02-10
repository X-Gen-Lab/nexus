/**
 * \file            stm32f1_it.c
 * \brief           STM32F1 interrupt handlers
 * \author          Nexus Team
 * \version         2.0.0
 * \date            2026-02-02
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Interrupt service routines for STM32F1 series.
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

#if defined(STM32F103xB) || defined(STM32F103xE) || defined(STM32F1)

/*---------------------------------------------------------------------------*/
/* Includes                                                                  */
/*---------------------------------------------------------------------------*/

#include "interrupt/stm32_interrupt.h"
#include "stm32f1xx_hal.h"


/*---------------------------------------------------------------------------*/
/* Cortex-M3 Processor Exceptions                                            */
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
/* STM32F1 Peripheral Interrupts                                             */
/*---------------------------------------------------------------------------*/

/* Window Watchdog */
STM32_ISR_HANDLER(WWDG_IRQHandler, WWDG_IRQn)

/* PVD through EXTI Line detection */
STM32_ISR_HANDLER(PVD_IRQHandler, PVD_IRQn)

/* Tamper */
STM32_ISR_HANDLER(TAMPER_IRQHandler, TAMPER_IRQn)

/* RTC */
STM32_ISR_HANDLER(RTC_IRQHandler, RTC_IRQn)

/* Flash */
STM32_ISR_HANDLER(FLASH_IRQHandler, FLASH_IRQn)

/* RCC */
STM32_ISR_HANDLER(RCC_IRQHandler, RCC_IRQn)

/* EXTI Line 0 */
STM32_ISR_HANDLER(EXTI0_IRQHandler, EXTI0_IRQn)

/* EXTI Line 1 */
STM32_ISR_HANDLER(EXTI1_IRQHandler, EXTI1_IRQn)

/* EXTI Line 2 */
STM32_ISR_HANDLER(EXTI2_IRQHandler, EXTI2_IRQn)

/* EXTI Line 3 */
STM32_ISR_HANDLER(EXTI3_IRQHandler, EXTI3_IRQn)

/* EXTI Line 4 */
STM32_ISR_HANDLER(EXTI4_IRQHandler, EXTI4_IRQn)

/* DMA1 Channel 1 */
STM32_ISR_HANDLER(DMA1_Channel1_IRQHandler, DMA1_Channel1_IRQn)

/* DMA1 Channel 2 */
STM32_ISR_HANDLER(DMA1_Channel2_IRQHandler, DMA1_Channel2_IRQn)

/* DMA1 Channel 3 */
STM32_ISR_HANDLER(DMA1_Channel3_IRQHandler, DMA1_Channel3_IRQn)

/* DMA1 Channel 4 */
STM32_ISR_HANDLER(DMA1_Channel4_IRQHandler, DMA1_Channel4_IRQn)

/* DMA1 Channel 5 */
STM32_ISR_HANDLER(DMA1_Channel5_IRQHandler, DMA1_Channel5_IRQn)

/* DMA1 Channel 6 */
STM32_ISR_HANDLER(DMA1_Channel6_IRQHandler, DMA1_Channel6_IRQn)

/* DMA1 Channel 7 */
STM32_ISR_HANDLER(DMA1_Channel7_IRQHandler, DMA1_Channel7_IRQn)

/* ADC1 and ADC2 */
STM32_ISR_HANDLER(ADC1_2_IRQHandler, ADC1_2_IRQn)

/* USB High Priority or CAN TX */
STM32_ISR_HANDLER(USB_HP_CAN1_TX_IRQHandler, USB_HP_CAN1_TX_IRQn)

/* USB Low Priority or CAN RX0 */
STM32_ISR_HANDLER(USB_LP_CAN1_RX0_IRQHandler, USB_LP_CAN1_RX0_IRQn)

/* CAN RX1 */
STM32_ISR_HANDLER(CAN1_RX1_IRQHandler, CAN1_RX1_IRQn)

/* CAN SCE */
STM32_ISR_HANDLER(CAN1_SCE_IRQHandler, CAN1_SCE_IRQn)

/* EXTI Line[9:5] */
STM32_ISR_HANDLER(EXTI9_5_IRQHandler, EXTI9_5_IRQn)

/* TIM1 Break */
STM32_ISR_HANDLER(TIM1_BRK_IRQHandler, TIM1_BRK_IRQn)

/* TIM1 Update */
STM32_ISR_HANDLER(TIM1_UP_IRQHandler, TIM1_UP_IRQn)

/* TIM1 Trigger and Commutation */
STM32_ISR_HANDLER(TIM1_TRG_COM_IRQHandler, TIM1_TRG_COM_IRQn)

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
STM32_ISR_HANDLER(RTCAlarm_IRQHandler, RTCAlarm_IRQn)

/* USB Wakeup from suspend */
STM32_ISR_HANDLER(USBWakeUp_IRQHandler, USBWakeUp_IRQn)

#if defined(STM32F103xE)
/* TIM8 Break */
STM32_ISR_HANDLER(TIM8_BRK_IRQHandler, TIM8_BRK_IRQn)

/* TIM8 Update */
STM32_ISR_HANDLER(TIM8_UP_IRQHandler, TIM8_UP_IRQn)

/* TIM8 Trigger and Commutation */
STM32_ISR_HANDLER(TIM8_TRG_COM_IRQHandler, TIM8_TRG_COM_IRQn)

/* TIM8 Capture Compare */
STM32_ISR_HANDLER(TIM8_CC_IRQHandler, TIM8_CC_IRQn)

/* ADC3 */
STM32_ISR_HANDLER(ADC3_IRQHandler, ADC3_IRQn)

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

/* TIM6 */
STM32_ISR_HANDLER(TIM6_IRQHandler, TIM6_IRQn)

/* TIM7 */
STM32_ISR_HANDLER(TIM7_IRQHandler, TIM7_IRQn)

/* DMA2 Channel 1 */
STM32_ISR_HANDLER(DMA2_Channel1_IRQHandler, DMA2_Channel1_IRQn)

/* DMA2 Channel 2 */
STM32_ISR_HANDLER(DMA2_Channel2_IRQHandler, DMA2_Channel2_IRQn)

/* DMA2 Channel 3 */
STM32_ISR_HANDLER(DMA2_Channel3_IRQHandler, DMA2_Channel3_IRQn)

/* DMA2 Channel 4 and Channel 5 */
STM32_ISR_HANDLER(DMA2_Channel4_5_IRQHandler, DMA2_Channel4_5_IRQn)
#endif /* STM32F103xE */

#endif /* STM32F1 */
