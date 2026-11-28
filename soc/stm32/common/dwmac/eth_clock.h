/**
 * \file            eth_clock.h
 * \brief           Reviewed MDC and fine PTP geometry from leased HCLK
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_ETH_CLOCK_H
#define NEXUS_PRIVATE_ETH_CLOCK_H

#include <stdint.h>

/** \brief RM0090 MDC divisors keep PHY management at or below 2.5 MHz. */
static inline uint8_t nx_eth_mdio_divider(uint32_t hz) {
    if (hz < 35000000U)
        return 8U;
    if (hz < 60000000U)
        return 12U;
    if (hz < 100000000U)
        return 0U;
    if (hz < 150000000U)
        return 4U;
    return 16U;
}

/** \brief Keep the fine-update accumulator slower than its HCLK input. */
static inline uint32_t nx_eth_ptp_increment(uint32_t hz) {
    return hz > 50000000U ? 20U : hz > 25000000U ? 40U : 80U;
}

/** \brief Round the 32-bit accumulator ratio after clock-range admission. */
static inline uint32_t nx_eth_ptp_addend(uint32_t hz, uint32_t increment) {
    uint64_t denominator = (uint64_t)hz * increment;
    uint64_t numerator = UINT64_C(1000000000) << 32U;
    return (uint32_t)((numerator + denominator / 2U) / denominator);
}

#endif /* NEXUS_PRIVATE_ETH_CLOCK_H */
