/**
 * \file            link.h
 *
 * \brief           STM32F102/F103 USB FS clock and external Board attachment.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_USB_LINK_H
#define NEXUS_STM32F1_USB_LINK_H

#include "lp.h"
#include "nexus/io/gpio.h"

#if defined(STM32F102xB)
#define NX_STM32F1_USB_LP_IRQ USB_LP_IRQn
#else
#define NX_STM32F1_USB_LP_IRQ USB_LP_CAN1_RX0_IRQn
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_stm32f1_ep0_config_t endpoint;
    RCC_TypeDef* rcc;
    NVIC_Type* nvic;
    const nx_gpio_signal_t* vbus;
    const nx_gpio_signal_t* pullup;
} nx_stm32f1_usb_link_config_t;

typedef struct {
    const nx_stm32f1_usb_link_config_t* config;
    nx_stm32f1_ep0_t endpoint;
    nx_time_us_t ready_at;
} nx_stm32f1_usb_link_t;

/**
 * \brief Own the selected USB gate and begin disconnected transceiver warm-up.
 * \note Uses the exact generated HSE/USBPRE/APB1 combination. The caller holds
 *       the assembly's exclusive PMA and exclusive Board GPIO bindings.
 */
nx_result_t
nx_stm32f1_usb_link_begin(nx_stm32f1_usb_link_t* link,
                          const nx_stm32f1_usb_link_config_t* config);

/**
 * \brief Attach the external D+ pull-up only after VBUS, PHY and EP0 are ready.
 * \note Caller has published the sole LP vector on this exact device. Any
 *       post-effect failure retains gate, endpoint and Board ownership.
 */
nx_result_t nx_stm32f1_usb_link_connect(nx_stm32f1_usb_link_t* link);

/**
 * \brief Disconnect, reset, drain and release the USB gate.
 * \note BUSY retains state until caller has disabled and joined LP delivery
 *       and the hardware reset readbacks prove source withdrawal.
 */
nx_result_t nx_stm32f1_usb_link_stop(nx_stm32f1_usb_link_t* link);

/** \brief Establish Board detach and hold the peripheral in force reset. */
nx_result_t nx_stm32f1_usb_link_disconnect(nx_stm32f1_usb_link_t* link);

/** \brief Verify the joined sources before releasing clock and EP0 loans. */
nx_result_t nx_stm32f1_usb_link_release(nx_stm32f1_usb_link_t* link);

#ifdef __cplusplus
}
#endif
#endif
