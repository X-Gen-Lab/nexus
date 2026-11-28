/**
 * \file            pma.h
 *
 * \brief           STM32F1 USB Device single-buffer packet memory.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_USB_PMA_H
#define NEXUS_STM32F1_USB_PMA_H

#include "nexus/core/status.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { NX_STM32F1_USB_PMA_BYTES = 512U, NX_STM32F1_USB_PMA_ENDPOINTS = 8U };

typedef struct {
    uint16_t tx_address;
    uint16_t tx_capacity;
    uint16_t rx_address;
    uint16_t rx_capacity;
} nx_stm32f1_usb_pma_endpoint_t;

/**
 * \brief           Validate and populate the fixed buffer table at PMA zero.
 *
 * \param[in,out]   pma: Live PMA bus window after the USB gate is enabled.
 * \param[in]       endpoints: Immutable endpoint-indexed logical byte ranges.
 * \param[in]       count: Number of rows, up to eight.
 * \return          Success or INVALID without effects.
 * \note            Cold owner only. USB endpoints stay quiescent, BTABLE is
 *                  zero, and CAN is excluded by the selected resource plan.
 *                  A zero capacity disables one direction and uses address 0.
 */
nx_result_t
nx_stm32f1_usb_pma_prepare(volatile uint16_t* pma,
                           const nx_stm32f1_usb_pma_endpoint_t* endpoints,
                           size_t count);

/**
 * \brief           Copy one packet and write its exact hardware TX count.
 *
 * \param[in,out]   pma: Prepared PMA bus window.
 * \param[in]       endpoint: Hardware row corresponding to the layout.
 * \param[in]       layout: Same immutable row accepted by prepare.
 * \param[in]       bytes: Disjoint packet storage, NULL for a ZLP.
 * \param[in]       length: At most the declared TX capacity.
 * \return          Success or INVALID without effects.
 * \note            The serialized owner proves TX is quiescent first. This
 *                  call does not arm VALID or establish packet completion.
 */
nx_result_t
nx_stm32f1_usb_pma_transmit(volatile uint16_t* pma, uint8_t endpoint,
                            const nx_stm32f1_usb_pma_endpoint_t* layout,
                            const uint8_t* bytes, size_t length);

/**
 * \brief           Copy the completed OUT packet into disjoint caller storage.
 *
 * \param[in]       pma: Prepared PMA bus window.
 * \param[in]       endpoint: Hardware row corresponding to the layout.
 * \param[in]       layout: Same immutable row accepted by prepare.
 * \param[out]      bytes: Writable capacity, NULL when capacity is zero.
 * \param[in]       capacity: At most the declared RX capacity.
 * \param[out]      count: Separate output containing the actual packet length.
 * \return          Success, INVALID or OVERFLOW. Error leaves outputs intact.
 * \note            The owner proves CTR_RX and stable packet storage first. It
 *                  owns acknowledgment, NAK and rearming after this read.
 *                  Even when rejecting an oversized packet, the physical
 *                  hardware storage remains owned by the caller.
 */
nx_result_t
nx_stm32f1_usb_pma_receive(const volatile uint16_t* pma, uint8_t endpoint,
                           const nx_stm32f1_usb_pma_endpoint_t* layout,
                           uint8_t* bytes, size_t capacity, size_t* count);

#if defined(NX_STM32F1_USB_PMA_EP1)
/** \brief Validate the whole map, then fill only the disabled EP1 row. */
nx_result_t
nx_stm32f1_usb_pma_prepare_ep1(volatile uint16_t* pma,
                               const nx_stm32f1_usb_pma_endpoint_t* control,
                               const nx_stm32f1_usb_pma_endpoint_t* ep1);
#endif

#ifdef __cplusplus
}
#endif

#endif
