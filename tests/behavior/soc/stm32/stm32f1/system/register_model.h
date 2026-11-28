/**
 * \file            register_model.h
 * \brief           Inject a modeled F103 TIM2, RCC, Flash and vector.
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_STM32F1_REGISTER_MODEL_H
#define NEXUS_TEST_STM32F1_REGISTER_MODEL_H

#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern RCC_TypeDef f1_test_rcc;
extern FLASH_TypeDef f1_test_flash;
extern TIM_TypeDef f1_test_timer;
extern NVIC_Type f1_test_nvic;

uint16_t f1_test_flash_kib(void);
void f1_test_write(volatile uint32_t* reg, uint32_t value);
void f1_test_enable_irq(IRQn_Type irq);
void f1_test_disable_irq(IRQn_Type irq);
void f1_test_clear_pending_irq(IRQn_Type irq);

#undef RCC
#undef FLASH
#undef TIM2
#undef NVIC
#define RCC   (&f1_test_rcc)
#define FLASH (&f1_test_flash)
#define TIM2  (&f1_test_timer)
#define NVIC  (&f1_test_nvic)

#undef NVIC_EnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_ClearPendingIRQ
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPendingIRQ
#undef NVIC_GetActive
#undef NVIC_SetPriority
#undef NVIC_GetPriority
#define NVIC_EnableIRQ(irq)       f1_test_enable_irq(irq)
#define NVIC_DisableIRQ(irq)      f1_test_disable_irq(irq)
#define NVIC_ClearPendingIRQ(irq) f1_test_clear_pending_irq(irq)
#define NVIC_GetEnableIRQ(irq)    ((NVIC->ISER[0] >> (uint32_t)(irq)) & 1U)
#define NVIC_GetPendingIRQ(irq)   ((NVIC->ISPR[0] >> (uint32_t)(irq)) & 1U)
#define NVIC_GetActive(irq)       ((NVIC->IABR[0] >> (uint32_t)(irq)) & 1U)
#define NVIC_SetPriority(irq, priority)                                        \
    (NVIC->IPR[(uint32_t)(irq)] =                                              \
         (uint8_t)((priority) << (8U - __NVIC_PRIO_BITS)))
#define NVIC_GetPriority(irq)                                                  \
    ((uint32_t)NVIC->IPR[(uint32_t)(irq)] >> (8U - __NVIC_PRIO_BITS))

#define NX_STM32F1_WRITE(reg, value) f1_test_write((reg), (value))
#define NX_STM32F1_FLASH_KIB()       f1_test_flash_kib()

#if defined(TEST_F1_STOP)
extern AFIO_TypeDef f1_test_afio;
extern EXTI_TypeDef f1_test_exti;
extern PWR_TypeDef f1_test_power;
extern RTC_TypeDef f1_test_rtc;
extern SCB_Type f1_test_scb;
extern SysTick_Type f1_test_systick;
void f1_test_stop_wait(void);

#undef AFIO
#undef EXTI
#undef PWR
#undef RTC
#undef SCB
#undef SysTick
#define AFIO                   (&f1_test_afio)
#define EXTI                   (&f1_test_exti)
#define PWR                    (&f1_test_power)
#define RTC                    (&f1_test_rtc)
#define SCB                    (&f1_test_scb)
#define SysTick                (&f1_test_systick)
#define NX_STM32F1_STOP_WAIT() f1_test_stop_wait()
#endif

#ifdef __cplusplus
}
#endif
#endif
