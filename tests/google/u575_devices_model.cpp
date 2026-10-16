/**
 * \file            u575_devices_model.cpp
 * \brief           Hardware-only U575 device protocol and reset leaves
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "u575_devices_model.h"
#include "stm32u575_provider.h"
#include <cstring>
TIM_TypeDef* g_u575_pwm;
nx_i2c_v2_registers_t* g_u575_i2c;
bool g_u575_i2c_berr;
bool g_u575_hsi_hold_on;
bool g_u575_hsi_hold_off;
SPI_TypeDef* g_u575_spi;
bool g_u575_spi_hold;
uint32_t g_u575_spi_reset_gap;
static uint32_t spi_eot_tick;
EXTI_TypeDef g_u575_exti;
ADC_TypeDef* g_u575_adc;
ADC_Common_TypeDef* g_u575_adc_common;
bool g_u575_adc_hold;
bool g_u575_adc_refuse_calibration;
FLASH_TypeDef* g_u575_physical_flash;
volatile uint8_t* g_u575_flash_memory;
uint32_t g_u575_flash_words;
uint32_t g_u575_flash_polls;
uint32_t g_u575_flash_erase;
bool g_u575_flash_ignore_start;
bool g_u575_flash_hold_eop_clear;
bool g_u575_flash_omit_eop;
static uint32_t flash_key;
static uint32_t flash_busy;
static uint32_t spi_received;
static uint32_t i2c_remaining;
static void i2c_next(void) {
    if (--i2c_remaining == 0U) {
        g_u575_i2c->isr &= ~static_cast<uint32_t>(I2C_ISR_TXIS | I2C_ISR_RXNE);
        if ((g_u575_i2c->cr2 & I2C_CR2_AUTOEND) != 0U)
            g_u575_i2c->isr = I2C_ISR_STOPF;
        else if ((g_u575_i2c->cr2 & I2C_CR2_RELOAD) != 0U)
            g_u575_i2c->isr |= I2C_ISR_TCR;
        else
            g_u575_i2c->isr |= I2C_ISR_TC;
    }
}
extern "C" uint32_t nx_u575_devices_read(volatile uint32_t* reg) {
    const uint32_t value = nx_u575_model_read(reg);
    if (g_u575_i2c != nullptr && reg == &g_u575_i2c->rxdr &&
        i2c_remaining != 0U)
        i2c_next();
    return value;
}
extern "C" void nx_u575_devices_write(volatile uint32_t* reg, uint32_t value) {
    if (reg == g_u575_model.ignore_write) {
        ++g_u575_model.writes;
        return;
    }
    if (reg == &g_u575_model.rcc.CR) {
        if ((value & RCC_CR_HSION) != 0U && !g_u575_hsi_hold_on)
            value |= RCC_CR_HSIRDY;
        if ((value & RCC_CR_HSION) == 0U && !g_u575_hsi_hold_off)
            value &= ~static_cast<uint32_t>(RCC_CR_HSIRDY);
    }
    if (g_u575_physical_flash != nullptr) {
        if (reg == &g_u575_physical_flash->NSKEYR) {
            if (flash_key == 0x45670123U && value == 0xcdef89abU)
                g_u575_physical_flash->NSCR &=
                    ~static_cast<uint32_t>(FLASH_NSCR_LOCK);
            flash_key = value;
        }
        if (reg == &g_u575_physical_flash->NSSR) {
            *reg &= ~(g_u575_flash_hold_eop_clear
                          ? value & ~static_cast<uint32_t>(FLASH_NSSR_EOP)
                          : value);
            return;
        }
        if (reg == &g_u575_physical_flash->NSCR &&
            (value & FLASH_NSCR_STRT) != 0U) {
            if (g_u575_flash_ignore_start)
                return;
            g_u575_flash_erase = value;
            const uint32_t page =
                ((value & static_cast<uint32_t>(FLASH_NSCR_PNB)) >>
                 FLASH_NSCR_PNB_Pos) +
                ((value & FLASH_NSCR_BKER) != 0U ? 128U : 0U);
            std::memset(const_cast<uint8_t*>(g_u575_flash_memory) +
                            page * 8192U,
                        0xff, 8192U);
            g_u575_physical_flash->NSSR |= FLASH_NSSR_BSY;
            flash_busy = 3U;
        }
    }
    if (g_u575_adc != nullptr) {
        if (reg == &g_u575_model.rcc.AHB3RSTR &&
            (value & RCC_AHB3RSTR_ADC4RST) != 0U &&
            reg != g_u575_model.ignore_write) {
            std::memset(g_u575_adc, 0, sizeof(*g_u575_adc));
            std::memset(g_u575_adc_common, 0, sizeof(*g_u575_adc_common));
        }
        if (reg == &g_u575_adc->ISR) {
            if (reg != g_u575_model.ignore_write)
                *reg &= ~value;
            return;
        }
        if (reg == &g_u575_adc->CR && reg != g_u575_model.ignore_write) {
            if ((value & ADC_CR_ADCAL) != 0U && g_u575_adc_refuse_calibration)
                return;
            if ((value & ADC_CR_ADVREGEN) != 0U)
                g_u575_adc->ISR |= ADC_ISR_LDORDY;
            if ((value & ADC_CR_ADEN) != 0U)
                g_u575_adc->ISR |= ADC_ISR_ADRDY;
            if ((value & ADC_CR_ADSTP) != 0U)
                value &= ~static_cast<uint32_t>(ADC_CR_ADSTP | ADC_CR_ADSTART);
        }
    }
    if (reg == &g_u575_exti.RPR1 || reg == &g_u575_exti.FPR1) {
        if (reg != g_u575_model.ignore_write)
            *reg &= ~value;
        return;
    }
    if (g_u575_spi != nullptr) {
        if (reg == &g_u575_model.rcc.APB2RSTR &&
            (value & RCC_APB2RSTR_SPI1RST) != 0U &&
            reg != g_u575_model.ignore_write) {
            g_u575_spi_reset_gap = g_u575_model.timer.CNT - spi_eot_tick;
            std::memset(g_u575_spi, 0, sizeof(*g_u575_spi));
            g_u575_spi->CFG1 = 7U | (7U << SPI_CFG1_CRCSIZE_Pos);
            g_u575_spi->SR = SPI_SR_TXP | SPI_SR_TXC;
            spi_received = 0U;
        }
        if (reg == &g_u575_spi->IFCR)
            g_u575_spi->SR &= ~value;
    }
    if (g_u575_i2c != nullptr) {
        if (reg == &g_u575_model.rcc.APB1RSTR1 &&
            (value & RCC_APB1RSTR1_I2C1RST) != 0U &&
            reg != g_u575_model.ignore_write)
            std::memset(g_u575_i2c, 0, sizeof(*g_u575_i2c));
        if (reg == &g_u575_i2c->cr2 && (value & I2C_CR2_NBYTES) != 0U) {
            i2c_remaining = (value & static_cast<uint32_t>(I2C_CR2_NBYTES)) >>
                            I2C_CR2_NBYTES_Pos;
            g_u575_i2c->isr =
                I2C_ISR_BUSY |
                ((value & I2C_CR2_RD_WRN) != 0U ? I2C_ISR_RXNE : I2C_ISR_TXIS);
            if (g_u575_i2c_berr)
                g_u575_i2c->isr |= I2C_ISR_BERR;
        }
        if (reg == &g_u575_i2c->icr)
            g_u575_i2c->isr &= ~value;
        if (reg == &g_u575_i2c->txdr && i2c_remaining != 0U)
            i2c_next();
    }
    if (g_u575_pwm != nullptr) {
        if (reg == &g_u575_model.rcc.APB1RSTR1 &&
            (value & RCC_APB1RSTR1_TIM3RST) != 0U &&
            reg != g_u575_model.ignore_write)
            std::memset(g_u575_pwm, 0, sizeof(*g_u575_pwm));
        if (reg == &g_u575_pwm->EGR && (value & TIM_EGR_UG) != 0U)
            g_u575_pwm->SR |= TIM_SR_UIF;
        if (reg == &g_u575_pwm->SR) {
            if (reg != g_u575_model.ignore_write)
                *reg &= value;
            return;
        }
    }
    nx_u575_model_write(reg, value);
}
extern "C" void nx_u575_flash_program32(volatile uint8_t* address,
                                        uint32_t value) {
    *reinterpret_cast<volatile uint32_t*>(address) = value;
    ++g_u575_flash_words;
    if (g_u575_flash_words % 4U == 0U) {
        g_u575_physical_flash->NSSR |= FLASH_NSSR_BSY;
        flash_busy = 3U;
    }
}
extern "C" void nx_u575_flash_poll(void* port) {
    (void)port;
    ++g_u575_flash_polls;
    ++g_u575_model.timer.CNT;
    if (flash_busy != 0U && --flash_busy == 0U) {
        g_u575_physical_flash->NSSR &= ~static_cast<uint32_t>(FLASH_NSSR_BSY);
        if (!g_u575_flash_omit_eop &&
            (g_u575_physical_flash->NSCR & FLASH_NSCR_EOPIE) != 0U) {
            g_u575_physical_flash->NSSR |= FLASH_NSSR_EOP;
            g_u575_model.irq_pending[FLASH_IRQn] = true;
        }
        g_u575_physical_flash->NSCR &= ~static_cast<uint32_t>(FLASH_NSCR_STRT);
    }
}
extern "C" void nx_u575_adc_poll(void* port) {
    (void)port;
    ++g_u575_model.timer.CNT;
    if ((g_u575_adc->CR & ADC_CR_ADCAL) != 0U)
        g_u575_adc->CR &= ~static_cast<uint32_t>(ADC_CR_ADCAL);
    if (!g_u575_adc_hold && (g_u575_adc->CR & ADC_CR_ADSTART) != 0U) {
        g_u575_adc->DR = 0xabcU;
        g_u575_adc->ISR |= ADC_ISR_EOC | ADC_ISR_EOS;
        g_u575_adc->CR &= ~static_cast<uint32_t>(ADC_CR_ADSTART);
    }
}
extern "C" void nx_u575_spi_write8(volatile uint32_t* reg, uint8_t value) {
    *reg = value;
    if (!g_u575_spi_hold) {
        g_u575_spi->RXDR = static_cast<uint8_t>(~value);
        g_u575_spi->SR |= SPI_SR_RXP;
    }
}
extern "C" uint8_t nx_u575_spi_read8(volatile uint32_t* reg) {
    const uint8_t value = static_cast<uint8_t>(*reg);
    g_u575_spi->SR &= ~static_cast<uint32_t>(SPI_SR_RXP);
    if (++spi_received == g_u575_spi->CR2) {
        g_u575_spi->SR |= SPI_SR_EOT | SPI_SR_TXC;
        spi_eot_tick = g_u575_model.timer.CNT;
    }
    return value;
}
extern "C" void nx_u575_spi_poll(void* port) {
    (void)port;
    ++g_u575_model.timer.CNT;
}
void nx_u575_devices_reset(void) {
    nx_u575_model_reset();
    g_u575_model.rcc.ICSCR3 = RCC_ICSCR3_HSITRIM_4;
    g_u575_pwm = nullptr;
    g_u575_i2c = nullptr;
    g_u575_spi = nullptr;
    g_u575_adc = nullptr;
    g_u575_adc_common = nullptr;
    g_u575_physical_flash = nullptr;
    g_u575_flash_memory = nullptr;
    g_u575_spi_hold = g_u575_adc_hold = false;
    g_u575_adc_refuse_calibration = false;
    g_u575_i2c_berr = false;
    g_u575_hsi_hold_on = g_u575_hsi_hold_off = false;
    i2c_remaining = spi_received = flash_key = flash_busy = 0U;
    spi_eot_tick = g_u575_spi_reset_gap = 0U;
    g_u575_flash_words = g_u575_flash_polls = g_u575_flash_erase = 0U;
    g_u575_flash_ignore_start = g_u575_flash_hold_eop_clear =
        g_u575_flash_omit_eop = false;
    std::memset(&g_u575_exti, 0, sizeof(g_u575_exti));
}
