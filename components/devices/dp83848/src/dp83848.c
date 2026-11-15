/**
 * \file            dp83848.c
 * \brief           DP83848 Clause22 identity and explicit link observations
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/dp83848/dp83848.h"

static bool valid(const nx_dp83848_t* phy, nx_time_us_t deadline) {
    return phy != NULL && phy->mdio != NULL && phy->address <= 31U &&
           (phy->expected_id == NX_DP83848_TI_ID ||
            phy->expected_id == NX_DP83848_NS_ID) &&
           deadline != NX_DEADLINE_NEVER;
}
static nx_result_t read_reg(nx_dp83848_t* phy, uint8_t reg, uint16_t* value,
                            nx_time_us_t deadline) {
    return nx_mdio_read(phy->mdio, phy->address, reg, value, deadline);
}
nx_result_t nx_dp83848_probe(nx_dp83848_t* phy, nx_time_us_t deadline) {
    if (!valid(phy, deadline))
        return NX_ERROR_INVALID;
    phy->probed = false;
    phy->configured = false;
    uint16_t high, low;
    nx_result_t result = read_reg(phy, 2U, &high, deadline);
    if (result != NX_SUCCESS)
        return result;
    result = read_reg(phy, 3U, &low, deadline);
    if (result != NX_SUCCESS)
        return result;
    uint32_t identity = ((uint32_t)high << 16U) | low;
    if ((identity & UINT32_C(0xFFFFFFF0)) != phy->expected_id)
        return NX_ERROR_UNSUPPORTED;
    phy->probed = true;
    return NX_SUCCESS;
}
nx_result_t nx_dp83848_reset(nx_dp83848_t* phy, nx_time_us_t deadline) {
    if (!valid(phy, deadline))
        return NX_ERROR_INVALID;
    if (!phy->probed)
        return NX_ERROR_STATE;
    phy->configured = false;
    nx_result_t result =
        nx_mdio_write(phy->mdio, phy->address, 0U, 0x8000U, deadline);
    if (result != NX_SUCCESS)
        return result;
    for (;;) {
        uint16_t control;
        result = read_reg(phy, 0U, &control, deadline);
        if (result != NX_SUCCESS)
            return result;
        if ((control & 0x8000U) == 0U)
            return NX_SUCCESS;
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
    }
}
nx_result_t nx_dp83848_configure(nx_dp83848_t* phy,
                                 const nx_dp83848_mode_t* mode,
                                 nx_time_us_t deadline) {
    if (!valid(phy, deadline) || mode == NULL ||
        (mode->autonegotiation &&
         (mode->advertisement == 0U || mode->advertisement > 15U)) ||
        (!mode->autonegotiation &&
         (mode->advertisement != 0U ||
          (mode->speed_mbps != 10U && mode->speed_mbps != 100U))))
        return NX_ERROR_INVALID;
    if (!phy->probed)
        return NX_ERROR_STATE;
    phy->configured = false;
    uint16_t control;
    nx_result_t result;
    if (mode->autonegotiation) {
        uint16_t advertisement =
            (uint16_t)(1U | ((uint16_t)mode->advertisement << 5U));
        result =
            nx_mdio_write(phy->mdio, phy->address, 4U, advertisement, deadline);
        if (result != NX_SUCCESS)
            return result;
        control = 0x1200U;
    } else {
        control = (uint16_t)((mode->speed_mbps == 100U ? 0x2000U : 0U) |
                             (mode->full_duplex ? 0x100U : 0U));
    }
    result = nx_mdio_write(phy->mdio, phy->address, 0U, control, deadline);
    if (result == NX_SUCCESS) {
        phy->mode = *mode;
        phy->configured = true;
    }
    return result;
}
nx_result_t nx_dp83848_link(nx_dp83848_t* phy, nx_eth_link_t* link,
                            nx_time_us_t deadline) {
    if (!valid(phy, deadline) || link == NULL ||
        (uintptr_t)link > UINTPTR_MAX - sizeof(*link) ||
        (uintptr_t)phy > UINTPTR_MAX - sizeof(*phy) ||
        ((uintptr_t)link < (uintptr_t)phy + sizeof(*phy) &&
         (uintptr_t)phy < (uintptr_t)link + sizeof(*link)))
        return NX_ERROR_INVALID;
    if (!phy->probed || !phy->configured)
        return NX_ERROR_STATE;
    uint16_t control, previous, current, status;
    nx_result_t result = read_reg(phy, 0U, &control, deadline);
    if (result != NX_SUCCESS)
        return result;
    if ((control & 0x8000U) != 0U)
        return NX_ERROR_BUSY;
    bool autonegotiation = (control & 0x1000U) != 0U;
    if (autonegotiation != phy->mode.autonegotiation ||
        (control & 0x4C00U) != 0U)
        return NX_ERROR_STATE;
    result = read_reg(phy, 1U, &previous, deadline);
    if (result == NX_SUCCESS)
        result = read_reg(phy, 1U, &current, deadline);
    if (result == NX_SUCCESS)
        result = read_reg(phy, 16U, &status, deadline);
    if (result != NX_SUCCESS)
        return result;
    (void)previous;
    if ((current & 0x10U) != 0U || (status & 0x60U) != 0U)
        return NX_ERROR_IO;
    nx_eth_link_t observed = {false, false, 0U};
    observed.up = (current & 4U) != 0U && (status & 1U) != 0U &&
                  (!autonegotiation ||
                   ((current & 0x20U) != 0U && (status & 0x10U) != 0U));
    if (observed.up) {
        observed.speed_mbps = (status & 2U) != 0U ? 10U : 100U;
        observed.full_duplex = (status & 4U) != 0U;
        if (!autonegotiation && (observed.speed_mbps != phy->mode.speed_mbps ||
                                 observed.full_duplex != phy->mode.full_duplex))
            return NX_ERROR_IO;
    }
    *link = observed;
    return NX_SUCCESS;
}
