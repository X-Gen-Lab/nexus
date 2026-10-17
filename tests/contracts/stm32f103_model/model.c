/**
 * \file            model.c
 * \brief           Model only external F103 hardware effects and faults
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "stm32f103_provider.h"
#include <string.h>
nx_f103_model_t g_f103_model;
nx_stm32f103_system_t g_nx_stm32f103_system;
void nx_f103_model_reset(void) {
    memset(&g_f103_model, 0, sizeof(g_f103_model));
    memset(&g_nx_stm32f103_system, 0, sizeof(g_nx_stm32f103_system));
    g_f103_model.rcc.CR =
        RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
    g_f103_model.gpio.CRL = UINT32_C(0x44444444);
    g_f103_model.gpio.CRH = UINT32_C(0x44444444);
    g_f103_model.privileged = true;
    g_f103_model.flash.OBR = FLASH_OBR_IWDG_SW;
    g_f103_model.watchdog.RLR = 4095U;
    g_f103_model.flash_kib = 128U;
    g_f103_model.device_id = 0x410U;
    g_nx_stm32f103_system.rcc = &g_f103_model.rcc;
    g_nx_stm32f103_system.flash = &g_f103_model.flash;
    g_nx_stm32f103_system.timer = &g_f103_model.timer;
}
uint32_t nx_f103_model_device_id(void) {
    ++g_f103_model.device_id_reads;
    return g_f103_model.device_id;
}
bool nx_f103_model_privileged(void) {
    return g_f103_model.privileged;
}
uint32_t nx_f103_model_read(volatile uint32_t* reg) {
    ++g_f103_model.reads;
    if (reg == &g_f103_model.debug.CR) {
        ++g_f103_model.debug_control_reads;
        /* ES096 2.2.2: normal software has no debug-register access. */
        return 0U;
    }
    if (g_f103_model.hide_timer_when_gated &&
        (uintptr_t)reg >= (uintptr_t)&g_f103_model.timer &&
        (uintptr_t)reg <
            (uintptr_t)&g_f103_model.timer + sizeof(g_f103_model.timer) &&
        (g_f103_model.rcc.APB1ENR & RCC_APB1ENR_TIM2EN) == 0U) {
        return 0U;
    }
    uint32_t value = *reg;
    if (g_f103_model.uart != NULL && reg == &g_f103_model.uart->DR) {
        g_f103_model.uart->SR &=
            ~(uint32_t)(USART_SR_RXNE | USART_SR_PE | USART_SR_FE |
                        USART_SR_NE | USART_SR_ORE | USART_SR_IDLE);
    }
    return value;
}
void nx_f103_model_write(volatile uint32_t* reg, uint32_t value) {
    ++g_f103_model.writes;
    if (reg == &g_f103_model.debug.CR) {
        ++g_f103_model.debug_control_writes;
        return;
    }
    if ((uintptr_t)reg >= (uintptr_t)&g_f103_model.timer &&
        (uintptr_t)reg <
            (uintptr_t)&g_f103_model.timer + sizeof(g_f103_model.timer) &&
        (g_f103_model.rcc.APB1ENR & RCC_APB1ENR_TIM2EN) == 0U) {
        ++g_f103_model.ungated_timer_writes;
    }
    if ((uintptr_t)reg >= (uintptr_t)&g_f103_model.timer &&
        (uintptr_t)reg <
            (uintptr_t)&g_f103_model.timer + sizeof(g_f103_model.timer)) {
        ++g_f103_model.timer_writes;
    }
    if (reg == g_f103_model.ignore_write) {
        return;
    }
    if (reg == &g_f103_model.timer.SR) {
        if (g_f103_model.hold_timer_ack && (value & TIM_SR_UIF) == 0U) {
            return;
        }
        *reg &= value;
        return;
    }
    if (reg == &g_f103_model.timer.DIER && value == 0U &&
        g_f103_model.pend_timer_on_dier_write && (*reg & TIM_DIER_UIE) != 0U) {
        g_f103_model.irq_pending[TIM2_IRQn] = true;
    }
    if (reg == &g_f103_model.timer.DIER && value == 0U &&
        g_f103_model.hold_timer_source_off) {
        return;
    }
    if (reg == &g_f103_model.timer.EGR && (value & TIM_EGR_UG) != 0U &&
        (g_f103_model.timer.CR2 & TIM_CR2_MMS) == 0U) {
        ++g_f103_model.timer_ug_pulses;
    }
    if (reg == &g_f103_model.timer.EGR && (value & TIM_EGR_UG) != 0U &&
        !g_f103_model.hold_update) {
        g_f103_model.timer.SR |= TIM_SR_UIF;
    }
    if (reg == &g_f103_model.pwm.EGR && (value & TIM_EGR_UG) != 0U &&
        !g_f103_model.hold_update)
        g_f103_model.pwm.SR |= TIM_SR_UIF;
    if (reg == &g_f103_model.timer.CNT && g_f103_model.hold_counter) {
        *reg = 17U;
        return;
    }
    if (reg == &g_f103_model.rcc.APB1ENR && g_f103_model.hold_timer_gate_off &&
        (*reg & RCC_APB1ENR_TIM2EN) != 0U) {
        value |= RCC_APB1ENR_TIM2EN;
    }
    if (reg == &g_f103_model.rcc.CSR && g_f103_model.hold_lsi_off &&
        (*reg & RCC_CSR_LSION) != 0U) {
        value |= RCC_CSR_LSION;
    }
    if (reg == &g_f103_model.exti.PR) {
        if (!g_f103_model.refuse_exti_pending) {
            *reg &= ~value;
            if (g_f103_model.reassert_exti_pending)
                *reg |= value;
        }
        return;
    }
    if (reg == &g_f103_model.exti.FTSR) {
        value = (value & ~g_f103_model.refused_falling) |
                (*reg & g_f103_model.refused_falling);
    }
    if (reg == &g_f103_model.watchdog.KR) {
        if (value == 0xccccU)
            g_f103_model.watchdog_enabled = true;
        if (value == 0xaaaaU)
            ++g_f103_model.feeds;
    }
    if ((reg == &g_f103_model.watchdog.PR ||
         reg == &g_f103_model.watchdog.RLR) &&
        g_f103_model.hold_watchdog_update) {
        g_f103_model.watchdog.SR |= IWDG_SR_PVU | IWDG_SR_RVU;
        return;
    }
    if (reg == &g_f103_model.rcc.APB2RSTR &&
        (value & RCC_APB2RSTR_SPI1RST) != 0U) {
        if (!g_f103_model.hold_spi_reset) {
            memset(&g_f103_model.spi, 0, sizeof(g_f103_model.spi));
            g_f103_model.spi.SR = SPI_SR_TXE;
            g_f103_model.spi_pending = false;
        }
    }
    if (reg == &g_f103_model.gpio.BSRR) {
        if ((value & 16U) != 0U)
            g_f103_model.timer.CNT += g_f103_model.advance_on_cs_release;
        if (g_f103_model.hold_cs_high && (value & 16U) != 0U)
            return;
        if ((value & (16U << 16U)) != 0U && (g_f103_model.gpio.ODR & 16U) != 0U)
            ++g_f103_model.cs_assertions;
        if ((value & 16U) != 0U && (g_f103_model.gpio.ODR & 16U) == 0U)
            ++g_f103_model.cs_releases;
        g_f103_model.gpio.ODR =
            (g_f103_model.gpio.ODR | (value & 0xffffU)) & ~(value >> 16U);
    }
    if (g_f103_model.uart != NULL) {
        if (reg == &g_f103_model.uart->SR) {
            *reg &= value;
            return;
        }
        if (reg == &g_f103_model.uart->DR) {
            g_f103_model.uart->SR &= ~(uint32_t)(USART_SR_TXE | USART_SR_TC);
            g_f103_model.last_tx = (uint8_t)value;
            ++g_f103_model.transmitted;
        }
    }
    *reg = value;
}
nx_result_t nx_f103_model_irq_control(int irq, bool enabled, uint8_t priority) {
    if (irq < 0 || irq >= 43 || priority >= 16U) {
        return NX_ERROR_INVALID;
    }
    if (enabled)
        ++g_f103_model.irq_enables[irq];
    if (enabled && !g_f103_model.hold_irq_on) {
        g_f103_model.irq_enabled[irq] = true;
        g_f103_model.priority[irq] = priority;
    }
    if (!enabled && !g_f103_model.hold_irq_off) {
        g_f103_model.irq_enabled[irq] = false;
        g_f103_model.irq_pending[irq] = false;
    }
    return g_f103_model.irq_enabled[irq] == enabled ? NX_SUCCESS : NX_ERROR_IO;
}

void nx_f103_model_watchdog_poll(void* port) {
    nx_stm32f103_watchdog_state_t* p = port;
    if (!g_f103_model.hold_lsi && (p->rcc->CSR & RCC_CSR_LSION) != 0U) {
        p->rcc->CSR |= RCC_CSR_LSIRDY;
    }
    if (!g_f103_model.hold_watchdog_update) {
        p->registers->SR = 0U;
    }
}

void nx_f103_model_spi_poll(void* port) {
    nx_stm32f103_spi_state_t* p = port;
    ++g_f103_model.timer.CNT;
    if (g_f103_model.spi_pending && !g_f103_model.hold_spi_rx) {
        p->registers->DR = g_f103_model.spi_response;
        p->registers->SR = SPI_SR_TXE | SPI_SR_RXNE;
        g_f103_model.spi_pending = false;
    }
}
void nx_f103_model_spi_write(SPI_TypeDef* spi, uint8_t byte) {
    ++g_f103_model.writes;
    if ((g_f103_model.gpio.ODR & 16U) != 0U) {
        g_f103_model.spi_cs_violation = true;
    }
    spi->DR = byte;
    spi->SR = SPI_SR_BSY;
    g_f103_model.spi_response = (uint8_t)(byte ^ 0xffU);
    g_f103_model.spi_pending = true;
}
uint8_t nx_f103_model_spi_read(SPI_TypeDef* spi) {
    uint8_t value = (uint8_t)spi->DR;
    spi->SR &= ~(uint32_t)SPI_SR_RXNE;
    return value;
}
void nx_f103_model_device_poll(unsigned kind, void* port) {
    ++g_f103_model.timer.CNT;
    if (kind == 1U) {
        nx_legacy_adc_state_t* p = port;
        p->registers->control2 &= ~12U;
        if ((p->registers->control2 & (1U << 22U)) != 0U) {
            p->registers->control2 &= ~(1U << 22U);
            p->registers->data = 1234U;
            p->registers->status |= 2U;
        }
    }
}
