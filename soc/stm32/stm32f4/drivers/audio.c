/**
 * \file            audio.c
 * \brief           I2S2/3 format, finite DMA and complete retained audio loans
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_audio.h"
#include "stm32f4xx_ll_spi.h"
#include <string.h>

#define AUDIO_ERRORS (SPI_SR_UDR | SPI_SR_OVR | SPI_SR_FRE)
#define TX_DONE      (1U << 16U)
#define RX_DONE      (1U << 17U)
#define DMA_ERROR    (1U << 18U)
#define DMA_INTERRUPTS                                                         \
    (DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE)

static uint32_t gate(const nx_stm32_audio_config_t* c) {
    return c->controller == 2U ? RCC_APB1ENR_SPI2EN : RCC_APB1ENR_SPI3EN;
}
static uint32_t reset_bit(const nx_stm32_audio_config_t* c) {
    return c->controller == 2U ? RCC_APB1RSTR_SPI2RST : RCC_APB1RSTR_SPI3RST;
}
static IRQn_Type vector(const nx_stm32_audio_config_t* c) {
    return c->controller == 2U ? SPI2_IRQn : SPI3_IRQn;
}
static uint8_t owner_bit(const nx_stm32_audio_config_t* c) {
    return (uint8_t)(1U << (c->controller - 2U));
}
static SPI_TypeDef* tx_bus(const nx_stm32_audio_config_t* c) {
    return c->receive ? (c->duplex ? c->extension : NULL) : c->registers;
}
static SPI_TypeDef* rx_bus(const nx_stm32_audio_config_t* c) {
    return c->receive ? c->registers : (c->duplex ? c->extension : NULL);
}
static size_t frame_bytes(const nx_stm32_audio_config_t* c) {
    return (size_t)(c->sample_bits == 16U ? 2U : 4U) *
           (c->standard >= 3U ? 1U : 2U);
}
static uint64_t greatest_divisor(uint64_t a, uint64_t b) {
    while (b != 0U) {
        uint64_t remainder = a % b;
        a = b;
        b = remainder;
    }
    return a;
}
static bool rate_matches(const nx_stm32_audio_config_t* c) {
    uint32_t channels = c->standard >= 3U ? 1U : 2U;
    uint32_t factor = c->mclk ? 128U * channels : c->slot_bits * channels;
    uint64_t numerator = (uint64_t)c->pll_input_hz * c->pll_n;
    uint64_t denominator = (uint64_t)c->pll_m * c->pll_r * factor *
                           (2U * c->divider + (c->odd ? 1U : 0U));
    if (denominator == 0U || numerator == 0U ||
        numerator > 192000U * denominator)
        return false;
    uint64_t divisor = greatest_divisor(numerator, denominator);
    return c->actual_rate.numerator_hz == numerator / divisor &&
           c->actual_rate.denominator == denominator / divisor;
}
static bool valid(const nx_stm32_audio_config_t* c) {
    if (c == NULL || c->registers == NULL || c->clock == NULL ||
        (c->controller != 2U && c->controller != 3U) ||
        c->common.poll_limit == 0U || c->common.priority > 15U ||
        c->standard > 4U ||
        (c->sample_bits != 16U && c->sample_bits != 24U &&
         c->sample_bits != 32U) ||
        (c->slot_bits != 16U && c->slot_bits != 32U) ||
        c->sample_bits > c->slot_bits || (c->duplex && c->extension == NULL) ||
        c->hclk_hz == 0U || c->pclk_hz == 0U || c->common.regions == NULL ||
        c->common.region_count == 0U || c->ws_gpio == NULL ||
        c->ck_gpio == NULL || c->ws_pin > 15U || c->ck_pin > 15U ||
        (!c->master && c->mclk) ||
        (tx_bus(c) != NULL && c->tx.stream == NULL) ||
        (rx_bus(c) != NULL && c->rx.stream == NULL))
        return false;
    if (c->master &&
        (c->divider < 2U || c->divider > 255U || c->pll_m < 2U ||
         c->pll_m > 63U || c->pll_n < 192U || c->pll_n > 432U ||
         c->pll_r < 2U || c->pll_r > 7U ||
         (uint64_t)c->pll_input_hz < (uint64_t)c->pll_m * 1000000U ||
         (uint64_t)c->pll_input_hz > (uint64_t)c->pll_m * 2000000U ||
         (uint64_t)c->pll_input_hz * c->pll_n <
             (uint64_t)c->pll_m * 192000000U ||
         (uint64_t)c->pll_input_hz * c->pll_n >
             (uint64_t)c->pll_m * 432000000U ||
         (uint64_t)c->pll_input_hz * c->pll_n >
             (uint64_t)c->pll_m * c->pll_r * 192000000U ||
         !rate_matches(c)))
        return false;
#ifndef NEXUS_STM32_MODEL
    if (c->registers != (c->controller == 2U ? SPI2 : SPI3) ||
        (c->duplex &&
         c->extension != (c->controller == 2U ? I2S2ext : I2S3ext)))
        return false;
#endif
    return true;
}
static bool dma_foreign(const nx_stm32_av_dma_t* d) {
    return d->stream != NULL &&
           ((NX_AV_READ(&d->stream->CR) & (DMA_SxCR_EN | DMA_INTERRUPTS)) !=
                0U ||
            (NX_AV_READ(&d->stream->FCR) & DMA_SxFCR_FEIE) != 0U ||
            NX_AV_IRQ_ENABLED(d->irq) != 0U ||
            NX_AV_IRQ_PENDING(d->irq) != 0U || NX_AV_IRQ_ACTIVE(d->irq) != 0U);
}
nx_result_t nx_stm32_audio_admit(const nx_stm32_audio_state_t* s,
                                 const nx_stm32_audio_config_t* c) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !valid(c) || s->owned)
        return NX_ERROR_INVALID;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if (rcc == NULL)
        return NX_ERROR_STATE;
    if ((NX_AV_READ(&rcc->APB1ENR) & gate(c)) != 0U ||
        (NX_AV_READ(&rcc->APB1RSTR) & reset_bit(c)) != 0U ||
        NX_AV_IRQ_ENABLED(vector(c)) != 0U ||
        NX_AV_IRQ_PENDING(vector(c)) != 0U || NX_AV_IRQ_ACTIVE(vector(c)) != 0U)
        return NX_ERROR_BUSY;
    if ((NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0U &&
        (dma_foreign(&c->tx) || dma_foreign(&c->rx)))
        return NX_ERROR_BUSY;
    if (c->master && c->clock->users != 0U &&
        (c->clock->pll_n != c->pll_n || c->clock->pll_r != c->pll_r ||
         (c->clock->users & owner_bit(c)) != 0U))
        return NX_ERROR_BUSY;
    if (c->master && c->clock->users == 0U &&
        ((NX_AV_READ(&rcc->CR) & (RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY)) != 0U ||
         (NX_AV_READ(&rcc->CFGR) & RCC_CFGR_I2SSRC) != 0U))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
static bool wait_bits(volatile uint32_t* reg, uint32_t mask, uint32_t value,
                      uint32_t limit) {
    for (uint32_t i = 0U; i < limit; ++i) {
        if ((NX_AV_READ(reg) & mask) == value)
            return true;
        NX_AV_POLL();
    }
    return false;
}
static nx_result_t clock_acquire(nx_stm32_audio_state_t* s) {
    const nx_stm32_audio_config_t* c = s->config;
    if (!c->master)
        return NX_SUCCESS;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    uint32_t mask = RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLSRC;
    uint32_t source = c->pll_m | (c->pll_hse ? RCC_PLLCFGR_PLLSRC_HSE : 0U);
    if ((NX_AV_READ(&rcc->PLLCFGR) & mask) != source)
        return NX_ERROR_STATE;
    nx_stm32_audio_clock_t* clock = c->clock;
    if (clock->users == 0U) {
        clock->saved_plli2s = NX_AV_READ(&rcc->PLLI2SCFGR);
        clock->saved_source = NX_AV_READ(&rcc->CFGR) & RCC_CFGR_I2SSRC;
        clock->pll_n = c->pll_n;
        clock->pll_r = c->pll_r;
        clock->acquired = true;
        clock->users = owner_bit(c);
        s->clock_user = true;
        if (!nx_stm32_av_bits(&rcc->CFGR, RCC_CFGR_I2SSRC, 0U) ||
            !nx_stm32_av_bits(&rcc->PLLI2SCFGR,
                              RCC_PLLI2SCFGR_PLLI2SN | RCC_PLLI2SCFGR_PLLI2SR,
                              ((uint32_t)c->pll_n << 6U) |
                                  ((uint32_t)c->pll_r << 28U)) ||
            !nx_stm32_av_bits(&rcc->CR, RCC_CR_PLLI2SON, RCC_CR_PLLI2SON) ||
            !wait_bits(&rcc->CR, RCC_CR_PLLI2SRDY, RCC_CR_PLLI2SRDY,
                       c->common.poll_limit))
            return NX_ERROR_IO;
    } else {
        if (!clock->acquired || clock->pll_n != c->pll_n ||
            clock->pll_r != c->pll_r ||
            (NX_AV_READ(&rcc->CR) & (RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY)) !=
                (RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY))
            return NX_ERROR_BUSY;
        clock->users |= owner_bit(c);
        s->clock_user = true;
    }
    if (c->receive && !c->duplex) {
        if (clock->cycle_users == 0U) {
            clock->saved_trace = NX_AV_READ(NX_AV_TRACE) & DCB_DEMCR_TRCENA_Msk;
            clock->saved_counter =
                NX_AV_READ(NX_AV_COUNTER_CONTROL) & DWT_CTRL_CYCCNTENA_Msk;
        }
        clock->cycle_users |= owner_bit(c);
        s->cycle_user = true;
        /* Preserve the free-running counter value and every unrelated bit. */
        if (!nx_stm32_av_bits(NX_AV_TRACE, DCB_DEMCR_TRCENA_Msk,
                              DCB_DEMCR_TRCENA_Msk) ||
            !nx_stm32_av_bits(NX_AV_COUNTER_CONTROL, DWT_CTRL_CYCCNTENA_Msk,
                              DWT_CTRL_CYCCNTENA_Msk))
            return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
static nx_result_t clock_release(nx_stm32_audio_state_t* s) {
    const nx_stm32_audio_config_t* c = s->config;
    nx_stm32_audio_clock_t* clock = c->clock;
    if (s->cycle_user) {
        uint8_t remaining = clock->cycle_users & (uint8_t)~owner_bit(c);
        if (remaining == 0U &&
            (!nx_stm32_av_bits(NX_AV_COUNTER_CONTROL, DWT_CTRL_CYCCNTENA_Msk,
                               clock->saved_counter) ||
             !nx_stm32_av_bits(NX_AV_TRACE, DCB_DEMCR_TRCENA_Msk,
                               clock->saved_trace)))
            return NX_ERROR_IO;
        clock->cycle_users = remaining;
        s->cycle_user = false;
    }
    if (!s->clock_user)
        return NX_SUCCESS;
    if ((clock->users & (uint8_t)~owner_bit(c)) != 0U) {
        clock->users &= (uint8_t)~owner_bit(c);
        s->clock_user = false;
        return NX_SUCCESS;
    }
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if (!nx_stm32_av_bits(&rcc->CR, RCC_CR_PLLI2SON, 0U) ||
        !wait_bits(&rcc->CR, RCC_CR_PLLI2SRDY, 0U, c->common.poll_limit) ||
        !nx_stm32_av_write(&rcc->PLLI2SCFGR, clock->saved_plli2s,
                           RCC_PLLI2SCFGR_PLLI2SN | RCC_PLLI2SCFGR_PLLI2SR) ||
        !nx_stm32_av_bits(&rcc->CFGR, RCC_CFGR_I2SSRC, clock->saved_source))
        return NX_ERROR_IO;
    clock->users = 0U;
    clock->acquired = false;
    s->clock_user = false;
    return NX_SUCCESS;
}
static uint32_t wire_format(const nx_stm32_audio_config_t* c) {
    static const uint32_t standards[5] = {
        LL_I2S_STANDARD_PHILIPS, LL_I2S_STANDARD_MSB, LL_I2S_STANDARD_LSB,
        LL_I2S_STANDARD_PCM_SHORT, LL_I2S_STANDARD_PCM_LONG};
    uint32_t data = c->sample_bits == 32U   ? LL_I2S_DATAFORMAT_32B
                    : c->sample_bits == 24U ? LL_I2S_DATAFORMAT_24B
                    : c->slot_bits == 32U   ? LL_I2S_DATAFORMAT_16B_EXTENDED
                                            : LL_I2S_DATAFORMAT_16B;
    return SPI_I2SCFGR_I2SMOD | standards[c->standard] | data |
           (c->cpol_high ? SPI_I2SCFGR_CKPOL : 0U);
}
nx_result_t nx_stm32_audio_prepare(nx_stm32_audio_state_t* s,
                                   const nx_stm32_audio_config_t* c) {
    nx_result_t result = nx_stm32_audio_admit(s, c);
    if (result != NX_SUCCESS)
        return result;
    if (!g_nx_stm32_system.started ||
        !nx_stm32_clock_profile_matches(g_nx_stm32_system.rcc, NULL) ||
        g_nx_stm32_system.core_hz != c->hclk_hz ||
        (NX_AV_READ(&g_nx_stm32_system.rcc->AHB1ENR) & RCC_AHB1ENR_DMA1EN) ==
            0U)
        return NX_ERROR_STATE;
    s->config = c;
    s->owned = true;
    s->clock_added = gate(c);
    s->priorities[0] = NX_AV_IRQ_PRIORITY_READ(vector(c));
    s->priorities[1] =
        c->tx.stream == NULL ? 0U : NX_AV_IRQ_PRIORITY_READ(c->tx.irq);
    s->priorities[2] =
        c->rx.stream == NULL ? 0U : NX_AV_IRQ_PRIORITY_READ(c->rx.irq);
    if (!nx_stm32_av_bits(&g_nx_stm32_system.rcc->APB1ENR, gate(c), gate(c)))
        return NX_ERROR_IO;
    if ((NX_AV_READ(&c->registers->I2SCFGR) & SPI_I2SCFGR_I2SE) != 0U ||
        NX_AV_READ(&c->registers->CR2) != 0U ||
        (c->duplex &&
         ((NX_AV_READ(&c->extension->I2SCFGR) & SPI_I2SCFGR_I2SE) != 0U ||
          NX_AV_READ(&c->extension->CR2) != 0U)))
        return NX_ERROR_BUSY;
    result = nx_stm32_av_pins(&c->common);
    if (result != NX_SUCCESS)
        return result;
    result = clock_acquire(s);
    if (result != NX_SUCCESS)
        return result;
    uint32_t mode =
        c->master ? (c->receive ? LL_I2S_MODE_MASTER_RX : LL_I2S_MODE_MASTER_TX)
                  : (c->receive ? LL_I2S_MODE_SLAVE_RX : LL_I2S_MODE_SLAVE_TX);
    uint32_t format = wire_format(c);
    /* LL's public bit definitions describe the hardware; direct checked
     * writes make the two-cell configuration one transactional owner. The
     * HAL transfer engine cannot coordinate these borrowed frame controls. */
    if (!nx_stm32_av_write(&c->registers->I2SCFGR, format | mode, 0xfffU) ||
        !nx_stm32_av_write(&c->registers->I2SPR,
                           c->master
                               ? c->divider | (c->odd ? SPI_I2SPR_ODD : 0U) |
                                     (c->mclk ? SPI_I2SPR_MCKOE : 0U)
                               : 2U,
                           0x3ffU))
        return NX_ERROR_IO;
    if (c->duplex &&
        (!nx_stm32_av_write(&c->extension->I2SCFGR,
                            format | (c->receive ? LL_I2S_MODE_SLAVE_TX
                                                 : LL_I2S_MODE_SLAVE_RX),
                            0xfffU) ||
         !nx_stm32_av_write(&c->extension->I2SPR, 2U, 0x3ffU)))
        return NX_ERROR_IO;
    s->initialized = true;
    return NX_SUCCESS;
}
bool nx_stm32_audio_live(nx_stm32_audio_state_t* s) {
    const nx_stm32_audio_config_t* c = s->config;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    bool ok = !s->history_lost && g_nx_stm32_system.started &&
              nx_stm32_clock_profile_matches(rcc, NULL) &&
              nx_stm32_av_timebase() &&
              g_nx_stm32_system.timer_error == NX_SUCCESS &&
              g_nx_stm32_system.core_hz == c->hclk_hz &&
              (NX_AV_READ(&rcc->APB1ENR) & gate(c)) != 0U &&
              (NX_AV_READ(&rcc->APB1RSTR) & reset_bit(c)) == 0U &&
              (NX_AV_READ(&rcc->AHB1ENR) & RCC_AHB1ENR_DMA1EN) != 0U &&
              (NX_AV_READ(&rcc->AHB1RSTR) & RCC_AHB1RSTR_DMA1RST) == 0U;
    if (c->master)
        ok = ok && (NX_AV_READ(&rcc->CFGR) & RCC_CFGR_I2SSRC) == 0U &&
             (NX_AV_READ(&rcc->CR) & (RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY)) ==
                 (RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY) &&
             (NX_AV_READ(&rcc->PLLI2SCFGR) &
              (RCC_PLLI2SCFGR_PLLI2SN | RCC_PLLI2SCFGR_PLLI2SR)) ==
                 (((uint32_t)c->pll_n << 6U) | ((uint32_t)c->pll_r << 28U));
    if (s->active != NULL && nx_stm32_av_epoch() != s->time_epoch)
        ok = false;
    if (!ok && s->active != NULL)
        s->history_lost = true;
    return ok;
}
uint32_t nx_stm32_audio_status(nx_stm32_audio_state_t* s, SPI_TypeDef* bus) {
    uint32_t value = NX_AV_READ(&bus->SR);
    if ((value & AUDIO_ERRORS) != 0U) {
        __atomic_fetch_or(&s->events, value & AUDIO_ERRORS, __ATOMIC_RELAXED);
        if (s->result == NX_SUCCESS)
            s->result = NX_ERROR_IO;
    }
    return value;
}
void nx_stm32_audio_irq(nx_stm32_audio_state_t* s) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    uint32_t errors = nx_stm32_audio_status(s, s->config->registers);
    if (s->config->duplex)
        errors |= nx_stm32_audio_status(s, s->config->extension);
    if ((errors & AUDIO_ERRORS) != 0U) {
        /* OVR stays level-active until the task reads DR during its proved
         * drain. Withdraw our source and vector before returning from IRQ. */
        (void)nx_stm32_av_bits(&s->config->registers->CR2, SPI_CR2_ERRIE, 0U);
        if (s->config->duplex)
            (void)nx_stm32_av_bits(&s->config->extension->CR2, SPI_CR2_ERRIE,
                                   0U);
        NX_AV_IRQ_DISABLE(vector(s->config));
    }
}
bool nx_stm32_audio_cycle_delay(nx_stm32_audio_state_t* s, uint32_t bits) {
    const nx_stm32_audio_config_t* c = s->config;
    uint64_t frequency = (uint64_t)c->actual_rate.numerator_hz * c->slot_bits *
                         (c->standard >= 3U ? 1U : 2U);
    uint64_t cycles =
        ((uint64_t)c->hclk_hz * c->actual_rate.denominator * bits + frequency -
         1U) /
        frequency;
    if (cycles == 0U || cycles >= UINT32_MAX / 2U ||
        (NX_AV_READ(NX_AV_COUNTER_CONTROL) & DWT_CTRL_CYCCNTENA_Msk) == 0U)
        return false;
    uint32_t start = NX_AV_CYCLES();
    for (uint32_t i = 0U; i < c->common.poll_limit; ++i) {
        if ((uint32_t)(NX_AV_CYCLES() - start) >= cycles)
            return true;
        NX_AV_POLL();
    }
    return false;
}
/** \brief Master RX has BSY=0 throughout; apply its format-specific shutdown.
 * \note The normal DMA leaves the last halfword for the CPU for modes needing
 *       n-1 RXNE. A late RXNE/OVR is a failed timing observation, never a false
 *       complete wire proof. DWT delays measure actual HCLK cycles, not polls.
 */
static bool master_receive_terminal(nx_stm32_audio_state_t* s) {
    const nx_stm32_audio_config_t* c = s->config;
    if (s->terminal_stopped)
        return true;
    SPI_TypeDef* bus = c->registers;
    uint32_t value = nx_stm32_audio_status(s, bus);
    if (s->receive_tail && (value & SPI_SR_RXNE) != 0U) {
        s->result = NX_ERROR_IO;
        return false;
    }
    uint32_t bits =
        c->sample_bits == 16U && c->slot_bits == 32U && c->standard == 2U ? 17U
                                                                          : 1U;
    if (!nx_stm32_audio_cycle_delay(s, bits) ||
        (s->receive_tail &&
         (nx_stm32_audio_status(s, bus) & SPI_SR_RXNE) != 0U)) {
        s->result = NX_ERROR_IO;
        return false;
    }
    if (!nx_stm32_av_bits(&bus->I2SCFGR, SPI_I2SCFGR_I2SE, 0U))
        return false;
    s->terminal_cycle = NX_AV_CYCLES();
    s->terminal_stopped = true;
    return true;
}
void nx_stm32_audio_dma_irq(nx_stm32_audio_state_t* s, bool receive) {
    if (s == NULL || !s->owned || s->active == NULL)
        return;
    const nx_stm32_av_dma_t* d = receive ? &s->config->rx : &s->config->tx;
    if (d->stream == NULL)
        return;
    uint32_t flags = nx_stm32_av_dma_flags(d);
    nx_stm32_av_dma_ack(d, flags);
    if ((flags & 0x0dU) != 0U) {
        __atomic_fetch_or(&s->events, DMA_ERROR, __ATOMIC_RELAXED);
        if (s->result == NX_SUCCESS)
            s->result = NX_ERROR_IO;
    }
    if ((flags & 0x20U) != 0U && NX_AV_READ(&d->stream->NDTR) == 0U) {
        __atomic_fetch_or(&s->events, receive ? RX_DONE : TX_DONE,
                          __ATOMIC_RELAXED);
        if (receive && s->config->master && s->config->receive &&
            !s->config->duplex)
            (void)master_receive_terminal(s);
    }
}
static bool idle_pins(const nx_stm32_audio_config_t* c) {
    bool ws = (NX_AV_READ(&c->ws_gpio->IDR) & (1U << c->ws_pin)) != 0U;
    bool ck = (NX_AV_READ(&c->ck_gpio->IDR) & (1U << c->ck_pin)) != 0U;
    return ws == (c->standard == 0U) && ck == c->cpol_high;
}
static bool contiguous_frame(const nx_frame_stream_t* frame) {
    if (frame == NULL || (uintptr_t)frame % _Alignof(nx_frame_stream_t) != 0U ||
        frame->layout.plane_count != 1U)
        return false;
    const nx_frame_plane_t* plane = &frame->layout.planes[0];
    return plane->offset == 0U && plane->row_bytes == plane->stride &&
           plane->row_bytes != 0U && plane->rows != 0U &&
           plane->rows <= SIZE_MAX / plane->row_bytes &&
           plane->rows * plane->row_bytes == frame->layout.bytes;
}
static nx_result_t tx_validate(nx_stm32_audio_state_t* s,
                               nx_audio_request_t* request,
                               nx_frame_dma_view_t* output) {
    nx_frame_dma_loan_t* loan = request->tx;
    if (loan == NULL || (uintptr_t)loan % _Alignof(nx_frame_dma_loan_t) != 0U ||
        loan->owner == NULL ||
        (uintptr_t)loan->owner % _Alignof(nx_frame_stream_t) != 0U ||
        loan->owner->stream == NULL ||
        (uintptr_t)loan->owner->stream % _Alignof(nx_stream_t) != 0U ||
        loan->identity != loan || !contiguous_frame(loan->owner))
        return NX_ERROR_INVALID;
    const nx_frame_plane_t* plane = &loan->owner->layout.planes[0];
    nx_frame_dma_view_t view;
    nx_result_t result = nx_frame_dma_view(loan, 0U, 0U, 0U, plane->row_bytes,
                                           plane->rows, &view);
    if (result != NX_SUCCESS)
        return result;
    if (view.span_bytes != loan->owner->layout.bytes ||
        view.span_bytes != loan->block.length)
        return NX_ERROR_INVALID;
    result = nx_dma_buffer_validate(s->config->common.regions,
                                    s->config->common.region_count, view.data,
                                    view.span_bytes, 2U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    nx_stream_t* stream = loan->owner->stream;
    if (nx_stm32_av_overlap(request, sizeof(*request), loan, sizeof(*loan)) ||
        nx_stm32_av_overlap(request, sizeof(*request), loan->owner,
                            sizeof(*loan->owner)) ||
        nx_stm32_av_overlap(request, sizeof(*request), stream,
                            sizeof(*stream)) ||
        nx_stm32_av_overlap(request, sizeof(*request), stream->slots,
                            stream->count * sizeof(*stream->slots)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < stream->count; ++i)
        if (nx_stm32_av_overlap(request, sizeof(*request),
                                stream->slots[i].data,
                                stream->slots[i].capacity) ||
            nx_stm32_av_overlap(s, sizeof(*s), stream->slots[i].data,
                                stream->slots[i].capacity))
            return NX_ERROR_INVALID;
    *output = view;
    return NX_SUCCESS;
}
static bool arm_dma(const nx_stm32_av_dma_t* d, SPI_TypeDef* bus, uint8_t* data,
                    size_t bytes, bool receive, bool* armed) {
    nx_stm32_av_dma_ack(d, 0x3dU);
    NX_AV_WRITE(&d->stream->PAR, (uint32_t)(uintptr_t)&bus->DR);
    NX_AV_WRITE(&d->stream->M0AR, (uint32_t)(uintptr_t)data);
    NX_AV_WRITE(&d->stream->M1AR, 0U);
    NX_AV_WRITE(&d->stream->NDTR, (uint32_t)(bytes / 2U));
    uint32_t cr = ((uint32_t)d->channel << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC |
                  DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0 | DMA_SxCR_PL_1 |
                  DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE |
                  (receive ? 0U : DMA_SxCR_DIR_0);
    if (NX_AV_READ(&d->stream->PAR) != (uint32_t)(uintptr_t)&bus->DR ||
        NX_AV_READ(&d->stream->M0AR) != (uint32_t)(uintptr_t)data ||
        NX_AV_READ(&d->stream->M1AR) != 0U ||
        NX_AV_READ(&d->stream->NDTR) != bytes / 2U)
        return false;
    bool configured = nx_stm32_av_write(&d->stream->CR, cr, UINT32_MAX) &&
                      nx_stm32_av_write(&d->stream->FCR, 0U,
                                        DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE);
    if (!configured)
        return false;
    *armed = true;
    return nx_stm32_av_bits(&d->stream->CR, DMA_SxCR_EN, DMA_SxCR_EN);
}
static nx_result_t submit(void* context, nx_audio_request_t* request) {
    nx_stm32_audio_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL ||
        (uintptr_t)request % _Alignof(nx_audio_request_t) != 0U ||
        !s->initialized)
        return NX_ERROR_INVALID;
    if (s->active != NULL)
        return NX_ERROR_BUSY;
    const nx_stm32_audio_config_t* c = s->config;
    if (!nx_stm32_audio_live(s))
        return NX_ERROR_STATE;
    bool tx = tx_bus(c) != NULL, rx = rx_bus(c) != NULL;
    if (nx_request_state(&request->base) != NX_REQUEST_READY ||
        (request->tx != NULL) != tx || (request->rx != NULL) != rx ||
        nx_stm32_av_overlap(request, sizeof(*request), s, sizeof(*s)) ||
        nx_deadline_expired(request->base.deadline, NX_AV_NOW()))
        return NX_ERROR_INVALID;
    size_t bytes = 0U;
    nx_frame_dma_view_t transmit = {0};
    if (tx) {
        nx_result_t result = tx_validate(s, request, &transmit);
        if (result != NX_SUCCESS)
            return result;
        bytes = transmit.span_bytes;
    }
    if (rx) {
        nx_frame_stream_t* frame = request->rx;
        if (!contiguous_frame(frame) || (tx && frame->layout.bytes != bytes) ||
            nx_stm32_av_overlap(frame, sizeof(*frame), s, sizeof(*s)) ||
            nx_stm32_av_overlap(frame, sizeof(*frame), request,
                                sizeof(*request)))
            return NX_ERROR_INVALID;
        bytes = frame->layout.bytes;
        nx_result_t result =
            nx_stm32_av_stream_validate(&c->common, frame->stream, bytes, 2U, s,
                                        sizeof(*s), request, sizeof(*request));
        if (result != NX_SUCCESS)
            return result;
        for (size_t i = 0U; i < frame->stream->count; ++i) {
            nx_stream_slot_t* slot = &frame->stream->slots[i];
            if (nx_stm32_av_overlap(slot->data, slot->capacity, frame,
                                    sizeof(*frame)) ||
                (tx &&
                 (nx_stm32_av_overlap(slot->data, slot->capacity, request->tx,
                                      sizeof(*request->tx)) ||
                  nx_stm32_av_overlap(slot->data, slot->capacity, transmit.data,
                                      transmit.span_bytes))))
                return NX_ERROR_INVALID;
        }
    }
    if (bytes == 0U || bytes % frame_bytes(c) != 0U || bytes / 2U > 65535U ||
        (c->master && c->receive && !c->duplex && bytes < 4U) ||
        (!c->master && !idle_pins(c)))
        return NX_ERROR_INVALID;
    nx_result_t admitted = nx_request_admit(&request->base, NX_REQUEST_QUEUED);
    if (admitted != NX_SUCCESS)
        return admitted;
    s->active = request;
    s->transmit = transmit;
    s->time_epoch = nx_stm32_av_epoch();
    s->tx_armed = false;
    s->rx_armed = false;
    s->tx_enable_attempted = false;
    s->rx_enable_attempted = false;
    s->wire_quiet = false;
    s->bytes = bytes;
    s->result = NX_SUCCESS;
    s->events = 0U;
    s->draining = false;
    s->receive_tail =
        c->master && c->receive && !c->duplex &&
        !(c->sample_bits == 16U && c->slot_bits == 32U && c->standard != 2U);
    s->terminal_stopped = false;
    if (rx) {
        nx_result_t result =
            nx_stream_reserve(request->rx->stream, &s->receive);
        if (result != NX_SUCCESS) {
            s->result = result;
            s->draining = true;
            return NX_SUCCESS;
        }
        s->filling = true;
    }
    /* SR has read-clear errors. The first observation follows admission. */
    nx_stm32_audio_irq(s);
    if (s->result != NX_SUCCESS) {
        s->draining = true;
        return NX_SUCCESS;
    }
    bool ok = s->result == NX_SUCCESS;
    if (rx) {
        size_t dma_bytes = bytes - (s->receive_tail ? 2U : 0U);
        ok = arm_dma(&c->rx, rx_bus(c), s->receive.data, dma_bytes, true,
                     &s->rx_armed) &&
             ok;
        NX_AV_IRQ_PRIORITY(c->rx.irq, c->common.priority);
        NX_AV_IRQ_CLEAR(c->rx.irq);
        NX_AV_IRQ_ENABLE(c->rx.irq);
        ok = nx_stm32_av_bits(&rx_bus(c)->CR2, SPI_CR2_RXDMAEN | SPI_CR2_ERRIE,
                              SPI_CR2_RXDMAEN | SPI_CR2_ERRIE) &&
             ok;
    }
    if (tx) {
        ok = arm_dma(&c->tx, tx_bus(c), (uint8_t*)(uintptr_t)s->transmit.data,
                     bytes, false, &s->tx_armed) &&
             ok;
        NX_AV_IRQ_PRIORITY(c->tx.irq, c->common.priority);
        NX_AV_IRQ_CLEAR(c->tx.irq);
        NX_AV_IRQ_ENABLE(c->tx.irq);
        ok = nx_stm32_av_bits(&tx_bus(c)->CR2, SPI_CR2_TXDMAEN | SPI_CR2_ERRIE,
                              SPI_CR2_TXDMAEN | SPI_CR2_ERRIE) &&
             ok;
    }
    NX_AV_IRQ_PRIORITY(vector(c), c->common.priority);
    NX_AV_IRQ_CLEAR(vector(c));
    NX_AV_IRQ_ENABLE(vector(c));
    if (ok && c->duplex) {
        if (c->receive)
            s->tx_enable_attempted = true;
        else
            s->rx_enable_attempted = true;
        ok = nx_stm32_av_bits(&c->extension->I2SCFGR, SPI_I2SCFGR_I2SE,
                              SPI_I2SCFGR_I2SE);
    }
    if (ok) {
        if (c->receive)
            s->rx_enable_attempted = true;
        else
            s->tx_enable_attempted = true;
        ok = nx_stm32_av_bits(&c->registers->I2SCFGR, SPI_I2SCFGR_I2SE,
                              SPI_I2SCFGR_I2SE);
    }
    if (!ok && s->result == NX_SUCCESS) {
        s->result = NX_ERROR_IO;
        s->draining = true;
    }
    s->observed_at = NX_AV_NOW();
    return NX_SUCCESS;
}
static nx_result_t quarantine(nx_stm32_audio_state_t* s, nx_result_t result) {
    nx_request_state_t state = nx_request_state(&s->active->base);
    if (state == NX_REQUEST_QUEUED || state == NX_REQUEST_ACTIVE)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
    (void)nx_request_transition(&s->active->base, NX_REQUEST_QUARANTINED);
    return result;
}
bool nx_stm32_audio_tx_idle(nx_stm32_audio_state_t* s, SPI_TypeDef* bus) {
    for (uint32_t i = 0U; i < s->config->common.poll_limit; ++i) {
        uint32_t value = nx_stm32_audio_status(s, bus);
        if ((value & SPI_SR_TXE) != 0U &&
            (nx_stm32_audio_status(s, bus) & SPI_SR_BSY) == 0U)
            return true;
        NX_AV_POLL();
    }
    return false;
}
static nx_result_t drain(nx_stm32_audio_state_t* s) {
    const nx_stm32_audio_config_t* c = s->config;
    if (!nx_stm32_audio_live(s))
        return quarantine(s, NX_ERROR_STATE);
    s->draining = true;
    nx_request_state_t state = nx_request_state(&s->active->base);
    if (state == NX_REQUEST_QUEUED || state == NX_REQUEST_ACTIVE)
        (void)nx_request_transition(&s->active->base, NX_REQUEST_DRAINING);
    SPI_TypeDef* tx = tx_bus(c);
    SPI_TypeDef* rx = rx_bus(c);
    if (tx != NULL && (!nx_stm32_av_bits(&tx->CR2, SPI_CR2_TXDMAEN, 0U) ||
                       !nx_stm32_av_dma_detach(&c->tx, c->common.poll_limit)))
        return quarantine(s, NX_ERROR_BUSY);
    if (tx != NULL && s->tx_enable_attempted && !s->wire_quiet &&
        !nx_stm32_audio_tx_idle(s, tx))
        return quarantine(s, NX_ERROR_BUSY);
    if (c->master && c->receive && !c->duplex && s->result == NX_SUCCESS) {
        if (!master_receive_terminal(s))
            return quarantine(s, NX_ERROR_BUSY);
        if (s->receive_tail) {
            bool last = false;
            for (uint32_t i = 0U; i < c->common.poll_limit; ++i) {
                if ((nx_stm32_audio_status(s, rx) & SPI_SR_RXNE) != 0U) {
                    last = true;
                    break;
                }
                NX_AV_POLL();
            }
            if (!last)
                return quarantine(s, NX_ERROR_BUSY);
            uint16_t tail = (uint16_t)NX_AV_READ(&rx->DR);
            memcpy(s->receive.data + s->bytes - 2U, &tail, sizeof(tail));
            s->receive_tail = false;
        }
    }
    if (c->duplex &&
        !nx_stm32_av_bits(&c->extension->I2SCFGR, SPI_I2SCFGR_I2SE, 0U))
        return quarantine(s, NX_ERROR_BUSY);
    if (!nx_stm32_av_bits(&c->registers->I2SCFGR, SPI_I2SCFGR_I2SE, 0U))
        return quarantine(s, NX_ERROR_BUSY);
    /* A later address or IRQ refusal cannot erase the already observed wire
     * drain. A disabled cell is allowed to deassert TXE. */
    s->wire_quiet = true;
    if (rx != NULL) {
        if (s->rx_enable_attempted && c->master && c->receive && !c->duplex &&
            !nx_stm32_audio_cycle_delay(s, c->slot_bits))
            return quarantine(s, NX_ERROR_BUSY);
        if (!nx_stm32_av_bits(&rx->CR2, SPI_CR2_RXDMAEN, 0U) ||
            !nx_stm32_av_dma_detach(&c->rx, c->common.poll_limit))
            return quarantine(s, NX_ERROR_BUSY);
        /* Drain residual DR/OVR only after memory is disabled; preserve every
         * read-clear error before the clearing read sequence. */
        uint32_t value = nx_stm32_audio_status(s, rx);
        if ((value & (SPI_SR_RXNE | SPI_SR_OVR)) != 0U) {
            (void)NX_AV_READ(&rx->DR);
            (void)nx_stm32_audio_status(s, rx);
        }
        if ((nx_stm32_audio_status(s, rx) &
             (SPI_SR_RXNE | SPI_SR_OVR | SPI_SR_BSY)) != 0U)
            return quarantine(s, NX_ERROR_BUSY);
    }
    if (!nx_stm32_av_write(&c->registers->CR2, 0U, UINT32_MAX) ||
        (c->duplex && !nx_stm32_av_write(&c->extension->CR2, 0U, UINT32_MAX)) ||
        !nx_stm32_av_irq_detach(vector(c)))
        return quarantine(s, NX_ERROR_BUSY);
    s->tx_remaining = tx == NULL     ? 0U
                      : !s->tx_armed ? s->bytes / 2U
                                     : NX_AV_READ(&c->tx.stream->NDTR);
    s->rx_remaining = rx == NULL     ? 0U
                      : !s->rx_armed ? s->bytes / 2U
                                     : NX_AV_READ(&c->rx.stream->NDTR);
    if ((tx != NULL && !nx_stm32_av_dma_addresses(&c->tx)) ||
        (rx != NULL && !nx_stm32_av_dma_addresses(&c->rx)))
        return quarantine(s, NX_ERROR_BUSY);
    nx_audio_request_t* request = s->active;
    if (s->result == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, NX_AV_NOW()))
        s->result = NX_ERROR_TIMEOUT;
    nx_result_t result = s->result;
    if (result == NX_SUCCESS && ((tx != NULL && s->tx_remaining != 0U) ||
                                 (rx != NULL && s->rx_remaining != 0U)))
        result = NX_ERROR_IO;
    if (tx != NULL) {
        const nx_dma_drain_facts_t facts = {s->bytes / 2U, s->tx_remaining,
                                            true, true, true};
        nx_result_t released =
            nx_frame_dma_release(request->tx->owner, request->tx, &facts);
        if (released != NX_SUCCESS)
            return quarantine(s, released);
    }
    if (s->filling) {
        nx_result_t released =
            result == NX_SUCCESS
                ? nx_frame_stream_publish(request->rx, &s->receive, 0U)
                : nx_stream_abort(request->rx->stream, &s->receive, true);
        if (released != NX_SUCCESS)
            return quarantine(s, released);
        s->filling = false;
    }
    s->active = NULL;
    s->draining = false;
    memset(&s->transmit, 0, sizeof(s->transmit));
    memset(&s->receive, 0, sizeof(s->receive));
    nx_request_settle(&request->base, result,
                      result == NX_SUCCESS ? s->bytes : 0U);
    return result;
}
static nx_result_t service(void* context) {
    nx_stm32_audio_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned)
        return NX_ERROR_STATE;
    if (s->active == NULL)
        return NX_SUCCESS;
    if (!nx_stm32_audio_live(s))
        return quarantine(s, NX_ERROR_STATE);
    nx_stm32_audio_irq(s);
    if (tx_bus(s->config) != NULL)
        nx_stm32_audio_dma_irq(s, false);
    if (rx_bus(s->config) != NULL)
        nx_stm32_audio_dma_irq(s, true);
    nx_time_us_t now = NX_AV_NOW();
    if (now < s->observed_at)
        s->history_lost = true;
    if (s->history_lost)
        return quarantine(s, NX_ERROR_STATE);
    if (nx_deadline_expired(s->active->base.deadline, now) &&
        s->result == NX_SUCCESS)
        s->result = NX_ERROR_TIMEOUT;
    if (s->draining || s->result != NX_SUCCESS)
        return drain(s);
    if (nx_request_state(&s->active->base) == NX_REQUEST_QUEUED) {
        if (!s->config->master && !idle_pins(s->config)) {
            s->result = NX_ERROR_IO;
            return drain(s);
        }
        bool armed =
            s->config->master || tx_bus(s->config) == NULL ||
            (NX_AV_READ(&s->config->tx.stream->NDTR) == s->bytes / 2U - 1U &&
             (nx_stm32_audio_status(s, tx_bus(s->config)) & SPI_SR_TXE) == 0U);
        uint64_t warmup_us =
            (3000000U + (uint64_t)s->config->pclk_hz - 1U) / s->config->pclk_hz;
        if (warmup_us < 3U)
            warmup_us = 3U;
        if (armed && (s->config->master || now - s->observed_at >= warmup_us))
            (void)nx_request_transition(&s->active->base, NX_REQUEST_ACTIVE);
    }
    uint32_t needed = (tx_bus(s->config) != NULL ? TX_DONE : 0U) |
                      (rx_bus(s->config) != NULL ? RX_DONE : 0U);
    if ((__atomic_load_n(&s->events, __ATOMIC_RELAXED) & needed) == needed)
        return drain(s);
    return NX_ERROR_BUSY;
}
static nx_result_t cancel(void* context, nx_audio_request_t* request) {
    nx_stm32_audio_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || request == NULL || s->active != request)
        return NX_ERROR_STATE;
    if (s->result == NX_SUCCESS)
        s->result = NX_ERROR_CANCELLED;
    s->draining = true;
    return service(s);
}
static nx_result_t stop(void* context) {
    nx_stm32_audio_state_t* s = context;
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL || !s->owned)
        return NX_ERROR_STATE;
    if (s->active != NULL) {
        (void)cancel(s, s->active);
        if (s->active != NULL)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
static nx_result_t rate(void* context, nx_audio_rate_t* output) {
    nx_stm32_audio_state_t* s = context;
    if (s == NULL || output == NULL || !s->owned ||
        nx_stm32_av_overlap(output, sizeof(*output), s, sizeof(*s)))
        return NX_ERROR_INVALID;
    if (s->active != NULL) {
        nx_audio_request_t* request = s->active;
        if (nx_stm32_av_overlap(output, sizeof(*output), request,
                                sizeof(*request)))
            return NX_ERROR_INVALID;
        nx_frame_stream_t* frames[2] = {
            request->tx == NULL ? NULL : request->tx->owner, request->rx};
        if (request->tx != NULL &&
            nx_stm32_av_overlap(output, sizeof(*output), request->tx,
                                sizeof(*request->tx)))
            return NX_ERROR_INVALID;
        for (size_t direction = 0U; direction < 2U; ++direction) {
            nx_frame_stream_t* frame = frames[direction];
            if (frame == NULL)
                continue;
            nx_stream_t* stream = frame->stream;
            if (nx_stm32_av_overlap(output, sizeof(*output), frame,
                                    sizeof(*frame)) ||
                nx_stm32_av_overlap(output, sizeof(*output), stream,
                                    sizeof(*stream)) ||
                nx_stm32_av_overlap(output, sizeof(*output), stream->slots,
                                    stream->count * sizeof(*stream->slots)))
                return NX_ERROR_INVALID;
            for (size_t i = 0U; i < stream->count; ++i)
                if (nx_stm32_av_overlap(output, sizeof(*output),
                                        stream->slots[i].data,
                                        stream->slots[i].capacity))
                    return NX_ERROR_INVALID;
        }
    }
    *output =
        s->config->master ? s->config->actual_rate : (nx_audio_rate_t){0U, 0U};
    return NX_SUCCESS;
}
nx_result_t nx_stm32_audio_close(nx_stm32_audio_state_t* s) {
    if (!nx_stm32_av_task())
        return NX_ERROR_CONTEXT;
    if (s == NULL)
        return NX_ERROR_INVALID;
    if (!s->owned)
        return NX_SUCCESS;
    /* Closing is irreversible admission withdrawal, even when a later drain
     * or resource readback fails. Existing borrowers can still use service. */
    s->initialized = false;
    nx_result_t result = stop(s);
    if (result != NX_SUCCESS)
        return result;
    const nx_stm32_audio_config_t* c = s->config;
    if (!nx_stm32_av_write(&c->registers->CR2, 0U, UINT32_MAX) ||
        !nx_stm32_av_bits(&c->registers->I2SCFGR, SPI_I2SCFGR_I2SE, 0U) ||
        (c->duplex &&
         (!nx_stm32_av_write(&c->extension->CR2, 0U, UINT32_MAX) ||
          !nx_stm32_av_bits(&c->extension->I2SCFGR, SPI_I2SCFGR_I2SE, 0U))) ||
        (c->tx.stream != NULL &&
         (!nx_stm32_av_dma_detach(&c->tx, c->common.poll_limit) ||
          !nx_stm32_av_dma_addresses(&c->tx))) ||
        (c->rx.stream != NULL &&
         (!nx_stm32_av_dma_detach(&c->rx, c->common.poll_limit) ||
          !nx_stm32_av_dma_addresses(&c->rx))) ||
        !nx_stm32_av_irq_detach(vector(c)))
        return NX_ERROR_BUSY;
    if (!nx_stm32_av_irq_priority_restore(vector(c), s->priorities[0]) ||
        (c->tx.stream != NULL &&
         !nx_stm32_av_irq_priority_restore(c->tx.irq, s->priorities[1])) ||
        (c->rx.stream != NULL &&
         !nx_stm32_av_irq_priority_restore(c->rx.irq, s->priorities[2])))
        return NX_ERROR_IO;
    result = clock_release(s);
    if (result != NX_SUCCESS)
        return result;
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    if (!nx_stm32_av_bits(&rcc->APB1RSTR, reset_bit(c), reset_bit(c)) ||
        !nx_stm32_av_bits(&rcc->APB1RSTR, reset_bit(c), 0U) ||
        !nx_stm32_av_bits(&rcc->APB1ENR, s->clock_added, 0U))
        return NX_ERROR_IO;
    s->initialized = false;
    s->owned = false;
    s->clock_added = 0U;
    s->config = NULL;
    return NX_SUCCESS;
}
const nx_audio_ops_t nx_stm32_audio_ops = {submit, service, cancel,
                                           stop,   rate,    NULL};
