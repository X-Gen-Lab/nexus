/**
 * \file            main.c
 * \brief           STM32F107 Clause22 installed-source consumer.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"

int main(void) {
    const nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS || started.cleanup != NX_SUCCESS)
        return 1;
    const nx_mdio_port_t* const phy = nx_factory_mdio_ref(NX_MDIO_REF_PHY);
    uint16_t status = 0U;
    const nx_result_t read = nx_mdio_read(
        phy, 1U, 1U, &status, nx_deadline_after(nx_time_now_us(), 1000U));
    const nx_result_t stopped = nx_platform_stop();
    return read == NX_SUCCESS && status != UINT16_MAX && stopped == NX_SUCCESS
               ? 0
               : 2;
}
