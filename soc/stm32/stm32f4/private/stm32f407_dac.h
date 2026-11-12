/**
 * \file            stm32f407_dac.h
 * \brief           Private F407 DAC selection and resource responsibility
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_DAC_H
#define NEXUS_STM32F407_DAC_H
#include "nexus/io/dac.h"
#include "stm32f407_system.h"
#include "stm32f4_exti_hardware.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Complete selected behavior, never runtime provider discovery. */
typedef enum {
    NX_STM32_DAC_DIRECT,
    NX_STM32_DAC_TRIGGERED,
    NX_STM32_DAC_NOISE,
    NX_STM32_DAC_TRIANGLE
} nx_stm32_dac_mode_t;
/** \brief Explicit internal sources or an already-owned EXTI9 hardware event.
 */
typedef enum {
    NX_STM32_DAC_TRIGGER_NONE,
    NX_STM32_DAC_TRIGGER_SOFTWARE,
    NX_STM32_DAC_TRIGGER_TIM6,
    NX_STM32_DAC_TRIGGER_TIM7,
    NX_STM32_DAC_TRIGGER_EXTI9
} nx_stm32_dac_trigger_t;
/** \brief ROM settings; codes are raw input units, not pre-shifted storage. */
typedef struct {
    DAC_TypeDef* registers;
    nx_dac_format_t format;
    nx_stm32_dac_mode_t mode;
    nx_stm32_dac_trigger_t trigger;
    uint16_t idle_codes[2];
    uint16_t amplitude;
    uint8_t channels;
    bool output_buffer;
    /** \brief Exact prepared event owner; NULL for every internal source.
     * \note The cold source precedes this consumer and outlives joined close.
     */
    const nx_stm32_exti_hardware_state_t* external;
} nx_stm32_dac_config_t;
/** \brief CPU/wave modes keep no DMA storage, request, worker or queue. */
typedef struct {
    const nx_stm32_dac_config_t* config;
    uint32_t clocks_added;
    uint32_t trigger_divisor;
    bool initialized;
    bool running;
    bool owned;
    bool faulted;
} nx_stm32_dac_state_t;
/** \brief Platform has already acquired and recorded the selected GPIO pads. */
nx_result_t nx_stm32_dac_prepare(nx_stm32_dac_state_t* state,
                                 const nx_stm32_dac_config_t* config);
/** \brief Reverse only owned effects; refusal retains the same live receipt. */
nx_result_t nx_stm32_dac_close(nx_stm32_dac_state_t* state);
/** \brief Arm conversion with the optional DMA route still gated off. */
nx_result_t nx_stm32_dac_arm(nx_stm32_dac_state_t* state, uint32_t hz,
                             nx_time_us_t deadline, bool run_timer);
/** \brief Observe the retained EXTI9 event owner, mux and live hardware route.
 * \param[in] config: Immutable selected facts, retained throughout operation.
 * \return True for internal sources or the exact prepared external route.
 */
bool nx_stm32_dac_external_live(const nx_stm32_dac_config_t* config);
/** \brief Enable external detection only after outputs and DMA are ready.
 * \param[in,out] state: Sole running controller, with TEN still gated.
 * \param[in] deadline: Live absolute startup deadline, including readback.
 * \return SUCCESS or error retaining enabled output responsibility.
 */
nx_result_t nx_stm32_dac_enable_external(nx_stm32_dac_state_t* state,
                                         nx_time_us_t deadline);
/** \brief Wait a finite startup interval after the final EN readback.
 * \param[in] deadline: Live caller deadline; NEVER leaves only the finite
 *                     startup bound, not an invalid clock value.
 * \return SUCCESS, STATE for invalid/backward time, TIMEOUT or EXHAUSTED.
 * \note Caller retains all responsibility for enabled outputs.
 */
nx_result_t nx_stm32_dac_wait_startup(nx_time_us_t deadline);
/** \brief Exclude the concrete owner and all retained source/config facts.
 * \param[in] state: Prepared base of the exact concrete selected owner.
 * \param[in] state_bytes: Full owner span, including an optional DMA extension.
 * \param[in] data: Separate proposed caller span.
 * \param[in] size: Positive nonwrapping span.
 * \param[in] alignment: Nonzero alignment required by the consumer.
 * \return True only when this span aliases no retained mutable fact.
 */
bool nx_stm32_dac_independent(const nx_stm32_dac_state_t* state,
                              size_t state_bytes, const void* data, size_t size,
                              size_t alignment);
/** \brief Shared CPU and DMA control observations use the same digital units.
 */
nx_result_t nx_stm32_dac_info(const void* context, nx_dac_info_t* output);
/** \brief Withdraw triggers and verify the declared digital idle output. */
nx_result_t nx_stm32_dac_stop(void* context, nx_time_us_t deadline);
/** \brief Observe converter state into independent caller storage. */
nx_result_t nx_stm32_dac_snapshot(const void* context, nx_dac_state_t* output);
extern const nx_dac_ops_t nx_stm32_dac_ops;
#ifdef __cplusplus
}
#endif
#endif
