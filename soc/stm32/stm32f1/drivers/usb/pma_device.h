/**
 * \file            pma_device.h
 * \brief           F1 PMA USB Device with EP0 and an optional EP1 pair.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_USB_PMA_DEVICE_H
#define NEXUS_STM32F1_USB_PMA_DEVICE_H

#include "link.h"

#if defined(NX_STM32F1_USB_PMA_EP1)
#include "nexus/io/usb_activation.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_stm32f1_usb_link_config_t link;
    nx_usb_event_t* events;
    uint8_t capacity;
    uint8_t priority;
#if defined(NX_STM32F1_USB_PMA_EP1)
    uint16_t ep1_type;
    nx_stm32f1_usb_pma_endpoint_t ep1_layout;
#endif
} nx_stm32f1_usb_pma_device_config_t;

typedef struct {
    nx_stm32f1_usb_link_t link;
    const nx_stm32f1_usb_pma_device_config_t* config;
    nx_time_us_t loss_time;
    uint64_t lost;
    uint64_t lost_total;
    uint8_t head;
    uint8_t count;
    uint8_t previous_priority;
    bool nvic_owned;
    bool attached;
    bool closing;
#if defined(NX_STM32F1_USB_PMA_EP1)
    nx_stm32f1_ep0_loan_t ep1[2];
#endif
} nx_stm32f1_usb_pma_device_t;

typedef struct {
    nx_stm32f1_usb_pma_device_t* device;
    bool in;
#if defined(NX_STM32F1_USB_PMA_EP1)
    uint8_t number;
#endif
} nx_stm32f1_usb_pma_endpoint_face_t;

extern const nx_usb_device_ops_t nx_stm32f1_usb_pma_device_ops;
extern const nx_usb_endpoint_ops_t nx_stm32f1_usb_pma_endpoint_ops;
#if defined(NX_STM32F1_USB_PMA_EP1)
extern const nx_usb_activation_ops_t nx_stm32f1_usb_pma_ep1_activation_ops;
#endif

/** \brief Own a disconnected board, link and EP0; privileged task only.
 * \note With the platform clock live, retry connect after at least two ticks.
 *       Errors after effects retain the owner until successful public stop.
 */
nx_result_t nx_stm32f1_usb_pma_device_begin(
    nx_stm32f1_usb_pma_device_t* state,
    const nx_stm32f1_usb_pma_device_config_t* config);

nx_result_t
nx_stm32f1_usb_pma_device_connect(nx_stm32f1_usb_pma_device_t* state);

/** \brief Sole LP20 vector; bounded masking, no user callback or buffer I/O. */
void nx_stm32f1_usb_pma_device_irq(nx_stm32f1_usb_pma_device_t* state);

/** \brief Write one LP NVIC W1 register without touching adjacent sources. */
void nx_stm32f1_usb_lp_nvic_enable(NVIC_Type* nvic);
void nx_stm32f1_usb_lp_nvic_disable(NVIC_Type* nvic);
void nx_stm32f1_usb_lp_nvic_ack(NVIC_Type* nvic);

#ifdef __cplusplus
}
#endif
#endif
