/**
 * \file            lifecycle_pins.c
 * \brief           Static shared-pad lifecycle and explicit cold context.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_lifecycle.h"

/** \brief Observe caller-established execution context without changing it. */
nx_result_t nx_stm32f4_lifecycle_context(const void* context,
                                         nx_lifecycle_operation_t operation) {
    if (context != NULL ||
        (operation != NX_LIFECYCLE_START && operation != NX_LIFECYCLE_STOP))
        return NX_ERROR_INVALID;
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
                   !nx_arch_irq_is_masked()
               ? NX_SUCCESS
               : NX_ERROR_CONTEXT;
}

/** \brief Refuse foreign selected resources and retained shared ownership. */
static nx_result_t admit(void* opaque, const void* config) {
    const nx_stm32_platform_pin_receipt_t* receipt = opaque;
    if (receipt == NULL)
        return NX_ERROR_INVALID;
    if (receipt->captured || receipt->clocks_added != 0U)
        return NX_ERROR_BUSY;
    return nx_stm32_platform_selected_preflight(config);
}

/** \brief Preserve actual shared clock additions even before pin capture. */
static nx_lifecycle_acquisition_t acquire(void* opaque, const void* config) {
    nx_stm32_platform_pin_receipt_t* receipt = opaque;
    nx_result_t result = nx_stm32_platform_pins_acquire(config, receipt);
    return (nx_lifecycle_acquisition_t){
        result,
        receipt != NULL && (receipt->captured || receipt->clocks_added != 0U)};
}

/** \brief Restore pads and gates only after all later resources have joined. */
static nx_result_t release(void* opaque, const void* config) {
    return nx_stm32_platform_pins_release(config, opaque);
}

const nx_lifecycle_ops_t nx_stm32f4_pins_lifecycle_ops = {admit, acquire,
                                                          release};
