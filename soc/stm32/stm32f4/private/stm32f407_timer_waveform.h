/**
 * \file            stm32f407_timer_waveform.h
 * \brief           Private single-owner F407 timer output and DMA controls
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_TIMER_WAVEFORM_H
#define NX_STM32F407_TIMER_WAVEFORM_H
#include "nexus/io/pwm_group.h"
#include "nexus/io/timer_pulse.h"
#include "nexus/io/timer_sequence.h"
#include "stm32f407_av.h"

#ifndef NEXUS_STM32_TIMER_PULSE
#if defined(NEXUS_STM32_MODEL)
#define NEXUS_STM32_TIMER_PULSE 1
#else
#define NEXUS_STM32_TIMER_PULSE 0
#endif
#endif
#ifndef NEXUS_STM32_TIMER_SEQUENCE
#if defined(NEXUS_STM32_MODEL)
#define NEXUS_STM32_TIMER_SEQUENCE 1
#else
#define NEXUS_STM32_TIMER_SEQUENCE 0
#endif
#endif

/** \brief One cold-selected face, sharing one physical timer responsibility. */
typedef enum nx_stm32_waveform_kind {
    NX_STM32_WAVE_GROUP,
    NX_STM32_WAVE_PULSE,
    NX_STM32_WAVE_SEQUENCE
} nx_stm32_waveform_kind_t;

/** \brief Immutable native timer facts resolved before any hardware access.
 * \note Output pads are ordered by ascending channel, with a complementary
 *       pad immediately after its primary. An advanced timer's BKIN is last.
 *       The System owns the source clock and TIM2; no configuration selects
 *       TIM2. DMA burst rows start at ARR and include the RCR/reserved item.
 */
typedef struct {
    nx_stm32_av_config_t common;
    nx_stm32_av_dma_t dma;
    TIM_TypeDef* registers;
    const volatile uint32_t* break_input;
    uint32_t break_mask;
    uint32_t timer_hz;
    uint32_t tick_hz;
    uint32_t period_ticks;
    uint32_t deadtime_ticks;
    uint32_t burst_budget_hclk_cycles;
    IRQn_Type irq;
    nx_stm32_waveform_kind_t kind;
    uint8_t controller;
    uint8_t channels;
    uint8_t master_mode;
    uint8_t sync_mode;
    uint8_t itr;
    bool complementary;
    bool internal_trigger;
    bool break_active_high;
    bool idle_high;
    bool idle_complement_high;
} nx_stm32_waveform_config_t;

/** \brief Exact static owner, retained until output, memory and IRQ joins.
 * \note No request or payload can outlive acquire-observed SETTLED. A failed
 *       stop keeps the configuration, gates, vectors and accepted storage.
 *       CPU group commits retain no caller pointer after returning.
 */
typedef struct {
    const nx_stm32_waveform_config_t* config;
    void* active;
    nx_frame_dma_view_t source;
    uint64_t time_epoch;
    nx_time_us_t observed_at;
    uint32_t duties[4];
    uint32_t trigger_ticks;
    uint32_t priorities[2];
    uint32_t events;
    size_t bytes;
    size_t dma_length;
    size_t remaining;
    nx_result_t result;
    uint8_t tail_phase;
    bool owned;
    bool initialized;
    bool shadow_valid;
    bool fault;
    bool quarantined;
    bool history_lost;
    bool timer_touched;
    bool timer_irq_owned;
    bool dma_irq_owned;
    bool dma_armed;
    bool outputs_armed;
    bool closing;
} nx_stm32_waveform_state_t;

/** \brief The same immutable group operations serve every selected timer. */
extern const nx_pwm_group_ops_t nx_stm32_pwm_group_ops;
/** \brief Separate one-pulse controls on the same selected physical owner. */
extern const nx_timer_pulse_ops_t nx_stm32_timer_pulse_ops;
/** \brief Native finite UPDATE-DMA controls on its single selected owner. */
extern const nx_timer_sequence_ops_t nx_stm32_timer_sequence_ops;
/** \brief Reject foreign owners before any Platform gate or pad effects. */
nx_result_t nx_stm32_waveform_admit(const nx_stm32_waveform_state_t* state,
                                    const nx_stm32_waveform_config_t* config);
/** \brief Prepare an idle selected owner without starting counter or DMA.
 * \param[in,out] state Zero static storage, retained on partial acquisition.
 * \param[in] config Immutable selected resources, units and safe levels.
 * \return Success or a failure retaining exact attempted responsibilities.
 * \note One privileged unmasked task; shared GPIO/DMA gates already exist.
 */
nx_result_t nx_stm32_waveform_prepare(nx_stm32_waveform_state_t* state,
                                      const nx_stm32_waveform_config_t* config);
/** \brief Revoke admission and join every output, request and IRQ publisher.
 * \param[in,out] state The same static identity as prepare and IRQ entries.
 * \return Success after final withdrawal; refusal preserves borrowed storage.
 * \note Serialized privileged task. Shared DMA controllers are never reset.
 */
nx_result_t nx_stm32_waveform_close(nx_stm32_waveform_state_t* state);
/** \brief Latch the selected timer's actual break/completion observations. */
void nx_stm32_waveform_irq(nx_stm32_waveform_state_t* state);
/** \brief Latch only the selected normal DMA stream's real completion/errors.
 */
void nx_stm32_waveform_dma_irq(nx_stm32_waveform_state_t* state);
#endif /* NX_STM32F407_TIMER_WAVEFORM_H */
