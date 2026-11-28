/**
 * \file            main.c
 * \brief           Consume the generated F103 EXTI13 face and raw level.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS) {
        return 1;
    }
    const nx_exti_port_t* button = nx_factory_exti_ref(NX_EXTI_REF_BUTTON);
    nx_exti_event_t event;
    size_t count = 0U;
    bool high = false;
    const nx_result_t level = nx_exti_port_level(button, &high);
    const nx_result_t read = nx_exti_port_read(button, &event, 1U, &count);
    const nx_result_t stopped = nx_platform_stop();
    return level == NX_SUCCESS &&
                   (read == NX_SUCCESS || read == NX_ERROR_EMPTY) &&
                   stopped == NX_SUCCESS
               ? 0
               : 2;
}
