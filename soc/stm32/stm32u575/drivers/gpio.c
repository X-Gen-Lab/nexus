/**
 * \file            gpio.c
 * \brief           U575 authorized GPIO latch and source-reviewed AF7 pins
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32u575_cold.h"
#include "stm32u575_provider.h"
#if defined(NEXUS_OS_MPU_GPIO) && NEXUS_OS_MPU_GPIO == 1
#include "stm32u575_gpio_mpu.h"
#endif
#ifndef NX_U575_GPIO_WRITE
#include "stm32u5xx_hal_gpio.h"
#define NX_U575_GPIO_WRITE(gpio, reset, set)                                   \
    HAL_GPIO_WriteMultipleStatePin((gpio), (reset), (set))
#endif
/** \brief Issue one validated nonempty command through the private GPIO port.
 * \note Empty masks remain successful without calling HAL's nonempty leaf.
 */
static inline void write_command(GPIO_TypeDef* gpio, uint32_t set,
                                 uint32_t reset) {
    if ((set | reset) != 0U)
        NX_U575_GPIO_WRITE(gpio, (uint16_t)reset, (uint16_t)set);
}
static nx_result_t write(void* context, uint32_t set, uint32_t reset) {
    nx_stm32u575_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || !p->output || (set & reset) != 0U)
        return NX_ERROR_INVALID;
    if (((set | reset) & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    write_command(p->registers, set, reset);
    return NX_SUCCESS;
}
static nx_result_t read(const void* context, uint32_t* value) {
    const nx_stm32u575_gpio_state_t* p = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (p == NULL || !p->initialized || p->registers == NULL || value == NULL ||
        (uintptr_t)value % _Alignof(uint32_t) != 0U ||
        (uintptr_t)value > UINTPTR_MAX - sizeof(*value) ||
        ((uintptr_t)value < (uintptr_t)p + sizeof(*p) &&
         (uintptr_t)p < (uintptr_t)value + sizeof(*value)) ||
        ((uintptr_t)value < (uintptr_t)p->registers + sizeof(*p->registers) &&
         (uintptr_t)p->registers < (uintptr_t)value + sizeof(*value)))
        return NX_ERROR_INVALID;
    *value = NX_U575_READ(&p->registers->IDR) & p->mask;
    return NX_SUCCESS;
}
static nx_result_t toggle(void* context, uint32_t mask) {
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    nx_stm32u575_gpio_state_t* p = context;
    if (p == NULL || !p->initialized || !p->output)
        return NX_ERROR_INVALID;
    if ((mask & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    uint32_t high = NX_U575_READ(&p->registers->ODR) & mask;
    return write(p, mask & ~high, high);
}
nx_result_t nx_stm32u575_gpio_initialize(nx_stm32u575_gpio_state_t* p,
                                         uint32_t initial) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL || p->mask == 0U ||
        (p->mask & ~0xffffU) != 0U)
        return NX_ERROR_INVALID;
    if ((initial & ~p->mask) != 0U)
        return NX_ERROR_PERMISSION;
    if (p->initialized || p->retained)
        return NX_ERROR_BUSY;
    /* PG[15:2] are isolated after reset. The external rail owner must already
     * establish its supply and remove isolation; a pad lease grants no power
     * policy. IO2SV observes configuration, never the actual rail voltage. */
    if (p->registers == NX_U575_GPIOG && (p->mask & ~3U) != 0U &&
        ((NX_U575_READ(&NX_U575_RCC->AHB3ENR) & RCC_AHB3ENR_PWREN) == 0U ||
         (NX_U575_READ(&NX_U575_PWR->SVMCR) & PWR_SVMCR_IO2SV) == 0U))
        return NX_ERROR_STATE;
    p->retained = true;
    write_command(p->registers, initial, p->mask & ~initial);
    nx_arch_dsb();
    if ((NX_U575_READ(&p->registers->ODR) & p->mask) != initial)
        return NX_ERROR_IO;
    for (unsigned pin = 0U; pin < 16U; ++pin)
        if ((p->mask & (1U << pin)) != 0U) {
            uint32_t shift = pin * 2U;
            NX_U575_WRITE(&p->registers->OTYPER,
                          NX_U575_READ(&p->registers->OTYPER) & ~(1U << pin));
            NX_U575_WRITE(&p->registers->PUPDR,
                          NX_U575_READ(&p->registers->PUPDR) & ~(3U << shift));
            nx_arch_dsb();
            if ((NX_U575_READ(&p->registers->OTYPER) & (1U << pin)) != 0U ||
                (NX_U575_READ(&p->registers->PUPDR) & (3U << shift)) != 0U)
                return NX_ERROR_IO;
            NX_U575_WRITE(
                &p->registers->MODER,
                (NX_U575_READ(&p->registers->MODER) & ~(3U << shift)) |
                    ((p->output ? 1U : 0U) << shift));
            nx_arch_dsb();
            if (((NX_U575_READ(&p->registers->MODER) >> shift) & 3U) !=
                (p->output ? 1U : 0U))
                return NX_ERROR_IO;
        }
    p->initialized = true;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_gpio_stop(nx_stm32u575_gpio_state_t* p,
                                   uint32_t inactive) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (p == NULL || p->registers == NULL || (inactive & ~p->mask) != 0U)
        return NX_ERROR_INVALID;
    if (!p->retained)
        return NX_SUCCESS;
    if (p->output) {
        write_command(p->registers, inactive, p->mask & ~inactive);
        nx_arch_dsb();
        if ((NX_U575_READ(&p->registers->ODR) & p->mask) != inactive)
            return NX_ERROR_IO;
    }
    p->initialized = false;
    p->retained = false;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_gpio_clock_enable(unsigned index) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (index > 7U)
        return NX_ERROR_UNSUPPORTED;
    uint32_t bit = 1U << index;
    volatile uint32_t* gate = &NX_U575_RCC->AHB2ENR1;
    NX_U575_WRITE(gate, NX_U575_READ(gate) | bit);
    nx_arch_dsb();
    return (NX_U575_READ(gate) & bit) != 0U ? NX_SUCCESS : NX_ERROR_IO;
}
static nx_result_t serial_pins_prepare(bool first) {
    nx_result_t result = nx_stm32u575_gpio_clock_enable(0U);
    if (result != NX_SUCCESS)
        return result;
    volatile uint32_t* gate =
        first ? &NX_U575_RCC->APB2ENR : &NX_U575_RCC->APB1ENR1;
    const uint32_t gate_mask =
        first ? RCC_APB2ENR_USART1EN : RCC_APB1ENR1_USART2EN;
    NX_U575_WRITE(gate, NX_U575_READ(gate) | gate_mask);
    nx_arch_dsb();
    if ((NX_U575_READ(gate) & gate_mask) == 0U)
        return NX_ERROR_IO;
#ifndef NX_U575_GPIOA
#define NX_U575_GPIOA GPIOA
#endif
    GPIO_TypeDef* gpio = NX_U575_GPIOA;
    const unsigned tx = first ? 9U : 2U;
    const unsigned rx = tx + 1U;
    const unsigned af_word = tx / 8U;
    const unsigned tx_af_shift = (tx % 8U) * 4U;
    const unsigned rx_af_shift = (rx % 8U) * 4U;
    const uint32_t bits = (1U << tx) | (1U << rx);
    const uint32_t fields = (3U << (2U * tx)) | (3U << (2U * rx));
    const uint32_t af = (15U << tx_af_shift) | (15U << rx_af_shift);
    const uint32_t af_value = (7U << tx_af_shift) | (7U << rx_af_shift);
    const uint32_t speed = (1U << (2U * tx)) | (1U << (2U * rx));
    const uint32_t pull = 1U << (2U * rx);
    const uint32_t mode = (2U << (2U * tx)) | (2U << (2U * rx));
    write_command(gpio, 1U << tx, 0U);
    nx_arch_dsb();
    if ((NX_U575_READ(&gpio->ODR) & (1U << tx)) == 0U)
        return NX_ERROR_IO;
    NX_U575_WRITE(&gpio->OTYPER, NX_U575_READ(&gpio->OTYPER) & ~bits);
    NX_U575_WRITE(&gpio->OSPEEDR,
                  (NX_U575_READ(&gpio->OSPEEDR) & ~fields) | speed);
    NX_U575_WRITE(&gpio->PUPDR, (NX_U575_READ(&gpio->PUPDR) & ~fields) | pull);
    NX_U575_WRITE(&gpio->AFR[af_word],
                  (NX_U575_READ(&gpio->AFR[af_word]) & ~af) | af_value);
    nx_arch_dsb();
    if ((NX_U575_READ(&gpio->OTYPER) & bits) != 0U ||
        (NX_U575_READ(&gpio->OSPEEDR) & fields) != speed ||
        (NX_U575_READ(&gpio->PUPDR) & fields) != pull ||
        (NX_U575_READ(&gpio->AFR[af_word]) & af) != af_value)
        return NX_ERROR_IO;
    NX_U575_WRITE(&gpio->MODER, (NX_U575_READ(&gpio->MODER) & ~fields) | mode);
    nx_arch_dsb();
    return (NX_U575_READ(&gpio->MODER) & fields) == mode &&
                   (NX_U575_READ(&gpio->AFR[af_word]) & af) == af_value
               ? NX_SUCCESS
               : NX_ERROR_IO;
}
nx_result_t nx_stm32u575_usart1_pins_prepare(void) {
    return serial_pins_prepare(true);
}
nx_result_t nx_stm32u575_usart2_pins_prepare(void) {
    return serial_pins_prepare(false);
}
nx_result_t nx_stm32u575_pin_alternate(GPIO_TypeDef* gpio, unsigned pin,
                                       unsigned af) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (gpio == NULL || pin > 15U || af > 15U)
        return NX_ERROR_INVALID;
    if ((NX_U575_READ(&gpio->LCKR) & (1U << pin)) != 0U)
        return NX_ERROR_BUSY;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint32_t shift = (pin % 8U) * 4U;
    uint32_t af_value =
        (NX_U575_READ(&gpio->AFR[pin / 8U]) & ~(15U << shift)) | (af << shift);
    NX_U575_WRITE(&gpio->AFR[pin / 8U], af_value);
    nx_arch_dsb();
    nx_result_t result = NX_ERROR_IO;
    if (NX_U575_READ(&gpio->AFR[pin / 8U]) == af_value) {
        uint32_t mode = (NX_U575_READ(&gpio->MODER) & ~(3U << (pin * 2U))) |
                        (2U << (pin * 2U));
        NX_U575_WRITE(&gpio->MODER, mode);
        nx_arch_dsb();
        if (NX_U575_READ(&gpio->MODER) == mode)
            result = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    return result;
}
const nx_gpio_ops_t nx_stm32u575_gpio_ops = {write, read, toggle};

#if defined(NEXUS_OS_MPU_GPIO) && NEXUS_OS_MPU_GPIO == 1
#ifndef NX_U575_MPU_GPIO_PORT
#define NX_U575_MPU_GPIO_PORT GPIOC
#endif
bool nx_stm32u575_gpio_mpu_validate(
    const nx_gpio_port_t* port, uint32_t mask,
    const nx_stm32u575_gpio_mpu_bounds_t* bounds) {
    if (port == NULL || bounds == NULL || port->ops != &nx_stm32u575_gpio_ops) {
        return false;
    }
    const nx_stm32u575_gpio_state_t* state = port->context;
    uintptr_t first = (uintptr_t)state;
    if (first % _Alignof(nx_stm32u575_gpio_state_t) != 0U ||
        first < bounds->ram_begin || first >= bounds->ram_end ||
        sizeof(*state) > bounds->ram_end - first) {
        return false;
    }
    uintptr_t last = first + sizeof(*state);
    if ((first < bounds->task_end && bounds->task_begin < last) ||
        (first < bounds->control_end && bounds->control_begin < last)) {
        return false;
    }
    /* This selected profile owns only the existing GPIOC PC7 output. Exact
     * ops identity and complete storage checks precede every private field
     * read; the final bounded method performs one BSRR store, with no OS. */
    return state->registers == NX_U575_MPU_GPIO_PORT && state->mask == 0x80U &&
           state->initialized && state->output && state->retained &&
           mask == 0x80U;
}
#endif

nx_result_t
nx_stm32u575_gpio_prepare(nx_stm32u575_gpio_state_t* port,
                          const nx_stm32u575_gpio_config_t* config) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (port == NULL || config == NULL || config->registers == NULL)
        return NX_ERROR_INVALID;
    if (port->retained || port->initialized)
        return NX_ERROR_BUSY;
    port->registers = config->registers;
    port->mask = config->mask;
    port->output = config->output;
    return nx_stm32u575_gpio_initialize(port, config->initial);
}
