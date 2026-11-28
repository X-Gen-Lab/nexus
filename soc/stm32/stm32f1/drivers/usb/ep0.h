/**
 * \file            ep0.h
 *
 * \brief           STM32F1 FS EP0 ownership under a serialized device owner.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_USB_EP0_H
#define NEXUS_STM32F1_USB_EP0_H

#include "nexus/io/usb_device.h"
#include "pma.h"
#include "stm32f1xx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NX_STM32F1_EP0_IDLE,
    NX_STM32F1_EP0_DATA_IN,
    NX_STM32F1_EP0_DATA_OUT,
    NX_STM32F1_EP0_STATUS_IN,
    NX_STM32F1_EP0_STATUS_OUT
} nx_stm32f1_ep0_phase_t;

typedef struct {
    USB_TypeDef* registers;
    volatile uint16_t* pma;
    nx_stm32f1_usb_pma_endpoint_t layout;
} nx_stm32f1_ep0_config_t;

typedef struct {
    nx_usb_request_t* request;
    nx_result_t terminating;
} nx_stm32f1_ep0_loan_t;

typedef struct {
    const nx_stm32f1_ep0_config_t* config;
    nx_stm32f1_ep0_loan_t in;
    nx_stm32f1_ep0_loan_t out;
    nx_usb_setup_t setup;
    nx_stm32f1_ep0_phase_t phase;
    uint16_t remaining;
    uint8_t staged_address;
    bool address_pending;
    bool observed_full_speed;
    bool reset_candidate;
} nx_stm32f1_ep0_t;

/**
 * \brief Form one EPnR write word from a stable register snapshot.
 * \note Acknowledge only observed CTR bits. Unacknowledged CTR bits always
 *       receive one even if absent in the snapshot. The change mask selects
 *       RW and toggle fields. Caller proves selected STAT fields are stable.
 */
uint16_t nx_stm32f1_usb_epr_word(uint16_t snapshot, uint16_t acknowledge,
                                 uint16_t changes, uint16_t desired);

/** \brief Write one previously formed EPnR command as a 16-bit access. */
void nx_stm32f1_usb_epr_write(volatile uint16_t* endpoint, uint16_t word);

/**
 * \brief Initialize the exact single-buffer EP0 after hardware reset.
 * \note Serialized caller owns MMIO, 48 MHz, APB1 and PMA, with the bus
 *       disconnected. State starts zeroed. No other endpoint is active.
 */
nx_result_t nx_stm32f1_usb_ep0_start(nx_stm32f1_ep0_t* state,
                                     const nx_stm32f1_ep0_config_t* config);

/**
 * \brief Stage SET_ADDRESS for a live matching standard no-data request.
 * \note DADDR changes only from the acknowledged matching status-IN packet.
 */
nx_result_t nx_stm32f1_usb_ep0_address(nx_stm32f1_ep0_t* state, uint64_t epoch,
                                       uint8_t address);

/**
 * \brief Borrow and arm one finite EP0 packet in the current stage.
 * \note One loan per direction. Control status OUT may be armed before
 *       or during DATA IN. All caller storage spans remain disjoint.
 */
nx_result_t nx_stm32f1_usb_ep0_submit(nx_stm32f1_ep0_t* state,
                                      nx_usb_request_t* request, bool in,
                                      nx_time_us_t now);

/**
 * \brief Request cancellation while retaining the packet loan.
 * \note Only a stable completion, a replacement SETUP or reset drain releases
 *       storage. This operation does not toggle a possibly moving STAT field.
 */
nx_result_t nx_stm32f1_usb_ep0_cancel(nx_stm32f1_ep0_t* state,
                                      nx_usb_request_t* request);

/** \brief Consume at most one stable EP0 event or enforce its finite deadline.
 */
nx_result_t nx_stm32f1_usb_ep0_service(nx_stm32f1_ep0_t* state,
                                       nx_time_us_t now);

/**
 * \brief Reset the control state after observation of real USB bus RESET.
 * \note A live RESET latch and both directions disabled prove that the old
 *       CTR latches may be retired. The outer owner clears RESET after success.
 */
nx_result_t nx_stm32f1_usb_ep0_bus_reset(nx_stm32f1_ep0_t* state);

/** \brief Sample the actual EP0, speed and address state after service. */
nx_result_t nx_stm32f1_usb_ep0_snapshot(const nx_stm32f1_ep0_t* state,
                                        nx_usb_device_state_t* output);

/** \brief Set or clear STALL on a proven idle control direction. */
nx_result_t nx_stm32f1_usb_ep0_stall(nx_stm32f1_ep0_t* state, bool in,
                                     bool stalled);

/**
 * \brief Release a loan after actual disconnect, IRQ withdrawal and reset.
 * \note Caller joins publishers and holds FRES. The helper verifies readback
 *       of reset, disabled EP0 and no CTR before publishing settlement.
 */
nx_result_t nx_stm32f1_usb_ep0_close(nx_stm32f1_ep0_t* state);

#ifdef __cplusplus
}
#endif
#endif
