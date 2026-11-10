/**
 * \file            main.c
 * \brief           Public typed GPIO and all-line EXTI software link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

/** \brief Retain actual providers without claiming a physical event stream. */
int main(void) {
    if (nx_platform_start().primary != NX_SUCCESS)
        return 1;
    const nx_gpio_port_t* drive = nx_factory_gpio_ref(NX_GPIO_REF_DRIVE);
    const nx_exti_port_t* first = nx_factory_exti_ref(NX_EXTI_REF_EDGE0);
    const nx_exti_port_t* last = nx_factory_exti_ref(NX_EXTI_REF_EDGE15);
    if (drive == NULL || first == NULL || last == NULL)
        return 2;
    uint32_t pins = 0U;
    bool high = false;
    nx_exti_event_t events[2];
    size_t count = 0U;
    (void)nx_gpio_port_read(drive, &pins);
    (void)nx_gpio_port_write(drive, 1U, 0U);
    (void)nx_exti_port_level(first, &high);
    (void)nx_exti_port_read(last, events, 2U, &count);
    (void)nx_platform_suspend();
    (void)nx_platform_resume();
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
