/**
 * \file            stm32u575_lptim.h
 * \brief           Exact TZEN0 LPTIM2 PCLK1 finite RUN counter ownership
 * \author          Nexus Team
 */
#ifndef NX_STM32U575_LPTIM_H
#define NX_STM32U575_LPTIM_H
#include "nexus/io/lptim.h"
#include "stm32u575_system.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Static single-executor LPTIM2 owner, no external pin or DMA lease.
 * \note Fixed MSIS4/PCLK1 divided by4 gives declared1MHz ticks. Hardware ARR
 *       must exceed reset CCR0, so submit accepts2..65536 and rejects1 without
 *       effects. This RUN-only mode establishes no Stop/VBAT continuity.
 *       Owner storage outlives domains, requests and observers. Generation
 *       never wraps, including restart after successful stop.
 */
typedef struct {
    LPTIM_TypeDef* registers;
    nx_lptim_request_t* active;
    uint64_t generation;
    nx_time_us_t ready_at;
    nx_result_t terminal;
    uint32_t progress;
    uint16_t polls;
    uint8_t stage;
    uint8_t saved_priority;
    bool retained;
    bool opened;
    bool gate_added;
    bool closing;
    bool launched;
    volatile bool event;
    bool terminal_set;
    bool reset_asserted;
    volatile bool tainted;
    bool priority_owned;
} nx_stm32u575_lptim_state_t;
extern const nx_lptim_ops_t nx_stm32u575_lptim_ops;
/** \brief Acquire only a cold exact LPTIM2 and preserve shared clock fields.
 * \note Task-only. A failed acquisition may retain only gate cleanup; continue
 *       stop until success. Foreign enabled/pending/active vectors are refused.
 */
nx_result_t nx_stm32u575_lptim_initialize(nx_stm32u575_lptim_state_t* state);
/** \brief Latch volatile UE/fault facts and isolate only this vector.
 * \note Configurable IRQ68 only, never NMI/HardFault. Request, terminal cause
 *       and stage remain single-task-owned; service publishes quarantine.
 */
void nx_stm32u575_lptim_irq(nx_stm32u575_lptim_state_t* state);
#ifdef __cplusplus
}
#endif
#endif
