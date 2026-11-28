/**
 * \file            app.c
 * \brief           Offset app that explicitly accepts the masked boot entry.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus_bindings.h"

int main(void) {
    const nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U ||
        masks.primask != 1U || masks.basepri != 0U || masks.faultmask != 0U)
        return 1;
    /* The vendor reset entry has installed this image's vectors and initialized
     * its C storage. This app owns the decision to unmask normal IRQs. */
    nx_arch_irq_restore((nx_arch_irq_state_t){0});
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 2;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
