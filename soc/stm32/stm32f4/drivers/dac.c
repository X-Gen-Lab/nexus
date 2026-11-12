/**
 * \file            dac.c
 * \brief           Selected F407 DAC controller owner using official LL
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_dac.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_dac.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_tim.h"

#define NX_DAC_TIMER_HZ NEXUS_APB1_TIMER_HZ

/** \brief Controls require a privileged, unmasked serialized task owner. */
static bool context_ok(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief LL literals contain offsets; combining them is not a channel mask. */
static uint32_t channel(unsigned index) {
    return index == 0U ? LL_DAC_CHANNEL_1 : LL_DAC_CHANNEL_2;
}
/** \brief Storage format and the converter's DOR unit remain distinct. */
static uint16_t maximum(const nx_stm32_dac_config_t* config) {
    return config->format == NX_DAC_FORMAT_U8 ? 255U : 4095U;
}
/** \brief Only the explicitly leased basic timer can be configured or stopped.
 */
static TIM_TypeDef* timer(const nx_stm32_dac_config_t* config) {
    return config->trigger == NX_STM32_DAC_TRIGGER_TIM6   ? TIM6
           : config->trigger == NX_STM32_DAC_TRIGGER_TIM7 ? TIM7
                                                          : NULL;
}
/** \brief The mask includes exactly this controller and selected trigger. */
static uint32_t clock_mask(const nx_stm32_dac_config_t* config) {
    return (uint32_t)RCC_APB1ENR_DACEN |
           (config->trigger == NX_STM32_DAC_TRIGGER_TIM6
                ? (uint32_t)RCC_APB1ENR_TIM6EN
            : config->trigger == NX_STM32_DAC_TRIGGER_TIM7
                ? (uint32_t)RCC_APB1ENR_TIM7EN
                : 0U);
}
/** \brief Readiness differs from a retained partial-initialization receipt. */
static bool ready(const nx_stm32_dac_state_t* state) {
    return state != NULL && state->config != NULL && state->initialized;
}
/** \brief Reject malformed constants before the first gate or pad write. */
static bool config_valid(const nx_stm32_dac_config_t* config) {
    if (config == NULL || config->registers != DAC || config->channels == 0U ||
        (config->channels & ~3U) != 0U ||
        (unsigned)config->format > (unsigned)NX_DAC_FORMAT_U12_LEFT ||
        (unsigned)config->mode > (unsigned)NX_STM32_DAC_TRIANGLE ||
        (unsigned)config->trigger > (unsigned)NX_STM32_DAC_TRIGGER_EXTI9 ||
        (config->trigger != NX_STM32_DAC_TRIGGER_EXTI9 &&
         config->external != NULL) ||
        (config->external != NULL && config->external->config != NULL &&
         config->external->config->line != 9U) ||
        ((config->mode == NX_STM32_DAC_DIRECT) !=
         (config->trigger == NX_STM32_DAC_TRIGGER_NONE))) {
        return false;
    }
    bool wave = config->mode == NX_STM32_DAC_NOISE ||
                config->mode == NX_STM32_DAC_TRIANGLE;
    uint32_t amplitude = config->amplitude;
    if ((wave && (amplitude == 0U || amplitude > 4095U ||
                  (amplitude & (amplitude + 1U)) != 0U ||
                  config->format == NX_DAC_FORMAT_U8)) ||
        (!wave && amplitude != 0U)) {
        return false;
    }
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((config->channels & (1U << index)) != 0U &&
            (uint32_t)config->idle_codes[index] + amplitude > maximum(config)) {
            return false;
        }
    }
    return true;
}
bool nx_stm32_dac_external_live(const nx_stm32_dac_config_t* config) {
    if (config->trigger != NX_STM32_DAC_TRIGGER_EXTI9)
        return true;
    const nx_stm32_exti_hardware_state_t* source = config->external;
    if (source == NULL || !source->owned || !source->initialized ||
        source->config == NULL)
        return false;
    const nx_stm32_exti_hardware_config_t* route = source->config;
    const uint32_t bit = 1U << 9U;
    uint32_t rising = (route->edge & NX_EXTI_RISING) != 0 ? bit : 0U;
    uint32_t falling = (route->edge & NX_EXTI_FALLING) != 0 ? bit : 0U;
    return route->line == 9U && route->gpio_index <= 8U &&
           route->gpio ==
               (GPIO_TypeDef*)(uintptr_t)(GPIOA_BASE +
                                          (uintptr_t)route->gpio_index *
                                              UINT32_C(0x400)) &&
           (RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) != 0U &&
           (RCC->AHB1ENR & (1U << route->gpio_index)) != 0U &&
           (RCC->APB2RSTR & RCC_APB2RSTR_SYSCFGRST) == 0U &&
           (RCC->AHB1RSTR & (1U << route->gpio_index)) == 0U &&
           ((SYSCFG->EXTICR[2] >> 4U) & 15U) == route->gpio_index &&
           (EXTI->EMR & bit) != 0U && (EXTI->IMR & bit) == 0U &&
           (EXTI->RTSR & bit) == rising && (EXTI->FTSR & bit) == falling;
}
/** \brief Program each selected channel using its exact supplier literal. */
static bool configure(const nx_stm32_dac_config_t* config) {
    uint32_t trigger = config->trigger == NX_STM32_DAC_TRIGGER_TIM6
                           ? LL_DAC_TRIG_EXT_TIM6_TRGO
                       : config->trigger == NX_STM32_DAC_TRIGGER_TIM7
                           ? LL_DAC_TRIG_EXT_TIM7_TRGO
                       : config->trigger == NX_STM32_DAC_TRIGGER_EXTI9
                           ? LL_DAC_TRIG_EXT_EXTI_LINE9
                           : LL_DAC_TRIG_SOFTWARE;
    uint32_t wave = config->mode == NX_STM32_DAC_NOISE
                        ? LL_DAC_WAVE_AUTO_GENERATION_NOISE
                    : config->mode == NX_STM32_DAC_TRIANGLE
                        ? LL_DAC_WAVE_AUTO_GENERATION_TRIANGLE
                        : LL_DAC_WAVE_AUTO_GENERATION_NONE;
    uint32_t level = 0U;
    for (uint32_t value = config->amplitude; value > 1U; value >>= 1U) {
        ++level;
    }
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((config->channels & (1U << index)) == 0U) {
            continue;
        }
        uint32_t selected = channel(index);
        LL_DAC_DisableDMAReq(config->registers, selected);
        LL_DAC_SetTriggerSource(config->registers, selected, trigger);
        LL_DAC_SetOutputBuffer(config->registers, selected,
                               config->output_buffer
                                   ? LL_DAC_OUTPUT_BUFFER_ENABLE
                                   : LL_DAC_OUTPUT_BUFFER_DISABLE);
        LL_DAC_SetWaveAutoGeneration(config->registers, selected, wave);
        if (wave == LL_DAC_WAVE_AUTO_GENERATION_NOISE) {
            LL_DAC_SetWaveNoiseLFSR(config->registers, selected,
                                    level << DAC_CR_MAMP1_Pos);
        } else if (wave == LL_DAC_WAVE_AUTO_GENERATION_TRIANGLE) {
            LL_DAC_SetWaveTriangleAmplitude(config->registers, selected,
                                            level << DAC_CR_MAMP1_Pos);
        }
        if (config->trigger == NX_STM32_DAC_TRIGGER_NONE ||
            config->trigger == NX_STM32_DAC_TRIGGER_EXTI9) {
            LL_DAC_DisableTrigger(config->registers, selected);
        } else {
            LL_DAC_EnableTrigger(config->registers, selected);
        }
    }
    nx_arch_dsb();
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((config->channels & (1U << index)) == 0U) {
            continue;
        }
        uint32_t selected = channel(index);
        if (LL_DAC_GetTriggerSource(config->registers, selected) != trigger ||
            LL_DAC_GetOutputBuffer(config->registers, selected) !=
                (config->output_buffer ? LL_DAC_OUTPUT_BUFFER_ENABLE
                                       : LL_DAC_OUTPUT_BUFFER_DISABLE) ||
            LL_DAC_GetWaveAutoGeneration(config->registers, selected) != wave ||
            LL_DAC_IsDMAReqEnabled(config->registers, selected) != 0U ||
            (LL_DAC_IsTriggerEnabled(config->registers, selected) != 0U) !=
                (config->trigger != NX_STM32_DAC_TRIGGER_NONE &&
                 config->trigger != NX_STM32_DAC_TRIGGER_EXTI9) ||
            (wave != LL_DAC_WAVE_AUTO_GENERATION_NONE &&
             LL_DAC_GetWaveTriangleAmplitude(config->registers, selected) !=
                 level << DAC_CR_MAMP1_Pos)) {
            return false;
        }
    }
    return true;
}
/** \brief Normalize public codes before writing a selected DHR encoding. */
static void put(const nx_stm32_dac_config_t* config, unsigned index,
                uint16_t code) {
    uint32_t selected = channel(index);
    if (config->format == NX_DAC_FORMAT_U8) {
        LL_DAC_ConvertData8RightAligned(config->registers, selected, code);
    } else if (config->format == NX_DAC_FORMAT_U12_LEFT) {
        LL_DAC_ConvertData12LeftAligned(config->registers, selected,
                                        (uint32_t)code << 4U);
    } else {
        LL_DAC_ConvertData12RightAligned(config->registers, selected, code);
    }
}
/** \brief One official dual register operation stages the whole pair. */
static void put_pair(const nx_stm32_dac_config_t* config, uint16_t first,
                     uint16_t second) {
    if (config->format == NX_DAC_FORMAT_U8) {
        LL_DAC_ConvertDualData8RightAligned(config->registers, first, second);
    } else if (config->format == NX_DAC_FORMAT_U12_LEFT) {
        LL_DAC_ConvertDualData12LeftAligned(
            config->registers, (uint32_t)first << 4U, (uint32_t)second << 4U);
    } else {
        LL_DAC_ConvertDualData12RightAligned(config->registers, first, second);
    }
}
/** \brief Idle values use the same code units and atomic pair operation. */
static void put_idle(const nx_stm32_dac_config_t* config) {
    if (config->channels == 3U) {
        put_pair(config, config->idle_codes[0], config->idle_codes[1]);
    } else {
        unsigned index = config->channels == NX_DAC_CHANNEL_1 ? 0U : 1U;
        put(config, index, config->idle_codes[index]);
    }
}
/** \brief Admit exact resources and retain any partial clock responsibility. */
nx_result_t nx_stm32_dac_prepare(nx_stm32_dac_state_t* state,
                                 const nx_stm32_dac_config_t* config) {
    if (state == NULL || !config_valid(config)) {
        return NX_ERROR_INVALID;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config != NULL || state->clocks_added != 0U || state->owned) {
        return NX_ERROR_BUSY;
    }
    if (!nx_stm32_dac_external_live(config))
        return NX_ERROR_STATE;
    if ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN) == 0U) {
        return NX_ERROR_STATE;
    }
    uint32_t pins = ((config->channels & 1U) != 0U ? 1U << 4U : 0U) |
                    ((config->channels & 2U) != 0U ? 1U << 5U : 0U);
    if ((GPIOA->LCKR & GPIO_LCKR_LCKK) != 0U && (GPIOA->LCKR & pins) != 0U) {
        return NX_ERROR_BUSY;
    }
    uint32_t clocks = clock_mask(config);
    state->config = config;
    state->clocks_added = clocks & ~RCC->APB1ENR;
    LL_APB1_GRP1_EnableClock(clocks);
    nx_arch_dsb();
    if (LL_APB1_GRP1_IsEnabledClock(clocks) == 0U) {
        return NX_ERROR_IO;
    }
    TIM_TypeDef* counter = timer(config);
    if ((config->registers->CR &
         (DAC_CR_EN1 | DAC_CR_EN2 | DAC_CR_DMAEN1 | DAC_CR_DMAEN2 |
          DAC_CR_DMAUDRIE1 | DAC_CR_DMAUDRIE2)) != 0U ||
        (counter != NULL &&
         ((counter->CR1 & TIM_CR1_CEN) != 0U || counter->DIER != 0U))) {
        nx_result_t cleanup = nx_stm32_dac_close(state);
        return cleanup == NX_SUCCESS ? NX_ERROR_BUSY : cleanup;
    }
    state->owned = true;
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((config->channels & (1U << index)) == 0U) {
            continue;
        }
        uint32_t pin = 1U << (index + 4U);
        LL_GPIO_SetPinPull(GPIOA, pin, LL_GPIO_PULL_NO);
        LL_GPIO_SetPinMode(GPIOA, pin, LL_GPIO_MODE_ANALOG);
        if (LL_GPIO_GetPinMode(GPIOA, pin) != LL_GPIO_MODE_ANALOG ||
            LL_GPIO_GetPinPull(GPIOA, pin) != LL_GPIO_PULL_NO) {
            return NX_ERROR_IO;
        }
    }
    if (!configure(config)) {
        return NX_ERROR_IO;
    }
    put_idle(config);
    state->initialized = true;
    return NX_SUCCESS;
}
/** \brief An observation cannot replace its provider state or ROM settings. */
bool nx_stm32_dac_independent(const nx_stm32_dac_state_t* state,
                              size_t state_bytes, const void* data, size_t size,
                              size_t alignment) {
    uintptr_t begin = (uintptr_t)data;
    if (state_bytes < sizeof(*state) || data == NULL || size == 0U ||
        begin % alignment != 0U || size > UINTPTR_MAX - begin) {
        return false;
    }
    const nx_stm32_exti_hardware_state_t* source = state->config->external;
    if (source != NULL &&
        ((begin < (uintptr_t)source + sizeof(*source) &&
          (uintptr_t)source < begin + size) ||
         (source->config != NULL &&
          begin < (uintptr_t)source->config + sizeof(*source->config) &&
          (uintptr_t)source->config < begin + size)))
        return false;
    return !(begin < (uintptr_t)state + state_bytes &&
             (uintptr_t)state < begin + size) &&
           !(begin < (uintptr_t)state->config + sizeof(*state->config) &&
             (uintptr_t)state->config < begin + size);
}
/** \brief Query immutable digital facts without measuring the analog output. */
nx_result_t nx_stm32_dac_info(const void* context, nx_dac_info_t* output) {
    const nx_stm32_dac_state_t* state = context;
    if (!ready(state)) {
        return NX_ERROR_STATE;
    }
    if (!nx_stm32_dac_independent(state, sizeof(*state), output,
                                  sizeof(*output), _Alignof(nx_dac_info_t))) {
        return NX_ERROR_INVALID;
    }
    const nx_stm32_dac_config_t* config = state->config;
    *output = (nx_dac_info_t){
        .format = config->format,
        .wave = config->mode == NX_STM32_DAC_NOISE      ? NX_DAC_WAVE_NOISE
                : config->mode == NX_STM32_DAC_TRIANGLE ? NX_DAC_WAVE_TRIANGLE
                                                        : NX_DAC_WAVE_NONE,
        .maximum_code = maximum(config),
        .amplitude = config->amplitude,
        .channel_mask = config->channels,
        .software_trigger = config->trigger == NX_STM32_DAC_TRIGGER_SOFTWARE,
        .timer_trigger = timer(config) != NULL,
        .external_trigger = config->trigger == NX_STM32_DAC_TRIGGER_EXTI9,
        .output_buffer = config->output_buffer};
    return NX_SUCCESS;
}
/** \brief Wave amplitude addition must not overflow the selected raw code. */
static bool code_valid(const nx_stm32_dac_config_t* config, uint16_t code) {
    return (uint32_t)code + config->amplitude <= maximum(config);
}
/** \brief An external edge can arrive now; stopped writes cannot rearm TEN. */
static bool external_running(const nx_stm32_dac_state_t* state) {
    const nx_stm32_dac_config_t* config = state->config;
    if (config->trigger != NX_STM32_DAC_TRIGGER_EXTI9)
        return true;
    if (!state->running || !nx_stm32_dac_external_live(config))
        return false;
    for (unsigned index = 0U; index < 2U; ++index)
        if ((config->channels & (1U << index)) != 0U &&
            (LL_DAC_IsEnabled(config->registers, channel(index)) == 0U ||
             LL_DAC_IsTriggerEnabled(config->registers, channel(index)) == 0U ||
             LL_DAC_GetTriggerSource(config->registers, channel(index)) !=
                 LL_DAC_TRIG_EXT_EXTI_LINE9))
            return false;
    return true;
}
/** \brief A scalar operation modifies only its authorized channel. */
static nx_result_t write(void* context, uint8_t selected, uint16_t code) {
    nx_stm32_dac_state_t* state = context;
    if (!ready(state) || state->faulted) {
        return NX_ERROR_STATE;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (!external_running(state))
        return NX_ERROR_STATE;
    if (selected < 1U || selected > 2U || !code_valid(state->config, code)) {
        return NX_ERROR_INVALID;
    }
    unsigned index = (unsigned)selected - 1U;
    if ((state->config->channels & (1U << index)) == 0U) {
        return NX_ERROR_PERMISSION;
    }
    if (state->config->trigger != NX_STM32_DAC_TRIGGER_NONE) {
        LL_DAC_EnableTrigger(state->config->registers, channel(index));
    }
    put(state->config, index, code);
    return NX_SUCCESS;
}
/** \brief Validate both values before one dual-register operation. */
static nx_result_t write_pair(void* context, uint16_t first, uint16_t second) {
    nx_stm32_dac_state_t* state = context;
    if (!ready(state) || state->faulted) {
        return NX_ERROR_STATE;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (!external_running(state))
        return NX_ERROR_STATE;
    if (state->config->channels != 3U) {
        return NX_ERROR_PERMISSION;
    }
    if (!code_valid(state->config, first) ||
        !code_valid(state->config, second)) {
        return NX_ERROR_INVALID;
    }
    if (state->config->trigger != NX_STM32_DAC_TRIGGER_NONE) {
        LL_DAC_EnableTrigger(state->config->registers, LL_DAC_CHANNEL_1);
        LL_DAC_EnableTrigger(state->config->registers, LL_DAC_CHANNEL_2);
    }
    put_pair(state->config, first, second);
    return NX_SUCCESS;
}
nx_result_t nx_stm32_dac_enable_external(nx_stm32_dac_state_t* state,
                                         nx_time_us_t deadline) {
    if (!nx_stm32_dac_external_live(state->config))
        return NX_ERROR_STATE;
    nx_time_us_t began = nx_time_now_us();
    if (began == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    if (nx_deadline_expired(deadline, began))
        return NX_ERROR_TIMEOUT;
    const nx_stm32_dac_config_t* config = state->config;
    for (unsigned index = 0U; index < 2U; ++index)
        if ((config->channels & (1U << index)) != 0U)
            LL_DAC_EnableTrigger(config->registers, channel(index));
    nx_arch_dsb();
    for (unsigned index = 0U; index < 2U; ++index)
        if ((config->channels & (1U << index)) != 0U &&
            (LL_DAC_IsTriggerEnabled(config->registers, channel(index)) == 0U ||
             LL_DAC_GetTriggerSource(config->registers, channel(index)) !=
                 LL_DAC_TRIG_EXT_EXTI_LINE9))
            return NX_ERROR_IO;
    if (!nx_stm32_dac_external_live(config))
        return NX_ERROR_STATE;
    nx_time_us_t now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER || now < began)
        return NX_ERROR_STATE;
    return nx_deadline_expired(deadline, now) ? NX_ERROR_TIMEOUT : NX_SUCCESS;
}
nx_result_t nx_stm32_dac_wait_startup(nx_time_us_t deadline) {
    nx_time_us_t began = nx_time_now_us(), until;
    if (began == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    /* One extra microsecond covers the fractional tick at the EN boundary. */
    nx_result_t result = nx_deadline_finite_after(
        began, LL_DAC_DELAY_STARTUP_VOLTAGE_SETTLING_US + 1U, &until);
    if (result != NX_SUCCESS)
        return result;
    for (;;) {
        nx_time_us_t now = nx_time_now_us();
        if (now == NX_DEADLINE_NEVER || now < began)
            return NX_ERROR_STATE;
        if (nx_deadline_expired(deadline, now))
            return NX_ERROR_TIMEOUT;
        if (now >= until)
            return NX_SUCCESS;
    }
}
/** \brief Disable DAC triggers while timer EGR installs its divider. */
nx_result_t nx_stm32_dac_arm(nx_stm32_dac_state_t* state, uint32_t hz,
                             nx_time_us_t deadline, bool run_timer) {
    if (!ready(state) || state->faulted) {
        return NX_ERROR_STATE;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->running) {
        return NX_ERROR_BUSY;
    }
    const nx_stm32_dac_config_t* config = state->config;
    if (!nx_stm32_dac_external_live(config))
        return NX_ERROR_STATE;
    TIM_TypeDef* counter = timer(config);
    if ((counter == NULL && hz != 0U) ||
        (counter != NULL && (hz == 0U || hz > 1000000U))) {
        return NX_ERROR_INVALID;
    }
    nx_time_us_t began = nx_time_now_us();
    if (began == NX_DEADLINE_NEVER)
        return NX_ERROR_STATE;
    if (nx_deadline_expired(deadline, began)) {
        return NX_ERROR_TIMEOUT;
    }
    if (counter != NULL) {
        for (unsigned index = 0U; index < 2U; ++index) {
            if ((config->channels & (1U << index)) != 0U) {
                LL_DAC_DisableTrigger(config->registers, channel(index));
            }
        }
        nx_arch_dsb();
        for (unsigned index = 0U; index < 2U; ++index) {
            if ((config->channels & (1U << index)) != 0U &&
                LL_DAC_IsTriggerEnabled(config->registers, channel(index)) !=
                    0U) {
                state->faulted = true;
                return NX_ERROR_IO;
            }
        }
        uint32_t ticks = (NX_DAC_TIMER_HZ + hz - 1U) / hz;
        uint32_t prescaler = (ticks - 1U) / 65536U + 1U;
        uint32_t period = (ticks + prescaler - 1U) / prescaler;
        LL_TIM_DisableCounter(counter);
        LL_TIM_EnableUpdateEvent(counter);
        LL_TIM_SetUpdateSource(counter, LL_TIM_UPDATESOURCE_REGULAR);
        LL_TIM_SetOnePulseMode(counter, LL_TIM_ONEPULSEMODE_REPETITIVE);
        LL_TIM_DisableARRPreload(counter);
        LL_TIM_SetPrescaler(counter, prescaler - 1U);
        LL_TIM_SetAutoReload(counter, period - 1U);
        LL_TIM_SetCounter(counter, 0U);
        LL_TIM_SetTriggerOutput(counter, LL_TIM_TRGO_UPDATE);
        LL_TIM_GenerateEvent_UPDATE(counter);
        LL_TIM_ClearFlag_UPDATE(counter);
        state->trigger_divisor = prescaler * period;
    }
    if (!configure(config)) {
        state->faulted = true;
        return NX_ERROR_IO;
    }
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((config->channels & (1U << index)) != 0U) {
            LL_DAC_Enable(config->registers, channel(index));
        }
    }
    nx_arch_dsb();
    state->running = true;
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((config->channels & (1U << index)) != 0U &&
            LL_DAC_IsEnabled(config->registers, channel(index)) == 0U) {
            state->faulted = true;
            return NX_ERROR_IO;
        }
    }
    nx_result_t stable = nx_stm32_dac_wait_startup(deadline);
    if (stable != NX_SUCCESS) {
        state->faulted = true;
        return stable;
    }
    if (counter != NULL) {
        if ((counter->CR1 &
             (TIM_CR1_OPM | TIM_CR1_UDIS | TIM_CR1_URS | TIM_CR1_ARPE)) != 0U ||
            /* This LL revision has a TRGO setter but no matching getter. */
            READ_BIT(counter->CR2, TIM_CR2_MMS) != LL_TIM_TRGO_UPDATE ||
            (LL_TIM_GetPrescaler(counter) + 1U) *
                    (LL_TIM_GetAutoReload(counter) + 1U) !=
                state->trigger_divisor) {
            state->faulted = true;
            return NX_ERROR_IO;
        }
        if (run_timer) {
            LL_TIM_EnableCounter(counter);
        }
        nx_arch_dsb();
        if ((LL_TIM_IsEnabledCounter(counter) != 0U) != run_timer) {
            state->faulted = true;
            return NX_ERROR_IO;
        }
    }
    if (config->trigger == NX_STM32_DAC_TRIGGER_EXTI9 && run_timer) {
        nx_result_t result = nx_stm32_dac_enable_external(state, deadline);
        if (result != NX_SUCCESS) {
            state->faulted = true;
            return result;
        }
    }
    nx_time_us_t now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER || now < began) {
        state->faulted = true;
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(deadline, now)) {
        state->faulted = true;
        return NX_ERROR_TIMEOUT;
    }
    return NX_SUCCESS;
}
/** \brief CPU modes start their selected timer after the common arm sequence.
 */
static nx_result_t start(void* context, uint32_t hz, nx_time_us_t deadline) {
    return nx_stm32_dac_arm(context, hz, deadline, true);
}
/** \brief Only this supplier primitive explicitly supports ORed LL literals. */
static nx_result_t trigger(void* context, uint8_t selected) {
    nx_stm32_dac_state_t* state = context;
    if (!ready(state) || state->faulted || !state->running) {
        return NX_ERROR_STATE;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (selected == 0U || (selected & ~3U) != 0U) {
        return NX_ERROR_INVALID;
    }
    if ((selected & state->config->channels) != selected) {
        return NX_ERROR_PERMISSION;
    }
    if (state->config->trigger != NX_STM32_DAC_TRIGGER_SOFTWARE) {
        return NX_ERROR_UNSUPPORTED;
    }
    uint32_t channels = ((selected & 1U) != 0U ? LL_DAC_CHANNEL_1 : 0U) |
                        ((selected & 2U) != 0U ? LL_DAC_CHANNEL_2 : 0U);
    LL_DAC_TrigSWConversion(state->config->registers, channels);
    return NX_SUCCESS;
}
/** \brief Detach trigger progression before installing the digital idle code.
 */
static nx_result_t idle(nx_stm32_dac_state_t* state, nx_time_us_t deadline) {
    const nx_stm32_dac_config_t* config = state->config;
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((config->channels & (1U << index)) != 0U) {
            uint32_t selected = channel(index);
            LL_DAC_DisableDMAReq(config->registers, selected);
            LL_DAC_DisableTrigger(config->registers, selected);
            LL_DAC_SetWaveAutoGeneration(config->registers, selected,
                                         LL_DAC_WAVE_AUTO_GENERATION_NONE);
        }
    }
    TIM_TypeDef* counter = timer(config);
    if (counter != NULL) {
        LL_TIM_DisableCounter(counter);
    }
    nx_arch_dsb();
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((config->channels & (1U << index)) != 0U &&
            (LL_DAC_IsDMAReqEnabled(config->registers, channel(index)) != 0U ||
             LL_DAC_IsTriggerEnabled(config->registers, channel(index)) != 0U ||
             LL_DAC_GetWaveAutoGeneration(config->registers, channel(index)) !=
                 LL_DAC_WAVE_AUTO_GENERATION_NONE)) {
            state->faulted = true;
            return NX_ERROR_BUSY;
        }
    }
    if (counter != NULL && LL_TIM_IsEnabledCounter(counter) != 0U) {
        state->faulted = true;
        return NX_ERROR_BUSY;
    }
    put_idle(config);
    for (unsigned attempt = 0U; attempt < 32U; ++attempt) {
        bool complete = true;
        for (unsigned index = 0U; index < 2U; ++index) {
            if ((config->channels & (1U << index)) != 0U) {
                uint32_t code = config->idle_codes[index];
                if (config->format == NX_DAC_FORMAT_U8) {
                    code <<= 4U;
                }
                complete = complete &&
                           LL_DAC_RetrieveOutputData(config->registers,
                                                     channel(index)) == code;
            }
        }
        if (complete) {
            state->running = state->faulted = false;
            return NX_SUCCESS;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            state->faulted = true;
            return NX_ERROR_TIMEOUT;
        }
    }
    state->faulted = true;
    return NX_ERROR_BUSY;
}
/** \brief Stop leaves pad/clock ownership with the same initialized device. */
nx_result_t nx_stm32_dac_stop(void* context, nx_time_us_t deadline) {
    nx_stm32_dac_state_t* state = context;
    if (!ready(state)) {
        return NX_ERROR_STATE;
    }
    return context_ok() ? idle(state, deadline) : NX_ERROR_CONTEXT;
}
/** \brief Hardware DOR is always right-aligned twelve-bit digital data. */
nx_result_t nx_stm32_dac_snapshot(const void* context, nx_dac_state_t* output) {
    const nx_stm32_dac_state_t* state = context;
    if (!ready(state)) {
        return NX_ERROR_STATE;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (!nx_stm32_dac_independent(state, sizeof(*state), output,
                                  sizeof(*output), _Alignof(nx_dac_state_t))) {
        return NX_ERROR_INVALID;
    }
    *output = (nx_dac_state_t){
        .running = state->running,
        .faulted = state->faulted,
        .trigger_divisor = state->trigger_divisor,
        .timer_clock_hz = timer(state->config) != NULL ? NX_DAC_TIMER_HZ : 0U};
    for (unsigned index = 0U; index < 2U; ++index) {
        if ((state->config->channels & (1U << index)) != 0U) {
            output->digital_output12[index] =
                (uint16_t)LL_DAC_RetrieveOutputData(state->config->registers,
                                                    channel(index));
            if (LL_DAC_IsEnabled(state->config->registers, channel(index)) !=
                0U) {
                output->enabled_channels |= (uint8_t)(1U << index);
            }
        }
    }
    return NX_SUCCESS;
}
/** \brief Actual disabled state precedes gate withdrawal and receipt release.
 */
nx_result_t nx_stm32_dac_close(nx_stm32_dac_state_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!context_ok()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config == NULL) {
        return state->clocks_added == 0U ? NX_SUCCESS : NX_ERROR_STATE;
    }
    if (state->owned) {
        nx_result_t result = idle(state, NX_DEADLINE_NEVER);
        if (result != NX_SUCCESS) {
            return result;
        }
        for (unsigned index = 0U; index < 2U; ++index) {
            if ((state->config->channels & (1U << index)) != 0U) {
                LL_DAC_Disable(state->config->registers, channel(index));
            }
        }
        nx_arch_dsb();
        for (unsigned index = 0U; index < 2U; ++index) {
            if ((state->config->channels & (1U << index)) != 0U &&
                LL_DAC_IsEnabled(state->config->registers, channel(index)) !=
                    0U) {
                state->faulted = true;
                return NX_ERROR_BUSY;
            }
        }
        state->owned = false;
    }
    state->initialized = false;
    LL_APB1_GRP1_DisableClock(state->clocks_added);
    nx_arch_dsb();
    if ((RCC->APB1ENR & state->clocks_added) != 0U) {
        return NX_ERROR_BUSY;
    }
    *state = (nx_stm32_dac_state_t){0};
    return NX_SUCCESS;
}
/** \brief Shared immutable operations never carry an instance context. */
const nx_dac_ops_t nx_stm32_dac_ops = {.info = nx_stm32_dac_info,
                                       .write = write,
                                       .write_pair = write_pair,
                                       .start = start,
                                       .trigger = trigger,
                                       .stop = nx_stm32_dac_stop,
                                       .state = nx_stm32_dac_snapshot};
