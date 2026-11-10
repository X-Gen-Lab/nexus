/**
 * \file            stm32f407_usb_otg.h
 * \brief           One physical core/PHY/pad/IRQ owner for managed USB roles
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_USB_OTG_H
#define NEXUS_STM32F407_USB_OTG_H
#include "nexus/components/usb_role.h"
#include "nexus/io/gpio.h"
#include "nexus/io/usb_otg.h"
#include "stm32f407_usb_host_cold.h"
#include "stm32f407_usb_hs_cold.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const nx_stm32_usb_cold_config_t* device_fs;
    const nx_stm32_usb_hs_cold_config_t* device_hs;
    const nx_stm32_usb_host_cold_config_t* host;
    const nx_gpio_port_t* source;
    const nx_gpio_port_t* present;
    const nx_gpio_port_t* overcurrent;
    uint32_t source_mask;
    uint32_t present_mask;
    uint32_t overcurrent_mask;
    uint32_t fault_timeout_us;
    nx_stm32_usb_phy_t phy;
    bool source_active_high;
    bool present_active_high;
    bool overcurrent_active_high;
} nx_stm32_usb_otg_config_t;

typedef struct nx_stm32_usb_otg {
    nx_usb_controller_t device;
    nx_stm32_usb_host_t host;
    nx_usb_role_t role;
    nx_usb_role_config_t role_config;
    const nx_stm32_usb_otg_config_t* config;
    nx_stm32_usb_hs_cold_receipt_t device_hs_receipt;
    nx_stm32_usb_host_cold_receipt_t host_receipt;
    nx_usb_device_port_t device_face;
    nx_usb_host_port_t host_face;
    uint64_t generation;
    nx_result_t fault;
    nx_usb_role_kind_t transport;
    uint8_t device_fs_leases;
    bool initialized;
    bool closing;
} nx_stm32_usb_otg_t;
typedef struct {
    nx_stm32_usb_otg_t* owner;
    nx_usb_endpoint_t* endpoint;
} nx_stm32_usb_otg_endpoint_t;

extern const nx_usb_otg_ops_t nx_stm32_usb_otg_ops;
extern const nx_usb_device_ops_t nx_stm32_usb_otg_device_ops;
extern const nx_usb_host_ops_t nx_stm32_usb_otg_host_ops;
extern const nx_usb_endpoint_ops_t nx_stm32_usb_otg_endpoint_ops;
extern const nx_usb_activation_ops_t nx_stm32_usb_otg_activation_ops;
extern const nx_usb_setup_decision_ops_t nx_stm32_usb_otg_setup_decision_ops;
/** \brief Prepare OFF without starting USB or sourcing VBUS.
 * \param[in,out] state: Fresh exact composite storage until successful close.
 * \param[in] config: Immutable single PHY and shared pin snapshot plan.
 * \return SUCCESS or failure retaining exact partial cleanup responsibility.
 * \note GPIO faces must already be initialized; source must read inactive.
 */
nx_result_t nx_stm32_usb_otg_prepare(nx_stm32_usb_otg_t* state,
                                     const nx_stm32_usb_otg_config_t* config);
/** \brief Finish OFF and join both transports before releasing the owner.
 * \param[in,out] state: Exact owner; packets, config and GPIO faces stay alive.
 * \return SUCCESS only after drain, source-off and observed discharge.
 */
nx_result_t nx_stm32_usb_otg_close(nx_stm32_usb_otg_t* state);
/** \brief Dispatch the one physical vector to only its current cold owner.
 * \param[in,out] state: Published owner through final source/IRQ withdrawal.
 */
void nx_stm32_usb_otg_irq(nx_stm32_usb_otg_t* state);
#ifdef __cplusplus
}
#endif
#endif
