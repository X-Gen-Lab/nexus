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
#include "watchdog_port.h"
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
/** \brief Model one SDK input-bank read without electrical assumptions. */
uint16_t gpio_input_port_get(uint32_t gpio) {
    return (uint16_t)GPIO_ISTAT(gpio);
}
/** \brief Model one SDK output-latch read for the serialized writer. */
uint16_t gpio_output_port_get(uint32_t gpio) {
    return (uint16_t)GPIO_OCTL(gpio);
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
/** \brief Preserve the official nine-bit DATA field on transmit. */
void usart_data_transmit(uint32_t usart, uint16_t data) {
    USART_DATA(usart) = (uint32_t)data & (uint32_t)USART_DATA_DATA;
}
/** \brief Preserve the official nine-bit DATA field on receive. */
uint16_t usart_data_receive(uint32_t usart) {
    return (uint16_t)(USART_DATA(usart) & (uint32_t)USART_DATA_DATA);
}
/** \brief           Reset the modeled SPI shifter and data state. */
void spi_i2s_deinit(uint32_t spi) {
    SPI_CTL0(spi) = 0u;
    SPI_CTL1(spi) = 0u;
    SPI_STAT(spi) = SPI_STAT_TBE;
}
/** \brief SDK disable requests do not fabricate idle shifter status. */
void spi_disable(uint32_t spi) {
    SPI_CTL0(spi) &= ~(uint32_t)SPI_CTL0_SPIEN;
}
/** \brief Model the SDK DATA store; fixtures own peripheral progress. */
void spi_i2s_data_transmit(uint32_t spi, uint16_t data) {
    SPI_DATA(spi) = data;
}
/** \brief Model the SDK DATA load without clearing modeled status. */
uint16_t spi_i2s_data_receive(uint32_t spi) {
    return (uint16_t)SPI_DATA(spi);
}
/** \brief           Reset our I2C controller without making the lines idle. */
void i2c_deinit(uint32_t i2c) {
    I2C_CTL0(i2c) = 0u;
    I2C_CTL1(i2c) = 0u;
    I2C_STAT0(i2c) = 0u;
    I2C_STAT1(i2c) = 0u;
}
/** \brief Keep ACK changes independent of the receive-window model. */
void i2c_ack_config(uint32_t i2c, uint32_t ack) {
    I2C_CTL0(i2c) = (I2C_CTL0(i2c) & ~(uint32_t)I2C_CTL0_ACKEN) | ack;
}
/** \brief START requests do not manufacture address acknowledgements. */
void i2c_start_on_bus(uint32_t i2c) {
    I2C_CTL0(i2c) |= (uint32_t)I2C_CTL0_START;
}
/** \brief STOP requests still need the model's independently injected drain. */
void i2c_stop_on_bus(uint32_t i2c) {
    I2C_CTL0(i2c) |= (uint32_t)I2C_CTL0_STOP;
}
/** \brief Model the SDK's eight-bit DATA store. */
void i2c_data_transmit(uint32_t i2c, uint8_t data) {
    I2C_DATA(i2c) = data;
}
/** \brief Model the SDK's eight-bit DATA observation. */
uint8_t i2c_data_receive(uint32_t i2c) {
    return (uint8_t)I2C_DATA(i2c);
}
/** \brief           Clear model timer registers. */
void timer_deinit(uint32_t timer) {
    memset((void*)(uintptr_t)timer, 0, 0x50u);
}
/** \brief The update-delayed SDK prescaler selection adds no update event. */
void timer_prescaler_config(uint32_t timer, uint16_t prescaler,
                            uint8_t reload) {
    TIMER_PSC(timer) = prescaler;
    if (reload == TIMER_PSC_RELOAD_NOW) {
        TIMER_SWEVG(timer) |= (uint32_t)TIMER_SWEVG_UPG;
    }
}
/** \brief Model the SDK's autoreload value store. */
void timer_autoreload_value_config(uint32_t timer, uint32_t autoreload) {
    TIMER_CAR(timer) = autoreload;
}
/** \brief Preserve SDK channel selection instead of aliasing every CCR. */
void timer_channel_output_pulse_value_config(uint32_t timer, uint16_t channel,
                                             uint32_t pulse) {
    if (channel <= TIMER_CH_3) {
        REG32(timer + UINT32_C(0x34) + (uint32_t)channel * 4U) = pulse;
    }
}
/** \brief Counter enable does not synthesize an update or PWM output. */
void timer_enable(uint32_t timer) {
    TIMER_CTL0(timer) |= (uint32_t)TIMER_CTL0_CEN;
}
/** \brief Counter disable changes only the selected CEN bit. */
void timer_disable(uint32_t timer) {
    TIMER_CTL0(timer) &= ~(uint32_t)TIMER_CTL0_CEN;
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
/** \brief SDK data reads do not manufacture a completed conversion. */
uint16_t adc_routine_data_read(uint32_t adc) {
    return (uint16_t)ADC_RDATA(adc);
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
/** \brief Latch completion and its pending vector only when ENDIE is set. */
static void flash_completed(void) {
    FMC_STAT &= ~FMC_STAT_BUSY;
    if ((FMC_CTL & FMC_CTL_ENDIE) != 0U) {
        FMC_STAT |= FMC_STAT_END;
        NVIC->ISPR[(unsigned)FMC_IRQn / 32U] |= 1U
                                                << ((unsigned)FMC_IRQn % 32U);
    }
}
/** \brief           Model one settled halfword pulse and count its occurrence.
 */
fmc_state_enum fmc_halfword_program(uint32_t address, uint16_t value) {
    ++g_gd32_model_pulses;
    *(uint16_t*)(uintptr_t)address &= value;
    flash_completed();
    return FMC_READY;
}
/** \brief           Model one settled 4 KiB page erase, not MCU qualification.
 */
fmc_state_enum fmc_page_erase(uint32_t address) {
    ++g_gd32_model_pulses;
    memset((void*)(uintptr_t)address, 0xFF, 4096u);
    flash_completed();
    return FMC_READY;
}
/** \brief           Model register synchronization, not a pure SDK helper. */
void nx_gd32_watchdog_model_write(volatile uint32_t* address, uint32_t value) {
    *address = value;
    if (g_gd32_model_fwdgt_config_fails &&
        (address == &FWDGT_PSC || address == &FWDGT_RLD)) {
        FWDGT_STAT = FWDGT_STAT_PUD | FWDGT_STAT_RUD;
    }
}
/** \brief           Keep configured shadow-update failures active. */
void nx_gd32_watchdog_model_poll(void* port) {
    (void)port;
    if (!g_gd32_model_fwdgt_config_fails) {
        FWDGT_STAT = 0U;
    }
}
