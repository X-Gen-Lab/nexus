/**
 * \file            stm32g474_control_cold.h
 * \brief           Private retained COMP1 and fixed control-route lifecycle
 * \author          Nexus Team
 */
#ifndef NX_STM32G474_CONTROL_COLD_H
#define NX_STM32G474_CONTROL_COLD_H
#include "stm32g474_provider.h"
#include "stm32g474_pwm_adc.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Selected-only receipt; platform retains the original pad snapshots.
 * \note Comparator restoration follows timer/vector withdrawal. A failed
 *       restoration keeps this receipt and the platform control clocks live.
 */
typedef struct {
    uint32_t comparator_saved;
    uint8_t priority;
    bool comparator_retained;
    bool vector_retained;
} nx_stm32g474_control_cold_t;
/** \brief Prepare the reviewed TIM1/COMP1 route and its exclusive break IRQ.
 * \param[in,out] port: Exclusive static coherent-group runtime.
 * \param[in] config: Immutable reviewed timer/comparator configuration.
 * \param[in,out] receipt: Zeroed static receipt, retained through failed start.
 * \return Status; close the same runtime and receipt before releasing pads.
 * \note Privileged unmasked task; platform owns gates, reset and monotonic
 * time.
 */
nx_result_t
nx_stm32g474_pwm_group_prepare(nx_stm32g474_pwm_group_state_t* port,
                               const nx_stm32g474_pwm_group_config_t* config,
                               nx_stm32g474_control_cold_t* receipt);
/** \brief Join the break vector, close TIM1 and restore the COMP1 lease.
 * \param[in,out] port: Same runtime; external callers have stopped submitting.
 * \param[in,out] receipt: Same receipt, preserved after any cleanup failure.
 * \return Success only after all owned sources and comparator restore.
 */
nx_result_t
nx_stm32g474_pwm_group_release(nx_stm32g474_pwm_group_state_t* port,
                               nx_stm32g474_control_cold_t* receipt);
/** \brief Prepare the exact combined TIM1/ADC1/COMP1 finite-window route.
 * \param[in,out] port: Exclusive static joint owner and embedded substate.
 * \param[in] config: Immutable exact configuration, retained by the provider.
 * \param[in,out] receipt: Static comparator lease; preserves partial failures.
 * \return Status; no acquisition or output starts during preparation.
 * \note Platform owns gates, reset, pad snapshots and live monotonic time.
 */
nx_result_t
nx_stm32g474_pwm_adc_prepare(nx_stm32g474_pwm_adc_state_t* port,
                             const nx_stm32g474_pwm_adc_config_t* config,
                             nx_stm32g474_control_cold_t* receipt);
/** \brief Close the joint owner before restoring its comparator lease.
 * \param[in,out] port: Same exclusive owner, requests/observers joined.
 * \param[in,out] receipt: Same retained receipt, retried after failed cleanup.
 * \return Status; platform pads and gates remain live until success.
 */
nx_result_t nx_stm32g474_pwm_adc_release(nx_stm32g474_pwm_adc_state_t* port,
                                         nx_stm32g474_control_cold_t* receipt);
#ifdef __cplusplus
}
#endif
/** \brief Configure only the selected analog inputs after capturing COMP1. */
static inline nx_result_t
nx_stm32g474_control_inputs_prepare(nx_stm32g474_control_cold_t* receipt,
                                    uint32_t fields, uint32_t csr) {
    receipt->comparator_saved =
        NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE;
    receipt->comparator_retained = true;
    NX_G474_WRITE(&GPIOA->BSRR, 1792U << 16U);
    nx_arch_dsb();
    if ((NX_G474_READ(&GPIOA->ODR) & 1792U) != 0U)
        return NX_ERROR_IO;
    NX_G474_WRITE(&GPIOA->PUPDR, NX_G474_READ(&GPIOA->PUPDR) & ~fields);
    NX_G474_WRITE(&GPIOA->MODER, NX_G474_READ(&GPIOA->MODER) | fields);
    nx_arch_dsb();
    if ((NX_G474_READ(&GPIOA->PUPDR) & fields) != 0U ||
        (NX_G474_READ(&GPIOA->MODER) & fields) != fields)
        return NX_ERROR_IO;
    NX_G474_WRITE(&COMP1->CSR, csr);
    nx_arch_dsb();
    if ((NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) != csr)
        return NX_ERROR_IO;
    uint64_t began = nx_stm32g474_system_now(&g_nx_stm32g474_system);
    uint32_t polls = 1000000U;
    while (nx_stm32g474_system_now(&g_nx_stm32g474_system) - began < 6U) {
        if (polls-- == 0U)
            return NX_ERROR_TIMEOUT;
        NX_G474_SYSTEM_POLL(&g_nx_stm32g474_system);
    }
    return NX_SUCCESS;
}
/** \brief Readback, including a refused restore, owns the release decision. */
static inline nx_result_t
nx_stm32g474_control_inputs_release(nx_stm32g474_control_cold_t* receipt) {
    if (!receipt->comparator_retained)
        return NX_SUCCESS;
    NX_G474_WRITE(&COMP1->CSR, receipt->comparator_saved);
    nx_arch_dsb();
    if ((NX_G474_READ(&COMP1->CSR) & ~(uint32_t)COMP_CSR_VALUE) !=
        receipt->comparator_saved)
        return NX_ERROR_IO;
    receipt->comparator_retained = false;
    return NX_SUCCESS;
}
#endif
