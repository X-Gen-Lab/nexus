/**
 * \file            gd32f470_timer_cold.h
 * \brief           Exact timer cold resources and retained output withdrawal
 * \author          Nexus Team
 */
#ifndef NX_GD32F470_TIMER_COLD_H
#define NX_GD32F470_TIMER_COLD_H
#include "gd32f470_platform_cold.h"
#include "gd32f470_timer_advanced.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "nexus/arch/arch.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif
#ifndef NX_PLATFORM_PRIORITY_WRITE
#define NX_PLATFORM_PRIORITY_WRITE(reg, value) ((reg) = (uint8_t)(value))
#endif

/** \brief Actual added gate, pin claims and complete original priority byte. */
typedef struct {
    uint32_t clock_added;
    uint8_t pin_leases;
    uint8_t priority;
    bool irq_leased;
} nx_gd32_timer_cold_t;

/** \brief Static capture policy with the unique PB6 snapshot slot. */
typedef struct {
    nx_timer_capture_config_t timer;
    uint16_t* snapshot;
    uint8_t priority;
    bool kernel;
} nx_gd32_capture_cold_config_t;

/** \brief Static complementary policy and unique PA8/PB13/PB12 snapshots. */
typedef struct {
    nx_advanced_pwm_config_t timer;
    uint16_t* snapshots;
    uint8_t priority;
    bool kernel;
} nx_gd32_advanced_cold_config_t;

/** \brief Only the serialized privileged, unmasked Thread may own cold I/O. */
static inline bool nx_gd32_timer_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Join every vector reference before restoring its complete byte. */
static inline nx_result_t
nx_gd32_timer_cold_irq_close(nx_gd32_timer_cold_t* cold, unsigned irq) {
    if (!cold->irq_leased)
        return NX_SUCCESS;
    unsigned bank = irq / 32U;
    uint32_t bit = UINT32_C(1) << (irq % 32U);
    NX_PLATFORM_REGISTER_WRITE(NVIC->ICER[bank], bit);
    NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[bank], bit);
    nx_arch_dsb();
    if (((NVIC->ISER[bank] | NVIC->ISPR[bank] | NVIC->IABR[bank]) & bit) != 0U)
        return NX_ERROR_BUSY;
    NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[irq], cold->priority);
    nx_arch_dsb();
    if (NVIC->IPR[irq] != cold->priority)
        return NX_ERROR_IO;
    cold->irq_leased = false;
    return NX_SUCCESS;
}

/** \brief Release one proved pad, retaining its bit on every failed write. */
static inline nx_result_t
nx_gd32_timer_cold_pin_close(nx_gd32_timer_cold_t* cold, uint8_t bit,
                             uint32_t gpio, uint8_t pin, uint16_t original) {
    if ((cold->pin_leases & bit) == 0U)
        return NX_SUCCESS;
    uint32_t mask = UINT32_C(3) << (pin * 2U);
    uint32_t mode = GPIO_CTL(gpio) & mask;
    if (mode != 0U && mode != mask) {
        NX_PLATFORM_REGISTER_WRITE(GPIO_CTL(gpio), GPIO_CTL(gpio) | mask);
        nx_arch_dsb();
        if ((GPIO_CTL(gpio) & mask) != mask)
            return NX_ERROR_IO;
    }
    nx_gd32_pin_restore(gpio, pin, original);
    if (nx_gd32_pin_capture(gpio, pin) != original)
        return NX_ERROR_IO;
    cold->pin_leases &= (uint8_t)~bit;
    return NX_SUCCESS;
}

/** \brief Claim one static pad after its bank clock is observed. */
static inline nx_result_t
nx_gd32_timer_cold_pin_open(nx_gd32_timer_cold_t* cold, uint8_t bit,
                            uint32_t gpio, uint8_t bank, uint8_t pin,
                            uint8_t alternate, uint16_t* original) {
    nx_result_t status = nx_gd32_gpio_clock_enable(bank);
    if (status != NX_SUCCESS)
        return status;
    *original = nx_gd32_pin_capture(gpio, pin);
    cold->pin_leases |= bit;
    return nx_gd32_pin_configure(gpio, pin, 2U, alternate, 0U, false);
}

/** \brief Complete the already leased vector; failed proof retains its byte. */
static inline nx_result_t nx_gd32_timer_cold_irq_open(unsigned irq,
                                                      uint8_t priority) {
    unsigned bank = irq / 32U;
    uint32_t bit = UINT32_C(1) << (irq % 32U);
    uint8_t encoded = (uint8_t)(priority << 4U);
    NX_PLATFORM_PRIORITY_WRITE(NVIC->IPR[irq], encoded);
    NX_PLATFORM_REGISTER_WRITE(NVIC->ICPR[bank], bit);
    NX_PLATFORM_REGISTER_WRITE(NVIC->ISER[bank], bit);
    nx_arch_dsb();
    return NVIC->IPR[irq] == encoded && (NVIC->ISER[bank] & bit) != 0U &&
                   (NVIC->ISPR[bank] & bit) == 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Admit vector and grouping before any peripheral gate write. */
static inline nx_result_t nx_gd32_timer_cold_admit(unsigned irq, bool kernel) {
    unsigned bank = irq / 32U;
    uint32_t bit = UINT32_C(1) << (irq % 32U);
    if (((NVIC->ISER[bank] | NVIC->ISPR[bank] | NVIC->IABR[bank]) & bit) != 0U)
        return NX_ERROR_BUSY;
    return kernel && NVIC_GetPriorityGrouping() > 3U ? NX_ERROR_PERMISSION
                                                     : NX_SUCCESS;
}

/** \brief Prepare exact TIMER3 CH0; all partial effects require the same close.
 */
static inline nx_result_t
nx_gd32_capture_cold_prepare(nx_timer_capture_controller_t* port,
                             nx_gd32_timer_cold_t* cold,
                             const nx_gd32_capture_cold_config_t* config) {
    if (!nx_gd32_timer_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || cold == NULL || config == NULL ||
        config->snapshot == NULL || config->priority > 15U)
        return NX_ERROR_INVALID;
    if (port->config != NULL || cold->clock_added != 0U ||
        cold->pin_leases != 0U || cold->irq_leased)
        return NX_ERROR_BUSY;
    nx_result_t status = nx_gd32_timer_cold_admit(TIMER3_IRQn, config->kernel);
    if (status != NX_SUCCESS)
        return status;
    cold->clock_added = ~RCU_APB1EN & RCU_APB1EN_TIMER3EN;
    NX_PLATFORM_REGISTER_WRITE(RCU_APB1EN, RCU_APB1EN | RCU_APB1EN_TIMER3EN);
    nx_arch_dsb();
    if ((RCU_APB1EN & RCU_APB1EN_TIMER3EN) == 0U)
        return NX_ERROR_IO;
    if ((TIMER_CTL0(TIMER3) & 1U) != 0U || TIMER_CHCTL2(TIMER3) != 0U ||
        TIMER_DMAINTEN(TIMER3) != 0U)
        return NX_ERROR_BUSY;
    cold->priority = NVIC->IPR[TIMER3_IRQn];
    cold->irq_leased = true;
    status = nx_gd32_timer_cold_pin_open(cold, 1U, GPIOB, 1U, 6U, 2U,
                                         config->snapshot);
    if (status != NX_SUCCESS)
        return status;
    status = nx_gd32_timer_capture_initialize(port, &config->timer);
    return status == NX_SUCCESS
               ? nx_gd32_timer_cold_irq_open(TIMER3_IRQn, config->priority)
               : status;
}

/** \brief Join capture ring and publishers before pad and clock restoration. */
static inline nx_result_t
nx_gd32_capture_cold_close(nx_timer_capture_controller_t* port,
                           nx_gd32_timer_cold_t* cold,
                           const nx_gd32_capture_cold_config_t* config) {
    if (!nx_gd32_timer_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || cold == NULL || config == NULL ||
        config->snapshot == NULL)
        return NX_ERROR_INVALID;
    if (port->config != NULL && cold->clock_added == 0U &&
        cold->pin_leases == 0U && !cold->irq_leased)
        return NX_ERROR_BUSY;
    nx_result_t status = port->config != NULL
                             ? nx_gd32_timer_capture_ops.stop(port)
                             : NX_SUCCESS;
    if (status != NX_SUCCESS)
        return status;
    status = nx_gd32_timer_cold_irq_close(cold, TIMER3_IRQn);
    if (status != NX_SUCCESS)
        return status;
    status = nx_gd32_timer_capture_release(port);
    if (status != NX_SUCCESS)
        return status;
    status =
        nx_gd32_timer_cold_pin_close(cold, 1U, GPIOB, 6U, *config->snapshot);
    if (status != NX_SUCCESS)
        return status;
    if (cold->clock_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCU_APB1EN, RCU_APB1EN & ~cold->clock_added);
        nx_arch_dsb();
        if ((RCU_APB1EN & cold->clock_added) != 0U)
            return NX_ERROR_IO;
        cold->clock_added = 0U;
    }
    return NX_SUCCESS;
}

/** \brief Prepare exact TIMER0 outputs and shared TIMER8 break-vector
 * admission. */
static inline nx_result_t
nx_gd32_advanced_cold_prepare(nx_advanced_pwm_controller_t* port,
                              nx_gd32_timer_cold_t* cold,
                              const nx_gd32_advanced_cold_config_t* config) {
    if (!nx_gd32_timer_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || cold == NULL || config == NULL ||
        config->snapshots == NULL || config->priority > 15U)
        return NX_ERROR_INVALID;
    if (port->config != NULL || cold->clock_added != 0U ||
        cold->pin_leases != 0U || cold->irq_leased)
        return NX_ERROR_BUSY;
    nx_result_t status =
        nx_gd32_timer_cold_admit(TIMER0_BRK_TIMER8_IRQn, config->kernel);
    if (status != NX_SUCCESS)
        return status;
    if ((RCU_APB2EN & RCU_APB2EN_TIMER8EN) != 0U)
        return NX_ERROR_BUSY;
    cold->clock_added = ~RCU_APB2EN & RCU_APB2EN_TIMER0EN;
    NX_PLATFORM_REGISTER_WRITE(RCU_APB2EN, RCU_APB2EN | RCU_APB2EN_TIMER0EN);
    nx_arch_dsb();
    if ((RCU_APB2EN & RCU_APB2EN_TIMER0EN) == 0U)
        return NX_ERROR_IO;
    if ((TIMER_CTL0(TIMER0) & 1U) != 0U || TIMER_CHCTL2(TIMER0) != 0U ||
        TIMER_DMAINTEN(TIMER0) != 0U ||
        (TIMER_CCHP(TIMER0) & UINT32_C(33536)) != 0U)
        return NX_ERROR_BUSY;
    cold->priority = NVIC->IPR[TIMER0_BRK_TIMER8_IRQn];
    cold->irq_leased = true;
    status = nx_gd32_timer_cold_pin_open(cold, 1U, GPIOA, 0U, 8U, 1U,
                                         &config->snapshots[0]);
    if (status == NX_SUCCESS)
        status = nx_gd32_timer_cold_pin_open(cold, 2U, GPIOB, 1U, 13U, 1U,
                                             &config->snapshots[1]);
    if (status == NX_SUCCESS)
        status = nx_gd32_timer_cold_pin_open(cold, 4U, GPIOB, 1U, 12U, 1U,
                                             &config->snapshots[2]);
    if (status != NX_SUCCESS)
        return status;
    status = nx_gd32_advanced_pwm_initialize(port, &config->timer);
    return status == NX_SUCCESS ? nx_gd32_timer_cold_irq_open(
                                      TIMER0_BRK_TIMER8_IRQn, config->priority)
                                : status;
}

/** \brief Prove inactive drive on one output before any group pad release. */
static inline nx_result_t nx_gd32_timer_cold_inactive(uint32_t gpio,
                                                      uint8_t pin, bool high) {
    uint32_t level = high ? UINT32_C(1) << pin : 0U;
    uint32_t set = UINT32_C(1) << (pin + (high ? 0U : 16U));
    uint32_t mask = UINT32_C(3) << (pin * 2U);
    uint32_t output = UINT32_C(1) << (pin * 2U);
    NX_PLATFORM_REGISTER_WRITE(GPIO_BOP(gpio), set);
    nx_arch_dsb();
    if ((GPIO_OCTL(gpio) & (UINT32_C(1) << pin)) != level)
        return NX_ERROR_IO;
    NX_PLATFORM_REGISTER_WRITE(GPIO_CTL(gpio),
                               (GPIO_CTL(gpio) & ~mask) | output);
    nx_arch_dsb();
    return (GPIO_CTL(gpio) & mask) == output &&
                   (GPIO_OCTL(gpio) & (UINT32_C(1) << pin)) == level
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Retain both safe outputs until all group inactive proofs succeed. */
static inline nx_result_t
nx_gd32_advanced_cold_close(nx_advanced_pwm_controller_t* port,
                            nx_gd32_timer_cold_t* cold,
                            const nx_gd32_advanced_cold_config_t* config) {
    if (!nx_gd32_timer_cold_context())
        return NX_ERROR_CONTEXT;
    if (port == NULL || cold == NULL || config == NULL ||
        config->snapshots == NULL)
        return NX_ERROR_INVALID;
    if (port->config != NULL && cold->clock_added == 0U &&
        cold->pin_leases == 0U && !cold->irq_leased)
        return NX_ERROR_BUSY;
    nx_result_t status =
        port->config != NULL ? nx_gd32_advanced_pwm_ops.stop(port) : NX_SUCCESS;
    if (status != NX_SUCCESS)
        return status;
    status = nx_gd32_timer_cold_irq_close(cold, TIMER0_BRK_TIMER8_IRQn);
    if (status != NX_SUCCESS)
        return status;
    status = nx_gd32_advanced_pwm_release(port);
    if (status != NX_SUCCESS)
        return status;
    if ((cold->pin_leases & 1U) != 0U) {
        status =
            nx_gd32_timer_cold_inactive(GPIOA, 8U, config->timer.idle_high);
        if (status != NX_SUCCESS)
            return status;
    }
    if ((cold->pin_leases & 2U) != 0U) {
        status = nx_gd32_timer_cold_inactive(
            GPIOB, 13U, config->timer.idle_complement_high);
        if (status != NX_SUCCESS)
            return status;
    }
    status = nx_gd32_timer_cold_pin_close(cold, 4U, GPIOB, 12U,
                                          config->snapshots[2]);
    if (status == NX_SUCCESS)
        status = nx_gd32_timer_cold_pin_close(cold, 2U, GPIOB, 13U,
                                              config->snapshots[1]);
    if (status == NX_SUCCESS)
        status = nx_gd32_timer_cold_pin_close(cold, 1U, GPIOA, 8U,
                                              config->snapshots[0]);
    if (status != NX_SUCCESS)
        return status;
    if (cold->clock_added != 0U) {
        NX_PLATFORM_REGISTER_WRITE(RCU_APB2EN, RCU_APB2EN & ~cold->clock_added);
        nx_arch_dsb();
        if ((RCU_APB2EN & cold->clock_added) != 0U)
            return NX_ERROR_IO;
        cold->clock_added = 0U;
    }
    return NX_SUCCESS;
}
#endif
