/**
 * \file            trigger_dma.c
 * \brief           F103RB finite timer-triggered regular results through DMA1
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/adc/trigger_dma.h"
#include "dispatch_checks.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_adc.h"
#include "system/platform.h"

#ifndef STM32F103xB
#error "ADC trigger DMA requires the reviewed F103xB controller layout"
#endif

#ifndef NX_STM32F1_ADC_TRIGGER_IRQ_IDLE
static bool idle_vector(IRQn_Type irq) {
    return NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&
           NVIC_GetActive(irq) == 0U;
}

/** \brief No source in this polling fixture can receive a producer IRQ. */
static bool vectors_idle(bool tim1) {
    return idle_vector(DMA1_Channel1_IRQn) &&
           (tim1 ? idle_vector(TIM1_BRK_IRQn) && idle_vector(TIM1_UP_IRQn) &&
                       idle_vector(TIM1_TRG_COM_IRQn) &&
                       idle_vector(TIM1_CC_IRQn)
                 : idle_vector(TIM3_IRQn));
}

#define NX_STM32F1_ADC_TRIGGER_IRQ_IDLE(tim1) vectors_idle(tim1)
#endif

#define ADC_IDLE          (LL_ADC_REG_TRIG_SOFTWARE | ADC_CR2_ADON)
#define ADC_TRIGGER_FLAGS (ADC_CR2_ADON | ADC_CR2_EXTTRIG | ADC_CR2_DMA)
#define DMA_FLAGS         (DMA_ISR_TCIF1 | DMA_ISR_HTIF1 | DMA_ISR_TEIF1)
#define DMA_CLEAR         (DMA_IFCR_CTCIF1 | DMA_IFCR_CHTIF1 | DMA_IFCR_CTEIF1)
#define DMA_HALFWORDS     (DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0)
#define TIM_ONE_PULSE     (TIM_CR1_OPM | TIM_CR1_URS)

#if defined(NX_STM32F1_ADC_SCAN_DMA)
typedef nx_stm32f1_adc_scan_dma_t adc_dma_state_t;
#define DMA_RANKS     2U
#define ADC_DMA_CLOSE nx_stm32f1_adc_scan_dma_close
#else
typedef nx_stm32f1_adc_trigger_dma_t adc_dma_state_t;
#define DMA_RANKS     1U
#define ADC_DMA_CLOSE nx_stm32f1_adc_trigger_dma_close
#endif

#define DMA_WIDTH (DMA_HALFWORDS | (DMA_RANKS == 2U ? DMA_CCR_MINC : 0U))

enum {
    TIMER_GATE = 1U,
    TIMER_OWNED = 2U,
    DMA_GATE = 4U,
    ARMED = 8U,
    LIVE = 16U,
    FAULTED = 32U,
    POLLS = 4096U
};

typedef struct {
    TIM_TypeDef* registers;
    volatile uint32_t* gate;
    volatile uint32_t* reset;
    uint32_t gate_mask;
    uint32_t reset_mask;
    uint32_t selection;
    uint32_t completion;
} trigger_timer_t;

static const trigger_timer_t tim1_compare = {TIM1,
                                             &RCC->APB2ENR,
                                             &RCC->APB2RSTR,
                                             RCC_APB2ENR_TIM1EN,
                                             RCC_APB2RSTR_TIM1RST,
                                             LL_ADC_REG_TRIG_EXT_TIM1_CH1,
                                             TIM_SR_UIF | TIM_SR_CC1IF};
static const trigger_timer_t tim3_update = {TIM3,
                                            &RCC->APB1ENR,
                                            &RCC->APB1RSTR,
                                            RCC_APB1ENR_TIM3EN,
                                            RCC_APB1RSTR_TIM3RST,
                                            LL_ADC_REG_TRIG_EXT_TIM3_TRGO,
                                            TIM_SR_UIF};

static const trigger_timer_t* trigger_timer(bool tim1) {
    return DMA_RANKS == 1U && tim1 ? &tim1_compare : &tim3_update;
}

static bool timer_idle(const trigger_timer_t* trigger) {
    const TIM_TypeDef* timer = trigger->registers;
    return timer->CR1 == 0U && timer->CR2 == 0U && timer->SMCR == 0U &&
           timer->DIER == 0U && timer->CCER == 0U && timer->CCMR1 == 0U &&
           timer->CCMR2 == 0U && timer->SR == 0U && timer->CNT == 0U &&
           timer->PSC == 0U && timer->ARR == UINT16_MAX &&
           (timer != TIM1 ||
            (timer->BDTR == 0U && timer->CCR1 == 0U && timer->RCR == 0U));
}

static bool lane_idle(void) {
    return DMA1_Channel1->CCR == 0U && DMA1_Channel1->CNDTR == 0U &&
           DMA1_Channel1->CPAR == 0U && DMA1_Channel1->CMAR == 0U &&
           (DMA1->ISR & DMA_FLAGS) == 0U;
}

static bool clocks_live(const trigger_timer_t* trigger) {
    const uint32_t dividers = RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                              RCC_CFGR_PPRE2 | RCC_CFGR_ADCPRE;
    return (RCC->CFGR & dividers) == RCC_CFGR_SWS_HSI &&
           (*trigger->gate & trigger->gate_mask) != 0U &&
           (RCC->APB2ENR & RCC_APB2ENR_ADC1EN) != 0U &&
           (RCC->AHBENR & RCC_AHBENR_DMA1EN) != 0U;
}

static bool timer_prepared(const trigger_timer_t* trigger) {
    const TIM_TypeDef* timer = trigger->registers;
    return timer->CR1 == TIM_ONE_PULSE && timer->PSC == 7U &&
           timer->ARR == 7U && timer->DIER == 0U && timer->CCER == 0U &&
           (timer == TIM1
                ? timer->CR2 == 0U && timer->CCMR1 == 0U && timer->CCR1 == 2U &&
                      timer->BDTR == 0U && timer->RCR == 0U
                : timer->CR2 == TIM_CR2_MMS_1);
}

static bool timer_prepare(const trigger_timer_t* trigger) {
    TIM_TypeDef* timer = trigger->registers;
    if (timer == TIM1) {
        /* A frozen CC1 generates an internal match. Keep every output off. */
        NX_STM32F1_WRITE(&timer->CCR1, 2U);
    } else {
        /* Load PSC without emitting the update TRGO. ADC is already clocked. */
        NX_STM32F1_WRITE(&timer->CR2, TIM_CR2_MMS_0);
    }
    NX_STM32F1_WRITE(&timer->CR1, TIM_ONE_PULSE);
    NX_STM32F1_WRITE(&timer->PSC, 7U);
    NX_STM32F1_WRITE(&timer->ARR, 7U);
    nx_arch_dsb();
    if (timer->CR1 != TIM_ONE_PULSE || timer->PSC != 7U || timer->ARR != 7U ||
        (timer != TIM1 && timer->CR2 != TIM_CR2_MMS_0))
        return false;
    NX_STM32F1_WRITE(&timer->EGR, TIM_EGR_UG);
    NX_STM32F1_WRITE(&timer->SR, 0U);
    if (timer != TIM1 &&
        !nx_stm32f1_write_masked(&timer->CR2, TIM_CR2_MMS, TIM_CR2_MMS_1))
        return false;
    nx_arch_dsb();
    return timer_prepared(trigger) && timer->CNT == 0U && timer->SR == 0U;
}

#if defined(NX_STM32F1_ADC_SCAN_DMA)
nx_result_t nx_stm32f1_adc_scan_dma_prepare(
    adc_dma_state_t* state, uint32_t apb2_hz, uint32_t reference_mv,
    uint32_t sample_code, uint32_t second_code,
    const nx_dma_memory_region_t* regions, size_t region_count) {
    const bool tim1 = false;
#else
nx_result_t
nx_stm32f1_adc_trigger_dma_prepare(adc_dma_state_t* state, uint32_t apb2_hz,
                                   uint32_t reference_mv, uint32_t sample_code,
                                   const nx_dma_memory_region_t* regions,
                                   size_t region_count, bool tim1) {
#endif
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || region_count == 0U || region_count > 16U)
        return NX_ERROR_INVALID;
    nx_result_t result = nx_dma_buffer_validate(
        regions, region_count, (const void*)(uintptr_t)&state->sample,
        sizeof(state->sample), _Alignof(uint16_t), NX_DMA_FROM_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32f1_system_live() || (RCC->CFGR & RCC_CFGR_PPRE1) != 0U)
        return NX_ERROR_STATE;
    const trigger_timer_t* trigger = trigger_timer(tim1);
    if (state->effects != 0U || state->adc.effects != 0U ||
        (*trigger->gate & trigger->gate_mask) != 0U ||
        (*trigger->reset & trigger->reset_mask) != 0U ||
        !NX_STM32F1_ADC_TRIGGER_IRQ_IDLE(tim1))
        return NX_ERROR_BUSY;

    state->tim1 = tim1;
    state->effects = TIMER_GATE;
    if (!nx_stm32f1_write_masked(trigger->gate, trigger->gate_mask,
                                 trigger->gate_mask))
        return NX_ERROR_IO;
    if (!timer_idle(trigger))
        return NX_ERROR_BUSY;
    if ((RCC->AHBENR & RCC_AHBENR_DMA1EN) == 0U) {
        state->effects |= DMA_GATE;
        if (!nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN,
                                     RCC_AHBENR_DMA1EN))
            return NX_ERROR_IO;
    }
    if (!lane_idle())
        return NX_ERROR_BUSY;
#if defined(NX_STM32F1_ADC_SCAN_DMA)
    result = nx_stm32f1_adc_prepare_scan(&state->adc, apb2_hz, reference_mv,
                                         sample_code, second_code,
                                         &state->scan_ticks);
#else
    result =
        nx_stm32f1_adc_prepare(&state->adc, apb2_hz, reference_mv, sample_code);
#endif
    if (result != NX_SUCCESS)
        return result;
    state->effects |= TIMER_OWNED;
    if (!timer_prepare(trigger))
        return NX_ERROR_IO;
    state->effects |= LIVE;
    return NX_SUCCESS;
}

/** \brief Disable the event then its source before withdrawing the DMA lane. */
static bool drain(adc_dma_state_t* state, bool complete) {
    const trigger_timer_t* trigger = trigger_timer(state->tim1);
    if (!clocks_live(trigger) ||
        !nx_stm32f1_write_masked(&trigger->registers->CR1, TIM_CR1_CEN, 0U))
        return false;
    const uint32_t idle = complete ? ADC_IDLE : 0U;
    if (READ_REG(ADC1->CR2) != idle)
        WRITE_REG(ADC1->CR2, idle);
    nx_arch_dsb();
    if (READ_REG(ADC1->CR2) != idle ||
        !nx_stm32f1_write_masked(&DMA1_Channel1->CCR, DMA_CCR_EN, 0U))
        return false;

#if defined(NX_STM32F1_ADC_SCAN_DMA)
    const bool clean = !complete || (DMA1->ISR & DMA_ISR_TEIF1) == 0U;
#endif
    /* ES096 §2.4.1: acknowledge the three individual events, never GIF1. */
    NX_STM32F1_WRITE(&DMA1->IFCR, DMA_CLEAR);
    NX_STM32F1_WRITE(&DMA1_Channel1->CCR, 0U);
    NX_STM32F1_WRITE(&DMA1_Channel1->CNDTR, 0U);
    NX_STM32F1_WRITE(&DMA1_Channel1->CPAR, 0U);
    NX_STM32F1_WRITE(&DMA1_Channel1->CMAR, 0U);
    nx_arch_dsb();
    if (!lane_idle())
        return false;
    state->effects &= (uint8_t)~ARMED;
#if defined(NX_STM32F1_ADC_SCAN_DMA)
    return clean;
#else
    return true;
#endif
}

nx_result_t ADC_DMA_CLOSE(adc_dma_state_t* state) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL)
        return NX_ERROR_INVALID;
    state->effects &= (uint8_t)~LIVE;
    if ((state->effects & ARMED) != 0U && !drain(state, false))
        return NX_ERROR_IO;
    const nx_result_t adc = nx_stm32f1_adc_close(&state->adc);
    if (adc != NX_SUCCESS)
        return adc;
    const trigger_timer_t* trigger = trigger_timer(state->tim1);
    if ((state->effects & TIMER_OWNED) != 0U) {
        if (!nx_stm32f1_write_masked(trigger->reset, trigger->reset_mask,
                                     trigger->reset_mask) ||
            !nx_stm32f1_write_masked(trigger->reset, trigger->reset_mask, 0U) ||
            !timer_idle(trigger))
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~TIMER_OWNED;
    }
    if ((state->effects & TIMER_GATE) != 0U) {
        if (!nx_stm32f1_write_masked(trigger->gate, trigger->gate_mask, 0U))
            return NX_ERROR_IO;
        state->effects &= (uint8_t)~TIMER_GATE;
    }
    if ((state->effects & DMA_GATE) != 0U &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_DMA1EN, 0U))
        return NX_ERROR_IO;
    state->effects = 0U;
    return NX_SUCCESS;
}

static const nx_adc_ops_t* selected_ops(void) {
#if defined(NX_STM32F1_ADC_SCAN_DMA)
    return &nx_stm32f1_adc_scan_dma_ops;
#else
    return &nx_stm32f1_adc_trigger_dma_ops;
#endif
}

static bool independent(const adc_dma_state_t* state, const void* ptr,
                        size_t bytes, size_t align) {
    return nx_io_dispatch_span(ptr, bytes, align) &&
           !nx_io_dispatch_overlap(ptr, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(ptr, bytes, (void*)PERIPH_BASE,
                                   UINT32_C(0x20000000)) &&
           !nx_io_dispatch_overlap(ptr, bytes, ADC1, sizeof(*ADC1)) &&
           !nx_io_dispatch_overlap(ptr, bytes, TIM3, sizeof(*TIM3)) &&
           !nx_io_dispatch_overlap(ptr, bytes, TIM1, sizeof(*TIM1)) &&
           !nx_io_dispatch_overlap(ptr, bytes, DMA1_Channel1,
                                   sizeof(*DMA1_Channel1)) &&
           !nx_io_dispatch_overlap(ptr, bytes, selected_ops(),
                                   sizeof(nx_adc_ops_t));
}

static bool arm(adc_dma_state_t* state, const trigger_timer_t* trigger) {
    TIM_TypeDef* timer = trigger->registers;
    const uint32_t armed = trigger->selection | ADC_TRIGGER_FLAGS;
    const uint32_t source = (uint32_t)(uintptr_t)&ADC1->DR;
    const uint32_t destination = (uint32_t)(uintptr_t)&state->sample;
    NX_STM32F1_WRITE(&timer->CNT, 0U);
    NX_STM32F1_WRITE(&timer->SR, 0U);
    WRITE_REG(ADC1->SR, 0U);
    NX_STM32F1_WRITE(&DMA1_Channel1->CPAR, source);
    NX_STM32F1_WRITE(&DMA1_Channel1->CMAR, destination);
    NX_STM32F1_WRITE(&DMA1_Channel1->CNDTR, DMA_RANKS);
    NX_STM32F1_WRITE(&DMA1_Channel1->CCR, DMA_WIDTH);
    nx_arch_dsb();
    if (timer->CNT != 0U || timer->SR != 0U || READ_REG(ADC1->SR) != 0U ||
        DMA1_Channel1->CPAR != source || DMA1_Channel1->CMAR != destination ||
        DMA1_Channel1->CNDTR != DMA_RANKS || DMA1_Channel1->CCR != DMA_WIDTH ||
        !nx_stm32f1_write_masked(&DMA1_Channel1->CCR, DMA_CCR_EN, DMA_CCR_EN))
        return false;
    WRITE_REG(ADC1->CR2, armed);
    nx_arch_dsb();
    if (READ_REG(ADC1->CR2) != armed)
        return false;
    NX_STM32F1_WRITE(&timer->CR1, TIM_ONE_PULSE | TIM_CR1_CEN);
    nx_arch_dsb();
    const uint32_t running = timer->CR1;
    /* An interrupt may delay readback until the one pulse has completed. */
    return (running & ~(uint32_t)TIM_CR1_CEN) == TIM_ONE_PULSE &&
           ((running & TIM_CR1_CEN) != 0U || (timer->SR & TIM_SR_UIF) != 0U);
}

static nx_result_t result_before(const trigger_timer_t* trigger,
                                 nx_time_us_t deadline) {
    TIM_TypeDef* timer = trigger->registers;
    for (unsigned poll = 0U; poll < POLLS; ++poll) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            break;
        const uint32_t flags = DMA1->ISR;
        if ((flags & DMA_ISR_TEIF1) != 0U || !clocks_live(trigger))
            return NX_ERROR_IO;
        if ((flags & DMA_ISR_TCIF1) != 0U) {
            if (DMA1_Channel1->CNDTR != 0U)
                return NX_ERROR_IO;
            /* A short TIM1 conversion can settle before the final update. */
            if ((timer->CR1 & TIM_CR1_CEN) == 0U)
                return (timer->SR & trigger->completion) == trigger->completion
                           ? NX_SUCCESS
                           : NX_ERROR_IO;
        }
    }
    return NX_ERROR_TIMEOUT;
}

static nx_result_t sample(void* context, uint16_t* values, size_t capacity,
                          nx_time_us_t deadline, size_t* count) {
    adc_dma_state_t* state = context;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state == NULL || capacity < DMA_RANKS ||
        capacity > SIZE_MAX / sizeof(*values) ||
        !independent(state, values, capacity * sizeof(*values),
                     _Alignof(uint16_t)) ||
        !independent(state, count, sizeof(*count), _Alignof(size_t)) ||
        nx_io_dispatch_overlap(values, capacity * sizeof(*values), count,
                               sizeof(*count)))
        return NX_ERROR_INVALID;
    *count = 0U;
    if ((state->effects & FAULTED) != 0U)
        return NX_ERROR_STATE;
    if ((state->effects & LIVE) == 0U)
        return NX_ERROR_STATE;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_UNSUPPORTED;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    const trigger_timer_t* trigger = trigger_timer(state->tim1);
    if (!clocks_live(trigger) || !lane_idle() ||
        !NX_STM32F1_ADC_TRIGGER_IRQ_IDLE(state->tim1) ||
        READ_REG(ADC1->CR2) != ADC_IDLE || !timer_prepared(trigger))
        return NX_ERROR_IO;

#if defined(NX_STM32F1_ADC_SCAN_DMA)
    if (ADC1->CR1 != ADC_CR1_SCAN || ADC1->SQR1 != ADC_SQR1_L_0 ||
        ADC1->SQR3 != (1U << ADC_SQR3_SQ2_Pos))
        return NX_ERROR_IO;
#endif
    state->effects |= ARMED;
    nx_result_t result =
        arm(state, trigger) ? result_before(trigger, deadline) : NX_ERROR_IO;
    if (!drain(state, result == NX_SUCCESS))
        result = NX_ERROR_IO;
    if (result == NX_SUCCESS) {
#if defined(NX_STM32F1_ADC_SCAN_DMA)
        const uint16_t captured[] = {state->sample[0], state->sample[1]};
        const bool valid = captured[0] <= 0xfffU && captured[1] <= 0xfffU;
#else
        const uint16_t captured[] = {state->sample};
        const bool valid = captured[0] <= 0xfffU;
#endif
        if (valid && !nx_deadline_expired(deadline, nx_time_now_us())) {
            for (size_t rank = 0U; rank < DMA_RANKS; ++rank)
                values[rank] = captured[rank];
            *count = DMA_RANKS;
            return NX_SUCCESS;
        }
        result = valid ? NX_ERROR_TIMEOUT : NX_ERROR_IO;
    }
    state->effects |= FAULTED;
    return result;
}

static nx_result_t info(const void* context, nx_adc_info_t* out) {
    const adc_dma_state_t* state = context;
    if (state == NULL ||
        !independent(state, out, sizeof(*out), _Alignof(nx_adc_info_t)))
        return NX_ERROR_INVALID;
    const nx_result_t result =
        nx_stm32f1_adc_info(&state->adc, out, selected_ops());
    if (DMA_RANKS > 1U && result == NX_SUCCESS)
        out->channel_count = DMA_RANKS;
    return result;
}

static nx_result_t sequence_info(const void* context,
                                 nx_adc_sequence_info_t* out) {
    const adc_dma_state_t* state = context;
    if (state == NULL || !independent(state, out, sizeof(*out),
                                      _Alignof(nx_adc_sequence_info_t)))
        return NX_ERROR_INVALID;
    const nx_result_t result =
        nx_stm32f1_adc_sequence_info(&state->adc, out, selected_ops());
#if defined(NX_STM32F1_ADC_SCAN_DMA)
    if (result == NX_SUCCESS) {
        out->channel_count = DMA_RANKS;
        out->scan_ticks = state->scan_ticks;
    }
#endif
    return result;
}

#if defined(NX_STM32F1_ADC_SCAN_DMA)
const nx_adc_ops_t nx_stm32f1_adc_scan_dma_ops = {
    .info = info, .sample = sample, .sequence_info = sequence_info};
#else
const nx_adc_ops_t nx_stm32f1_adc_trigger_dma_ops = {
    .info = info, .sample = sample, .sequence_info = sequence_info};
#endif
