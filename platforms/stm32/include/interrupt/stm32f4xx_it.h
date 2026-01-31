/**
 * \file            stm32f4xx_it.h
 * \brief           STM32F4xx interrupt handlers interface
 * \author          Nexus Team
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

#ifndef NEXUS_STM32F4XX_IT_H
#define NEXUS_STM32F4XX_IT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*---------------------------------------------------------------------------*/
/* Cortex-M4 Processor Exceptions Handlers                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Non-Maskable Interrupt handler
 */
void NMI_Handler(void);

/**
 * \brief           Hard Fault interrupt handler
 */
void HardFault_Handler(void);

/**
 * \brief           Memory Management Fault handler
 */
void MemManage_Handler(void);

/**
 * \brief           Bus Fault interrupt handler
 */
void BusFault_Handler(void);

/**
 * \brief           Usage Fault interrupt handler
 */
void UsageFault_Handler(void);

/**
 * \brief           Supervisor Call interrupt handler
 */
void SVC_Handler(void);

/**
 * \brief           Debug Monitor interrupt handler
 */
void DebugMon_Handler(void);

/**
 * \brief           PendSV interrupt handler
 */
void PendSV_Handler(void);

/**
 * \brief           SysTick interrupt handler
 */
void SysTick_Handler(void);

/*---------------------------------------------------------------------------*/
/* STM32F4xx Peripheral Interrupt Handlers                                  */
/*---------------------------------------------------------------------------*/

/* Add peripheral interrupt handler declarations here as needed */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* NEXUS_STM32F4XX_IT_H */
