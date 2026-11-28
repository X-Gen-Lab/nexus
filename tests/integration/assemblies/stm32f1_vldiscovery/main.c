/**
 * \file            main.c
 * \brief           Select the board's two LEDs using the USER button.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/time.h"
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;

    const nx_gpio_signal_t* const user =
        nx_factory_gpio_signal_ref(NX_GPIO_REF_USER);
    const nx_gpio_signal_t* const blue =
        nx_factory_gpio_signal_ref(NX_GPIO_REF_BLUE);
    const nx_gpio_signal_t* const green =
        nx_factory_gpio_signal_ref(NX_GPIO_REF_GREEN);
    const nx_time_us_t end = nx_deadline_after(nx_time_now_us(), 250000U);
    bool valid = true;
    do {
        bool pressed = false;
        valid = nx_gpio_signal_read(user, &pressed) == NX_SUCCESS &&
                nx_gpio_signal_write(blue, pressed) == NX_SUCCESS &&
                nx_gpio_signal_write(green, !pressed) == NX_SUCCESS;
    } while (valid && !nx_deadline_expired(end, nx_time_now_us()));

    const nx_result_t stopped = nx_platform_stop();
    return valid && stopped == NX_SUCCESS ? 0 : 2;
}
