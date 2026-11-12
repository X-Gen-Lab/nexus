/**
 * \file            dac_dma.c
 * \brief           F407 DAC DMA owner with bounded, joined withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_dac_dma.h"
#include "stm32f407_provider.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_dac.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_tim.h"
#include <string.h>

enum { IDLE, ACTIVE, TAIL, DRAIN };
enum { COUNTED = 1U, FAILED = 2U, RECOVERING = 4U };
#define DRAIN_US 200U
#define TIMER_HZ NEXUS_APB1_TIMER_HZ

/** \brief These controls belong to one privileged, unmasked task executor. */
static bool context_ok(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief First-member composition keeps one immutable cold receipt. */
static const nx_stm32_dac_dma_config_t*
config_of(const nx_stm32_dac_dma_state_t* state) {
    return (const nx_stm32_dac_dma_config_t*)state->dac.config;
}
/** \brief Dual writes use the channel-one request, never two DMA streams. */
static uint32_t channel(const nx_stm32_dac_dma_state_t* state) {
    return state->dac.config->channels == 2U ? LL_DAC_CHANNEL_2
                                             : LL_DAC_CHANNEL_1;
}
/** \brief RM0090 fixes selector seven to stream five or six. */
static uint32_t stream_id(const nx_stm32_dac_dma_state_t* state) {
    return state->dac.config->channels == 2U ? LL_DMA_STREAM_6
                                             : LL_DMA_STREAM_5;
}
/** \brief Direct observations accompany LL writes to the selected stream. */
static DMA_Stream_TypeDef* stream(const nx_stm32_dac_dma_state_t* state) {
    return state->dac.config->channels == 2U ? DMA1_Stream6 : DMA1_Stream5;
}
/** \brief The second publisher is TIM6_DAC even when TIM7 supplies TRGO. */
static IRQn_Type vector(const nx_stm32_dac_dma_state_t* state, unsigned index) {
    return index != 0U                         ? TIM6_DAC_IRQn
           : state->dac.config->channels == 2U ? DMA1_Stream6_IRQn
                                               : DMA1_Stream5_IRQn;
}
/** \brief The selected basic timer has no shared timebase or IRQ client. */
static TIM_TypeDef* timer(const nx_stm32_dac_dma_state_t* state) {
    nx_stm32_dac_trigger_t source = state->dac.config->trigger;
    if (source == NX_STM32_DAC_TRIGGER_EXTI9)
        source = config_of(state)->recovery_trigger;
    return source == NX_STM32_DAC_TRIGGER_TIM6 ? TIM6 : TIM7;
}
/** \brief Stream flags occupy one independent W1C field in HISR/HIFCR. */
static unsigned flag_shift(const nx_stm32_dac_dma_state_t* state) {
    return state->dac.config->channels == 2U ? 16U : 6U;
}
/** \brief One W1C store atomically clears only this owner's stream flags. */
static void clear_flags(const nx_stm32_dac_dma_state_t* state) {
    WRITE_REG(DMA1->HIFCR, 0x3dU << flag_shift(state));
}
/** \brief DAC underrun is acknowledged only for the leased request channel. */
static void clear_underrun(const nx_stm32_dac_dma_state_t* state) {
    if (state->dac.config->channels == 2U) {
        LL_DAC_ClearFlag_DMAUDR2(DAC);
    } else {
        LL_DAC_ClearFlag_DMAUDR1(DAC);
    }
}
/** \brief Neither a zero count nor TC can override a DAC health error. */
static bool underrun(const nx_stm32_dac_dma_state_t* state) {
    return state->dac.config->channels == 2U
               ? LL_DAC_IsActiveFlag_DMAUDR2(DAC) != 0U
               : LL_DAC_IsActiveFlag_DMAUDR1(DAC) != 0U;
}
/** \brief Freeze request generation before draining the memory engine. */
static void gate(const nx_stm32_dac_dma_state_t* state) {
    LL_TIM_DisableCounter(timer(state));
    if ((state->dac.config->channels & 1U) != 0U) {
        LL_DAC_DisableTrigger(DAC, LL_DAC_CHANNEL_1);
    }
    if ((state->dac.config->channels & 2U) != 0U) {
        LL_DAC_DisableTrigger(DAC, LL_DAC_CHANNEL_2);
    }
    LL_DAC_DisableDMAReq(DAC, channel(state));
    nx_arch_dsb();
}
/** \brief Error precedence and the drain timer survive repeated cancellation.
 */
static void begin_drain(nx_stm32_dac_dma_state_t* state, nx_result_t result,
                        nx_time_us_t now) {
    if (result == NX_ERROR_IO) {
        state->events |= FAILED;
        state->dac.faulted = true;
    }
    if (state->phase != DRAIN) {
        state->terminal = result;
        if (nx_deadline_finite_after(now, DRAIN_US, &state->drain_deadline) !=
            NX_SUCCESS) {
            state->drain_deadline = now == NX_DEADLINE_NEVER ? now - 1U : now;
        }
        state->phase = DRAIN;
    }
    if ((state->events & FAILED) != 0U) {
        state->terminal = NX_ERROR_IO;
    }
    gate(state);
}
/** \brief Temporary gate withdrawal also retains inherited clock ownership.
 */
static bool restore_clock(nx_stm32_dac_dma_state_t* state) {
    if (!state->clock_restore_pending) {
        return true;
    }
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);
    nx_arch_dsb();
    if (LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_DAC1) == 0U) {
        return false;
    }
    state->clock_restore_pending = false;
    return true;
}
/** \brief No payload or descriptor is inspected by the IRQ latch. */
static void latch(nx_stm32_dac_dma_state_t* state, nx_time_us_t now) {
    uint32_t flags = (DMA1->HISR >> flag_shift(state)) & 0x3dU;
    if ((flags & 0x0dU) != 0U ||
        (underrun(state) && (state->events & RECOVERING) == 0U)) {
        begin_drain(state, NX_ERROR_IO, now);
    } else if ((flags & 0x20U) != 0U && state->phase == ACTIVE &&
               !config_of(state)->circular) {
        if (stream(state)->NDTR != 0U) {
            begin_drain(state, NX_ERROR_IO, now);
        } else {
            LL_DAC_DisableDMAReq(DAC, channel(state));
            /* Pause the exclusive timer: a fresh update cannot be mistaken
             * for a refused flag clear. Finite tails may insert an interval. */
            LL_TIM_DisableCounter(timer(state));
            nx_arch_dsb();
            bool stopped = LL_TIM_IsEnabledCounter(timer(state)) == 0U;
            if (stopped) {
                LL_TIM_SetCounter(timer(state), 0U);
                LL_TIM_ClearFlag_UPDATE(timer(state));
            }
            nx_arch_dsb();
            uint64_t period = ((uint64_t)state->dac.trigger_divisor * 1000000U +
                               TIMER_HZ - 1U) /
                                  TIMER_HZ +
                              1U;
            if (!stopped || LL_DAC_IsDMAReqEnabled(DAC, channel(state)) != 0U ||
                LL_TIM_IsActiveFlag_UPDATE(timer(state)) != 0U ||
                LL_TIM_GetCounter(timer(state)) != 0U) {
                begin_drain(state, NX_ERROR_IO, now);
            } else {
                LL_TIM_EnableCounter(timer(state));
                nx_arch_dsb();
                if (LL_TIM_IsEnabledCounter(timer(state)) == 0U ||
                    nx_deadline_finite_after(nx_time_now_us(), period,
                                             &state->tail_deadline) !=
                        NX_SUCCESS) {
                    begin_drain(state, NX_ERROR_IO, now);
                } else {
                    state->phase = TAIL;
                }
            }
        }
    }
    clear_flags(state);
    clear_underrun(state);
    if (state->phase != ACTIVE) {
        /* Mask latched terminal publishers even if a source refuses W1C.
         * The serialized task must remain able to progress withdrawal. */
        NVIC_DisableIRQ(vector(state, 0U));
        NVIC_DisableIRQ(vector(state, 1U));
    }
}
/** \brief IRQ writes finite scalar facts and then publishes an optional hint.
 */
void nx_stm32_dac_dma_irq(nx_stm32_dac_dma_state_t* state) {
    if (state == NULL || !state->initialized) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (state->phase != IDLE) {
        latch(state, nx_time_now_us());
    } else {
        clear_flags(state);
        clear_underrun(state);
    }
    const nx_irq_wake_t* wake = state->wake;
    nx_arch_irq_restore(saved);
    (void)nx_irq_wake_signal(wake);
}
/** \brief Foreign enabled, pending or executing publishers are never adopted.
 */
static bool vector_busy(IRQn_Type irq) {
    return NVIC_GetEnableIRQ(irq) != 0U || NVIC_GetPendingIRQ(irq) != 0U ||
           NVIC_GetActive(irq) != 0U;
}
/** \brief Acquire exact resources, recording responsibility before each store.
 */
nx_result_t nx_stm32_dac_dma_prepare(nx_stm32_dac_dma_state_t* state,
                                     const nx_stm32_dac_dma_config_t* config) {
    if (state == NULL || config == NULL || config->regions == NULL ||
        config->region_count == 0U ||
        config->region_count > SIZE_MAX / sizeof(*config->regions) ||
        config->priority > 15U || config->dac.mode != NX_STM32_DAC_TRIGGERED ||
        (config->dac.trigger != NX_STM32_DAC_TRIGGER_TIM6 &&
         config->dac.trigger != NX_STM32_DAC_TRIGGER_TIM7 &&
         config->dac.trigger != NX_STM32_DAC_TRIGGER_EXTI9)) {
        return NX_ERROR_INVALID;
    }
    bool external = config->dac.trigger == NX_STM32_DAC_TRIGGER_EXTI9;
    if (external && !config->circular)
        return NX_ERROR_UNSUPPORTED;
    if ((external && config->recovery_trigger != NX_STM32_DAC_TRIGGER_TIM6 &&
         config->recovery_trigger != NX_STM32_DAC_TRIGGER_TIM7) ||
        (!external && config->recovery_trigger != NX_STM32_DAC_TRIGGER_NONE))
        return NX_ERROR_INVALID;
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->dac.config != NULL || state->initialized || state->clock_added ||
        state->vector_leases != 0U || state->clock_restore_pending) {
        return NX_ERROR_BUSY;
    }
    TIM_TypeDef* recovery =
        config->recovery_trigger == NX_STM32_DAC_TRIGGER_TIM6 ? TIM6 : TIM7;
    if (external &&
        ((recovery->CR1 & TIM_CR1_CEN) != 0U || recovery->DIER != 0U))
        return NX_ERROR_BUSY;
    IRQn_Type irq =
        config->dac.channels == 2U ? DMA1_Stream6_IRQn : DMA1_Stream5_IRQn;
    DMA_Stream_TypeDef* selected =
        config->dac.channels == 2U ? DMA1_Stream6 : DMA1_Stream5;
    if (vector_busy(irq) || vector_busy(TIM6_DAC_IRQn) ||
        (selected->CR & DMA_SxCR_EN) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_stm32_dac_prepare(&state->dac, &config->dac);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (external) {
        uint32_t bit = config->recovery_trigger == NX_STM32_DAC_TRIGGER_TIM6
                           ? (uint32_t)RCC_APB1ENR_TIM6EN
                           : (uint32_t)RCC_APB1ENR_TIM7EN;
        /* One existing cold receipt owns this recovery-only gate too. No
         * event source or counter is started by taking the static lease. */
        state->dac.clocks_added |= bit & ~RCC->APB1ENR;
        LL_APB1_GRP1_EnableClock(bit);
        nx_arch_dsb();
        if ((RCC->APB1ENR & bit) == 0U)
            return NX_ERROR_IO;
        if ((recovery->CR1 & TIM_CR1_CEN) != 0U || recovery->DIER != 0U)
            return NX_ERROR_BUSY;
    }
    state->frame_bytes =
        (uint8_t)((config->dac.format == NX_DAC_FORMAT_U8 ? 1U : 2U) *
                  (config->dac.channels == 3U ? 2U : 1U));
    state->clock_added =
        LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_DMA1) == 0U;
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    nx_arch_dsb();
    if (LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_DMA1) == 0U) {
        return NX_ERROR_IO;
    }
    if ((selected->CR & DMA_SxCR_EN) != 0U) {
        return NX_ERROR_BUSY;
    }
    state->route_owned = true;
    clear_flags(state);
    clear_underrun(state);
    for (unsigned index = 0U; index < 2U; ++index) {
        irq = vector(state, index);
        state->saved_priorities[index] = (uint8_t)NVIC_GetPriority(irq);
        state->vector_leases |= (uint8_t)(1U << index);
        NVIC_SetPriority(irq, config->priority);
        NVIC_EnableIRQ(irq);
        nx_arch_dsb();
        if (NVIC_GetPriority(irq) != config->priority ||
            NVIC_GetEnableIRQ(irq) == 0U) {
            return NX_ERROR_IO;
        }
    }
    state->initialized = true;
    return NX_SUCCESS;
}

/** \brief Metadata cannot replace the controller or its immutable settings. */
static bool independent(const nx_stm32_dac_dma_state_t* state, const void* data,
                        size_t size, size_t alignment) {
    uintptr_t begin = (uintptr_t)data;
    if (!nx_stm32_dac_independent(&state->dac, sizeof(*state), data, size,
                                  alignment)) {
        return false;
    }
    const nx_stm32_dac_dma_config_t* config = config_of(state);
    const void* objects[] = {
        config, config->regions, state->wake, state->active,
        state->active != NULL ? state->active->data : NULL};
    size_t sizes[] = {sizeof(*config),
                      config->region_count * sizeof(*config->regions),
                      state->wake != NULL ? sizeof(*state->wake) : 0U,
                      state->active != NULL ? sizeof(*state->active) : 0U,
                      state->active != NULL ? state->active->length : 0U};
    for (unsigned index = 0U; index < 5U; ++index) {
        if (sizes[index] != 0U &&
            begin < (uintptr_t)objects[index] + sizes[index] &&
            (uintptr_t)objects[index] < begin + size) {
            return false;
        }
    }
    return true;
}
/** \brief The supplier exposes single-channel addresses but no dual getter. */
static uint32_t destination(const nx_stm32_dac_dma_state_t* state) {
    const nx_stm32_dac_config_t* config = state->dac.config;
    if (config->channels == 3U) {
        return (uint32_t)(uintptr_t)(config->format == NX_DAC_FORMAT_U8
                                         ? &DAC->DHR8RD
                                     : config->format == NX_DAC_FORMAT_U12_LEFT
                                         ? &DAC->DHR12LD
                                         : &DAC->DHR12RD);
    }
    return LL_DAC_DMA_GetRegAddr(
        DAC, channel(state),
        config->format == NX_DAC_FORMAT_U8
            ? LL_DAC_DMA_REG_DATA_8BITS_RIGHT_ALIGNED
        : config->format == NX_DAC_FORMAT_U12_LEFT
            ? LL_DAC_DMA_REG_DATA_12BITS_LEFT_ALIGNED
            : LL_DAC_DMA_REG_DATA_12BITS_RIGHT_ALIGNED);
}
/** \brief External playback still needs real internal events for ES0182.
 * \note No memory address is attached here. TSEL changes only with EN clear;
 *       the exclusively leased recovery timer stays stopped during playback.
 */
static nx_result_t recovery_source(nx_stm32_dac_dma_state_t* state) {
    if (state->dac.config->trigger != NX_STM32_DAC_TRIGGER_EXTI9)
        return NX_SUCCESS;
    const uint8_t channels = state->dac.config->channels;
    LL_DAC_Disable(DAC, LL_DAC_CHANNEL_1);
    LL_DAC_Disable(DAC, LL_DAC_CHANNEL_2);
    nx_arch_dsb();
    if (LL_DAC_IsEnabled(DAC, LL_DAC_CHANNEL_1) != 0U ||
        LL_DAC_IsEnabled(DAC, LL_DAC_CHANNEL_2) != 0U)
        return NX_ERROR_IO;
    uint32_t source = timer(state) == TIM6 ? LL_DAC_TRIG_EXT_TIM6_TRGO
                                           : LL_DAC_TRIG_EXT_TIM7_TRGO;
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((channels & (1U << index)) == 0U)
            continue;
        uint32_t selected = index == 0U ? LL_DAC_CHANNEL_1 : LL_DAC_CHANNEL_2;
        LL_DAC_SetTriggerSource(DAC, selected, source);
        LL_DAC_EnableTrigger(DAC, selected);
        LL_DAC_Enable(DAC, selected);
    }
    nx_arch_dsb();
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((channels & (1U << index)) == 0U)
            continue;
        uint32_t selected = index == 0U ? LL_DAC_CHANNEL_1 : LL_DAC_CHANNEL_2;
        if (LL_DAC_GetTriggerSource(DAC, selected) != source ||
            LL_DAC_IsTriggerEnabled(DAC, selected) == 0U ||
            LL_DAC_IsEnabled(DAC, selected) == 0U)
            return NX_ERROR_IO;
    }
    return nx_stm32_dac_wait_startup(state->active->base.deadline);
}
/** \brief ES0182 2.6.1 requires observed underrun and a complete restart.
 * \note DMAEN, a clock cycle or W1C alone never proves an empty request latch.
 *       An idle, exclusively owned timer creates real TRGO events while every
 *       memory target is detached. Expected UDR cannot publish an IRQ failure.
 *       The following ordinary arm/start reconfigures DAC, DMA and triggers.
 */
static nx_result_t restart(nx_stm32_dac_dma_state_t* state) {
    uint32_t selected = stream_id(state);
    if (underrun(state)) {
        return NX_ERROR_IO;
    }
    LL_DAC_DisableIT_DMAUDR1(DAC);
    LL_DAC_DisableIT_DMAUDR2(DAC);
    LL_DMA_DisableIT_TC(DMA1, selected);
    LL_DMA_DisableIT_HT(DMA1, selected);
    LL_DMA_DisableIT_TE(DMA1, selected);
    LL_DMA_DisableIT_DME(DMA1, selected);
    LL_DMA_DisableIT_FE(DMA1, selected);
    for (unsigned index = 0U; index < 2U; ++index) {
        NVIC_DisableIRQ(vector(state, index));
        NVIC_ClearPendingIRQ(vector(state, index));
    }
    nx_arch_dsb();
    if (LL_DMA_IsEnabledStream(DMA1, selected) != 0U ||
        LL_TIM_IsEnabledCounter(timer(state)) != 0U) {
        return NX_ERROR_IO;
    }
    for (unsigned index = 0U; index < 2U; ++index) {
        if (vector_busy(vector(state, index))) {
            return NX_ERROR_IO;
        }
    }
    LL_DMA_SetMemoryAddress(DMA1, selected, 0U);
    LL_DMA_SetMemory1Address(DMA1, selected, 0U);
    LL_DMA_SetPeriphAddress(DMA1, selected, 0U);
    LL_DMA_SetDataLength(DMA1, selected, 0U);
    nx_arch_dsb();
    if (stream(state)->M0AR != 0U || stream(state)->M1AR != 0U ||
        stream(state)->PAR != 0U || stream(state)->NDTR != 0U) {
        return NX_ERROR_IO;
    }
    /* UG is a peripheral event; DAC_SWTRIGR does not generate DMA requests.
     * Three APB cycles plus a quantization tick allow the digital conversion
     * to progress. Success still requires observing actual UDR, not elapsed
     * time alone or waiting for the request's potentially 1 Hz waveform. */
    const uint64_t spacing =
        (UINT64_C(3000000) + NEXUS_APB1_HZ - 1U) / NEXUS_APB1_HZ + 1U;
    LL_TIM_SetTriggerOutput(timer(state), LL_TIM_TRGO_RESET);
    nx_result_t ready = recovery_source(state);
    if (ready != NX_SUCCESS)
        return ready;
    LL_DAC_EnableDMAReq(DAC, channel(state));
    nx_arch_dsb();
    if (LL_DAC_IsDMAReqEnabled(DAC, channel(state)) == 0U ||
        READ_BIT(timer(state)->CR2, TIM_CR2_MMS) != LL_TIM_TRGO_RESET) {
        return NX_ERROR_IO;
    }
    for (unsigned pulse = 0U; pulse < 2U; ++pulse) {
        nx_time_us_t before = nx_time_now_us();
        if (before == NX_DEADLINE_NEVER)
            return NX_ERROR_STATE;
        if (nx_deadline_expired(state->active->base.deadline, before)) {
            return NX_ERROR_TIMEOUT;
        }
        /* This source can only convert idle: every memory/IRQ target is off.
         * Retain its UDR attribution even when the accepted deadline expires
         * before the next observation; failed W1C remains a hardware fault. */
        state->events |= RECOVERING;
        LL_TIM_GenerateEvent_UPDATE(timer(state));
        nx_arch_dsb();
        nx_time_us_t began = nx_time_now_us();
        nx_time_us_t after;
        if (began == NX_DEADLINE_NEVER)
            return NX_ERROR_STATE;
        if (nx_deadline_finite_after(began, spacing, &after) != NX_SUCCESS) {
            return NX_ERROR_EXHAUSTED;
        }
        nx_time_us_t now;
        do {
            now = nx_time_now_us();
            if (now == NX_DEADLINE_NEVER || now < began)
                return NX_ERROR_STATE;
            if (nx_deadline_expired(state->active->base.deadline, now)) {
                return NX_ERROR_TIMEOUT;
            }
        } while (now < after);
        if (underrun(state)) {
            break;
        }
    }
    if (!underrun(state)) {
        return NX_ERROR_IO;
    }
    gate(state);
    LL_DAC_Disable(DAC, LL_DAC_CHANNEL_1);
    LL_DAC_Disable(DAC, LL_DAC_CHANNEL_2);
    nx_arch_dsb();
    if (LL_DAC_IsDMAReqEnabled(DAC, channel(state)) != 0U ||
        LL_DAC_IsEnabled(DAC, LL_DAC_CHANNEL_1) != 0U ||
        LL_DAC_IsEnabled(DAC, LL_DAC_CHANNEL_2) != 0U ||
        LL_DAC_IsTriggerEnabled(DAC, channel(state)) != 0U) {
        return NX_ERROR_IO;
    }
    state->clock_restore_pending = true;
    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_DAC1);
    nx_arch_dsb();
    if (LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_DAC1) != 0U ||
        !restore_clock(state)) {
        return NX_ERROR_IO;
    }
    clear_underrun(state);
    nx_arch_dsb();
    if (underrun(state)) {
        return NX_ERROR_IO;
    }
    state->events &= ~(uint32_t)RECOVERING;
    state->dac.running = false;
    return nx_stm32_dac_arm(&state->dac, state->active->trigger_hz,
                            state->active->base.deadline, false);
}
/** \brief Direct mode has equal memory/peripheral widths and no FIFO loan. */
static nx_result_t start_dma(nx_stm32_dac_dma_state_t* state) {
    uint32_t selected = stream_id(state);
    uint32_t width = state->frame_bytes == 1U   ? LL_DMA_MDATAALIGN_BYTE
                     : state->frame_bytes == 2U ? LL_DMA_MDATAALIGN_HALFWORD
                                                : LL_DMA_MDATAALIGN_WORD;
    LL_DMA_DisableIT_TC(DMA1, selected);
    LL_DMA_DisableIT_HT(DMA1, selected);
    LL_DMA_DisableIT_TE(DMA1, selected);
    LL_DMA_DisableIT_DME(DMA1, selected);
    LL_DMA_DisableIT_FE(DMA1, selected);
    LL_DMA_DisableDoubleBufferMode(DMA1, selected);
    LL_DMA_DisableFifoMode(DMA1, selected);
    LL_DMA_SetMemoryBurstxfer(DMA1, selected, LL_DMA_MBURST_SINGLE);
    LL_DMA_SetPeriphBurstxfer(DMA1, selected, LL_DMA_PBURST_SINGLE);
    LL_DMA_SetChannelSelection(DMA1, selected, LL_DMA_CHANNEL_7);
    LL_DMA_ConfigTransfer(
        DMA1, selected,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_MEMORY_INCREMENT |
            LL_DMA_PERIPH_NOINCREMENT | LL_DMA_PRIORITY_HIGH | width |
            (width >> 2U) |
            (config_of(state)->circular ? LL_DMA_MODE_CIRCULAR
                                        : LL_DMA_MODE_NORMAL));
    LL_DMA_SetMemoryAddress(DMA1, selected,
                            (uint32_t)(uintptr_t)state->active->data);
    LL_DMA_SetMemory1Address(DMA1, selected, 0U);
    LL_DMA_SetPeriphAddress(DMA1, selected, destination(state));
    LL_DMA_SetDataLength(
        DMA1, selected, (uint32_t)(state->active->length / state->frame_bytes));
    if (stream(state)->NDTR != state->active->length / state->frame_bytes) {
        return NX_ERROR_IO;
    }
    state->events &= ~(uint32_t)COUNTED;
    clear_flags(state);
    clear_underrun(state);
    LL_DMA_EnableIT_TE(DMA1, selected);
    LL_DMA_EnableIT_DME(DMA1, selected);
    LL_DMA_EnableIT_FE(DMA1, selected);
    if (!config_of(state)->circular) {
        LL_DMA_EnableIT_TC(DMA1, selected);
    }
    if (state->dac.config->channels == 2U) {
        LL_DAC_EnableIT_DMAUDR2(DAC);
    } else {
        LL_DAC_EnableIT_DMAUDR1(DAC);
    }
    for (unsigned index = 0U; index < 2U; ++index) {
        NVIC_ClearPendingIRQ(vector(state, index));
        NVIC_EnableIRQ(vector(state, index));
    }
    LL_DMA_EnableStream(DMA1, selected);
    nx_arch_dsb();
    if (LL_DMA_IsEnabledStream(DMA1, selected) == 0U ||
        stream(state)->M0AR != (uint32_t)(uintptr_t)state->active->data ||
        stream(state)->PAR != destination(state) ||
        stream(state)->NDTR != state->active->length / state->frame_bytes) {
        return NX_ERROR_IO;
    }
    LL_DAC_EnableDMAReq(DAC, channel(state));
    nx_arch_dsb();
    if (LL_DAC_IsDMAReqEnabled(DAC, channel(state)) == 0U) {
        return NX_ERROR_IO;
    }
    nx_time_us_t now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    if (nx_deadline_expired(state->active->base.deadline, now)) {
        return NX_ERROR_TIMEOUT;
    }
    if (state->dac.config->trigger == NX_STM32_DAC_TRIGGER_EXTI9)
        return nx_stm32_dac_enable_external(&state->dac,
                                            state->active->base.deadline);
    LL_TIM_EnableCounter(timer(state));
    nx_arch_dsb();
    if (LL_TIM_IsEnabledCounter(timer(state)) == 0U) {
        return NX_ERROR_IO;
    }
    now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    return nx_deadline_expired(state->active->base.deadline, now)
               ? NX_ERROR_TIMEOUT
               : NX_SUCCESS;
}
/** \brief Validate all geometry before admission, payload access or MMIO. */
static nx_result_t submit(void* context, nx_dac_tx_request_t* request) {
    nx_stm32_dac_dma_state_t* state = context;
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->closing || state->dac.faulted) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    bool external = state->dac.config->trigger == NX_STM32_DAC_TRIGGER_EXTI9;
    if (!nx_stm32_dac_external_live(state->dac.config))
        return NX_ERROR_STATE;
    if (!independent(state, request, sizeof(*request),
                     _Alignof(nx_dac_tx_request_t)) ||
        !independent(state, request->data, request->length,
                     state->frame_bytes) ||
        request->length == 0U || request->length % state->frame_bytes != 0U ||
        request->length / state->frame_bytes > 65535U ||
        (external
             ? request->trigger_hz != 0U
             : request->trigger_hz == 0U || request->trigger_hz > 1000000U) ||
        (uintptr_t)request->data > UINT32_MAX ||
        request->length > UINT32_MAX - (uintptr_t)request->data) {
        return NX_ERROR_INVALID;
    }
    if ((uintptr_t)request < (uintptr_t)request->data + request->length &&
        (uintptr_t)request->data < (uintptr_t)request + sizeof(*request)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = nx_dma_buffer_validate(
        config_of(state)->regions, config_of(state)->region_count,
        request->data, request->length, state->frame_bytes, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_time_us_t now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    if (nx_deadline_expired(request->base.deadline, now)) {
        return NX_ERROR_TIMEOUT;
    }
    if (LL_DMA_IsEnabledStream(DMA1, stream_id(state)) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        state->active = request;
        state->phase = ACTIVE;
        state->events = COUNTED;
        state->last_frame = 0U;
        state->transferred = 0U;
        state->terminal = NX_SUCCESS;
    }
    nx_arch_irq_restore(saved);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!config_of(state)->circular) {
        memcpy(&state->last_frame,
               (const uint8_t*)request->data + request->length -
                   state->frame_bytes,
               state->frame_bytes);
    }
    result = nx_stm32_dac_stop(&state->dac, request->base.deadline);
    if (result == NX_SUCCESS) {
        /* TSEL may be changed only with EN clear, including recovery. */
        LL_DAC_Disable(DAC, LL_DAC_CHANNEL_1);
        LL_DAC_Disable(DAC, LL_DAC_CHANNEL_2);
        nx_arch_dsb();
        result = LL_DAC_IsEnabled(DAC, LL_DAC_CHANNEL_1) != 0U ||
                         LL_DAC_IsEnabled(DAC, LL_DAC_CHANNEL_2) != 0U
                     ? NX_ERROR_IO
                     : nx_stm32_dac_arm(&state->dac, request->trigger_hz,
                                        request->base.deadline, false);
    }
    if (result == NX_SUCCESS) {
        result = restart(state);
    }
    saved = nx_arch_irq_save();
    if (result == NX_SUCCESS && state->phase == ACTIVE) {
        result = start_dma(state);
    }
    if (result != NX_SUCCESS) {
        begin_drain(state, result == NX_ERROR_TIMEOUT ? result : NX_ERROR_IO,
                    nx_time_now_us());
    }
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief DOR observations use twelve-bit units regardless of frame encoding.
 */
static bool last_latched(const nx_stm32_dac_dma_state_t* state) {
    uint32_t value = state->last_frame;
    unsigned lane = state->dac.config->format == NX_DAC_FORMAT_U8 ? 8U : 16U;
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((state->dac.config->channels & (1U << index)) == 0U) {
            continue;
        }
        uint32_t code = value & (lane == 8U ? 0xffU : 0xffffU);
        if (state->dac.config->format == NX_DAC_FORMAT_U8) {
            code <<= 4U;
        } else if (state->dac.config->format == NX_DAC_FORMAT_U12_LEFT) {
            code >>= 4U;
        }
        if (LL_DAC_RetrieveOutputData(DAC, index == 0U
                                               ? LL_DAC_CHANNEL_1
                                               : LL_DAC_CHANNEL_2) != code) {
            return false;
        }
        value >>= lane;
    }
    return true;
}
/** \brief Stable EN/count and cleared hardware addresses precede publication.
 */
static bool withdraw(nx_stm32_dac_dma_state_t* state) {
    if (!restore_clock(state)) {
        return false;
    }
    uint32_t selected = stream_id(state);
    gate(state);
    LL_DMA_DisableIT_TC(DMA1, selected);
    LL_DMA_DisableIT_HT(DMA1, selected);
    LL_DMA_DisableIT_TE(DMA1, selected);
    LL_DMA_DisableIT_DME(DMA1, selected);
    LL_DMA_DisableIT_FE(DMA1, selected);
    if (state->dac.config->channels == 2U) {
        LL_DAC_DisableIT_DMAUDR2(DAC);
    } else {
        LL_DAC_DisableIT_DMAUDR1(DAC);
    }
    LL_DMA_DisableStream(DMA1, selected);
    nx_arch_dsb();
    uint32_t remaining = LL_DMA_GetDataLength(DMA1, selected);
    uint32_t triggers =
        ((state->dac.config->channels & 1U) != 0U ? (uint32_t)DAC_CR_TEN1
                                                  : 0U) |
        ((state->dac.config->channels & 2U) != 0U ? (uint32_t)DAC_CR_TEN2 : 0U);
    uint32_t dac_irq = state->dac.config->channels == 2U
                           ? (uint32_t)DAC_CR_DMAUDRIE2
                           : (uint32_t)DAC_CR_DMAUDRIE1;
    if (LL_DMA_IsEnabledStream(DMA1, selected) != 0U ||
        remaining != LL_DMA_GetDataLength(DMA1, selected) ||
        LL_DAC_IsDMAReqEnabled(DAC, channel(state)) != 0U ||
        LL_TIM_IsEnabledCounter(timer(state)) != 0U ||
        (DAC->CR & (triggers | dac_irq)) != 0U ||
        (stream(state)->CR & (DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
                              DMA_SxCR_DMEIE)) != 0U ||
        (stream(state)->FCR & DMA_SxFCR_FEIE) != 0U) {
        return false;
    }
    if ((state->events & COUNTED) == 0U) {
        size_t count = state->active->length / state->frame_bytes;
        if (remaining > count) {
            state->terminal = NX_ERROR_IO;
            state->events |= FAILED;
            state->transferred = 0U;
        } else {
            state->transferred = (count - remaining) * state->frame_bytes;
        }
        state->events |= COUNTED;
    }
    LL_DMA_SetMemoryAddress(DMA1, selected, 0U);
    LL_DMA_SetMemory1Address(DMA1, selected, 0U);
    LL_DMA_SetDataLength(DMA1, selected, 0U);
    for (unsigned index = 0U; index < 2U; ++index) {
        NVIC_DisableIRQ(vector(state, index));
        NVIC_ClearPendingIRQ(vector(state, index));
    }
    if ((underrun(state) && (state->events & RECOVERING) == 0U) ||
        ((DMA1->HISR >> flag_shift(state)) & 0x0dU) != 0U) {
        state->terminal = NX_ERROR_IO;
        state->events |= FAILED;
    }
    clear_flags(state);
    clear_underrun(state);
    nx_arch_dsb();
    if (underrun(state)) {
        state->terminal = NX_ERROR_IO;
        state->events |= FAILED;
        state->dac.faulted = true;
        return false;
    }
    if (stream(state)->M0AR != 0U || stream(state)->M1AR != 0U ||
        ((DMA1->HISR >> flag_shift(state)) & 0x3dU) != 0U) {
        return false;
    }
    for (unsigned index = 0U; index < 2U; ++index) {
        IRQn_Type irq = vector(state, index);
        if (NVIC_GetEnableIRQ(irq) != 0U || NVIC_GetPendingIRQ(irq) != 0U ||
            NVIC_GetActive(irq) != 0U) {
            return false;
        }
    }
    state->events &= ~(uint32_t)RECOVERING;
    return true;
}
/** \brief Progress is caller-owned; IRQs never retain a request reference. */
static void service(void* context) {
    nx_stm32_dac_dma_state_t* state = context;
    if (!context_ok() || !state->initialized || state->active == NULL) {
        return;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_time_us_t now = nx_time_now_us();
    if (!restore_clock(state)) {
        nx_request_state_t next =
            nx_deadline_expired(state->drain_deadline, now)
                ? NX_REQUEST_QUARANTINED
                : NX_REQUEST_DRAINING;
        (void)nx_request_transition(&state->active->base, next);
        nx_arch_irq_restore(saved);
        return;
    }
    if (state->phase != DRAIN &&
        (now == NX_DEADLINE_NEVER ||
         !nx_stm32_dac_external_live(state->dac.config)))
        begin_drain(state, NX_ERROR_IO, now);
    latch(state, now);
    if (state->phase != DRAIN &&
        nx_deadline_expired(state->active->base.deadline, now)) {
        begin_drain(state, NX_ERROR_TIMEOUT, now);
    }
    if (state->phase == TAIL && now >= state->tail_deadline &&
        LL_TIM_IsActiveFlag_UPDATE(timer(state)) != 0U && last_latched(state)) {
        begin_drain(state, NX_SUCCESS, now);
    }
    if (state->phase != DRAIN) {
        nx_arch_irq_restore(saved);
        return;
    }
    bool joined = withdraw(state);
    nx_arch_irq_restore(saved);
    if (joined && state->terminal != NX_SUCCESS) {
        joined =
            nx_stm32_dac_stop(&state->dac, NX_DEADLINE_NEVER) == NX_SUCCESS;
    }
    saved = nx_arch_irq_save();
    if (!joined) {
        nx_request_state_t next =
            nx_deadline_expired(state->drain_deadline, now)
                ? NX_REQUEST_QUARANTINED
                : NX_REQUEST_DRAINING;
        (void)nx_request_transition(&state->active->base, next);
    } else {
        nx_dac_tx_request_t* request = state->active;
        nx_result_t result = state->terminal;
        size_t transferred = state->transferred;
        state->dac.running = false;
        state->dac.faulted = (state->events & FAILED) != 0U;
        state->active = NULL;
        state->phase = IDLE;
        nx_request_settle(&request->base, result, transferred);
    }
    nx_arch_irq_restore(saved);
}
/** \brief Cancel keeps the same active borrow until verified withdrawal. */
static nx_result_t cancel(void* context, nx_dac_tx_request_t* request) {
    nx_stm32_dac_dma_state_t* state = context;
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->active != request) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    begin_drain(state, NX_ERROR_CANCELLED, nx_time_now_us());
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief Detach notification only after both publishers have been joined. */
static nx_result_t attach(void* context, const nx_irq_wake_t* wake,
                          uint8_t syscall_ceiling) {
    nx_stm32_dac_dma_state_t* state = context;
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized || state->closing) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (wake != NULL && wake->calls_kernel && !config_of(state)->kernel_bound) {
        return NX_ERROR_PERMISSION;
    }
    for (unsigned index = 0U; index < 2U; ++index) {
        if (NVIC_GetActive(vector(state, index)) != 0U ||
            NVIC_GetPendingIRQ(vector(state, index)) != 0U) {
            return NX_ERROR_BUSY;
        }
        nx_result_t result = nx_stm32_irq_wake_validate(
            wake, vector(state, index), syscall_ceiling);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    state->wake = wake;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}
/** \brief A finite tail is an explicit executor deadline, without a worker. */
static nx_time_us_t next_service(const void* context) {
    const nx_stm32_dac_dma_state_t* state = context;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_time_us_t next = NX_DEADLINE_NEVER;
    if (state->active != NULL) {
        if (nx_request_state(&state->active->base) == NX_REQUEST_QUARANTINED) {
            nx_arch_irq_restore(saved);
            return NX_DEADLINE_NEVER;
        }
        next = state->active->base.deadline;
        if (state->phase == TAIL && state->tail_deadline < next) {
            next = state->tail_deadline;
        } else if (state->phase == DRAIN) {
            next = nx_time_now_us();
        }
    }
    nx_arch_irq_restore(saved);
    return next;
}
/** \brief DMA observations also protect the extended state and live metadata.
 */
static nx_result_t info(const void* context, nx_dac_info_t* output) {
    const nx_stm32_dac_dma_state_t* state = context;
    if (!state->initialized) {
        return NX_ERROR_STATE;
    }
    return independent(state, output, sizeof(*output), _Alignof(nx_dac_info_t))
               ? nx_stm32_dac_info(&state->dac, output)
               : NX_ERROR_INVALID;
}
/** \brief Snapshot preserves the same read-only extended-state boundary. */
static nx_result_t snapshot(const void* context, nx_dac_state_t* output) {
    const nx_stm32_dac_dma_state_t* state = context;
    if (!state->initialized || state->clock_restore_pending) {
        return NX_ERROR_STATE;
    }
    return independent(state, output, sizeof(*output), _Alignof(nx_dac_state_t))
               ? nx_stm32_dac_snapshot(&state->dac, output)
               : NX_ERROR_INVALID;
}
/** \brief Stop joins an active TX before installing the declared idle code. */
static nx_result_t stop(void* context, nx_time_us_t deadline) {
    nx_stm32_dac_dma_state_t* state = context;
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->initialized) {
        return NX_ERROR_STATE;
    }
    if (!restore_clock(state)) {
        return NX_ERROR_BUSY;
    }
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    clear_underrun(state);
    nx_arch_dsb();
    if (underrun(state)) {
        state->dac.faulted = true;
        return NX_ERROR_BUSY;
    }
    return nx_stm32_dac_stop(&state->dac, deadline);
}
/** \brief Reverse DMA first so a failed gate retains the same base receipt. */
nx_result_t nx_stm32_dac_dma_close(nx_stm32_dac_dma_state_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->dac.config == NULL) {
        return NX_SUCCESS;
    }
    state->closing = true;
    if (!restore_clock(state)) {
        return NX_ERROR_BUSY;
    }
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
        if (state->active != NULL) {
            return NX_ERROR_BUSY;
        }
    }
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((state->vector_leases & (1U << index)) == 0U) {
            continue;
        }
        IRQn_Type irq = vector(state, index);
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);
        NVIC_SetPriority(irq, state->saved_priorities[index]);
        nx_arch_dsb();
        if (vector_busy(irq) ||
            NVIC_GetPriority(irq) != state->saved_priorities[index]) {
            return NX_ERROR_BUSY;
        }
        state->vector_leases &= (uint8_t) ~(1U << index);
    }
    if (state->route_owned) {
        gate(state);
        LL_DMA_DisableStream(DMA1, stream_id(state));
        nx_arch_dsb();
        if (LL_DMA_IsEnabledStream(DMA1, stream_id(state)) != 0U ||
            LL_DAC_IsDMAReqEnabled(DAC, channel(state)) != 0U) {
            return NX_ERROR_BUSY;
        }
        clear_flags(state);
        clear_underrun(state);
        state->route_owned = false;
    }
    if (state->clock_added) {
        if (LL_DMA_IsEnabledStream(DMA1, stream_id(state)) != 0U) {
            return NX_ERROR_BUSY;
        }
        LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_DMA1);
        nx_arch_dsb();
        if (LL_AHB1_GRP1_IsEnabledClock(LL_AHB1_GRP1_PERIPH_DMA1) != 0U) {
            return NX_ERROR_BUSY;
        }
        state->clock_added = false;
    }
    nx_result_t result = nx_stm32_dac_close(&state->dac);
    if (result == NX_SUCCESS) {
        *state = (nx_stm32_dac_dma_state_t){0};
    }
    return result;
}
/** \brief CPU writes and free-running starts cannot race this DMA owner. */
const nx_dac_ops_t nx_stm32_dac_dma_ops = {
    .info = info, .stop = stop, .state = snapshot};
const nx_dac_tx_ops_t nx_stm32_dac_dma_tx_ops = {.submit = submit,
                                                 .cancel = cancel,
                                                 .service = service,
                                                 .attach_wake = attach,
                                                 .next_service = next_service};
