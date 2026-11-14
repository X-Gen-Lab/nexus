/**
 * \file            main.c
 * \brief           Minimal public GPIO factory and lifecycle consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/io/gpio.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include <stdint.h>

/** \brief Read the authorized GPIO snapshot, then stop the platform. */
int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS ||
        started.remaining_effects != 0U) {
        return 1;
    }
    const nx_gpio_port_t* status = nx_factory_gpio_ref(NX_GPIO_REF_STATUS);
    uint32_t value = 0U;
    nx_result_t sampled = nx_gpio_port_read(status, &value);
    nx_result_t stopped = nx_platform_stop();
    return sampled == NX_SUCCESS && stopped == NX_SUCCESS ? 0 : 2;
}
