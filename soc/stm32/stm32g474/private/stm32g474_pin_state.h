/**
 * \file            stm32g474_pin_state.h
 * \brief           Private static G474 GPIO-state lease binding
 * \author          Nexus Team
 */
#ifndef NX_STM32G474_PIN_STATE_H
#define NX_STM32G474_PIN_STATE_H
#include "nexus/arch/arch.h"
#include "pin_state_impl.h"
#include "stm32g474xx.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Caller-owned static binding; Platform owns the pad and bank gate.
 * \note The privileged serialized constructor supplies the exact G474 port,
 *       matching bank bit and immutable plan under an exclusive lifetime lease.
 *       Readable register agreement is not authorization. Plans, face, state
 *       and GPIO/RCC storage cannot overlap. No other writer, IRQ/NMI or clock
 *       owner may mutate selected pads during a call. No gate/reset takeover.
 *       Keep all storage until private stop succeeds, including failed init.
 */
typedef struct {
    GPIO_TypeDef* gpio;
    RCC_TypeDef* rcc;
    const nx_pin_state_plan_t* plan;
    uint32_t bank_mask;
    nx_pin_state_control_t control;
} nx_stm32g474_pin_state_t;
/** \brief Immutable exact named-state connection; no new pin snapshot. */
typedef struct {
    GPIO_TypeDef* gpio;
    RCC_TypeDef* rcc;
    const nx_pin_state_plan_t* plan;
    uint32_t bank_mask;
} nx_stm32g474_pin_state_config_t;
/** \brief Bind a fresh named-state face before acquiring its C controller.
 * \param[in,out] state: Static storage with no retained/initialized epoch.
 * \param[in] config: Reviewed static GPIO/RCC/plan, preserved through stop.
 * \return Status; partial effects retain the existing private stop obligation.
 */
nx_result_t
nx_stm32g474_pin_state_prepare(nx_stm32g474_pin_state_t* state,
                               const nx_stm32g474_pin_state_config_t* config);
extern const nx_pin_state_ops_t nx_stm32g474_pin_state_ops;
/** \brief Acquire a cold input/analog pad set and prove default; retain on IO.
 */
nx_result_t nx_stm32g474_pin_state_initialize(nx_stm32g474_pin_state_t* state);
/**
 * \brief Prove selected drivers withdrawn before releasing this provider.
 * \note Platform independently restores its original cold image/bank lease.
 *       Init failure has no committed default but still needs this cleanup.
 *       Success is the final provider access; later select cannot reinitialize.
 */
nx_result_t nx_stm32g474_pin_state_stop(nx_stm32g474_pin_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
