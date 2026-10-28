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
#include <sys/mman.h>

nx_stm32_system_t g_nx_stm32_system;
nx_stm32_io_nvic_model_t g_nx_stm32_io_nvic;
static unsigned s_disable;
static unsigned s_clear;
static unsigned s_clock_failure;

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
/** \brief Cold hardware access uses a privileged model owner. */
bool nx_arch_is_privileged(void) {
    return true;
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
/** \brief CPU instruction synchronization is a separate host-only leaf. */
void nx_arch_isb(void) {
}
/** \brief Model electrical facts use one deterministic observation time. */
nx_time_us_t nx_time_now_us(void) {
    return 1U;
}
/** \brief Record the actual production last-line vector disable choice. */
void nx_stm32_model_nvic_disable(int irq) {
    assert(irq == (int)EXTI9_5_IRQn);
    ++s_disable;
    NVIC->ISER[(unsigned)irq / 32U] &= ~(1U << ((unsigned)irq % 32U));
}
/** \brief Record pending acknowledgement while preserving other lines. */
void nx_stm32_model_nvic_clear(int irq) {
    assert(irq == (int)EXTI9_5_IRQn);
    ++s_clear;
    NVIC->ISPR[(unsigned)irq / 32U] &= ~(1U << ((unsigned)irq % 32U));
}

/** \brief Force a late line conflict after GPIO/clock acquisition. */
static void exti_rollback_test(void) {
    RCC->AHB1ENR = 1U << 2U;
    RCC->APB2ENR = RCC_APB2ENR_USART1EN;
    GPIOA->MODER = 0x55555555U;
    GPIOA->OTYPER = 0xA5A5U;
    GPIOA->OSPEEDR = 0xAAAAAAAAU;
    GPIOA->PUPDR = 0x55555555U;
    GPIOA->AFR[0] = 0x12345678U;
    uint16_t before = nx_stm32_pin_capture(GPIOA, 6U);
    EXTI->IMR = 1U << 6U;
    assert(nx_test_prepare_edge6() == NX_ERROR_BUSY);
    assert(!s_nx_port_edge6.initialized);
    assert(nx_stm32_pin_capture(GPIOA, 6U) == before);
    assert(RCC->AHB1ENR == (1U << 2U));
    assert(RCC->APB2ENR == RCC_APB2ENR_USART1EN);
    assert(EXTI->IMR == (1U << 6U));
    EXTI->IMR = 0U;
    /* A real edge can become pending immediately after source publication. */
    NVIC->pending_on_enable = 1U;
    assert(nx_test_prepare_edge5() == NX_SUCCESS);
    assert(NVIC_GetEnableIRQ(EXTI9_5_IRQn) == 1U);
    assert(NVIC_GetPendingIRQ(EXTI9_5_IRQn) == 1U);
    NVIC->pending_on_enable = 0U;
    assert(nx_test_prepare_edge6() == NX_SUCCESS);
    assert(NVIC_GetPendingIRQ(EXTI9_5_IRQn) == 1U);
    EXTI->PR = (1U << 5U) | (1U << 6U);
    EXTI9_5_IRQHandler();
    assert(s_nx_port_edge5.count == 1U && s_nx_port_edge6.count == 1U);
    s_disable = 0U;
    s_clear = 0U;
    assert(nx_test_stop_edge6() == NX_SUCCESS && s_disable == 0U);
    assert(NVIC_GetEnableIRQ(EXTI9_5_IRQn) == 1U &&
           NVIC_GetPendingIRQ(EXTI9_5_IRQn) == 1U);
    assert(nx_test_stop_edge5() == NX_SUCCESS && s_disable == 1U &&
           s_clear == 1U);
    assert(NVIC_GetEnableIRQ(EXTI9_5_IRQn) == 0U &&
           NVIC_GetPendingIRQ(EXTI9_5_IRQn) == 0U);
}

/** \brief Preserve a conflicting live timer and restore the acquired pin. */
static void pwm_rollback_test(void) {
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
    RCC->AHB1ENR = 1U << 2U;
    uint16_t before = nx_stm32_pin_capture(GPIOA, 6U);
    TIM3->CR1 = TIM_CR1_CEN;
    TIM3->PSC = 12U;
    TIM3->ARR = 10U;
    assert(nx_test_prepare_pwm0() == NX_ERROR_BUSY);
    assert(!s_nx_port_pwm0.initialized && !s_nx_idle_pwm0.initialized);
    assert(nx_stm32_pin_capture(GPIOA, 6U) == before);
    assert(TIM3->CR1 == TIM_CR1_CEN && TIM3->PSC == 12U && TIM3->ARR == 10U);
    assert(RCC->APB1ENR == RCC_APB1ENR_TIM2EN);
    assert(RCC->AHB1ENR == (1U << 2U));
    TIM3->CR1 = 0U;
    assert(nx_test_prepare_pwm0() == NX_SUCCESS);
    assert(nx_pwm_port_start(&s_nx_face_pwm0) == NX_SUCCESS);
    assert(nx_test_stop_pwm0() == NX_SUCCESS);
    assert(!s_nx_port_pwm0.initialized && !s_nx_idle_pwm0.initialized);
    assert((GPIOA->MODER & (3U << 12U)) == (1U << 12U));
    assert(GPIOA->BSRR == (1U << 22U));
    assert((RCC->APB1ENR & RCC_APB1ENR_TIM3EN) == 0U);
}

/** \brief Restore previously changed pins when a later acquisition fails. */
static void spi_rollback_test(void) {
    RCC->AHB1ENR = 1U << 2U;
    RCC->APB2ENR = RCC_APB2ENR_USART1EN;
    uint16_t before = nx_stm32_pin_capture(GPIOA, 5U);
    s_clock_failure = 2U;
    assert(nx_test_prepare_spi0() == NX_ERROR_IO);
    assert(nx_stm32_pin_capture(GPIOA, 5U) == before);
    assert(RCC->AHB1ENR == (1U << 2U));
    assert(RCC->APB2ENR == RCC_APB2ENR_USART1EN);
    assert(!s_nx_cs_sensor0.initialized);
    assert(nx_test_prepare_spi0() == NX_SUCCESS);
    assert(s_nx_cs_sensor0.initialized && SPI1->CR1 == 0U);
    assert(nx_test_stop_spi0() == NX_SUCCESS);
    assert(!s_nx_cs_sensor0.initialized && GPIOE->BSRR == 8U);
    assert((RCC->APB2ENR & RCC_APB2ENR_SPI1EN) == 0U);
}

/** \brief The generated I2C owner retains exactly its reviewed line facts. */
static void i2c_route_test(void) {
    RCC->APB1ENR = RCC_APB1ENR_TIM2EN;
    RCC->AHB1ENR = 1U << 2U;
    assert(nx_test_prepare_i2c0() == NX_SUCCESS);
    assert(s_nx_port_i2c0.line_gpio == GPIOB);
    assert(s_nx_port_i2c0.line_mask == ((1U << 6U) | (1U << 7U)));
    assert(s_nx_port_i2c0.registers == I2C1);
    assert(nx_test_stop_i2c0() == NX_SUCCESS);
    assert(!s_nx_port_i2c0.initialized);
    assert(RCC->APB1ENR == RCC_APB1ENR_TIM2EN);
}

/** \brief Execute actual generated construction, failure and release paths. */
int main(void) {
    const size_t length = 0x80000U;
    void* mapping =
        mmap((void*)UINT32_C(0x40000000), length, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    assert(mapping == (void*)UINT32_C(0x40000000));
    g_nx_stm32_system.rcc = RCC;
    exti_rollback_test();
    pwm_rollback_test();
    spi_rollback_test();
    i2c_route_test();
    assert(munmap(mapping, length) == 0);
    puts("STM32 generated constructor rollback and release model passed");
    return 0;
}
