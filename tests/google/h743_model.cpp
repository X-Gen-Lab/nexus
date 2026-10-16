/**
 * \file            h743_model.cpp
 * \brief           Hardware-only H743 MMIO and IRQ observation/refusal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-12
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_model.h"
#include "stm32h743_provider.h"
#include <cstring>
#undef nx_arch_irq_save
#undef nx_arch_irq_restore
/* Read-only CPU identity fields are initialized once; the model resets only
 * the writable control fields observed by production preflight. */
static SCB_Type s_h743_core{};
extern "C" {
nx_h743_model_t g_h743_model;
nx_stm32h743_system_t g_nx_stm32h743_system;
void nx_h743_model_reset(void) {
    std::memset(&g_h743_model, 0, sizeof(g_h743_model));
    std::memset(&g_nx_stm32h743_system, 0, sizeof(g_nx_stm32h743_system));
    g_h743_model.privileged = true;
    s_h743_core.CCR = 0U;
    s_h743_core.ICSR = 0U;
    s_h743_core.CPACR = 0U;
    g_h743_model.core = &s_h743_core;
    g_h743_model.rcc.CR = RCC_CR_HSION | RCC_CR_HSIRDY;
    g_h743_model.power.CR3 = PWR_CR3_LDOEN;
    g_h743_model.power.CSR1 = PWR_CSR1_ACTVOSRDY;
    g_h743_model.flash.ACR = FLASH_ACR_LATENCY_7WS;
    g_h743_model.flash_kib = 2048;
    g_h743_model.device_id = 0x450;
    g_h743_model.silicon_id = 0x20030450U;
    g_h743_model.group = 3U;
    g_h743_model.actual_core_hz = 64000000U;
    g_h743_model.spi_registers.CFG1 = 7U | (7U << SPI_CFG1_CRCSIZE_Pos);
    g_h743_model.cache_context_valid = true;
    g_nx_stm32h743_system.rcc = &g_h743_model.rcc;
    g_nx_stm32h743_system.flash = &g_h743_model.flash;
    g_nx_stm32h743_system.power = &g_h743_model.power;
    g_nx_stm32h743_system.timer = &g_h743_model.timer;
    g_nx_stm32h743_system.core = g_h743_model.core;
}
bool nx_h743_model_privileged(void) {
    return g_h743_model.privileged;
}
uint32_t nx_h743_model_read(volatile uint32_t* reg) {
    ++g_h743_model.reads;
    uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    uintptr_t timer = reinterpret_cast<uintptr_t>(&g_h743_model.timer);
    if (g_h743_model.hide_timer_when_gated && address >= timer &&
        address < timer + sizeof(g_h743_model.timer) &&
        (g_h743_model.rcc.APB1LENR & RCC_APB1LENR_TIM2EN) == 0U)
        return 0U;
    uint32_t value = *reg;
    if (g_h743_model.uart != nullptr && reg == &g_h743_model.uart->RDR)
        g_h743_model.uart->ISR &= ~(uint32_t)USART_ISR_RXNE_RXFNE;
    return value;
}
void nx_h743_model_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == &g_h743_model.timer.DIER && value == 0U &&
        g_h743_model.timer_pending_on_dier) {
        g_h743_model.timer_pending_on_dier = false;
        g_h743_model.timer.SR |= TIM_SR_UIF;
        g_h743_model.irq_pending[TIM2_IRQn] = true;
    }
    uintptr_t address = reinterpret_cast<uintptr_t>(reg);
    uintptr_t timer = reinterpret_cast<uintptr_t>(&g_h743_model.timer);
    if (address >= timer && address < timer + sizeof(g_h743_model.timer))
        ++g_h743_model.timer_writes;
    if (reg == &g_h743_model.rcc.APB1LRSTR)
        ++g_h743_model.timer_reset_writes;
    ++g_h743_model.writes;
    GPIO_TypeDef* banks[] = {&g_h743_model.gpio_b, &g_h743_model.gpio_d,
                             &g_h743_model.gpio_a, &g_h743_model.gpio_e};
    for (GPIO_TypeDef* gpio : banks) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(gpio);
        if (address >= base && address - base < sizeof(*gpio))
            ++g_h743_model.gpio_writes;
    }
    if (reg == g_h743_model.ignore_write)
        return;
    if (g_h743_model.pwm != nullptr) {
        TIM_TypeDef* pwm = g_h743_model.pwm;
        if (reg == &pwm->EGR && (value & TIM_EGR_UG) != 0U &&
            !g_h743_model.hold_pwm_update)
            pwm->SR |= TIM_SR_UIF;
        if (reg == &pwm->SR) {
            *reg &= value;
            return;
        }
        if (reg == &g_h743_model.rcc.APB1LRSTR &&
            (value & RCC_APB1LRSTR_TIM3RST) != 0U &&
            !g_h743_model.hold_pwm_reset)
            std::memset(pwm, 0, sizeof(*pwm));
    }
    if (reg == &g_h743_model.exti.PR1) {
        *reg &= ~value;
        return;
    }
    if (g_h743_model.adc != nullptr && reg == &g_h743_model.adc->CR) {
        if ((g_h743_model.silicon_id & 0x30000000U) == 0x10000000U)
            value &= ~(uint32_t)(ADC_CR_BOOST_1 | (3U << 6U));
        if ((value & ADC_CR_ADCAL) != 0U &&
            g_h743_model.refuse_adc_calibration_command)
            return;
        const uint32_t linear = ADC_CR_LINCALRDYW1 | ADC_CR_LINCALRDYW2 |
                                ADC_CR_LINCALRDYW3 | ADC_CR_LINCALRDYW4 |
                                ADC_CR_LINCALRDYW5 | ADC_CR_LINCALRDYW6;
        const uint32_t changed = (*reg ^ value) & linear;
        if ((value & ADC_CR_DEEPPWD) != 0U)
            g_h743_model.adc_calibrated = false;
        if (changed != 0U &&
            (((changed & (changed - 1U)) != 0U) || (*reg & ADC_CR_ADEN) == 0U ||
             (*reg & (ADC_CR_ADSTART | ADC_CR_JADSTART)) != 0U)) {
            ++g_h743_model.adc_illegal_linear_writes;
            value = (value & ~linear) | (*reg & linear);
        }
    }
    if (g_h743_model.adc != nullptr && reg == &g_h743_model.adc->ISR) {
        *reg &= ~value;
        return;
    }
    if (reg == &g_h743_model.rcc.APB1LRSTR &&
        (value & RCC_APB1LRSTR_I2C1RST) != 0U)
        std::memset(&g_h743_model.i2c_registers, 0,
                    sizeof(g_h743_model.i2c_registers));
    if (g_h743_model.physical_flash != nullptr) {
        auto* p = static_cast<nx_stm32h743_flash_state_t*>(
            g_h743_model.physical_flash);
        for (unsigned bank = 0; bank < 2U; ++bank) {
            volatile uint32_t* cr =
                bank == 0U ? &p->registers->CR1 : &p->registers->CR2;
            volatile uint32_t* key =
                bank == 0U ? &p->registers->KEYR1 : &p->registers->KEYR2;
            volatile uint32_t* sr =
                bank == 0U ? &p->registers->SR1 : &p->registers->SR2;
            volatile uint32_t* ccr =
                bank == 0U ? &p->registers->CCR1 : &p->registers->CCR2;
            if (reg == key && value == 0xcdef89abU)
                *cr &= ~(uint32_t)FLASH_CR_LOCK;
            if (reg == ccr) {
                *sr &= ~value;
                return;
            }
            if (reg == cr && (value & FLASH_CR_START) != 0U) {
                if (g_h743_model.hold_flash_start)
                    return;
                uint32_t sector =
                    (value & (uint32_t)FLASH_CR_SNB) >> FLASH_CR_SNB_Pos;
                std::memset(
                    (void*)(p->memory + bank * 1048576U + sector * 131072U),
                    0xff, 131072U);
                *sr |= g_h743_model.flash_pulse_error ? FLASH_SR_OPERR
                                                      : FLASH_SR_EOP;
            }
        }
    }
    if (g_h743_model.spi != nullptr) {
        SPI_TypeDef* spi = g_h743_model.spi;
        if (reg == &g_h743_model.rcc.APB2RSTR &&
            (value & RCC_APB2RSTR_SPI4RST) != 0U &&
            !g_h743_model.hold_spi_reset) {
            if ((spi->SR & SPI_SR_EOT) != 0U &&
                g_h743_model.timer.CNT < g_h743_model.spi_tail_until)
                g_h743_model.spi_tail_truncated = true;
            std::memset(spi, 0, sizeof(*spi));
            spi->CFG1 = 7U | (7U << SPI_CFG1_CRCSIZE_Pos);
            spi->SR = SPI_SR_TXP | SPI_SR_TXC;
            g_h743_model.spi_sent = g_h743_model.spi_received = 0U;
        }
        if (reg == &spi->IFCR) {
            spi->SR &= ~value;
            return;
        }
    }
    if (g_h743_model.watchdog != nullptr &&
        (reg == &g_h743_model.watchdog->PR ||
         reg == &g_h743_model.watchdog->RLR) &&
        g_h743_model.hold_watchdog_update)
        g_h743_model.watchdog->SR |= IWDG_SR_PVU | IWDG_SR_RVU;
    if (reg == &g_h743_model.timer.CR2 && g_h743_model.hold_timer_mms_restore &&
        (g_h743_model.timer.CR2 & TIM_CR2_MMS) == TIM_CR2_MMS_0 &&
        (value & TIM_CR2_MMS) == 0U)
        return;
    if (reg == &g_h743_model.rcc.APB1LENR && g_h743_model.hold_timer_gate_off &&
        (g_h743_model.rcc.APB1LENR & RCC_APB1LENR_TIM2EN) != 0U &&
        (value & RCC_APB1LENR_TIM2EN) == 0U)
        return;
    if (reg == &g_h743_model.rcc.CR) {
        const uint32_t readonly = RCC_CR_HSIRDY | RCC_CR_HSIDIVF;
        const uint32_t previous = *reg;
        *reg = (value & ~readonly) | (previous & readonly);
        if ((previous & RCC_CR_HSIDIV) != (value & RCC_CR_HSIDIV)) {
            *reg &= ~(uint32_t)RCC_CR_HSIDIVF;
            ++g_h743_model.divider_writes;
        }
        return;
    }
    if (reg == &g_h743_model.timer.EGR && (value & TIM_EGR_UG) != 0U) {
        if ((g_h743_model.timer.CR2 & TIM_CR2_MMS) == 0U)
            ++g_h743_model.timer_trgo_pulses;
        g_h743_model.timer.SR |= TIM_SR_UIF;
    }
    if (reg == &g_h743_model.timer.CR1 &&
        (g_h743_model.timer.CR1 & TIM_CR1_CEN) == 0U &&
        (value & TIM_CR1_CEN) != 0U &&
        (g_h743_model.timer.CR2 & TIM_CR2_MMS) == TIM_CR2_MMS_0)
        ++g_h743_model.timer_trgo_pulses;
    if (reg == &g_h743_model.timer.SR) {
        *reg &= value;
        return;
    }
    for (GPIO_TypeDef* gpio : banks)
        if (reg == &gpio->BSRR) {
            gpio->ODR |= value & 0xffffU;
            gpio->ODR &= ~(value >> 16U);
        }
    if (g_h743_model.uart != nullptr) {
        USART_TypeDef* uart = g_h743_model.uart;
        if (reg == &uart->ICR)
            uart->ISR &= ~value;
        if (reg == &uart->TDR) {
            uart->ISR &= ~(uint32_t)(USART_ISR_TXE_TXFNF | USART_ISR_TC);
            ++g_h743_model.transmitted;
        }
        if (reg == &uart->RQR && (value & USART_RQR_RXFRQ) != 0U)
            uart->ISR &= ~(uint32_t)USART_ISR_RXNE_RXFNE;
    }
    *reg = value;
}
nx_result_t nx_h743_model_irq_control(int irq, bool enabled, uint8_t priority) {
    if (irq < 0 || irq >= 150 || priority >= 16U) {
        return NX_ERROR_INVALID;
    }
    if ((enabled && !g_h743_model.hold_irq_on) ||
        (!enabled && !g_h743_model.hold_irq_off)) {
        g_h743_model.irq_enabled[irq] = enabled;
        g_h743_model.irq_pending[irq] = false;
        g_h743_model.priority[irq] = priority;
    }
    return g_h743_model.irq_enabled[irq] == enabled ? NX_SUCCESS : NX_ERROR_IO;
}
void nx_h743_model_uart_poll(void* context) {
    auto* port = static_cast<nx_stm32h743_uart_state_t*>(context);
    bool on = (port->registers->CR1 & USART_CR1_UE) != 0U;
    if (on && !g_h743_model.hold_uart_on)
        port->registers->ISR |= USART_ISR_TEACK | USART_ISR_REACK;
    if (!on && !g_h743_model.hold_uart_off)
        port->registers->ISR &= ~(uint32_t)(USART_ISR_TEACK | USART_ISR_REACK);
}
void nx_h743_model_watchdog_poll(void* context) {
    (void)context;
    if (g_h743_model.watchdog != nullptr && !g_h743_model.hold_watchdog_update)
        g_h743_model.watchdog->SR = 0U;
}
void nx_h743_model_adc_poll(void* context) {
    (void)context;
    ++g_h743_model.timer.CNT;
    ADC_TypeDef* adc = g_h743_model.adc;
    if (adc == nullptr)
        return;
    if ((adc->CR & ADC_CR_ADCAL) != 0U && !g_h743_model.hold_adc_calibration) {
        g_h743_model.adc_calibrated = (adc->CR & ADC_CR_ADCALLIN) != 0U;
        adc->CR &= ~(uint32_t)ADC_CR_ADCAL;
        if (g_h743_model.adc_calibrated)
            adc->CR |= ADC_CR_LINCALRDYW1 | ADC_CR_LINCALRDYW2 |
                       ADC_CR_LINCALRDYW3 | ADC_CR_LINCALRDYW4 |
                       ADC_CR_LINCALRDYW5 | ADC_CR_LINCALRDYW6;
    }
    if ((adc->CR & ADC_CR_ADSTP) != 0U)
        adc->CR &= ~(uint32_t)(ADC_CR_ADSTART | ADC_CR_ADSTP);
    if ((adc->CR & ADC_CR_ADDIS) != 0U)
        adc->CR &= ~(uint32_t)(ADC_CR_ADEN | ADC_CR_ADDIS);
    if ((adc->CR & ADC_CR_ADEN) != 0U)
        adc->ISR |= ADC_ISR_ADRDY;
    if ((adc->CR & ADC_CR_ADSTART) != 0U && !g_h743_model.hold_adc_conversion) {
        adc->DR = 0xabcdU;
        adc->ISR |= ADC_ISR_EOC | ADC_ISR_EOS;
        adc->CR &= ~(uint32_t)ADC_CR_ADSTART;
    }
}
void nx_h743_model_flash_poll(void* context) {
    (void)context;
    g_h743_model.timer.CNT += g_h743_model.flash_poll_advance;
}
void nx_h743_model_flash_write32(volatile uint32_t* address, uint32_t value) {
    ++g_h743_model.flash_store_count;
    *address = value;
    auto* p =
        static_cast<nx_stm32h743_flash_state_t*>(g_h743_model.physical_flash);
    uintptr_t offset = reinterpret_cast<uintptr_t>(address) -
                       reinterpret_cast<uintptr_t>(p->memory);
    if ((offset & 31U) == 28U) {
        volatile uint32_t* sr =
            offset < 1048576U ? &p->registers->SR1 : &p->registers->SR2;
        *sr |= g_h743_model.flash_pulse_error ? FLASH_SR_OPERR : FLASH_SR_EOP;
        if (g_h743_model.hold_flash_busy)
            *sr |= FLASH_SR_BSY | FLASH_SR_QW;
    }
}
void nx_h743_model_spi_write8(volatile uint32_t* reg, uint8_t value) {
    *reg = value;
    ++g_h743_model.spi_sent;
    if (!g_h743_model.hold_spi_progress) {
        g_h743_model.spi->RXDR = (uint8_t)~value;
        g_h743_model.spi->SR |= SPI_SR_RXP;
    }
}
uint8_t nx_h743_model_spi_read8(volatile uint32_t* reg) {
    ++g_h743_model.spi_received;
    g_h743_model.spi->SR &= ~(uint32_t)SPI_SR_RXP;
    if (g_h743_model.spi_received == g_h743_model.spi->CR2) {
        g_h743_model.spi->SR |= SPI_SR_EOT | SPI_SR_TXC;
        uint32_t divisor =
            2U << ((g_h743_model.spi->CFG1 & SPI_CFG1_MBR) >> SPI_CFG1_MBR_Pos);
        uint32_t clock = g_nx_stm32h743_system.core_hz;
        g_h743_model.spi_tail_until =
            g_h743_model.timer.CNT + (divisor * 1000000U + clock - 1U) / clock;
    }
    return (uint8_t)*reg;
}
void nx_h743_model_spi_poll(void* context) {
    (void)context;
    if (!g_h743_model.hold_spi_time)
        ++g_h743_model.timer.CNT;
}
void nx_h743_model_system_poll(void* system) {
    (void)system;
    if (g_h743_model.divider_writes == 0U)
        return;
    ++g_h743_model.divider_polls;
    const uint32_t divider = g_h743_model.rcc.CR & RCC_CR_HSIDIV;
    if ((divider == RCC_CR_HSIDIV_2 && g_h743_model.hold_divider_ready) ||
        (divider == RCC_CR_HSIDIV_1 && g_h743_model.hold_divider_restore))
        return;
    g_h743_model.rcc.CR |= RCC_CR_HSIDIVF;
    g_h743_model.actual_core_hz = 64000000U >> (divider >> RCC_CR_HSIDIV_Pos);
}
nx_arch_cache_control_result_t
nx_h743_model_cache_enable(nx_arch_cache_control_t* control, uint32_t mask) {
    ++g_h743_model.cache_enable_calls;
    g_h743_model.cache_context_valid &= nx_arch_irq_is_masked();
    if (g_h743_model.cache_selector_failure) {
        control->saved_selector = 1U;
        control->selector_pending = true;
        return NX_ARCH_CACHE_IO;
    }
    if (g_h743_model.cache_enable_result == NX_ARCH_CACHE_INVALID ||
        g_h743_model.cache_enable_result == NX_ARCH_CACHE_CONTEXT ||
        g_h743_model.cache_enable_result == NX_ARCH_CACHE_UNSUPPORTED ||
        g_h743_model.cache_enable_result == NX_ARCH_CACHE_STATE)
        return g_h743_model.cache_enable_result;
    control->owned_mask = mask;
    if ((mask & NX_ARCH_CACHE_DATA) != 0U)
        g_h743_model.core->CCR |= SCB_CCR_DC_Msk;
    if ((mask & NX_ARCH_CACHE_INSTRUCTION) != 0U)
        g_h743_model.core->CCR |= SCB_CCR_IC_Msk;
    return g_h743_model.cache_enable_result;
}
nx_arch_cache_control_result_t
nx_h743_model_cache_disable(nx_arch_cache_control_t* control) {
    ++g_h743_model.cache_disable_calls;
    g_h743_model.cache_context_valid &= nx_arch_irq_is_masked();
    g_h743_model.cache_disable_core_hz = g_h743_model.actual_core_hz;
    if (g_h743_model.cache_disable_result != NX_ARCH_CACHE_OK)
        return g_h743_model.cache_disable_result;
    g_h743_model.core->CCR &= ~(uint32_t)(SCB_CCR_DC_Msk | SCB_CCR_IC_Msk);
    std::memset(control, 0, sizeof(*control));
    return NX_ARCH_CACHE_OK;
}
static void critical_observe(bool entering) {
    if (g_h743_model.critical_observer != nullptr &&
        !g_h743_model.observing_critical) {
        g_h743_model.observing_critical = true;
        g_h743_model.critical_observer(g_h743_model.critical_context, entering);
        g_h743_model.observing_critical = false;
    }
}
nx_arch_irq_state_t nx_h743_model_irq_save(void) {
    /* Deliver one external configurable IRQ before the real mask instruction.
     */
    auto* before = g_h743_model.before_irq_save;
    if (before != nullptr) {
        g_h743_model.before_irq_save = nullptr;
        before(g_h743_model.critical_context);
    }
    nx_arch_irq_state_t previous = nx_arch_irq_save();
    if (previous.value == 0U)
        critical_observe(true);
    return previous;
}
void nx_h743_model_irq_restore(nx_arch_irq_state_t previous) {
    nx_arch_irq_restore(previous);
    if (previous.value == 0U)
        critical_observe(false);
}
}
