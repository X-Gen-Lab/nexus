/**
 * \file            i2s2.c
 * \brief           F103ZE HSI8 finite I2S2 master transmitter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2s2.h"
#include "dispatch_checks.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_gpio.h"
#include "system/platform.h"

#if !defined(STM32F103xE)
#error "The selected I2S2 profile is reviewed for STM32F103ZET6"
#endif

#ifndef NX_STM32F1_SPI_STATUS
#define NX_STM32F1_SPI_STATUS(reg) ((reg)->SR)
#endif
#ifndef NX_STM32F1_IRQ_ENABLED
#define NX_STM32F1_IRQ_ENABLED(irq)             NVIC_GetEnableIRQ(irq)
#define NX_STM32F1_IRQ_PENDING(irq)             NVIC_GetPendingIRQ(irq)
#define NX_STM32F1_IRQ_ACTIVE(irq)              NVIC_GetActive(irq)
#define NX_STM32F1_IRQ_PRIORITY(irq)            NVIC_GetPriority(irq)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, value) NVIC_SetPriority((irq), (value))
#define NX_STM32F1_IRQ_ENABLE(irq)              NVIC_EnableIRQ(irq)
#define NX_STM32F1_IRQ_DISABLE(irq)             NVIC_DisableIRQ(irq)
#define NX_STM32F1_IRQ_CLEAR(irq)               NVIC_ClearPendingIRQ(irq)
#endif

#define AUDIO_IRQ DMA1_Channel5_IRQn
#define WS_PIN    12U
#define CK_PIN    13U
#define SD_PIN    15U
#define PINS      ((uint16_t)((1U << WS_PIN) | (1U << CK_PIN) | (1U << SD_PIN)))
#define GPIO_AF   ((uint32_t)(GPIO_CRL_CNF0_1 | LL_GPIO_SPEED_FREQ_HIGH))
#define PIN_MODES                                                              \
    ((GPIO_AF << (4U * (WS_PIN - 8U))) | (GPIO_AF << (4U * (CK_PIN - 8U))) |   \
     (GPIO_AF << (4U * (SD_PIN - 8U))))
#define I2S_FORMAT ((uint32_t)(SPI_I2SCFGR_I2SMOD | SPI_I2SCFGR_I2SCFG_1))
#define I2S_RATE_BITS                                                          \
    ((uint32_t)(SPI_I2SPR_I2SDIV | SPI_I2SPR_ODD | SPI_I2SPR_MCKOE))
#define I2S_ERRORS ((uint32_t)(SPI_SR_OVR | SPI_SR_UDR))
#define DMA_MODE                                                               \
    ((uint32_t)(DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 |                 \
                DMA_CCR_MSIZE_0 | DMA_CCR_TCIE | DMA_CCR_TEIE))
#define DMA_ACTIVE                                                             \
    ((uint32_t)(DMA_CCR_EN | DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_TEIE))
#define DMA_EVENTS ((uint32_t)(DMA_ISR_TCIF5 | DMA_ISR_HTIF5 | DMA_ISR_TEIF5))
#define DMA_ACK                                                                \
    ((uint32_t)(DMA_IFCR_CTCIF5 | DMA_IFCR_CHTIF5 | DMA_IFCR_CTEIF5))
#define RATE_BASE    UINT32_C(250000)
#define MAX_DRAIN_US UINT32_C(1000)

static uint32_t prescaler(const nx_stm32f1_i2s2_config_t* config) {
    return config->divider | (config->odd ? SPI_I2SPR_ODD : 0U);
}

/** \brief Check the static rational against the selected 16-bit stereo wire. */
static bool valid_rate(const nx_stm32f1_i2s2_config_t* config) {
    const uint64_t numerator = config->rate.numerator_hz;
    const uint64_t denominator = config->rate.denominator;
    const uint32_t divisor = 2U * config->divider + (config->odd ? 1U : 0U);
    return divisor >= 4U && numerator != 0U && numerator <= RATE_BASE &&
           denominator != 0U && denominator <= 511U &&
           numerator * divisor == RATE_BASE * denominator;
}

static bool hsi8(void) {
    const uint32_t clock = RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1;
    return (RCC->CR & RCC_CR_HSIRDY) != 0U &&
           (RCC->CFGR & clock) == RCC_CFGR_SWS_HSI;
}

static bool vector_idle(IRQn_Type irq) {
    return !NX_STM32F1_IRQ_ENABLED(irq) && !NX_STM32F1_IRQ_PENDING(irq) &&
           !NX_STM32F1_IRQ_ACTIVE(irq);
}

static bool dma_clock(void) {
    return (RCC->AHBENR & RCC_AHBENR_DMA1EN) != 0U;
}

static bool spi_clock(void) {
    return (RCC->APB1ENR & RCC_APB1ENR_SPI2EN) != 0U;
}

static bool source_clocks(void) {
    return hsi8() && dma_clock() && spi_clock();
}

nx_result_t nx_stm32f1_i2s2_admit(const nx_stm32f1_i2s2_t* state,
                                  const nx_stm32f1_i2s2_config_t* config) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || config == NULL || config->regions == NULL ||
        config->region_count == 0U || config->region_count > 16U ||
        config->priority >= (1U << __NVIC_PRIO_BITS) || !valid_rate(config))
        return NX_ERROR_INVALID;
    if (state->config != NULL || spi_clock() ||
        (RCC->APB1RSTR & RCC_APB1RSTR_SPI2RST) != 0U ||
        !vector_idle(AUDIO_IRQ) || !vector_idle(SPI2_IRQn))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_i2s2_prepare(nx_stm32f1_i2s2_t* state,
                                    const nx_stm32f1_i2s2_config_t* config) {
    nx_result_t result = nx_stm32f1_i2s2_admit(state, config);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32f1_system_live() || !hsi8())
        return NX_ERROR_STATE;
    state->config = config;
    state->gate_added = !dma_clock();
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN,
                                 RCC_AHBENR_DMA1EN))
        return NX_ERROR_IO;
    if (DMA1_Channel5->CCR != 0U || DMA1_Channel5->CNDTR != 0U ||
        DMA1_Channel5->CPAR != 0U || DMA1_Channel5->CMAR != 0U ||
        (DMA1->ISR & DMA_EVENTS) != 0U)
        return NX_ERROR_BUSY;
    state->lane_owned = true;

    state->spi_gate_added = true;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_SPI2EN,
                                 RCC_APB1ENR_SPI2EN))
        return NX_ERROR_IO;
    if (SPI2->CR1 != 0U || SPI2->CR2 != 0U || SPI2->I2SCFGR != 0U ||
        SPI2->I2SPR != 2U || (NX_STM32F1_SPI_STATUS(SPI2) & SPI_SR_BSY) != 0U)
        return NX_ERROR_BUSY;
    state->spi_owned = true;
    result = nx_stm32f1_pads_capture(&state->pads, GPIOB, PINS);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32f1_pads_apply(&state->pads, 0U, PIN_MODES,
                                   state->pads.latch & ~(uint32_t)PINS);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32f1_write_masked(&SPI2->I2SPR, I2S_RATE_BITS,
                                 prescaler(config)) ||
        !nx_stm32f1_write_masked(&SPI2->I2SCFGR, UINT16_MAX, I2S_FORMAT))
        return NX_ERROR_IO;
    state->previous_priority = (uint8_t)NX_STM32F1_IRQ_PRIORITY(AUDIO_IRQ);
    state->irq_owned = true;
    NX_STM32F1_IRQ_SET_PRIORITY(AUDIO_IRQ, config->priority);
    if (NX_STM32F1_IRQ_PRIORITY(AUDIO_IRQ) != config->priority)
        return NX_ERROR_IO;
    state->ready = true;
    return NX_SUCCESS;
}

static bool metadata(const nx_stm32f1_i2s2_t* state, const void* ptr,
                     size_t bytes, size_t alignment) {
    uintptr_t begin = (uintptr_t)ptr;
    if (!nx_io_dispatch_span(ptr, bytes, alignment))
        return false;
    const uintptr_t end = begin + bytes;
    return !(begin < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(begin < UINT32_C(0xf0000000) && end > UINT32_C(0xe0000000)) &&
           !nx_io_dispatch_overlap(ptr, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(ptr, bytes, state->config,
                                   sizeof(*state->config));
}

static nx_result_t frame_view(const nx_stm32f1_i2s2_t* state,
                              const nx_audio_request_t* request,
                              nx_frame_dma_view_t* view) {
    if (!metadata(state, request, sizeof(*request),
                  _Alignof(nx_audio_request_t)) ||
        request->rx != NULL ||
        !metadata(state, request->tx, sizeof(*request->tx),
                  _Alignof(nx_frame_dma_loan_t)))
        return NX_ERROR_INVALID;
    const nx_frame_dma_loan_t* loan = request->tx;
    const nx_frame_stream_t* owner = loan->owner;
    if (!metadata(state, owner, sizeof(*owner), _Alignof(nx_frame_stream_t)) ||
        !metadata(state, owner->stream, sizeof(*owner->stream),
                  _Alignof(nx_stream_t)))
        return NX_ERROR_INVALID;
    if (nx_io_dispatch_overlap(request, sizeof(*request), loan,
                               sizeof(*loan)) ||
        nx_io_dispatch_overlap(request, sizeof(*request), owner,
                               sizeof(*owner)) ||
        nx_io_dispatch_overlap(request, sizeof(*request), owner->stream,
                               sizeof(*owner->stream)))
        return NX_ERROR_INVALID;
    const nx_stream_t* stream = owner->stream;
    if (stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots))
        return NX_ERROR_INVALID;
    const size_t slots_bytes = stream->count * sizeof(*stream->slots);
    if (!metadata(state, stream->slots, slots_bytes,
                  _Alignof(nx_stream_slot_t)) ||
        nx_io_dispatch_overlap(request, sizeof(*request), stream->slots,
                               slots_bytes))
        return NX_ERROR_INVALID;
    const nx_frame_plane_t* plane = &owner->layout.planes[0];
    const size_t bytes = owner->layout.bytes;
    if (owner->layout.plane_count != 1U || plane->offset != 0U ||
        plane->row_bytes == 0U || plane->row_bytes != plane->stride ||
        plane->rows == 0U || plane->rows > SIZE_MAX / plane->row_bytes ||
        plane->rows * plane->row_bytes != bytes || bytes % 4U != 0U ||
        bytes > 2U * UINT16_MAX || loan->identity != loan)
        return NX_ERROR_INVALID;
    const nx_result_t result = nx_frame_dma_view(
        loan, 0U, 0U, 0U, plane->row_bytes, plane->rows, view);
    if (result != NX_SUCCESS)
        return result;
    if (view->span_bytes != bytes || !metadata(state, view->data, bytes, 2U) ||
        nx_io_dispatch_overlap(view->data, bytes, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    return nx_dma_buffer_validate(state->config->regions,
                                  state->config->region_count, view->data,
                                  bytes, 2U, NX_DMA_TO_DEVICE);
}

static bool disable_sources(void) {
    NX_STM32F1_IRQ_DISABLE(AUDIO_IRQ);
    const bool request_off =
        (SPI2->CR2 & SPI_CR2_TXDMAEN) == 0U ||
        nx_stm32f1_write_masked(&SPI2->CR2, SPI_CR2_TXDMAEN, 0U);
    const bool engine_off =
        (DMA1_Channel5->CCR & DMA_ACTIVE) == 0U ||
        nx_stm32f1_write_masked(&DMA1_Channel5->CCR, DMA_ACTIVE, 0U);
    nx_arch_dsb();
    return request_off && engine_off && !NX_STM32F1_IRQ_ENABLED(AUDIO_IRQ);
}

static void drain(nx_stm32f1_i2s2_t* state, nx_result_t result,
                  nx_time_us_t now) {
    if (!state->draining) {
        state->terminal = result;
        state->draining = true;
        if (nx_deadline_finite_after(now, MAX_DRAIN_US,
                                     &state->drain_deadline) != NX_SUCCESS)
            state->drain_deadline = 0U;
        (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    }
    NX_STM32F1_IRQ_DISABLE(AUDIO_IRQ);
    if (source_clocks())
        (void)disable_sources();
}

static void observe(nx_stm32f1_i2s2_t* state, nx_time_us_t now) {
    if (state->draining)
        return;
    if (!source_clocks()) {
        drain(state, NX_ERROR_IO, now);
        return;
    }
    const uint32_t events = DMA1->ISR;
    const uint32_t status = NX_STM32F1_SPI_STATUS(SPI2);
    if ((events & DMA_ISR_TEIF5) != 0U || (status & I2S_ERRORS) != 0U)
        drain(state, NX_ERROR_IO, now);
    else if (nx_deadline_expired(state->active->base.deadline, now))
        drain(state, NX_ERROR_TIMEOUT, now);
    else if ((events & DMA_ISR_TCIF5) != 0U)
        drain(state, NX_SUCCESS, now);
}

void nx_stm32f1_i2s2_irq(nx_stm32f1_i2s2_t* state) {
    if (state == NULL || state->active == NULL)
        return;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    observe(state, nx_time_now_us());
    nx_arch_irq_restore(guard);
}

static bool arm(nx_stm32f1_i2s2_t* state, const uint8_t* samples) {
    DMA_Channel_TypeDef* lane = DMA1_Channel5;
    if (!nx_stm32f1_write_masked(&lane->CPAR, UINT32_MAX,
                                 (uint32_t)(uintptr_t)&SPI2->DR) ||
        !nx_stm32f1_write_masked(&lane->CMAR, UINT32_MAX,
                                 (uint32_t)(uintptr_t)samples) ||
        !nx_stm32f1_write_masked(&lane->CNDTR, UINT16_MAX, state->count) ||
        !nx_stm32f1_write_masked(&lane->CCR, UINT16_MAX, DMA_MODE | DMA_CCR_EN))
        return false;
    NX_STM32F1_IRQ_ENABLE(AUDIO_IRQ);
    nx_arch_dsb();
    if (!NX_STM32F1_IRQ_ENABLED(AUDIO_IRQ) ||
        !nx_stm32f1_write_masked(&SPI2->CR2, SPI_CR2_TXDMAEN, SPI_CR2_TXDMAEN))
        return false;
    state->enabled_attempted = true;
    return nx_stm32f1_write_masked(&SPI2->I2SCFGR, SPI_I2SCFGR_I2SE,
                                   SPI_I2SCFGR_I2SE);
}

static nx_result_t submit(void* context, nx_audio_request_t* request) {
    nx_stm32f1_i2s2_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!state->ready || state->closing || !source_clocks())
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    nx_frame_dma_view_t view;
    const nx_result_t valid = frame_view(state, request, &view);
    if (valid != NX_SUCCESS)
        return valid;
    if (request->base.deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;

    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    const nx_time_us_t now = nx_time_now_us();
    nx_result_t result = NX_ERROR_BUSY;
    if (SPI2->I2SCFGR == I2S_FORMAT &&
        SPI2->I2SPR == prescaler(state->config) && SPI2->CR2 == 0U &&
        (DMA1_Channel5->CCR & DMA_ACTIVE) == 0U &&
        (DMA1->ISR & DMA_EVENTS) == 0U && vector_idle(AUDIO_IRQ) &&
        vector_idle(SPI2_IRQn)) {
        result = nx_deadline_expired(request->base.deadline, now)
                     ? NX_ERROR_TIMEOUT
                     : nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
        if (result == NX_SUCCESS) {
            state->active = request;
            state->count = (uint16_t)(view.span_bytes / 2U);
            state->remaining = state->count;
            state->enabled_attempted = false;
            state->wire_quiet = false;
            state->draining = false;
            if (!arm(state, view.data))
                drain(state, NX_ERROR_IO, now);
        }
    }
    nx_arch_irq_restore(guard);
    return result;
}

/** \brief TXE is meaningful while enabled and must precede a fresh BSY0. */
static bool wire_stopped(nx_stm32f1_i2s2_t* state) {
    if (state->wire_quiet)
        return true;
    if (state->enabled_attempted) {
        const uint32_t first = NX_STM32F1_SPI_STATUS(SPI2);
        const uint32_t second = NX_STM32F1_SPI_STATUS(SPI2);
        if (state->terminal == NX_SUCCESS &&
            ((first | second) & I2S_ERRORS) != 0U)
            state->terminal = NX_ERROR_IO;
        if ((first & SPI_SR_TXE) == 0U || (second & SPI_SR_BSY) != 0U)
            return false;
    }
    if (!nx_stm32f1_write_masked(&SPI2->I2SCFGR, SPI_I2SCFGR_I2SE, 0U))
        return false;
    state->wire_quiet = true;
    return true;
}

static bool retire_addresses(void) {
    DMA_Channel_TypeDef* lane = DMA1_Channel5;
    return nx_stm32f1_write_masked(&lane->CPAR, UINT32_MAX, 0U) &&
           nx_stm32f1_write_masked(&lane->CMAR, UINT32_MAX, 0U);
}

static nx_result_t finish(nx_stm32f1_i2s2_t* state) {
    if (!source_clocks() || !disable_sources() ||
        NX_STM32F1_IRQ_ACTIVE(AUDIO_IRQ) || !vector_idle(SPI2_IRQn))
        return NX_ERROR_BUSY;
    const uint32_t rest = DMA1_Channel5->CNDTR;
    if (rest > state->count)
        return NX_ERROR_BUSY;
    state->remaining = (uint16_t)rest;
    if (state->terminal == NX_SUCCESS &&
        (rest != 0U || (DMA1->ISR & DMA_ISR_TEIF5) != 0U))
        state->terminal = NX_ERROR_IO;
    if (!wire_stopped(state))
        return NX_ERROR_BUSY;
    /* ES0340 2.4.1/2.4.2: exact independent event bits, 32-bit accesses. */
    NX_STM32F1_WRITE(&DMA1->IFCR, DMA_ACK);
    NX_STM32F1_IRQ_CLEAR(AUDIO_IRQ);
    nx_arch_dsb();
    if ((DMA1->ISR & DMA_EVENTS) != 0U || !vector_idle(AUDIO_IRQ) ||
        !retire_addresses())
        return NX_ERROR_BUSY;
    const nx_dma_drain_facts_t facts = {state->count, state->remaining, true,
                                        true, true};
    nx_audio_request_t* request = state->active;
    const nx_result_t released =
        nx_frame_dma_release(request->tx->owner, request->tx, &facts);
    if (released != NX_SUCCESS)
        return NX_ERROR_BUSY;
    const nx_result_t result = state->terminal;
    state->active = NULL;
    nx_request_settle(&request->base, result,
                      result == NX_SUCCESS ? 2U * (size_t)state->count : 0U);
    return result;
}

static nx_result_t service(void* context) {
    nx_stm32f1_i2s2_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->active == NULL)
        return NX_SUCCESS;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    const nx_time_us_t now = nx_time_now_us();
    observe(state, now);
    const nx_result_t result = state->draining ? finish(state) : NX_ERROR_BUSY;
    if (state->active != NULL && state->draining &&
        nx_deadline_expired(state->drain_deadline, now) &&
        nx_request_state(&state->active->base) != NX_REQUEST_QUARANTINED)
        (void)nx_request_transition(&state->active->base,
                                    NX_REQUEST_QUARANTINED);
    nx_arch_irq_restore(guard);
    return result;
}

static nx_result_t cancel(void* context, nx_audio_request_t* request) {
    nx_stm32f1_i2s2_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (request == NULL)
        return NX_ERROR_INVALID;
    if (state->active != request)
        return nx_request_state(&request->base) == NX_REQUEST_SETTLED
                   ? NX_SUCCESS
                   : NX_ERROR_STATE;
    const nx_arch_irq_state_t guard = nx_arch_irq_save();
    drain(state, NX_ERROR_CANCELLED, nx_time_now_us());
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}

static nx_result_t join(nx_stm32f1_i2s2_t* state) {
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        (void)service(state);
    }
    return state->active == NULL ? NX_SUCCESS : NX_ERROR_BUSY;
}

static nx_result_t stop(void* context) {
    nx_stm32f1_i2s2_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    return state->ready ? join(state) : NX_ERROR_STATE;
}

/** \brief Queries cannot write into immutable configuration or live loans. */
static bool query_output(const nx_stm32f1_i2s2_t* state, const void* output,
                         size_t bytes, size_t alignment) {
    const nx_stm32f1_i2s2_config_t* config = state->config;
    if (!metadata(state, output, bytes, alignment) ||
        nx_io_dispatch_overlap(output, bytes, config->regions,
                               config->region_count * sizeof(*config->regions)))
        return false;
    const nx_audio_request_t* request = state->active;
    if (request == NULL)
        return true;
    const nx_frame_dma_loan_t* loan = request->tx;
    const nx_frame_stream_t* frames = loan->owner;
    const nx_stream_t* stream = frames->stream;
    return !nx_io_dispatch_overlap(output, bytes, request, sizeof(*request)) &&
           !nx_io_dispatch_overlap(output, bytes, loan, sizeof(*loan)) &&
           !nx_io_dispatch_overlap(output, bytes, frames, sizeof(*frames)) &&
           !nx_io_dispatch_overlap(output, bytes, stream, sizeof(*stream)) &&
           !nx_io_dispatch_overlap(output, bytes, stream->slots,
                                   stream->count * sizeof(*stream->slots)) &&
           !nx_io_dispatch_overlap(output, bytes, loan->block.data,
                                   loan->block.length);
}

static nx_result_t rate(void* context, nx_audio_rate_t* output) {
    const nx_stm32f1_i2s2_t* state = context;
    if (!state->ready || !source_clocks() ||
        SPI2->I2SPR != prescaler(state->config))
        return NX_ERROR_STATE;
    if (!query_output(state, output, sizeof(*output),
                      _Alignof(nx_audio_rate_t)))
        return NX_ERROR_INVALID;
    *output = state->config->rate;
    return NX_SUCCESS;
}

static nx_result_t layout(void* context, nx_audio_layout_t* output) {
    const nx_stm32f1_i2s2_t* state = context;
    if (!state->ready)
        return NX_ERROR_STATE;
    if (!query_output(state, output, sizeof(*output),
                      _Alignof(nx_audio_layout_t)))
        return NX_ERROR_INVALID;
    *output = (nx_audio_layout_t){
        NX_AUDIO_STORAGE_LITTLE_ENDIAN, NX_FRAME_SAMPLE_LSB, 3U, 16U, 16U, 2U};
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_i2s2_close(nx_stm32f1_i2s2_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->config == NULL)
        return NX_SUCCESS;
    state->closing = true;
    const nx_result_t stopped = join(state);
    if (stopped != NX_SUCCESS)
        return stopped;
    if (state->irq_owned) {
        NX_STM32F1_IRQ_DISABLE(AUDIO_IRQ);
        NX_STM32F1_IRQ_SET_PRIORITY(AUDIO_IRQ, state->previous_priority);
        nx_arch_dsb();
        if (!vector_idle(AUDIO_IRQ) ||
            NX_STM32F1_IRQ_PRIORITY(AUDIO_IRQ) != state->previous_priority)
            return NX_ERROR_BUSY;
        state->irq_owned = false;
    }
    if (state->lane_owned) {
        if (!dma_clock() || (DMA1_Channel5->CCR & DMA_ACTIVE) != 0U ||
            !retire_addresses() ||
            !nx_stm32f1_write_masked(&DMA1_Channel5->CCR, UINT16_MAX, 0U) ||
            !nx_stm32f1_write_masked(&DMA1_Channel5->CNDTR, UINT16_MAX, 0U))
            return NX_ERROR_BUSY;
        state->lane_owned = false;
    } else if (state->gate_added && dma_clock() &&
               (DMA1_Channel5->CCR & DMA_CCR_EN) != 0U)
        return NX_ERROR_BUSY;

    if (state->spi_owned) {
        if (!spi_clock() || (SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) != 0U ||
            (NX_STM32F1_SPI_STATUS(SPI2) & SPI_SR_BSY) != 0U ||
            !nx_stm32f1_write_masked(&SPI2->I2SCFGR, UINT16_MAX, 0U) ||
            !nx_stm32f1_write_masked(&SPI2->I2SPR, I2S_RATE_BITS, 2U))
            return NX_ERROR_BUSY;
        state->spi_owned = false;
    } else if (state->spi_gate_added && spi_clock() &&
               (SPI2->I2SCFGR & SPI_I2SCFGR_I2SE) != 0U)
        return NX_ERROR_BUSY;
    const nx_result_t restored = nx_stm32f1_pads_restore(&state->pads);
    if (restored != NX_SUCCESS)
        return restored;
    if (state->spi_gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_SPI2EN, 0U))
        return NX_ERROR_IO;
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN, 0U))
        return NX_ERROR_IO;
    *state = (nx_stm32f1_i2s2_t){0};
    return NX_SUCCESS;
}

const nx_audio_ops_t nx_stm32f1_i2s2_ops = {.submit = submit,
                                            .service = service,
                                            .cancel = cancel,
                                            .stop = stop,
                                            .rate = rate,
                                            .layout = layout};
