/**
 * \file            main.c
 * \brief           Exact F103RB one-shot reset witness firmware reference.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/soc/stm32f103rb_wwdg_warning.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    if (nx_stm32f103rb_wwdg_warning_prepare() != NX_SUCCESS) {
        (void)nx_stm32f103rb_wwdg_warning_close();
        (void)nx_platform_stop();
        return 2;
    }
    bool observed = false;
    const nx_result_t taken = nx_stm32f103rb_wwdg_warning_take(&observed);
    const bool watchdog_reset = (nx_reset_cause() & NX_RESET_WWDG) != 0U;
    if (taken != NX_SUCCESS || observed || watchdog_reset) {
        const nx_result_t close = nx_stm32f103rb_wwdg_warning_close();
        const nx_result_t stopped = nx_platform_stop();
        return taken == NX_SUCCESS && observed && watchdog_reset &&
                       close == NX_SUCCESS && stopped == NX_SUCCESS
                   ? 0
                   : 3;
    }
    const nx_watchdog_port_t* const watchdog =
        nx_factory_watchdog_ref(NX_WATCHDOG_REF_WWDG);
    nx_watchdog_state_t state = {0};
    const nx_result_t activated =
        nx_watchdog_port_enable(watchdog, 15000U, false, &state);
    if (!state.enabled) {
        (void)nx_stm32f103rb_wwdg_warning_close();
        (void)nx_platform_stop();
        return 4;
    }
    if (activated == NX_SUCCESS)
        (void)nx_stm32f103rb_wwdg_warning_arm(watchdog, 4U);
    for (;;) {
    }
}
