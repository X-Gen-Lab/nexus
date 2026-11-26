/**
 * \file            gpio_consumer.c
 * \brief           One public logical GPIO consumer for exact F4 devices
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-25
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Link the same signal operations without chip or pad knowledge. */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_gpio_signal_t* signals[] = {
        nx_factory_gpio_signal_ref(NX_GPIO_REF_STATUS),
        nx_factory_gpio_signal_ref(NX_GPIO_REF_AUX)};
    nx_result_t result = NX_SUCCESS;
    for (size_t i = 0U; i < sizeof(signals) / sizeof(signals[0]); ++i) {
        result = nx_gpio_signal_write(signals[i], true);
        if (result != NX_SUCCESS)
            break;
        result = nx_gpio_signal_write(signals[i], false);
        if (result != NX_SUCCESS)
            break;
    }
    nx_result_t stopped = nx_platform_stop();
    return result == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
}
