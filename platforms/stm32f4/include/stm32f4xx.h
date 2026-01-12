/**
 * \file            stm32f4xx.h
 * \brief           STM32F4xx Device Header (Simplified)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-12
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \note            This is a simplified header for demonstration.
 *                  In production, use the official CMSIS headers.
 */

#ifndef STM32F4XX_H
#define STM32F4XX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Peripheral base addresses
 */
#define PERIPH_BASE         0x40000000UL
#define AHB1PERIPH_BASE     (PERIPH_BASE + 0x00020000UL)
#define APB1PERIPH_BASE     PERIPH_BASE
#define APB2PERIPH_BASE     (PERIPH_BASE + 0x00010000UL)

#define GPIOA_BASE          (AHB1PERIPH_BASE + 0x0000UL)
#define GPIOB_BASE          (AHB1PERIPH_BASE + 0x0400UL)
#define GPIOC_BASE          (AHB1PERIPH_BASE + 0x0800UL)
#define GPIOD_BASE          (AHB1PERIPH_BASE + 0x0C00UL)
#define GPIOE_BASE          (AHB1PERIPH_BASE + 0x1000UL)
#define GPIOF_BASE          (AHB1PERIPH_BASE + 0x1400UL)
#define GPIOG_BASE          (AHB1PERIPH_BASE + 0x1800UL)
#define GPIOH_BASE          (AHB1PERIPH_BASE + 0x1C00UL)

#define RCC_BASE            (AHB1PERIPH_BASE + 0x3800UL)

#define USART1_BASE         (APB2PERIPH_BASE + 0x1000UL)
#define USART2_BASE         (APB1PERIPH_BASE + 0x4400UL)
#define USART3_BASE         (APB1PERIPH_BASE + 0x4800UL)

/**
 * \brief           GPIO Register Structure
 */
typedef struct {
    volatile uint32_t MODER;    /**< Mode register */
    volatile uint32_t OTYPER;   /**< Output type register */
    volatile uint32_t OSPEEDR;  /**< Output speed register */
    volatile uint32_t PUPDR;    /**< Pull-up/pull-down register */
    volatile uint32_t IDR;      /**< Input data register */
    volatile uint32_t ODR;      /**< Output data register */
    volatile uint32_t BSRR;     /**< Bit set/reset register */
    volatile uint32_t LCKR;     /**< Lock register */
    volatile uint32_t AFR[2];   /**< Alternate function registers */
} GPIO_TypeDef;

/**
 * \brief           RCC Register Structure
 */
typedef struct {
    volatile uint32_t CR;       /**< Clock control register */
    volatile uint32_t PLLCFGR;  /**< PLL configuration register */
    volatile uint32_t CFGR;     /**< Clock configuration register */
    volatile uint32_t CIR;      /**< Clock interrupt register */
    volatile uint32_t AHB1RSTR; /**< AHB1 reset register */
    volatile uint32_t AHB2RSTR; /**< AHB2 reset register */
    volatile uint32_t AHB3RSTR; /**< AHB3 reset register */
    uint32_t RESERVED0;
    volatile uint32_t APB1RSTR; /**< APB1 reset register */
    volatile uint32_t APB2RSTR; /**< APB2 reset register */
    uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;  /**< AHB1 enable register */
    volatile uint32_t AHB2ENR;  /**< AHB2 enable register */
    volatile uint32_t AHB3ENR;  /**< AHB3 enable register */
    uint32_t RESERVED2;
    volatile uint32_t APB1ENR;  /**< APB1 enable register */
    volatile uint32_t APB2ENR;  /**< APB2 enable register */
} RCC_TypeDef;

/**
 * \brief           USART Register Structure
 */
typedef struct {
    volatile uint32_t SR;       /**< Status register */
    volatile uint32_t DR;       /**< Data register */
    volatile uint32_t BRR;      /**< Baud rate register */
    volatile uint32_t CR1;      /**< Control register 1 */
    volatile uint32_t CR2;      /**< Control register 2 */
    volatile uint32_t CR3;      /**< Control register 3 */
    volatile uint32_t GTPR;     /**< Guard time and prescaler */
} USART_TypeDef;

/**
 * \brief           Peripheral instances
 */
#define GPIOA   ((GPIO_TypeDef*)GPIOA_BASE)
#define GPIOB   ((GPIO_TypeDef*)GPIOB_BASE)
#define GPIOC   ((GPIO_TypeDef*)GPIOC_BASE)
#define GPIOD   ((GPIO_TypeDef*)GPIOD_BASE)
#define GPIOE   ((GPIO_TypeDef*)GPIOE_BASE)
#define GPIOF   ((GPIO_TypeDef*)GPIOF_BASE)
#define GPIOG   ((GPIO_TypeDef*)GPIOG_BASE)
#define GPIOH   ((GPIO_TypeDef*)GPIOH_BASE)

#define RCC     ((RCC_TypeDef*)RCC_BASE)

#define USART1  ((USART_TypeDef*)USART1_BASE)
#define USART2  ((USART_TypeDef*)USART2_BASE)
#define USART3  ((USART_TypeDef*)USART3_BASE)

/**
 * \brief           RCC AHB1ENR bit definitions
 */
#define RCC_AHB1ENR_GPIOAEN     (1UL << 0)
#define RCC_AHB1ENR_GPIOBEN     (1UL << 1)
#define RCC_AHB1ENR_GPIOCEN     (1UL << 2)
#define RCC_AHB1ENR_GPIODEN     (1UL << 3)
#define RCC_AHB1ENR_GPIOEEN     (1UL << 4)
#define RCC_AHB1ENR_GPIOFEN     (1UL << 5)
#define RCC_AHB1ENR_GPIOGEN     (1UL << 6)
#define RCC_AHB1ENR_GPIOHEN     (1UL << 7)

/**
 * \brief           System clock frequency
 */
extern uint32_t SystemCoreClock;

/**
 * \brief           System initialization
 */
void SystemInit(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32F4XX_H */
