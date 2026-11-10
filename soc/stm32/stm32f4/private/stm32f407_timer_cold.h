/**
 * \file            stm32f407_timer_cold.h
 * \brief           Exact timer cold resources and retained output withdrawal
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_TIMER_COLD_H
#define NX_STM32F407_TIMER_COLD_H
#include "stm32f407_platform_cold.h"
#include "stm32f407_timer_advanced.h"
#include "stm32f407_timer_hardware.h"
#include "stm32f407_timer_pins.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))

#endif
#ifndef NX_PLATFORM_PRIORITY_WRITE
#define NX_PLATFORM_PRIORITY_WRITE(reg, value) ((reg) = (uint8_t)(value))
#endif

/** \brief Full cold owner and pin/IRQ/gate receipts, retained through rollback.
 */
typedef struct {
    const void* configuration;
    uint8_t pin_leases;
    uint8_t priority;
    bool irq_leased;
    bool clock_owned;
} nx_stm32_timer_cold_t;

/** \brief Static controller/channel policy with one unique pad snapshot. */
typedef struct {
    nx_timer_capture_config_t timer;
    uint16_t* snapshot;
    uint8_t priority;
    bool kernel;
    TIM_TypeDef* registers;
} nx_stm32_capture_cold_config_t;

/** \brief Static selected complementary policy and three unique pin snapshots.
 */
typedef struct {
    nx_advanced_pwm_config_t timer;
    uint16_t* snapshots;
    uint8_t priority;
    bool kernel;
} nx_stm32_advanced_cold_config_t;

/** \brief Only the serialized privileged, unmasked Thread may own cold I/O. */
static inline bool nx_stm32_timer_cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Overflowing or intersecting retained object ranges are invalid. */
static inline bool nx_stm32_timer_cold_overlap(const void* left,
                                               size_t left_bytes,
                                               const void* right,
                                               size_t right_bytes) {
    uintptr_t a = (uintptr_t)left, b = (uintptr_t)right;
    return left_bytes > UINTPTR_MAX - a || right_bytes > UINTPTR_MAX - b ||
           (a < b + right_bytes && b < a + left_bytes);
}
/** \brief Check full objects before dereferencing configuration members. */
static inline bool
nx_stm32_timer_cold_objects(const void* port, size_t port_bytes,
                            const nx_stm32_timer_cold_t* cold,
                            const void* config, size_t config_bytes) {
    return port != NULL && cold != NULL && config != NULL &&
           !nx_stm32_timer_cold_overlap(port, port_bytes, cold,
                                        sizeof(*cold)) &&
           !nx_stm32_timer_cold_overlap(port, port_bytes, config,
                                        config_bytes) &&
           !nx_stm32_timer_cold_overlap(cold, sizeof(*cold), config,
                                        config_bytes);
}
/** \brief Saving a pin must never overwrite cleanup or immutable policy. */
static inline bool
nx_stm32_timer_cold_snapshots(const uint16_t* snapshots, size_t bytes,
                              const void* port, size_t port_bytes,
                              const nx_stm32_timer_cold_t* cold,
                              const void* config, size_t config_bytes) {
    return snapshots != NULL && (uintptr_t)snapshots % sizeof(uint16_t) == 0U &&
           !nx_stm32_timer_cold_overlap(snapshots, bytes, port, port_bytes) &&
           !nx_stm32_timer_cold_overlap(snapshots, bytes, cold,
                                        sizeof(*cold)) &&
           !nx_stm32_timer_cold_overlap(snapshots, bytes, config, config_bytes);
}

/** \brief Join every vector reference before restoring its complete byte. */
static inline nx_result_t
nx_stm32_timer_cold_irq_close(nx_stm32_timer_cold_t* cold, unsigned irq) {
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
nx_stm32_timer_cold_pin_close(nx_stm32_timer_cold_t* cold, uint8_t bit,
                              GPIO_TypeDef* gpio, uint8_t pin,
                              uint16_t original) {
    if ((cold->pin_leases & bit) == 0U)
        return NX_SUCCESS;
    uint32_t mask = UINT32_C(3) << (pin * 2U);
    uint32_t mode = gpio->MODER & mask;
    if (mode != 0U && mode != mask) {
        NX_PLATFORM_REGISTER_WRITE(gpio->MODER, gpio->MODER | mask);
        nx_arch_dsb();
        if ((gpio->MODER & mask) != mask)
            return NX_ERROR_IO;
    }
    nx_stm32_pin_restore(gpio, pin, original);
    if (nx_stm32_pin_capture(gpio, pin) != original)
        return NX_ERROR_IO;
    cold->pin_leases &= (uint8_t)~bit;
    return NX_SUCCESS;
}

/** \brief Claim one static pad after its bank clock is observed. */
static inline nx_result_t
nx_stm32_timer_cold_pin_open(nx_stm32_timer_cold_t* cold, uint8_t bit,
                             GPIO_TypeDef* gpio, uint8_t bank, uint8_t pin,
                             uint8_t alternate, uint16_t* original) {
    nx_result_t status = nx_stm32_gpio_clock_enable(bank);
    if (status != NX_SUCCESS)
        return status;
    *original = nx_stm32_pin_capture(gpio, pin);
    cold->pin_leases |= bit;
    return nx_stm32_pin_configure(gpio, pin, 2U, alternate, 0U, false);
}

/** \brief Complete the already leased vector; failed proof retains its byte. */
static inline nx_result_t nx_stm32_timer_cold_irq_open(unsigned irq,
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
static inline nx_result_t nx_stm32_timer_cold_admit(unsigned irq, bool kernel) {
    unsigned bank = irq / 32U;
    uint32_t bit = UINT32_C(1) << (irq % 32U);
    if (((NVIC->ISER[bank] | NVIC->ISPR[bank] | NVIC->IABR[bank]) & bit) != 0U)
        return NX_ERROR_BUSY;
    return kernel && ((SCB->AIRCR >> 8U) & 7U) > 3U ? NX_ERROR_PERMISSION
                                                    : NX_SUCCESS;
}

/** \brief Exact cold-only controller facts; hot registers stay specialized. */
typedef struct {
    TIM_TypeDef* registers;
    const nx_timer_capture_ops_t* ops;
    nx_result_t (*initialize)(nx_timer_capture_controller_t* port,
                              const nx_timer_capture_config_t* config);
    nx_result_t (*release)(nx_timer_capture_controller_t* port);
    uint32_t gate;
    uint8_t irq;
} nx_stm32_capture_hardware_t;

static const nx_stm32_capture_hardware_t s_nx_capture_tim3 = {
    TIM3,
    &nx_stm32_tim3_timer_capture_ops,
    nx_stm32_tim3_timer_capture_initialize,
    nx_stm32_tim3_timer_capture_release,
    RCC_APB1ENR_TIM3EN,
    TIM3_IRQn};
static const nx_stm32_capture_hardware_t s_nx_capture_tim4 = {
    TIM4,
    &nx_stm32_timer_capture_ops,
    nx_stm32_timer_capture_initialize,
    nx_stm32_timer_capture_release,
    RCC_APB1ENR_TIM4EN,
    TIM4_IRQn};
static const nx_stm32_capture_hardware_t s_nx_capture_tim5 = {
    TIM5,
    &nx_stm32_tim5_timer_capture_ops,
    nx_stm32_tim5_timer_capture_initialize,
    nx_stm32_tim5_timer_capture_release,
    RCC_APB1ENR_TIM5EN,
    TIM5_IRQn};

static const nx_stm32_capture_hardware_t s_nx_capture_tim1 = {
    TIM1,
    &nx_stm32_tim1_timer_capture_ops,
    nx_stm32_tim1_timer_capture_initialize,
    nx_stm32_tim1_timer_capture_release,
    RCC_APB2ENR_TIM1EN,
    TIM1_CC_IRQn};
static const nx_stm32_capture_hardware_t s_nx_capture_tim8 = {
    TIM8,
    &nx_stm32_tim8_timer_capture_ops,
    nx_stm32_tim8_timer_capture_initialize,
    nx_stm32_tim8_timer_capture_release,
    RCC_APB2ENR_TIM8EN,
    TIM8_CC_IRQn};

static const nx_stm32_capture_hardware_t s_nx_capture_tim9 = {
    TIM9,
    &nx_stm32_tim9_timer_capture_ops,
    nx_stm32_tim9_timer_capture_initialize,
    nx_stm32_tim9_timer_capture_release,
    RCC_APB2ENR_TIM9EN,
    TIM1_BRK_TIM9_IRQn};
static const nx_stm32_capture_hardware_t s_nx_capture_tim10 = {
    TIM10,
    &nx_stm32_tim10_timer_capture_ops,
    nx_stm32_tim10_timer_capture_initialize,
    nx_stm32_tim10_timer_capture_release,
    RCC_APB2ENR_TIM10EN,
    TIM1_UP_TIM10_IRQn};
static const nx_stm32_capture_hardware_t s_nx_capture_tim11 = {
    TIM11,
    &nx_stm32_tim11_timer_capture_ops,
    nx_stm32_tim11_timer_capture_initialize,
    nx_stm32_tim11_timer_capture_release,
    RCC_APB2ENR_TIM11EN,
    TIM1_TRG_COM_TIM11_IRQn};
static const nx_stm32_capture_hardware_t s_nx_capture_tim12 = {
    TIM12,
    &nx_stm32_tim12_timer_capture_ops,
    nx_stm32_tim12_timer_capture_initialize,
    nx_stm32_tim12_timer_capture_release,
    RCC_APB1ENR_TIM12EN,
    TIM8_BRK_TIM12_IRQn};
static const nx_stm32_capture_hardware_t s_nx_capture_tim13 = {
    TIM13,
    &nx_stm32_tim13_timer_capture_ops,
    nx_stm32_tim13_timer_capture_initialize,
    nx_stm32_tim13_timer_capture_release,
    RCC_APB1ENR_TIM13EN,
    TIM8_UP_TIM13_IRQn};
static const nx_stm32_capture_hardware_t s_nx_capture_tim14 = {
    TIM14,
    &nx_stm32_tim14_timer_capture_ops,
    nx_stm32_tim14_timer_capture_initialize,
    nx_stm32_tim14_timer_capture_release,
    RCC_APB1ENR_TIM14EN,
    TIM8_TRG_COM_TIM14_IRQn};

/** \brief Advanced input timers share APB2, not general-purpose APB1 gates. */
static inline volatile uint32_t*
nx_stm32_capture_gate_register(const nx_stm32_capture_hardware_t* hardware) {
    return nx_stm32_timer_apb2(hardware->registers) ? &RCC->APB2ENR
                                                    : &RCC->APB1ENR;
}

/** \brief Acquire one exact channel and preserve its full cleanup identity. */
static inline nx_result_t
nx_stm32_capture_prepare_for(nx_timer_capture_controller_t* port,
                             nx_stm32_timer_cold_t* cold,
                             const nx_stm32_capture_cold_config_t* config,
                             const nx_stm32_capture_hardware_t* hardware) {
    if (!nx_stm32_timer_cold_context())
        return NX_ERROR_CONTEXT;
    if (!nx_stm32_timer_cold_objects(port, sizeof(*port), cold, config,
                                     sizeof(*config)) ||
        !nx_stm32_timer_cold_snapshots(config->snapshot, sizeof(uint16_t), port,
                                       sizeof(*port), cold, config,
                                       sizeof(*config)) ||
        config->priority > 15U)
        return NX_ERROR_INVALID;
    if (port->config != NULL || cold->configuration != NULL ||
        cold->clock_owned || cold->pin_leases != 0U || cold->irq_leased)
        return NX_ERROR_BUSY;
    nx_stm32_timer_pin_t pad;
    if (config->registers != hardware->registers ||
        !nx_stm32_capture_pin(hardware->registers, config->timer.channel, &pad))
        return NX_ERROR_INVALID;
    nx_result_t status =
        nx_stm32_timer_cold_admit(hardware->irq, config->kernel);
    if (status != NX_SUCCESS)
        return status;
    volatile uint32_t* clock = nx_stm32_capture_gate_register(hardware);
    if ((*clock & hardware->gate) != 0U ||
        nx_stm32_capture_peer_busy(hardware->registers))
        return NX_ERROR_BUSY;
    cold->configuration = config;
    cold->clock_owned = true;
    NX_PLATFORM_REGISTER_WRITE(*clock, *clock | hardware->gate);
    nx_arch_dsb();
    if ((*clock & hardware->gate) == 0U)
        return NX_ERROR_IO;
    if (nx_stm32_capture_peer_busy(hardware->registers))
        return NX_ERROR_BUSY;
    if ((hardware->registers->CR1 & 1U) != 0U ||
        hardware->registers->CCER != 0U || hardware->registers->DIER != 0U ||
        ((hardware->registers == TIM1 || hardware->registers == TIM8) &&
         hardware->registers->BDTR != 0U))
        return NX_ERROR_BUSY;
    cold->priority = NVIC->IPR[hardware->irq];
    cold->irq_leased = true;
    status = nx_stm32_timer_cold_pin_open(
        cold, 1U, pad.gpio, pad.bank, pad.pin,
        (uint8_t)nx_stm32_timer_af(hardware->registers), config->snapshot);
    if (status != NX_SUCCESS)
        return status;
    status = hardware->initialize(port, &config->timer);
    return status == NX_SUCCESS
               ? nx_stm32_timer_cold_irq_open(hardware->irq, config->priority)
               : status;
}

/** \brief Reclaim only the exact controller/config after the last lease. */
static inline nx_result_t
nx_stm32_capture_close_for(nx_timer_capture_controller_t* port,
                           nx_stm32_timer_cold_t* cold,
                           const nx_stm32_capture_cold_config_t* config,
                           const nx_stm32_capture_hardware_t* hardware) {
    if (!nx_stm32_timer_cold_context())
        return NX_ERROR_CONTEXT;
    if (!nx_stm32_timer_cold_objects(port, sizeof(*port), cold, config,
                                     sizeof(*config)) ||
        !nx_stm32_timer_cold_snapshots(config->snapshot, sizeof(uint16_t), port,
                                       sizeof(*port), cold, config,
                                       sizeof(*config)))
        return NX_ERROR_INVALID;
    if (cold->configuration == NULL) {
        return port->config == NULL && !cold->clock_owned &&
                       cold->pin_leases == 0U && !cold->irq_leased
                   ? hardware->release(port)
                   : NX_ERROR_BUSY;
    }
    nx_stm32_timer_pin_t pad;
    if (cold->configuration != config ||
        config->registers != hardware->registers ||
        (port->config != NULL && port->config != &config->timer) ||
        !nx_stm32_capture_pin(hardware->registers, config->timer.channel, &pad))
        return NX_ERROR_INVALID;
    if (!cold->clock_owned)
        return NX_ERROR_STATE;
    nx_result_t status =
        port->config != NULL ? hardware->ops->stop(port) : NX_SUCCESS;
    if (status != NX_SUCCESS)
        return status;
    if (nx_stm32_capture_peer_busy(hardware->registers))
        return NX_ERROR_BUSY;
    status = nx_stm32_timer_cold_irq_close(cold, hardware->irq);
    if (status != NX_SUCCESS)
        return status;
    status = hardware->release(port);
    if (status != NX_SUCCESS)
        return status;
    status = nx_stm32_timer_cold_pin_close(cold, 1U, pad.gpio, pad.pin,
                                           *config->snapshot);
    if (status != NX_SUCCESS)
        return status;
    volatile uint32_t* clock = nx_stm32_capture_gate_register(hardware);
    NX_PLATFORM_REGISTER_WRITE(*clock, *clock & ~hardware->gate);
    nx_arch_dsb();
    if ((*clock & hardware->gate) != 0U)
        return NX_ERROR_IO;
    cold->clock_owned = false;
    cold->configuration = NULL;
    return NX_SUCCESS;
}

/** \brief Select only the immutable TIM4 acquisition facts. */
static inline nx_result_t
nx_stm32_capture_cold_prepare(nx_timer_capture_controller_t* port,
                              nx_stm32_timer_cold_t* cold,
                              const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config, &s_nx_capture_tim4);
}
/** \brief End only the TIM4 session, including partially acquired resources. */
static inline nx_result_t
nx_stm32_capture_cold_close(nx_timer_capture_controller_t* port,
                            nx_stm32_timer_cold_t* cold,
                            const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim4);
}
/** \brief Select only the immutable TIM3 acquisition facts. */
static inline nx_result_t nx_stm32_tim3_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config, &s_nx_capture_tim3);
}
/** \brief End only the TIM3 session; failed proof preserves its owner. */
static inline nx_result_t
nx_stm32_tim3_capture_cold_close(nx_timer_capture_controller_t* port,
                                 nx_stm32_timer_cold_t* cold,
                                 const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim3);
}
/** \brief Select only the immutable TIM5 acquisition facts. */
static inline nx_result_t nx_stm32_tim5_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config, &s_nx_capture_tim5);
}
/** \brief End only the TIM5 session; failed proof preserves its owner. */
static inline nx_result_t
nx_stm32_tim5_capture_cold_close(nx_timer_capture_controller_t* port,
                                 nx_stm32_timer_cold_t* cold,
                                 const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim5);
}

/** \brief Acquire only TIM1 capture and its dedicated CC vector. */
static inline nx_result_t nx_stm32_tim1_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config, &s_nx_capture_tim1);
}
/** \brief Return TIM1 only after the ring and CC publisher are detached. */
static inline nx_result_t
nx_stm32_tim1_capture_cold_close(nx_timer_capture_controller_t* port,
                                 nx_stm32_timer_cold_t* cold,
                                 const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim1);
}
/** \brief Acquire only TIM8 capture and its dedicated CC vector. */
static inline nx_result_t nx_stm32_tim8_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config, &s_nx_capture_tim8);
}
/** \brief Return TIM8 without changing a peer's shared break/update IRQ. */
static inline nx_result_t
nx_stm32_tim8_capture_cold_close(nx_timer_capture_controller_t* port,
                                 nx_stm32_timer_cold_t* cold,
                                 const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim8);
}

/** \brief Prepare exact TIM9 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim9_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config, &s_nx_capture_tim9);
}
/** \brief Close exact TIM9 capture without changing its vector peer. */
static inline nx_result_t
nx_stm32_tim9_capture_cold_close(nx_timer_capture_controller_t* port,
                                 nx_stm32_timer_cold_t* cold,
                                 const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim9);
}
/** \brief Prepare exact TIM10 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim10_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config,
                                        &s_nx_capture_tim10);
}
/** \brief Close exact TIM10 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim10_capture_cold_close(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim10);
}
/** \brief Prepare exact TIM11 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim11_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config,
                                        &s_nx_capture_tim11);
}
/** \brief Close exact TIM11 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim11_capture_cold_close(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim11);
}
/** \brief Prepare exact TIM12 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim12_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config,
                                        &s_nx_capture_tim12);
}
/** \brief Close exact TIM12 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim12_capture_cold_close(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim12);
}
/** \brief Prepare exact TIM13 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim13_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config,
                                        &s_nx_capture_tim13);
}
/** \brief Close exact TIM13 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim13_capture_cold_close(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim13);
}
/** \brief Prepare exact TIM14 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim14_capture_cold_prepare(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_prepare_for(port, cold, config,
                                        &s_nx_capture_tim14);
}
/** \brief Close exact TIM14 capture without changing its vector peer. */
static inline nx_result_t nx_stm32_tim14_capture_cold_close(
    nx_timer_capture_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_capture_cold_config_t* config) {
    return nx_stm32_capture_close_for(port, cold, config, &s_nx_capture_tim14);
}

/** \brief Read-only physical facts; runtime state/config layouts stay fixed. */
typedef struct {
    TIM_TypeDef* registers;
    const nx_advanced_pwm_ops_t* ops;
    nx_result_t (*initialize)(nx_advanced_pwm_controller_t* port,
                              const nx_advanced_pwm_config_t* config);
    nx_result_t (*release)(nx_advanced_pwm_controller_t* port);
    volatile uint32_t* peer_clock;
    uint32_t gate;
    uint32_t peer_gate;
    uint8_t irq;
    uint8_t alternate;
} nx_stm32_advanced_hardware_t;

static const nx_stm32_advanced_hardware_t s_nx_advanced_tim1 = {
    TIM1,
    &nx_stm32_advanced_pwm_ops,
    nx_stm32_advanced_pwm_initialize,
    nx_stm32_advanced_pwm_release,
    &RCC->APB2ENR,
    RCC_APB2ENR_TIM1EN,
    RCC_APB2ENR_TIM9EN,
    TIM1_BRK_TIM9_IRQn,
    1U};
static const nx_stm32_advanced_hardware_t s_nx_advanced_tim8 = {
    TIM8,
    &nx_stm32_tim8_advanced_pwm_ops,
    nx_stm32_tim8_advanced_pwm_initialize,
    nx_stm32_tim8_advanced_pwm_release,
    &RCC->APB1ENR,
    RCC_APB2ENR_TIM8EN,
    RCC_APB1ENR_TIM12EN,
    TIM8_BRK_TIM12_IRQn,
    3U};

/** \brief One cold algorithm owns exact pins, timer gate and shared peer. */
static inline nx_result_t
nx_stm32_advanced_prepare_for(nx_advanced_pwm_controller_t* port,
                              nx_stm32_timer_cold_t* cold,
                              const nx_stm32_advanced_cold_config_t* config,
                              const nx_stm32_advanced_hardware_t* hardware) {
    if (!nx_stm32_timer_cold_context())
        return NX_ERROR_CONTEXT;
    if (!nx_stm32_timer_cold_objects(port, sizeof(*port), cold, config,
                                     sizeof(*config)))
        return NX_ERROR_INVALID;
    if (!nx_stm32_timer_cold_snapshots(config->snapshots, 3U * sizeof(uint16_t),
                                       port, sizeof(*port), cold, config,
                                       sizeof(*config)))
        return NX_ERROR_INVALID;
    if (port->config != NULL || cold->configuration != NULL ||
        cold->clock_owned || cold->pin_leases != 0U || cold->irq_leased)
        return NX_ERROR_BUSY;
    nx_stm32_timer_pin_t input;
    if (config->priority > 15U ||
        !nx_stm32_advanced_pin(hardware->registers, config->timer.channel, 2U,
                               &input) ||
        config->timer.break_input != &input.gpio->IDR ||
        config->timer.break_mask != (UINT32_C(1) << input.pin))
        return NX_ERROR_INVALID;
    nx_result_t status =
        nx_stm32_timer_cold_admit(hardware->irq, config->kernel);
    if (status != NX_SUCCESS)
        return status;
    if ((*hardware->peer_clock & hardware->peer_gate) != 0U ||
        (RCC->APB2ENR & hardware->gate) != 0U)
        return NX_ERROR_BUSY;
    cold->configuration = config;
    cold->clock_owned = true;
    NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR, RCC->APB2ENR | hardware->gate);
    nx_arch_dsb();
    if ((RCC->APB2ENR & hardware->gate) == 0U)
        return NX_ERROR_IO;
    if ((hardware->registers->CR1 & 1U) != 0U ||
        hardware->registers->CCER != 0U || hardware->registers->DIER != 0U ||
        (hardware->registers->BDTR & UINT32_C(33536)) != 0U)
        return NX_ERROR_BUSY;
    cold->priority = NVIC->IPR[hardware->irq];
    cold->irq_leased = true;
    for (unsigned index = 0U; index < 3U && status == NX_SUCCESS; ++index) {
        nx_stm32_timer_pin_t pin;
        if (!nx_stm32_advanced_pin(hardware->registers, config->timer.channel,
                                   index, &pin))
            return NX_ERROR_INVALID;
        status = nx_stm32_timer_cold_pin_open(
            cold, (uint8_t)(1U << index), pin.gpio, pin.bank, pin.pin,
            hardware->alternate, &config->snapshots[index]);
    }
    if (status != NX_SUCCESS)
        return status;
    status = hardware->initialize(port, &config->timer);
    return status == NX_SUCCESS
               ? nx_stm32_timer_cold_irq_open(hardware->irq, config->priority)
               : status;
}

/** \brief Select only immutable TIM1 facts; hot controls remain specialized. */
static inline nx_result_t
nx_stm32_advanced_cold_prepare(nx_advanced_pwm_controller_t* port,
                               nx_stm32_timer_cold_t* cold,
                               const nx_stm32_advanced_cold_config_t* config) {
    return nx_stm32_advanced_prepare_for(port, cold, config,
                                         &s_nx_advanced_tim1);
}
/** \brief Select only immutable TIM8 facts, including its TIM12 peer. */
static inline nx_result_t nx_stm32_tim8_advanced_cold_prepare(
    nx_advanced_pwm_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_advanced_cold_config_t* config) {
    return nx_stm32_advanced_prepare_for(port, cold, config,
                                         &s_nx_advanced_tim8);
}

/** \brief Prove inactive drive on one output before any group pad release. */
static inline nx_result_t nx_stm32_timer_cold_inactive(GPIO_TypeDef* gpio,
                                                       uint8_t pin, bool high) {
    uint32_t level = high ? UINT32_C(1) << pin : 0U;
    uint32_t set = UINT32_C(1) << (pin + (high ? 0U : 16U));
    uint32_t mask = UINT32_C(3) << (pin * 2U);
    uint32_t output = UINT32_C(1) << (pin * 2U);
    NX_PLATFORM_REGISTER_WRITE(gpio->BSRR, set);
    nx_arch_dsb();
    if ((gpio->ODR & (UINT32_C(1) << pin)) != level)
        return NX_ERROR_IO;
    NX_PLATFORM_REGISTER_WRITE(gpio->MODER, (gpio->MODER & ~mask) | output);
    nx_arch_dsb();
    return (gpio->MODER & mask) == output &&
                   (gpio->ODR & (UINT32_C(1) << pin)) == level
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

/** \brief Retain the full config through IRQ, pin and gate withdrawal. */
static inline nx_result_t
nx_stm32_advanced_close_for(nx_advanced_pwm_controller_t* port,
                            nx_stm32_timer_cold_t* cold,
                            const nx_stm32_advanced_cold_config_t* config,
                            const nx_stm32_advanced_hardware_t* hardware) {
    if (!nx_stm32_timer_cold_context())
        return NX_ERROR_CONTEXT;
    if (!nx_stm32_timer_cold_objects(port, sizeof(*port), cold, config,
                                     sizeof(*config)))
        return NX_ERROR_INVALID;
    if (!nx_stm32_timer_cold_snapshots(config->snapshots, 3U * sizeof(uint16_t),
                                       port, sizeof(*port), cold, config,
                                       sizeof(*config)))
        return NX_ERROR_INVALID;
    if (cold->configuration == NULL) {
        return port->config == NULL && !cold->clock_owned &&
                       cold->pin_leases == 0U && !cold->irq_leased
                   ? hardware->release(port)
                   : NX_ERROR_BUSY;
    }
    nx_stm32_timer_pin_t input;
    if (!nx_stm32_advanced_pin(hardware->registers, config->timer.channel, 2U,
                               &input) ||
        config->timer.break_input != &input.gpio->IDR ||
        config->timer.break_mask != (UINT32_C(1) << input.pin) ||
        cold->configuration != config ||
        (port->config != NULL && port->config != &config->timer))
        return NX_ERROR_INVALID;
    if (!cold->clock_owned)
        return NX_ERROR_STATE;
    nx_result_t status =
        port->config != NULL ? hardware->ops->stop(port) : NX_SUCCESS;
    if (status != NX_SUCCESS)
        return status;
    status = nx_stm32_timer_cold_irq_close(cold, hardware->irq);
    if (status != NX_SUCCESS)
        return status;
    status = hardware->release(port);
    if (status != NX_SUCCESS)
        return status;
    /* Every group output is observed inactive before the first pin restore. */
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((cold->pin_leases & (1U << index)) == 0U)
            continue;
        nx_stm32_timer_pin_t pin;
        if (!nx_stm32_advanced_pin(hardware->registers, config->timer.channel,
                                   index, &pin))
            return NX_ERROR_INVALID;
        bool high = index == 0U ? config->timer.idle_high
                                : config->timer.idle_complement_high;
        status = nx_stm32_timer_cold_inactive(pin.gpio, pin.pin, high);
        if (status != NX_SUCCESS)
            return status;
    }
    for (unsigned remaining = 3U; remaining != 0U; --remaining) {
        unsigned index = remaining - 1U;
        nx_stm32_timer_pin_t pin;
        if (!nx_stm32_advanced_pin(hardware->registers, config->timer.channel,
                                   index, &pin))
            return NX_ERROR_INVALID;
        status = nx_stm32_timer_cold_pin_close(cold, (uint8_t)(1U << index),
                                               pin.gpio, pin.pin,
                                               config->snapshots[index]);
        if (status != NX_SUCCESS)
            return status;
    }
    NX_PLATFORM_REGISTER_WRITE(RCC->APB2ENR, RCC->APB2ENR & ~hardware->gate);
    nx_arch_dsb();
    if ((RCC->APB2ENR & hardware->gate) != 0U)
        return NX_ERROR_IO;
    cold->clock_owned = false;
    cold->configuration = NULL;
    return NX_SUCCESS;
}

/** \brief Close only TIM1, preserving a failed cold cleanup receipt. */
static inline nx_result_t
nx_stm32_advanced_cold_close(nx_advanced_pwm_controller_t* port,
                             nx_stm32_timer_cold_t* cold,
                             const nx_stm32_advanced_cold_config_t* config) {
    return nx_stm32_advanced_close_for(port, cold, config, &s_nx_advanced_tim1);
}
/** \brief Close only TIM8, preserving a failed cold cleanup receipt. */
static inline nx_result_t nx_stm32_tim8_advanced_cold_close(
    nx_advanced_pwm_controller_t* port, nx_stm32_timer_cold_t* cold,
    const nx_stm32_advanced_cold_config_t* config) {
    return nx_stm32_advanced_close_for(port, cold, config, &s_nx_advanced_tim8);
}

#endif
