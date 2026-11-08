/**
 * \file            entropy_instance.c
 * \brief           Explicit static construction of the single F407 RNG owner
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/soc/stm32f407_entropy.h"
#include "stm32f407_entropy.h"

static nx_stm32f407_entropy_state_t s_owner;
static const nx_entropy_port_t s_face = {&nx_stm32f407_entropy_ops, &s_owner};

/** \brief Return the same static face without observing or changing hardware.
 */
const nx_entropy_port_t* nx_stm32f407_entropy_instance(void) {
    return &s_face;
}

/** \brief The unique private state retains every cold-start effect. */
nx_result_t nx_stm32f407_entropy_instance_start(
    const nx_stm32f407_entropy_config_t* config) {
    return nx_stm32f407_entropy_start(&s_owner, config);
}

/** \brief Release that exact owner's effects before Platform teardown. */
nx_result_t nx_stm32f407_entropy_instance_stop(void) {
    return nx_stm32f407_entropy_stop(&s_owner);
}
