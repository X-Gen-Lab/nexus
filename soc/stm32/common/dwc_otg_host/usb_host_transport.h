/**
 * \file            usb_host_transport.h
 * \brief           Shared STM32 DWC HCD transport configuration and ownership
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_DWC_OTG_HOST_TRANSPORT_H
#define NEXUS_STM32_DWC_OTG_HOST_TRANSPORT_H

#include "nexus/arch/arch.h"
#include "nexus/io/dma.h"
#include "nexus/io/usb_host.h"
#include "nexus/io/wake.h"
#if defined(NX_STM32F1_USB_HOST_BULK) && defined(NX_STM32F1_USB_HOST_CORE)
#error "Select only one STM32F1 USB Host logical pipe layout"
#endif
#if defined(NX_STM32F1_USB_HOST_BULK)
#define NX_STM32F1_USB_HOST_VIEWS 4U
#elif defined(NX_STM32F1_USB_HOST_CORE)
#define NX_STM32F1_USB_HOST_VIEWS 2U
#endif
#if defined(NX_STM32F1_USB_HOST_VIEWS) && !defined(NX_STM32F1_USB_HOST)
#error "Logical pipe views require the single-channel STM32F1 transport"
#endif
#ifndef USE_HAL_HCD_REGISTER_CALLBACKS
#define USE_HAL_HCD_REGISTER_CALLBACKS 1U
#elif USE_HAL_HCD_REGISTER_CALLBACKS != 1U
#error "USB Host requires the selected private HCD callback ABI"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nx_usb_host_request_t* request;
    nx_usb_host_pipe_t pipe;
    nx_time_us_t drain_deadline;
    nx_result_t terminal;
    uint32_t transferred;
    uint32_t interrupt_mask;
    bool configured;
    bool complete;
    bool halt_requested;
#if defined(NX_STM32F1_USB_HOST)
    bool wire_activity;
#endif
} nx_stm32_usb_host_channel_t;

/** \brief Readonly reviewed controller, FIFO, IRQ and DMA domain selection.
 * \note GPIO/PHY power/VBUS and shared clocks are explicitly handed over by
 *       the outer Platform/role owner before initialize. FS/HS internal PHY
 *       require an established 48 MHz clock; ULPI requires external 60 MHz.
 *       This config cannot establish PCB routing or electrical qualification.
 */
typedef struct {
    nx_stm32_usb_host_channel_t* channels;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_irq_policy_t irq_policy;
    uint32_t drain_timeout_us;
    uint32_t ulpi_clock_hz;
    uint16_t rx_fifo_words;
    uint16_t nonperiodic_fifo_words;
    uint16_t periodic_fifo_words;
    uint8_t channel_count;
    uint8_t priority;
    nx_stm32_usb_phy_t phy;
    bool dma;
    /** \brief Optional immutable logical-pipe permutation, channel_count bytes.
     * \note NULL selects identity. Keep it alive through complete cold stop.
     */
    const uint8_t* pipe_channels;
} nx_stm32_usb_host_config_t;

/** \brief One exact static transport; vendor Handle never enters public IO. */
typedef struct {
    HCD_HandleTypeDef hal;
    const nx_stm32_usb_host_config_t* config;
    nx_usb_host_state_t snapshot;
    uint32_t publishers;
    uint32_t zero_word;
    uint8_t clock_receipt;
#if defined(NX_STM32F1_USB_HOST_VIEWS)
    nx_usb_host_pipe_t logical_pipes[NX_STM32F1_USB_HOST_VIEWS];
    uint8_t bound_pipe;
#endif
} nx_stm32_usb_host_t;

extern const nx_usb_host_ops_t nx_stm32_usb_host_ops;

/** \brief Pure cold validation before GPIO, clock or HAL effects.
 * \param[in] state: Readable controller storage, never modified.
 * \param[in] config: Readable complete immutable transport facts.
 * \return SUCCESS or INVALID; no clock, pin or request is retained.
 */
nx_result_t
nx_stm32_usb_host_validate(const nx_stm32_usb_host_t* state,
                           const nx_stm32_usb_host_config_t* config);

/**
 * \brief Initialize one FS/HS core after explicit pin/power/clock handover.
 * \param[in,out] state: Zero-initialized static state, not a live owner.
 * \param[in] config: Immutable selected storage and physical facts.
 * \param[in] deadline: Finite absolute Core deadline.
 * \return SUCCESS or rejection; partial failure retains effects for stop.
 * \note Privileged unmasked task. State/config/channels survive successful
 *       stop. No HAL_Init, heap, shared registry or worker is used.
 */
nx_result_t
nx_stm32_usb_host_initialize(nx_stm32_usb_host_t* state,
                             const nx_stm32_usb_host_config_t* config,
                             nx_time_us_t deadline);

/** \brief Selected IRQ only; HAL callback facts are finalized by task service.
 * \param[in,out] state: Stable initialized provider until IRQ withdrawal joins.
 * \note IRQ/task serialization follows the private controller owner contract.
 */
void nx_stm32_usb_host_irq(nx_stm32_usb_host_t* state);

#ifdef __cplusplus
}
#endif
#endif
