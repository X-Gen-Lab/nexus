/**
 * \file            i2c_v2_long_factory_model.cpp
 * \brief           SFR-only START, byte, TCR, STOP and reset behavior
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_v2_long_factory_model.h"
#if defined(NX_I2C_LONG_G474)
#include "stm32g474_provider.h"
#elif defined(NX_I2C_LONG_H743)
#include "h743_flash_fixture.h"
#include "stm32h743_provider.h"
#else
#include "stm32u575_provider.h"
#endif
#include <cstring>
nx_i2c_long_factory_model_t g_long_i2c{};
#if defined(NX_I2C_LONG_G474)
extern "C" {
ADC_TypeDef g_g474_acquisition_adc[2];
ADC_Common_TypeDef g_g474_acquisition_common;
TIM_TypeDef g_g474_acquisition_timer;
DMA_TypeDef g_g474_acquisition_dma;
DMA_Channel_TypeDef g_g474_acquisition_channel;
DMAMUX_Channel_TypeDef g_g474_acquisition_mux;
I2C_TypeDef g_g474_acquisition_i2c;
SYSCFG_TypeDef g_g474_acquisition_syscfg;
}
/* Reuse the maintained acquisition fixture's peripheral leaves; there is no
 * replacement of ADC/I2C initialization or of any provider operation. */
extern "C" void nx_g474_acquisition_factory_write(volatile uint32_t* reg,
                                                  uint32_t value) {
    if (reg == &ADC1->ISR) {
        ADC1->ISR &= ~value;
        return;
    }
    if (reg == &DMA1->IFCR) {
        const_cast<volatile uint32_t&>(DMA1->ISR) &= ~value;
        return;
    }
    nx_g474_model_write(reg, value);
}
extern "C" void nx_g474_acquisition_factory_poll(nx_stm32g474_adc_state_t* p) {
    ++g_g474_model.timer.CNT;
    if ((p->registers->CR & ADC_CR_ADCAL) != 0U)
        p->registers->CR &= ~(uint32_t)ADC_CR_ADCAL;
    if ((p->registers->CR & ADC_CR_ADEN) != 0U)
        p->registers->ISR |= ADC_ISR_ADRDY;
    if ((p->registers->CR & ADC_CR_ADSTP) != 0U)
        p->registers->CR &= ~(uint32_t)(ADC_CR_ADSTP | ADC_CR_ADSTART);
    if ((p->registers->CR & ADC_CR_ADDIS) != 0U)
        p->registers->CR &= ~(uint32_t)(ADC_CR_ADDIS | ADC_CR_ADEN);
}
#endif
namespace {
constexpr uint32_t start = I2C_CR2_START;
constexpr uint32_t reload = I2C_CR2_RELOAD;
constexpr uint32_t autoend = I2C_CR2_AUTOEND;
constexpr uint32_t read_direction = I2C_CR2_RD_WRN;
constexpr uint32_t busy = I2C_ISR_BUSY;
constexpr uint32_t tcr = I2C_ISR_TCR;
constexpr uint32_t tc = I2C_ISR_TC;
constexpr uint32_t stopf = I2C_ISR_STOPF;
constexpr uint32_t txis = I2C_ISR_TXIS;
constexpr uint32_t rxne = I2C_ISR_RXNE;
constexpr uint32_t byte_mask = I2C_CR2_NBYTES;
#if defined(NX_I2C_LONG_U575)
nx_i2c_v2_registers_t u5_i2c{};
SPI_TypeDef u5_spi{};
ADC_TypeDef u5_adc{};
ADC_Common_TypeDef u5_adc_common{};
#endif
void tick() {
#if defined(NX_I2C_LONG_G474)
    ++g_g474_model.timer.CNT;
#elif defined(NX_I2C_LONG_H743)
    ++g_h743_model.timer.CNT;
#else
    ++g_u575_model.timer.CNT;
#endif
}
uint32_t base_read(volatile uint32_t* reg) {
#if defined(NX_I2C_LONG_G474)
    return nx_g474_model_read(reg);
#elif defined(NX_I2C_LONG_H743)
    return nx_h743_model_read(reg);
#else
    return nx_u575_devices_read(reg);
#endif
}
void base_write(volatile uint32_t* reg, uint32_t value) {
#if defined(NX_I2C_LONG_G474)
    nx_g474_model_write(reg, value);
#elif defined(NX_I2C_LONG_H743)
    nx_h743_model_write(reg, value);
#else
    nx_u575_devices_write(reg, value);
#endif
}
void finish_chunk() {
    auto& r = *g_long_i2c.registers;
    if ((r.cr2 & reload) != 0U) {
        g_long_i2c.waiting_reload = true;
        g_long_i2c.tcr_wait = g_long_i2c.tcr_delay_polls;
    } else if ((r.cr2 & autoend) != 0U) {
        r.isr = (r.isr & ~(busy | txis | tc | tcr)) | stopf;
        g_long_i2c.running = false;
        ++g_long_i2c.stops;
        if (g_long_i2c.event != nullptr)
            g_long_i2c.event(NX_LONG_STOP, false);
    } else {
        r.isr |= tc;
    }
}
} /* namespace */
extern "C" void nx_i2c_long_factory_reset(void) {
    g_long_i2c = {};
#if defined(NX_I2C_LONG_G474)
    nx_g474_model_reset();
    std::memset(g_g474_acquisition_adc, 0, sizeof(g_g474_acquisition_adc));
    std::memset(ADC12_COMMON, 0, sizeof(*ADC12_COMMON));
    std::memset(TIM6, 0, sizeof(*TIM6));
    std::memset(DMA1, 0, sizeof(*DMA1));
    std::memset(DMA1_Channel1, 0, sizeof(*DMA1_Channel1));
    std::memset(DMAMUX1_Channel0, 0, sizeof(*DMAMUX1_Channel0));
    std::memset(I2C1, 0, sizeof(*I2C1));
    std::memset(SYSCFG, 0, sizeof(*SYSCFG));
    ADC1->CR = ADC2->CR = ADC_CR_DEEPPWD;
    g_long_i2c.reset = &RCC->APB1RSTR1;
    g_long_i2c.reset_mask = RCC_APB1RSTR1_I2C1RST;
#elif defined(NX_I2C_LONG_H743)
    nx_h743_model_reset();
    nexus_test::h743_fixture::ResetExternalCpu();
    PWR->D3CR = PWR_D3CR_VOS_0 | PWR_D3CR_VOSRDY;
    PWR->CSR1 = PWR_CSR1_ACTVOS_0 | PWR_CSR1_ACTVOSRDY;
    g_h743_model.pwm = TIM3;
    g_h743_model.spi = SPI4;
    g_h743_model.adc = ADC1;
    ADC1->CR = ADC2->CR = ADC_CR_DEEPPWD;
    ADC1->CFGR = ADC_CFGR_JQDIS;
    FLASH->OPTSR_CUR = FLASH_OPTSR_IWDG1_SW;
    IWDG1->WINR = IWDG1->RLR = 4095U;
    g_long_i2c.reset = &RCC->APB1LRSTR;
    g_long_i2c.reset_mask = RCC_APB1LRSTR_I2C1RST;
#else
    nx_u575_devices_reset();
    u5_i2c = {};
    u5_spi = {};
    u5_adc = {};
    u5_adc_common = {};
    g_u575_i2c = &u5_i2c;
    g_u575_spi = &u5_spi;
    g_u575_adc = &u5_adc;
    g_u575_adc_common = &u5_adc_common;
    g_u575_physical_flash = &g_u575_model.flash;
    g_u575_model.flash.NSCR = FLASH_NSCR_LOCK | FLASH_NSCR_OPTLOCK;
    g_u575_model.rcc.AHB3ENR = RCC_AHB3ENR_PWREN;
    g_u575_model.power.SVMCR = PWR_SVMCR_ASV;
    u5_spi.SR = SPI_SR_TXP | SPI_SR_TXC;
    u5_spi.CFG1 = 7U | (7U << SPI_CFG1_CRCSIZE_Pos);
    g_long_i2c.reset = &RCC->APB1RSTR1;
    g_long_i2c.reset_mask = RCC_APB1RSTR1_I2C1RST;
#endif
    g_long_i2c.registers = (nx_i2c_v2_registers_t*)(void*)I2C1;
    g_long_i2c.registers->isr = I2C_ISR_TXE;
}
extern "C" uint32_t nx_i2c_long_factory_read(volatile uint32_t* reg) {
    auto& r = *g_long_i2c.registers;
    const uint32_t value = base_read(reg);
    if (reg == &r.rxdr) {
        if ((r.isr & rxne) == 0U) {
            ++g_long_i2c.illegal_controls;
            return value;
        }
        r.isr &= ~rxne;
    }
    return value;
}
extern "C" void nx_i2c_long_factory_write(volatile uint32_t* reg,
                                          uint32_t value) {
    auto& m = g_long_i2c;
    auto& r = *m.registers;
    if (reg == m.reset) {
        const bool asserted =
            (value & m.reset_mask) != 0U && (*reg & m.reset_mask) == 0U;
        base_write(reg, value);
        if (asserted) {
            r = {};
            r.isr = I2C_ISR_TXE;
            m.running = m.pending_start = m.pending_tx = m.waiting_reload =
                false;
            m.remaining = 0U;
            ++m.resets;
        }
        return;
    }
    if (reg == &r.icr) {
        uint32_t clear =
            value & (I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF |
                     I2C_ICR_ARLOCF | I2C_ICR_OVRCF);
        if (m.refuse_stop_clear && (r.isr & stopf) != 0U)
            clear &= ~(uint32_t)I2C_ICR_STOPCF;
        r.isr &= ~clear;
        return;
    }
    if (reg == &r.txdr) {
        if ((r.isr & txis) == 0U || m.remaining == 0U || m.pending_tx) {
            ++m.illegal_controls;
            return;
        }
        r.txdr = value;
        r.isr &= ~(txis | (uint32_t)I2C_ISR_TXE);
        m.pending_tx = true;
        m.transmitted.push_back(static_cast<uint8_t>(value));
        return;
    }
    if (reg == &r.cr2 && value == 0U) {
        if (m.refuse_control_clear && m.stops != 0U)
            return;
        /* CR2 writes never clear ISR; STOPF has its independent W1C. */
        r.cr2 = 0U;
        return;
    }
    if (reg != &r.cr2) {
        base_write(reg, value);
        return;
    }
    bool accepted = true;
    const size_t count =
        static_cast<size_t>((value & byte_mask) >> I2C_CR2_NBYTES_Pos);
    if ((value & start) != 0U) {
        if ((r.cr2 & start) != 0U || (m.running && (r.isr & tc) == 0U)) {
            ++m.illegal_controls;
            accepted = false;
        }
    } else if ((r.isr & tcr) == 0U || !m.waiting_reload) {
        ++m.illegal_controls;
        accepted = false;
    } else if (m.refuse_next_reload) {
        m.refuse_next_reload = false;
        ++m.refused_reloads;
        accepted = false;
    }
    if (count == 0U) {
        ++m.illegal_controls;
        accepted = false;
    }
    m.controls.push_back({value, accepted});
    if (!accepted)
        return;
    if ((value & start) != 0U) {
        m.pending_start = true;
        m.received_in_message = 0U;
    } else {
        ++m.reloads;
    }
    r.cr2 = value;
    r.isr = (r.isr & ~(tc | tcr | txis)) | busy;
    m.running = true;
    m.waiting_reload = false;
    m.remaining = count;
}
extern "C" void nx_i2c_long_factory_poll(nx_i2c_v2_state_t* state) {
    auto& m = g_long_i2c;
    auto& r = *m.registers;
    tick();
    if (state->registers != m.registers) {
        ++m.illegal_controls;
        return;
    }
    if (m.pending_start) {
        m.pending_start = false;
        r.cr2 &= ~start;
        ++m.addresses;
        if (m.event != nullptr)
            m.event(NX_LONG_ADDRESS, (r.cr2 & read_direction) != 0U);
    }
    if (m.pending_tx) {
        m.pending_tx = false;
        r.isr |= I2C_ISR_TXE;
        --m.remaining;
        if (m.remaining == 0U)
            finish_chunk();
    }
    if (m.waiting_reload) {
        if (m.tcr_wait != 0U)
            --m.tcr_wait;
        else
            r.isr |= tcr;
        return;
    }
    if (!m.running || m.remaining == 0U)
        return;
    if ((r.cr2 & read_direction) == 0U) {
        r.isr |= txis;
    } else if ((r.isr & rxne) == 0U) {
        r.rxdr = static_cast<uint8_t>(m.received_in_message * 37U + 0x5AU);
        ++m.received_in_message;
        r.isr |= rxne;
        --m.remaining;
        if (m.remaining == 0U)
            finish_chunk();
    }
}
