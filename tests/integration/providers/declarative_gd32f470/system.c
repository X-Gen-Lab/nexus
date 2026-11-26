/**
 * \file            system.c
 * \brief           Reset-only IRC16M and an explicitly owned GD32 time domain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32_reference.h"
#include "gd32f4xx_timer.h"
#include "nexus/arch/arch.h"
#include "nexus/core/fault.h"

#if !defined(NX_GD32_GPIO_SELECTED) || !defined(NX_GD32_UART_SELECTED)
#error "The reference package must select its actual shared resource users"
#endif

static volatile uint64_t s_overflow_us;
static volatile bool s_clock_live;
static bool s_timer_owned;
static bool s_resources;
static bool s_gpio_clock;
static bool s_uart_route;
static uint32_t s_gpio_mode;
static uint32_t s_gpio_type;
static uint32_t s_gpio_speed;
static uint32_t s_gpio_pull;
static uint32_t s_gpio_af;

uint32_t SystemCoreClock = IRC16M_VALUE;

void _init(void);
void _fini(void);
/** \brief Newlib startup has no platform-owned pre-constructor operation. */
__attribute__((weak)) void _init(void) {
}
/** \brief Newlib shutdown has no implicit hardware lifecycle operation. */
__attribute__((weak)) void _fini(void) {
}

extern const uint32_t __gVectors[];

#define NX_UART_PADS      (GPIO_PIN_9 | GPIO_PIN_10)
#define NX_UART_FIELDS    ((UINT32_C(3) << 18U) | (UINT32_C(3) << 20U))
#define NX_UART_AF_FIELDS UINT32_C(0x00000ff0)

/** \brief The reference never changes a foreign oscillator or divider. */
static bool reset_clock(void) {
    return (RCU_CTL & (RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB)) ==
               (RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB) &&
           (RCU_CTL & (RCU_CTL_PLLEN | RCU_CTL_HXTALEN)) == 0U &&
           (RCU_CFG0 & (RCU_CFG0_SCS | RCU_CFG0_SCSS | RCU_CFG0_AHBPSC |
                        RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC)) == 0U;
}

/** \brief Reset startup owns only CPU access and the selected Flash vector. */
void SystemInit(void) {
    SCB->CPACR |= UINT32_C(0x00f00000);
    SCB->VTOR = (uint32_t)(uintptr_t)__gVectors;
    nx_arch_dsb();
    nx_arch_isb();
}

/** \brief Query readiness without treating numeric zero as a clock proof. */
bool nx_gd32_system_ready(void) {
    return s_clock_live && reset_clock() &&
           (RCU_APB1EN & RCU_APB1EN_TIMER1EN) != 0U &&
           (TIMER_CTL0(TIMER1) & TIMER_CTL0_CEN) != 0U;
}

/** \brief Start one reset-only timer source; no scheduler or external crystal.
 */
nx_platform_start_result_t nx_gd32_system_start(void) {
    nx_platform_start_result_t result = {NX_SUCCESS, NX_SUCCESS, 0U};
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        result.primary = NX_ERROR_CONTEXT;
        return result;
    }
    if (s_timer_owned) {
        result.primary = NX_ERROR_BUSY;
        result.remaining_effects = 1U;
        return result;
    }
    if (!reset_clock() || (RCU_APB1EN & RCU_APB1EN_TIMER1EN) != 0U ||
        NVIC_GetEnableIRQ(TIMER1_IRQn) != 0U ||
        NVIC_GetPendingIRQ(TIMER1_IRQn) != 0U ||
        NVIC_GetActive(TIMER1_IRQn) != 0U) {
        result.primary = NX_ERROR_BUSY;
        return result;
    }
    rcu_periph_clock_enable(RCU_TIMER1);
    /* A gated register block could have hidden a foreign configuration. The
     * gate receipt never grants permission to reset that foreign controller. */
    if (TIMER_CTL0(TIMER1) != 0U || TIMER_CTL1(TIMER1) != 0U ||
        TIMER_SMCFG(TIMER1) != 0U || TIMER_DMAINTEN(TIMER1) != 0U ||
        TIMER_CHCTL0(TIMER1) != 0U || TIMER_CHCTL1(TIMER1) != 0U ||
        TIMER_CHCTL2(TIMER1) != 0U || TIMER_PSC(TIMER1) != 0U) {
        rcu_periph_clock_disable(RCU_TIMER1);
        result.primary = NX_ERROR_BUSY;
        return result;
    }
    s_timer_owned = true;
    timer_parameter_struct timer;
    timer_struct_para_init(&timer);
    timer.prescaler = 15U;
    timer.period = UINT32_MAX;
    timer_init(TIMER1, &timer);
    timer_counter_value_config(TIMER1, 0U);
    timer_interrupt_flag_clear(TIMER1, TIMER_INT_UP);
    timer_interrupt_enable(TIMER1, TIMER_INT_UP);
    s_overflow_us = 0U;
    NVIC_SetPriority(TIMER1_IRQn, 0U);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    NVIC_EnableIRQ(TIMER1_IRQn);
    timer_enable(TIMER1);
    nx_arch_dsb();
    nx_arch_isb();
    s_clock_live = true;
    if (!nx_gd32_system_ready() || TIMER_PSC(TIMER1) != 15U ||
        TIMER_CAR(TIMER1) != UINT32_MAX ||
        NVIC_GetEnableIRQ(TIMER1_IRQn) == 0U) {
        s_clock_live = false;
        result.primary = NX_ERROR_IO;
        result.cleanup = nx_gd32_system_release(true);
        result.remaining_effects = s_timer_owned ? 1U : 0U;
    }
    return result;
}

/** \brief Account one observed wrap; service must precede the next full wrap.
 */
void TIMER1_IRQHandler(void) {
    if (s_clock_live &&
        timer_interrupt_flag_get(TIMER1, TIMER_INT_UP) != RESET) {
        timer_interrupt_flag_clear(TIMER1, TIMER_INT_UP);
        if (timer_interrupt_flag_get(TIMER1, TIMER_INT_UP) != RESET ||
            s_overflow_us > UINT64_MAX - (UINT64_C(1) << 32U)) {
            s_clock_live = false;
            timer_interrupt_disable(TIMER1, TIMER_INT_UP);
            NVIC_DisableIRQ(TIMER1_IRQn);
            (void)nx_arch_irq_save();
            nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO,
                             "gd32 reference timer", 0U);
            for (;;) {
                nx_arch_dsb();
            }
        } else {
            s_overflow_us += UINT64_C(1) << 32U;
        }
    }
    nx_arch_dsb();
}

/** \brief Snapshot one live domain without clearing its owned overflow fact. */
nx_time_us_t nx_time_now_us(void) {
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint64_t result = UINT64_MAX;
    if (nx_gd32_system_ready()) {
        uint64_t base = s_overflow_us;
        uint32_t count = timer_counter_read(TIMER1);
        if (timer_interrupt_flag_get(TIMER1, TIMER_INT_UP) != RESET) {
            base += UINT64_C(1) << 32U;
            count = timer_counter_read(TIMER1);
        }
        result = base + count;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Acquire shared bank/UART clocks and the reviewed silicon AF route. */
nx_result_t nx_gd32_resources_acquire(void) {
    if (s_resources)
        return NX_ERROR_BUSY;
    if (!nx_gd32_system_ready())
        return NX_ERROR_STATE;
    if (!NX_GD32_GPIO_SELECTED && !NX_GD32_UART_SELECTED)
        return NX_SUCCESS;
    if (NX_GD32_UART_SELECTED && ((RCU_APB2EN & RCU_APB2EN_USART0EN) != 0U ||
                                  NVIC_GetEnableIRQ(USART0_IRQn) != 0U ||
                                  NVIC_GetPendingIRQ(USART0_IRQn) != 0U ||
                                  NVIC_GetActive(USART0_IRQn) != 0U))
        return NX_ERROR_BUSY;
    s_gpio_clock = (RCU_AHB1EN & RCU_AHB1EN_PAEN) == 0U;
    s_resources = true;
    s_uart_route = false;
    rcu_periph_clock_enable(RCU_GPIOA);
    nx_arch_dsb();
    if ((RCU_AHB1EN & RCU_AHB1EN_PAEN) == 0U)
        return NX_ERROR_IO;
    /* The gate can hide a previously locked pad. Do not touch AF or output
     * fields until the selected route has passed this live-bank check. */
    if (NX_GD32_UART_SELECTED && (GPIO_LOCK(GPIOA) & GPIO_LOCK_LKK) != 0U &&
        (GPIO_LOCK(GPIOA) & NX_UART_PADS) != 0U) {
        if (s_gpio_clock)
            rcu_periph_clock_disable(RCU_GPIOA);
        nx_arch_dsb();
        if (s_gpio_clock && (RCU_AHB1EN & RCU_AHB1EN_PAEN) != 0U)
            return NX_ERROR_IO;
        s_resources = false;
        return NX_ERROR_BUSY;
    }
    if (NX_GD32_UART_SELECTED) {
        s_gpio_mode = GPIO_CTL(GPIOA) & NX_UART_FIELDS;
        s_gpio_type = GPIO_OMODE(GPIOA) & NX_UART_PADS;
        s_gpio_speed = GPIO_OSPD(GPIOA) & NX_UART_FIELDS;
        s_gpio_pull = GPIO_PUD(GPIOA) & NX_UART_FIELDS;
        s_gpio_af = GPIO_AFSEL1(GPIOA) & NX_UART_AF_FIELDS;
        s_uart_route = true;
    }
    if (NX_GD32_UART_SELECTED) {
        gpio_af_set(GPIOA, GPIO_AF_7, NX_UART_PADS);
        gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                                NX_UART_PADS);
        gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, NX_UART_PADS);
        rcu_periph_clock_enable(RCU_USART0);
    }
    nx_arch_dsb();
    if (NX_GD32_UART_SELECTED &&
        ((GPIO_CTL(GPIOA) & NX_UART_FIELDS) !=
             (NX_UART_FIELDS & UINT32_C(0xaaaaaaaa)) ||
         (GPIO_OMODE(GPIOA) & NX_UART_PADS) != 0U ||
         (GPIO_OSPD(GPIOA) & NX_UART_FIELDS) !=
             (NX_UART_FIELDS & UINT32_C(0xaaaaaaaa)) ||
         (GPIO_PUD(GPIOA) & NX_UART_FIELDS) != 0U ||
         (GPIO_AFSEL1(GPIOA) & NX_UART_AF_FIELDS) != UINT32_C(0x770) ||
         (RCU_APB2EN & RCU_APB2EN_USART0EN) == 0U))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

/** \brief Release only after UART and all GPIO-view users have joined. */
nx_result_t nx_gd32_resources_release(void) {
    if (!s_resources)
        return NX_SUCCESS;
    if (s_uart_route) {
        if ((USART_CTL0(USART0) & USART_CTL0_UEN) != 0U ||
            NVIC_GetEnableIRQ(USART0_IRQn) != 0U ||
            NVIC_GetActive(USART0_IRQn) != 0U)
            return NX_ERROR_BUSY;
        rcu_periph_clock_disable(RCU_USART0);
        nx_arch_dsb();
        if ((RCU_APB2EN & RCU_APB2EN_USART0EN) != 0U)
            return NX_ERROR_IO;
        GPIO_AFSEL1(GPIOA) =
            (GPIO_AFSEL1(GPIOA) & ~NX_UART_AF_FIELDS) | s_gpio_af;
        GPIO_OMODE(GPIOA) = (GPIO_OMODE(GPIOA) & ~NX_UART_PADS) | s_gpio_type;
        GPIO_OSPD(GPIOA) = (GPIO_OSPD(GPIOA) & ~NX_UART_FIELDS) | s_gpio_speed;
        GPIO_PUD(GPIOA) = (GPIO_PUD(GPIOA) & ~NX_UART_FIELDS) | s_gpio_pull;
        GPIO_CTL(GPIOA) = (GPIO_CTL(GPIOA) & ~NX_UART_FIELDS) | s_gpio_mode;
    }
    nx_arch_dsb();
    if (s_uart_route &&
        ((GPIO_AFSEL1(GPIOA) & NX_UART_AF_FIELDS) != s_gpio_af ||
         (GPIO_OMODE(GPIOA) & NX_UART_PADS) != s_gpio_type ||
         (GPIO_OSPD(GPIOA) & NX_UART_FIELDS) != s_gpio_speed ||
         (GPIO_PUD(GPIOA) & NX_UART_FIELDS) != s_gpio_pull ||
         (GPIO_CTL(GPIOA) & NX_UART_FIELDS) != s_gpio_mode))
        return NX_ERROR_IO;
    if (s_gpio_clock)
        rcu_periph_clock_disable(RCU_GPIOA);
    nx_arch_dsb();
    if (s_gpio_clock && (RCU_AHB1EN & RCU_AHB1EN_PAEN) != 0U)
        return NX_ERROR_IO;
    s_resources = false;
    s_uart_route = false;
    return NX_SUCCESS;
}

/** \brief End this clock domain only after every consumer has quiesced. */
nx_result_t nx_gd32_system_release(bool started) {
    (void)started;
    if (s_resources)
        return NX_ERROR_BUSY;
    if (!s_timer_owned)
        return NX_SUCCESS;
    s_clock_live = false;
    timer_interrupt_disable(TIMER1, TIMER_INT_UP);
    timer_disable(TIMER1);
    NVIC_DisableIRQ(TIMER1_IRQn);
    NVIC_ClearPendingIRQ(TIMER1_IRQn);
    nx_arch_dsb();
    nx_arch_isb();
    if (NVIC_GetEnableIRQ(TIMER1_IRQn) != 0U ||
        NVIC_GetPendingIRQ(TIMER1_IRQn) != 0U ||
        NVIC_GetActive(TIMER1_IRQn) != 0U)
        return NX_ERROR_IO;
    rcu_periph_reset_enable(RCU_TIMER1RST);
    rcu_periph_reset_disable(RCU_TIMER1RST);
    rcu_periph_clock_disable(RCU_TIMER1);
    s_timer_owned = false;
    return NX_SUCCESS;
}
