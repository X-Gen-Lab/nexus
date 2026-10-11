/**
 * \file            model.c
 * \brief           Host MMIO model driving the real GD32 provider source
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "gd32f470_provider.h"
#include "gd32f4xx.h"
#include "private/system.h"
#include <string.h>

NVIC_Type g_gd32_model_nvic;
uint32_t g_gd32_reset_flags;
uint64_t g_gd32_model_now;
bool g_gd32_model_isr;
bool g_gd32_model_reset_fails;
bool g_gd32_model_adc_works;
bool g_gd32_model_i2c_stop_works;
bool g_gd32_model_fwdgt_config_fails;
bool g_gd32_model_debug_freeze_fails;
bool g_gd32_model_flash_lock_works = true;
bool g_gd32_model_tc_before_critical;
uint32_t g_gd32_model_pulses;
uint32_t g_gd32_model_mask;
uint32_t g_gd32_model_barrier_advance;
static uint32_t s_host_ordering;

/** \brief           Model a progressing clock and selected hardware side
 * effects. */
uint64_t nx_time_now_us(void) {
    ++g_gd32_model_now;
    if (g_gd32_model_adc_works) {
        ADC_CTL1(ADC0) &= ~(ADC_CTL1_RSTCLB | ADC_CTL1_CLB);
        if ((ADC_CTL1(ADC0) & ADC_CTL1_SWRCST) != 0u) {
            ADC_RDATA(ADC0) = 123u + ADC_RSQ2(ADC0);
            ADC_STAT(ADC0) |= ADC_STAT_EOC;
            ADC_CTL1(ADC0) &= ~ADC_CTL1_SWRCST;
        }
    }
    if (g_gd32_model_i2c_stop_works) {
        I2C_CTL0(I2C0) &= ~I2C_CTL0_STOP;
        I2C_CTL0(I2C1) &= ~I2C_CTL0_STOP;
    }
    return g_gd32_model_now;
}
/** \brief           Save the model's incoming mask. */
nx_arch_irq_state_t nx_arch_irq_save(void) {
    if (g_gd32_model_tc_before_critical && !g_gd32_model_mask) {
        g_gd32_model_tc_before_critical = false;
        USART_STAT0(USART0) = USART_STAT0_TC;
        USART0_IRQHandler();
    }
    nx_arch_irq_state_t saved = {g_gd32_model_mask};
    g_gd32_model_mask = 1u;
    return saved;
}
/** \brief           Restore the model's incoming mask. */
void nx_arch_irq_restore(nx_arch_irq_state_t saved) {
    g_gd32_model_mask = saved.value;
}
/** \brief           Report injected exception context. */
bool nx_arch_in_isr(void) {
    return g_gd32_model_isr;
}
/** \brief           Inspect the modeled incoming mask. */
bool nx_arch_irq_is_masked(void) {
    return g_gd32_model_mask != 0u;
}
/** \brief           Host MMIO stores already complete in this synchronous
 * model. */
void nx_arch_dsb(void) {
    g_gd32_model_now += g_gd32_model_barrier_advance;
    if (g_gd32_model_debug_freeze_fails) {
        DBG_CTL1 &= ~DBG_CTL1_FWDGT_HOLD;
    }
}
/** \brief           Report the model's implemented interrupt-mask domain. */
nx_arch_irq_masks_t nx_arch_irq_masks(void) {
    nx_arch_irq_masks_t masks = {g_gd32_model_mask, 0u, 0u};
    return masks;
}
/** \brief           The GD register fixture injects one external exception. */
uint32_t nx_arch_exception_number(void) {
    return g_gd32_model_isr ? 16u : 0u;
}
/** \brief           This register fixture models privileged execution only. */
bool nx_arch_is_privileged(void) {
    return true;
}
/** \brief           Order synchronous host accesses without hardware claims. */
void nx_arch_dmb(void) {
    /* A real sequentially consistent operation retains host ordering and is
     * observable by race instrumentation, unlike its unsupported fence hook.
     * This fixture still establishes no physical MMIO completion or timing. */
    (void)__atomic_fetch_add(&s_host_ordering, 0u, __ATOMIC_SEQ_CST);
}
/** \brief           The host register fixture has no instruction pipeline. */
void nx_arch_isb(void) {
    (void)__atomic_fetch_add(&s_host_ordering, 0u, __ATOMIC_SEQ_CST);
}
/** \brief           No physical DWT counter exists in the host fixture. */
bool nx_arch_cycle_snapshot(uint32_t* cycles) {
    (void)cycles;
    return false;
}
/** \brief           Clock enable is observed separately from register
 * algorithms. */
void rcu_periph_clock_enable(rcu_periph_enum periph) {
    REG32(RCU + ((uint32_t)periph >> 6U)) |= 1U << ((uint32_t)periph & 31U);
}
/** \brief           Clock release never fabricates a controller reset. */
void rcu_periph_clock_disable(rcu_periph_enum periph) {
    REG32(RCU + ((uint32_t)periph >> 6U)) &= ~(1U << ((uint32_t)periph & 31U));
}
/** \brief           Inject a UART reset success or a stuck enabled controller.
 */
void rcu_periph_reset_enable(rcu_periph_reset_enum periph) {
    if (periph == RCU_USART0RST || periph == RCU_USART1RST) {
        uint32_t uart = periph == RCU_USART0RST ? USART0 : USART1;
        USART_CTL0(uart) = g_gd32_model_reset_fails ? USART_CTL0_UEN : 0u;
        USART_CTL1(uart) = 0u;
        USART_CTL2(uart) = 0u;
    }
}
/** \brief           Release the modeled peripheral reset pulse. */
void rcu_periph_reset_disable(rcu_periph_reset_enum periph) {
    (void)periph;
}
/** \brief           Model pin mode bits using the SDK register layout. */
void gpio_mode_set(uint32_t gpio, uint32_t mode, uint32_t pull, uint32_t pins) {
    for (unsigned pin = 0u; pin < 16u; ++pin) {
        if ((pins & (1u << pin)) != 0u) {
            GPIO_CTL(gpio) =
                (GPIO_CTL(gpio) & ~(3u << (pin * 2u))) | (mode << (pin * 2u));
            GPIO_PUD(gpio) =
                (GPIO_PUD(gpio) & ~(3u << (pin * 2u))) | (pull << (pin * 2u));
        }
    }
}
/** \brief           Model push-pull/open-drain output type. */
void gpio_output_options_set(uint32_t gpio, uint8_t type, uint32_t speed,
                             uint32_t pins) {
    (void)speed;
    GPIO_OMODE(gpio) = (GPIO_OMODE(gpio) & ~pins) | (type ? pins : 0u);
}
/** \brief           Model exact selected AF nibbles. */
void gpio_af_set(uint32_t gpio, uint32_t af, uint32_t pins) {
    for (unsigned pin = 0u; pin < 16u; ++pin) {
        if ((pins & (1u << pin)) != 0u) {
            volatile uint32_t* reg =
                (volatile uint32_t*)(uintptr_t)(gpio + 0x20u + (pin / 8u) * 4u);
            unsigned shift = (pin % 8u) * 4u;
            *reg = (*reg & ~(15u << shift)) | (af << shift);
        }
    }
}
/** \brief           Configure the declared 100 MHz USART0 divider. */
void usart_baudrate_set(uint32_t usart, uint32_t baud) {
    USART_BAUD(usart) = (100000000u + baud / 2u) / baud;
}
/** \brief           Reset modeled UART hardware registers. */
void usart_deinit(uint32_t usart) {
    USART_CTL0(usart) = 0u;
    USART_CTL1(usart) = 0u;
    USART_CTL2(usart) = 0u;
    USART_STAT0(usart) = USART_STAT0_TBE | USART_STAT0_TC;
}
/** \brief           Reset the modeled SPI shifter and data state. */
void spi_i2s_deinit(uint32_t spi) {
    SPI_CTL0(spi) = 0u;
    SPI_CTL1(spi) = 0u;
    SPI_STAT(spi) = SPI_STAT_TBE;
}
/** \brief           Reset our I2C controller without making the lines idle. */
void i2c_deinit(uint32_t i2c) {
    I2C_CTL0(i2c) = 0u;
    I2C_CTL1(i2c) = 0u;
    I2C_STAT0(i2c) = 0u;
    I2C_STAT1(i2c) = 0u;
}
/** \brief           Clear model timer registers. */
void timer_deinit(uint32_t timer) {
    memset((void*)(uintptr_t)timer, 0, 0x50u);
}
/** \brief           Reset the modeled ADC without supplying conversion success.
 */
void adc_deinit(void) {
    memset((void*)(uintptr_t)ADC0, 0, 0x50u);
}
/** \brief           Record ADC common clock selection. */
void adc_clock_config(uint32_t divider) {
    ADC_SYNCCTL = divider;
}
/** \brief           Unlock the modeled physical Flash controller. */
void fmc_unlock(void) {
    FMC_CTL &= ~FMC_CTL_LK;
}
/** \brief           Model lock readback failure without releasing ownership. */
void fmc_lock(void) {
    if (g_gd32_model_flash_lock_works) {
        FMC_CTL |= FMC_CTL_LK;
    }
}
/** \brief           Clear modeled latched Flash errors. */
void fmc_flag_clear(uint32_t flags) {
    FMC_STAT &= ~flags;
}
/** \brief           Model one settled halfword pulse and count its occurrence.
 */
fmc_state_enum fmc_halfword_program(uint32_t address, uint16_t value) {
    ++g_gd32_model_pulses;
    *(uint16_t*)(uintptr_t)address &= value;
    return FMC_READY;
}
/** \brief           Model one settled 4 KiB page erase, not MCU qualification.
 */
fmc_state_enum fmc_page_erase(uint32_t address) {
    ++g_gd32_model_pulses;
    memset((void*)(uintptr_t)address, 0xFF, 4096u);
    return FMC_READY;
}
/** \brief           Inject watchdog configuration failure before enable. */
ErrStatus fwdgt_config(uint16_t reload, uint8_t prescaler) {
    if (g_gd32_model_fwdgt_config_fails) {
        return ERROR;
    }
    FWDGT_RLD = reload;
    FWDGT_PSC = prescaler;
    return SUCCESS;
}
