/**
 * \file            stm32f407_usb_hs_clock.h
 * \brief           Explicit OTG_HS and external ULPI clock ownership
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_USB_HS_CLOCK_H
#define NEXUS_STM32F407_USB_HS_CLOCK_H
#include "usb_device_controller.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t consumers;
    uint8_t phase;
    bool ulpi;
} nx_stm32_usb_hs_clock_state_t;
extern const nx_usb_clock_ops_t nx_stm32_usb_hs_clock_ops;
/** \brief Acquire HS/ULPI gates without changing the selected shared PLL.
 * \param[in,out] state: Fresh zero-initialized owner, kept until full stop.
 * \param[in] ulpi: External ULPI PHY when true, internal full-speed otherwise.
 * \param[in] ulpi_clock_hz: Explicit 60000000 for ULPI, zero for internal PHY.
 * \param[in] deadline: Finite absolute Core deadline.
 * \return SUCCESS or failure retaining exact partial cleanup responsibility.
 * \note Privileged task after physical PHY power and clock handover. Declared
 *       external clock frequency is not a measurement or PCB qualification.
 */
nx_result_t nx_stm32_usb_hs_clock_start(nx_stm32_usb_hs_clock_state_t* state,
                                        bool ulpi, uint32_t ulpi_clock_hz,
                                        nx_time_us_t deadline);
/** \brief Withdraw only owned gates after every transport consumer drains.
 * \param[in,out] state: Exact original retained clock owner.
 * \param[in] deadline: Finite absolute Core deadline in the unchanged epoch.
 * \return SUCCESS or failure retaining cleanup; active consumers return BUSY.
 */
nx_result_t nx_stm32_usb_hs_clock_stop(nx_stm32_usb_hs_clock_state_t* state,
                                       nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
