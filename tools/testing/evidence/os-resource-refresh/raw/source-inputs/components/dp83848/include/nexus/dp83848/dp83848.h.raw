/**
 * \file            dp83848.h
 * \brief           Explicit DP83848 identity, reset and observed link mechanism
 * \author          Nexus Team
 */
#ifndef NEXUS_DP83848_H
#define NEXUS_DP83848_H
#include "nexus/io/ethernet.h"
#include "nexus/io/mdio.h"
#ifdef __cplusplus
extern "C" {
#endif
#define NX_DP83848_TI_ID    UINT32_C(0x20005CA0)
#define NX_DP83848_NS_ID    UINT32_C(0x20005C90)
#define NX_DP83848_10_HALF  1U
#define NX_DP83848_10_FULL  2U
#define NX_DP83848_100_HALF 4U
#define NX_DP83848_100_FULL 8U
/** \brief External policy explicitly selects autonegotiation or fixed mode. */
typedef struct {
    uint8_t advertisement;
    bool autonegotiation;
    bool full_duplex;
    uint16_t speed_mbps;
} nx_dp83848_mode_t;
/**
 * \brief Caller-owned PHY state; no board address, reset GPIO or worker
 * default. \note expected_id selects TI or NS model, ignoring revision nibble
 * only. One task owner serializes this component and the shared MDIO port.
 */
typedef struct {
    const nx_mdio_port_t* mdio;
    uint32_t expected_id;
    uint8_t address;
    bool probed;
    bool configured;
    nx_dp83848_mode_t mode;
} nx_dp83848_t;
/** \brief Read exact supported identity; mismatch never becomes a default PHY.
 * \param[in,out] phy: Explicit MDIO face, address and expected supported model.
 * \param[in] deadline: Finite absolute MDIO clock deadline.
 * \return Success, UNSUPPORTED for identity mismatch, or MDIO error.
 */
nx_result_t nx_dp83848_probe(nx_dp83848_t* phy, nx_time_us_t deadline);
/**
 * \brief Bounded BMCR soft reset and observed self-clearing acknowledgement.
 * \param[in,out] phy: Proved identity and caller-owned component state.
 * \param[in] deadline: Finite absolute MDIO clock deadline.
 * \return Success after real reset acknowledgement, or state/MDIO error.
 * \note Task-only finite absolute deadline; reset restraps the device and
 *       clears configured truth. This is not a GPIO, power or strap proof.
 */
nx_result_t nx_dp83848_reset(nx_dp83848_t* phy, nx_time_us_t deadline);
/**
 * \brief Start explicit negotiation or fixed mode, never wait for fake link.
 * \param[in,out] phy: Proved identity, kept live through operations.
 * \param[in] mode: Explicit fixed mode or negotiation advertisement.
 * \param[in] deadline: Finite absolute MDIO clock deadline.
 * \return Success means configured registers; otherwise no configured truth.
 * \note Task-only; identity must be proved. SUCCESS means registers configured,
 *       not autonegotiation complete. Failures retain configured=false.
 */
nx_result_t nx_dp83848_configure(nx_dp83848_t* phy,
                                 const nx_dp83848_mode_t* mode,
                                 nx_time_us_t deadline);
/**
 * \brief Observe latch-low BMSR twice and DP83848 PHYSTS speed/duplex truth.
 * \param[in,out] phy: Probed and configured caller-owned component.
 * \param[out] link: Observed value, separate from component storage.
 * \param[in] deadline: Finite absolute MDIO clock deadline.
 * \return Success copies observed link; other results preserve output.
 * \note Task-only; output changes only on SUCCESS. up requires actual link and
 *       complete autonegotiation when selected. Protocol or remote fault does
 *       not invent speed/link. MAC application remains an external action.
 */
nx_result_t nx_dp83848_link(nx_dp83848_t* phy, nx_eth_link_t* link,
                            nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
