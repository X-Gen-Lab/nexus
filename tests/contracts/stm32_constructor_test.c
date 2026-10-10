/**
 * \file            stm32_constructor_test.c
 * \brief           Actual generated cold-start rollback register model
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_provider.h"
#include <assert.h>
#include <stdio.h>

nx_stm32_system_t g_nx_stm32_system;
GPIO_TypeDef g_nx_stm32_gpioa_model;
static RCC_TypeDef s_rcc;
static EXTI_TypeDef s_exti;
static SYSCFG_TypeDef s_mux;
static TIM_TypeDef s_timer;
static GPIO_TypeDef s_gpioe;
static SPI_TypeDef s_spi;
static unsigned s_disable;
static unsigned s_clear;
static unsigned s_clock_failure;

#undef RCC
#define RCC (&s_rcc)
#undef EXTI
#define EXTI (&s_exti)
#undef SYSCFG
#define SYSCFG (&s_mux)
#undef TIM3
#define TIM3 (&s_timer)
#undef GPIOE
#define GPIOE (&s_gpioe)
#undef SPI1
#define SPI1 (&s_spi)
#undef NVIC_SetPriority
#define NVIC_SetPriority(irq, priority) ((void)(irq), (void)(priority))
#undef NVIC_EnableIRQ
#define NVIC_EnableIRQ(irq) ((void)(irq))
#undef NVIC_ClearPendingIRQ
#define NVIC_ClearPendingIRQ(irq) nx_stm32_model_nvic_clear((int)(irq))

#include "stm32_constructors.h"

/** \brief Inject a late GPIO readback failure after earlier pin mutation. */
uint32_t nx_stm32_model_gpio_clock_read(const volatile uint32_t* reg) {
    if (s_clock_failure != 0U && --s_clock_failure == 0U) {
        return *reg & ~1U;
    }
    return *reg;
}

/** \brief Constructors never execute an SPI transaction. */
void nx_stm32_model_io_poll(unsigned kind, void* port) {
    (void)kind;
    (void)port;
    assert(false);
}

/** \brief Model incoming interrupt state without ARM instructions. */
nx_arch_irq_state_t nx_arch_irq_save(void) {
    return (nx_arch_irq_state_t){0U};
}
/** \brief Restore the deterministic incoming model state. */
void nx_arch_irq_restore(nx_arch_irq_state_t state) {
    (void)state;
}
/** \brief Constructors are tested only from task context. */
bool nx_arch_in_isr(void) {
    return false;
}
/** \brief Constructors run with an unmasked model interrupt state. */
bool nx_arch_irq_is_masked(void) {
    return false;
}
/** \brief Model barrier only; physical ordering is covered separately. */
void nx_arch_dsb(void) {
}
/** \brief Model electrical facts use one deterministic observation time. */
nx_time_us_t nx_time_now_us(void) {
    return 1U;
}
/** \brief Record the actual production last-line vector disable choice. */
void nx_stm32_model_nvic_disable(int irq) {
    assert(irq == (int)EXTI9_5_IRQn);
    ++s_disable;
}
/** \brief Record pending acknowledgement while preserving other lines. */
void nx_stm32_model_nvic_clear(int irq) {
    assert(irq == (int)EXTI9_5_IRQn);
    ++s_clear;
}

/** \brief Force a late line conflict after GPIO/clock acquisition. */
static void exti_rollback_test(void) {
    s_rcc.AHB1ENR = 1U << 2U;
    s_rcc.APB2ENR = RCC_APB2ENR_USART1EN;
    g_nx_stm32_gpioa_model.MODER = 0x55555555U;
    g_nx_stm32_gpioa_model.OTYPER = 0xA5A5U;
    g_nx_stm32_gpioa_model.OSPEEDR = 0xAAAAAAAAU;
    g_nx_stm32_gpioa_model.PUPDR = 0x55555555U;
    g_nx_stm32_gpioa_model.AFR[0] = 0x12345678U;
    uint16_t before = nx_stm32_pin_capture(GPIOA, 6U);
    s_exti.IMR = 1U << 6U;
    assert(prepare_edge6() == NX_ERROR_BUSY);
    assert(!s_nx_port_edge6.initialized);
    assert(nx_stm32_pin_capture(GPIOA, 6U) == before);
    assert(s_rcc.AHB1ENR == (1U << 2U));
    assert(s_rcc.APB2ENR == RCC_APB2ENR_USART1EN);
    assert(s_exti.IMR == (1U << 6U));
    s_exti.IMR = 0U;
    assert(prepare_edge5() == NX_SUCCESS);
    assert(prepare_edge6() == NX_SUCCESS);
    s_exti.PR = (1U << 5U) | (1U << 6U);
    EXTI9_5_IRQHandler();
    assert(s_nx_port_edge5.count == 1U && s_nx_port_edge6.count == 1U);
    s_disable = 0U;
    s_clear = 0U;
    assert(stop_edge6() == NX_SUCCESS && s_disable == 0U);
    assert(stop_edge5() == NX_SUCCESS && s_disable == 1U && s_clear == 1U);
}

/** \brief Preserve a conflicting live timer and restore the acquired pin. */
static void pwm_rollback_test(void) {
    s_rcc.APB1ENR = RCC_APB1ENR_TIM2EN;
    s_rcc.AHB1ENR = 1U << 2U;
    uint16_t before = nx_stm32_pin_capture(GPIOA, 6U);
    s_timer.CR1 = TIM_CR1_CEN;
    s_timer.PSC = 12U;
    s_timer.ARR = 10U;
    assert(prepare_pwm0() == NX_ERROR_BUSY);
    assert(!s_nx_port_pwm0.initialized && !s_nx_idle_pwm0.initialized);
    assert(nx_stm32_pin_capture(GPIOA, 6U) == before);
    assert(s_timer.CR1 == TIM_CR1_CEN && s_timer.PSC == 12U &&
           s_timer.ARR == 10U);
    assert(s_rcc.APB1ENR == RCC_APB1ENR_TIM2EN);
    assert(s_rcc.AHB1ENR == (1U << 2U));
    s_timer.CR1 = 0U;
    assert(prepare_pwm0() == NX_SUCCESS);
    assert(nx_pwm_port_start(&s_nx_face_pwm0) == NX_SUCCESS);
    assert(stop_pwm0() == NX_SUCCESS);
    assert(!s_nx_port_pwm0.initialized && !s_nx_idle_pwm0.initialized);
    assert((GPIOA->MODER & (3U << 12U)) == (1U << 12U));
    assert(GPIOA->BSRR == (1U << 22U));
    assert((s_rcc.APB1ENR & RCC_APB1ENR_TIM3EN) == 0U);
}

/** \brief Restore previously changed pins when a later acquisition fails. */
static void spi_rollback_test(void) {
    s_rcc.AHB1ENR = 1U << 2U;
    s_rcc.APB2ENR = RCC_APB2ENR_USART1EN;
    uint16_t before = nx_stm32_pin_capture(GPIOA, 5U);
    s_clock_failure = 2U;
    assert(prepare_spi0() == NX_ERROR_IO);
    assert(nx_stm32_pin_capture(GPIOA, 5U) == before);
    assert(s_rcc.AHB1ENR == (1U << 2U));
    assert(s_rcc.APB2ENR == RCC_APB2ENR_USART1EN);
    assert(!s_nx_cs_sensor0.initialized);
    assert(prepare_spi0() == NX_SUCCESS);
    assert(s_nx_cs_sensor0.initialized && s_spi.CR1 == 0U);
    assert(stop_spi0() == NX_SUCCESS);
    assert(!s_nx_cs_sensor0.initialized && s_gpioe.BSRR == 8U);
    assert((s_rcc.APB2ENR & RCC_APB2ENR_SPI1EN) == 0U);
}

/** \brief Execute actual generated construction, failure and release paths. */
int main(void) {
    g_nx_stm32_system.rcc = &s_rcc;
    exti_rollback_test();
    pwm_rollback_test();
    spi_rollback_test();
    puts("STM32 generated constructor rollback and release model passed");
    return 0;
}
