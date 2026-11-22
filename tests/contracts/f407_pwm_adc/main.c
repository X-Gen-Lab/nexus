/**
 * \file            main.c
 * \brief           Exact selected coupled factory and lifecycle link consumer
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    if (nx_platform_start().primary != NX_SUCCESS)
        return 1;
    const nx_pwm_adc_t* drive = nx_factory_pwm_adc_ref(NX_PWM_ADC_REF_DRIVE);
    nx_pwm_adc_state_t state;
    if (nx_pwm_adc_state(drive, &state) != NX_SUCCESS)
        return 2;
    return nx_platform_stop() == NX_SUCCESS ? 0 : 3;
}
