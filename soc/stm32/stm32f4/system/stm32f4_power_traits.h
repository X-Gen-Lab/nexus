/**
 * \file            stm32f4_power_traits.h
 * \brief           Exact private F4 deep-power register and interrupt traits
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_POWER_TRAITS_H
#define NEXUS_STM32F4_POWER_TRAITS_H

#include "stm32f4xx.h"

#if defined(STM32F411xE)
#define NX_POWER_IRQ_COUNT 86U
#define NX_POWER_GPIO_GATES                                                    \
    (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN |         \
     RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOHEN)
#define NX_POWER_WAKE_PA0  PWR_CSR_EWUP
#define NX_POWER_WAKE_PC13 0U
#elif defined(STM32F446xx)
#define NX_POWER_IRQ_COUNT 97U
#define NX_POWER_GPIO_GATES                                                    \
    (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN |         \
     RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOFEN |         \
     RCC_AHB1ENR_GPIOGEN | RCC_AHB1ENR_GPIOHEN)
/* RM0390 PWR_CSR, locked LL_PWR_WAKEUP_PIN1/2 and DS10693 exact pad table. */
#define NX_POWER_WAKE_PA0  PWR_CSR_EWUP1
#define NX_POWER_WAKE_PC13 PWR_CSR_EWUP2
#elif defined(STM32F479xx)
#define NX_POWER_IRQ_COUNT 93U
#define NX_POWER_GPIO_GATES                                                    \
    (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN |         \
     RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOFEN |         \
     RCC_AHB1ENR_GPIOGEN | RCC_AHB1ENR_GPIOHEN | RCC_AHB1ENR_GPIOIEN |         \
     RCC_AHB1ENR_GPIOJEN | RCC_AHB1ENR_GPIOKEN)
#define NX_POWER_WAKE_PA0  PWR_CSR_EWUP
#define NX_POWER_WAKE_PC13 0U
#elif defined(STM32F405xx) || defined(STM32F407xx) || defined(STM32F415xx) ||  \
    defined(STM32F417xx)
#define NX_POWER_IRQ_COUNT 82U
#define NX_POWER_GPIO_GATES                                                    \
    (RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN |         \
     RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_GPIOFEN |         \
     RCC_AHB1ENR_GPIOGEN | RCC_AHB1ENR_GPIOHEN | RCC_AHB1ENR_GPIOIEN)
#define NX_POWER_WAKE_PA0  PWR_CSR_EWUP
#define NX_POWER_WAKE_PC13 0U
#else
#error "Deep power requires an independently reviewed exact F4 device"
#endif

#define NX_POWER_IRQ_BANKS ((NX_POWER_IRQ_COUNT + 31U) / 32U)
#define NX_POWER_WAKE_BITS (NX_POWER_WAKE_PA0 | NX_POWER_WAKE_PC13)

#if defined(RCC_AHB1ENR_CCMDATARAMEN)
#define NX_POWER_CCM_GATE RCC_AHB1ENR_CCMDATARAMEN
#else
#define NX_POWER_CCM_GATE 0U
#endif
#if defined(RCC_AHB1ENR_BKPSRAMEN)
#define NX_POWER_BACKUP_GATE RCC_AHB1ENR_BKPSRAMEN
#else
#define NX_POWER_BACKUP_GATE 0U
#endif

/** \brief Ordinary CSR controls never acknowledge historical UDRDY status.
 * \note RM0390 5.4.2 / RM0386 5.6.2 define both UDRDY bits as rc_w1.
 */
static inline uint32_t nx_stm32_power_csr_control(uint32_t value) {
#if defined(PWR_CSR_UDRDY)
    return value & ~(uint32_t)PWR_CSR_UDRDY;
#else
    return value;
#endif
}

#endif /* NEXUS_STM32F4_POWER_TRAITS_H */
