/**
 * \file            early_warning_port.c
 * \brief           Own F103RB WWDG IRQ0 and the firmware-local backup lease.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/watchdog/early_warning.h"
#include "nexus/soc/stm32f103rb_wwdg_warning.h"
#include "system/platform.h"

static nx_stm32f1_wwdg_ewi_t warning;

void WWDG_IRQHandler(void);

void WWDG_IRQHandler(void) {
    (void)nx_stm32f1_wwdg_ewi_irq(&warning);
}

nx_result_t nx_stm32f103rb_wwdg_warning_prepare(void) {
    if (!nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }
    return nx_stm32f1_wwdg_ewi_prepare(&warning);
}

nx_result_t nx_stm32f103rb_wwdg_warning_take(bool* observed) {
    return nx_stm32f1_wwdg_ewi_take(&warning, observed);
}

nx_result_t nx_stm32f103rb_wwdg_warning_arm(const nx_watchdog_port_t* watchdog,
                                            uint8_t irq_priority) {
    return nx_stm32f1_wwdg_ewi_arm(&warning, watchdog, irq_priority);
}

nx_result_t nx_stm32f103rb_wwdg_warning_close(void) {
    return nx_stm32f1_wwdg_ewi_close(&warning);
}
