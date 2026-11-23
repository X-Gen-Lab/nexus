/**
 * \file            stm32f4_freertos_stop.h
 * \brief           Explicit one-shot low-speed FreeRTOS STOP reference
 * \author          Nexus Team
 */
#ifndef NEXUS_SOC_STM32F4_FREERTOS_STOP_H
#define NEXUS_SOC_STM32F4_FREERTOS_STOP_H
#include "nexus/soc/stm32f4_power.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Immutable authorization, retained for one actual Idle attempt.
 * \note The product first suspends Platform, joins all other Core borrowers,
 *       and closes the exact ClockDomain. RTC stays suspended with its sole
 *       32768Hz LSE owner, PRER127/255, div16/reload255 periodic wake and
 * EXTI22 event route. max_elapsed_us includes wake, restore and quantization;
 *       it is an externally established bound, not a measured WCET. drift_ppm
 *       bounds relative RTC/Core frequency error in both directions.
 */
typedef struct {
    nx_clock_barrier_t barrier;
    const nx_rtc_port_t* rtc;
    uint32_t poll_limit;
    uint32_t max_elapsed_us;
    uint32_t drift_ppm;
} nx_stm32_freertos_stop_config_t;
/** \brief Stable caller-owned one-shot and its observable time envelope.
 * \note Read only from its serialized owner after Idle returned. elapsed
 *       bounds include declared drift. uncertainty_us additionally includes
 *       the discarded partial first tick; these are software bounds, not
 *       physical precision measurements. Old Core deadlines stay invalid.
 */
typedef struct {
    const nx_stm32_freertos_stop_config_t* config;
    nx_stm32_stop_receipt_t power;
    uint64_t elapsed_min_us;
    uint64_t elapsed_max_us;
    uint64_t accounted_ticks;
    uint64_t uncertainty_us;
    uint32_t discarded_phase_us;
    nx_result_t result;
    bool armed;
    bool attempted;
} nx_stm32_freertos_stop_t;
/** \brief Product returns its stable selected owner, or NULL for shallow Idle.
 * \note No registry or runtime binding. The selected object and configuration
 *       remain valid through the scheduler. The default returns NULL.
 */
nx_stm32_freertos_stop_t* nx_stm32_freertos_stop_context(void);
/** \brief Arm one attempt without modifying SysTick, RTC or kernel time.
 * \return SUCCESS or rejection without retained effects.
 * \note Privileged unmasked Thread; all product users are already joined.
 *       The product subsequently blocks using kernel-owned timing. An Idle
 *       window shorter than max_elapsed_us leaves the one-shot armed and uses
 *       ordinary shallow ticking. A qualified attempt consumes the arm once,
 *       even on failure; the product must inspect its receipt before rearming.
 */
nx_result_t
nx_stm32_freertos_stop_arm(nx_stm32_freertos_stop_t* state,
                           const nx_stm32_freertos_stop_config_t* config);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_SOC_STM32F4_FREERTOS_STOP_H */
