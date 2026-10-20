/**
 * \file            model.c
 * \brief           GD32F303 external W0C, GPIO latch and NVIC fault model
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "model.h"
#include "gd32f303_provider.h"
bool g_gd303_irq[60];
bool g_gd303_pending[60];
uint8_t g_gd303_priority[60];
unsigned g_gd303_writes;
volatile uint32_t* g_gd303_ignore;
bool g_gd303_hold_update;
bool g_gd303_update_trgo;
bool g_gd303_spi_hold_rx;
bool g_gd303_spi_bad_cs;
unsigned g_gd303_spi_bytes;
bool g_gd303_exti_hold_pending;
bool g_gd303_exti_reassert_pending;
bool g_gd303_carry_on_low_read;
void nx_gd303_model_spi_poll(void) {
    TIMER_CNT(TIMER1)++;
}
uint32_t g_gd303_density = (48U << 16U) | 256U;
uint32_t g_gd303_debug_ctl0;
bool g_gd303_hold_irq_off;
bool g_gd303_pend_on_timer_withdraw;
bool g_gd303_hold_late_irq_off;
bool g_gd303_hold_cs_release;
bool g_gd303_hold_spi_reset_release;
uint32_t nx_gd303_model_read(volatile uint32_t* reg) {
    /* The real debug address lies inside ASan ShadowGap on x86_64. */
    if (reg == &DBG_CTL0)
        return g_gd303_debug_ctl0;
    uint32_t value = *reg;
    if (reg == &TIMER_CNT(TIMER1) && g_gd303_carry_on_low_read) {
        g_gd303_carry_on_low_read = false;
        TIMER_CNT(TIMER1) = 0U;
        TIMER_CNT(TIMER4) = (TIMER_CNT(TIMER4) + 1U) & 0xffffU;
        TIMER_INTF(TIMER1) |= TIMER_INTF_UPIF;
    }
    if (reg == &SPI_DATA(SPI0)) {
        SPI_STAT(SPI0) &= ~(SPI_STAT_RBNE | SPI_STAT_TRANS);
        SPI_STAT(SPI0) |= SPI_STAT_TBE;
    }
    if (reg == &USART_DATA(USART0))
        USART_STAT0(USART0) &=
            ~(USART_STAT0_RBNE | USART_STAT0_ORERR | USART_STAT0_PERR |
              USART_STAT0_FERR | USART_STAT0_NERR | USART_STAT0_IDLEF);
    return value;
}
void nx_gd303_model_write(volatile uint32_t* reg, uint32_t value) {
    ++g_gd303_writes;
    if (reg == g_gd303_ignore)
        return;
    if (reg == &DBG_CTL0) {
        g_gd303_debug_ctl0 = value;
        return;
    }
    if (reg == &EXTI_PD) {
        if (!g_gd303_exti_hold_pending)
            *reg &= ~value;
        if (g_gd303_exti_reassert_pending)
            *reg |= value;
        return;
    }
    if (reg == &RCU_APB2RST) {
        if (g_gd303_hold_spi_reset_release &&
            (value & RCU_APB2RST_SPI0RST) == 0U)
            return;
        *reg = value;
        if ((value & RCU_APB2RST_SPI0RST) != 0U) {
            SPI_CTL0(SPI0) = SPI_CTL1(SPI0) = SPI_I2SCTL(SPI0) = 0U;
            SPI_STAT(SPI0) = SPI_STAT_TBE;
        }
    } else if (reg == &SPI_DATA(SPI0)) {
        ++g_gd303_spi_bytes;
        if ((GPIO_OCTL(GPIOA) & 16U) != 0U)
            g_gd303_spi_bad_cs = true;
        *reg = value ^ 0xffU;
        SPI_STAT(SPI0) |= SPI_STAT_TRANS | SPI_STAT_TBE;
        if (!g_gd303_spi_hold_rx)
            SPI_STAT(SPI0) |= SPI_STAT_RBNE;
    } else if (reg == &USART_STAT0(USART0))
        *reg &= value;
    else if (reg == &USART_DATA(USART0)) {
        *reg = value;
        USART_STAT0(USART0) &= ~(USART_STAT0_TC | USART_STAT0_TBE);
    } else if (reg == &TIMER_INTF(TIMER1) || reg == &TIMER_INTF(TIMER4))
        *reg &= value;
    else if (reg == &TIMER_SWEVG(TIMER4)) {
        *reg = value;
        if (!g_gd303_hold_update)
            TIMER_INTF(TIMER4) |= TIMER_INTF_UPIF;
    } else if (reg == &TIMER_SWEVG(TIMER2)) {
        *reg = value;
        if (!g_gd303_hold_update)
            TIMER_INTF(TIMER2) |= TIMER_INTF_UPIF;
    } else if (reg == &TIMER_SWEVG(TIMER1)) {
        *reg = value;
        if ((TIMER_CTL1(TIMER1) & TIMER_CTL1_MMC) == 0U)
            g_gd303_update_trgo = true;
        if (!g_gd303_hold_update)
            TIMER_INTF(TIMER1) |= TIMER_INTF_UPIF;
    } else if (reg == &GPIO_BOP(GPIOA)) {
        if (g_gd303_hold_cs_release && (value & 16U) != 0U)
            return;
        *reg = value;
        GPIO_OCTL(GPIOA) =
            (GPIO_OCTL(GPIOA) | (value & 0xffffU)) & ~(value >> 16U);
    } else {
        if (reg == &TIMER_DMAINTEN(TIMER1) && value == 0U &&
            g_gd303_pend_on_timer_withdraw)
            g_gd303_pending[TIMER1_IRQn] = true;
        *reg = value;
    }
}
nx_result_t nx_gd303_model_irq(int irq, bool enable, uint8_t priority) {
    if (irq < 0 || irq >= 60)
        return NX_ERROR_INVALID;
    if (!enable && (g_gd303_hold_irq_off ||
                    (g_gd303_hold_late_irq_off && g_gd303_pending[irq])))
        return NX_ERROR_IO;
    g_gd303_irq[irq] = enable;
    g_gd303_pending[irq] = false;
    g_gd303_priority[irq] = priority;
    return NX_SUCCESS;
}
void nx_gd303_model_device_poll(unsigned kind, void* port) {
    uint32_t low = (TIMER_CNT(TIMER1) & 0xffffU) + 1U;
    TIMER_CNT(TIMER1) = low & 0xffffU;
    if (low > 0xffffU) {
        TIMER_INTF(TIMER1) |= TIMER_INTF_UPIF;
        uint32_t high = (TIMER_CNT(TIMER4) & 0xffffU) + 1U;
        TIMER_CNT(TIMER4) = high & 0xffffU;
        if (high > 0xffffU)
            TIMER_INTF(TIMER4) |= TIMER_INTF_UPIF;
    }
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
