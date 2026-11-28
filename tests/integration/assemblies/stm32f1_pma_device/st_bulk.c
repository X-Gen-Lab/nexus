/**
 * \file            st_bulk.c
 * \brief           Explicit F103 PMA fixture through the real ST Device Core.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "st_bulk.h"

enum { PACKET_BYTES = 64U, DATA_IN = 0x81U, DATA_OUT = 0x01U, ENDPOINTS = 4U };

typedef enum { WAITING, RECEIVING, TRANSMITTING } echo_state_t;

static nx_st_usb_device_stack_t s_nx_core;
static nx_st_usb_device_slot_t s_nx_slots[ENDPOINTS];
static nx_st_usb_device_stack_config_t s_nx_configuration;
_Alignas(4) static uint8_t s_nx_staging[ENDPOINTS][PACKET_BYTES];
static uint8_t s_nx_echo[PACKET_BYTES];
static echo_state_t s_nx_echo_state;

/* VID/PID are deliberately unassigned in a software validation fixture. */
static uint8_t s_nx_device[18] = {18U, 1U,           0U, 2U, 0U, 0U,
                                  0U,  PACKET_BYTES, 0U, 0U, 0U, 0U,
                                  0U,  1U,           0U, 0U, 0U, 1U};
static uint8_t s_nx_full[32] = {9U,    2U,           32U, 0U, 1U, 1U, 0U,
                                0xC0U, 0U,           9U,  4U, 0U, 0U, 2U,
                                0xFFU, 0U,           0U,  0U, 7U, 5U, DATA_IN,
                                2U,    PACKET_BYTES, 0U,  0U, 7U, 5U, DATA_OUT,
                                2U,    PACKET_BYTES, 0U,  0U};

static uint8_t* device_descriptor(USBD_SpeedTypeDef speed, uint16_t* bytes) {
    (void)speed;
    *bytes = sizeof(s_nx_device);
    return s_nx_device;
}

static uint8_t* full_descriptor(uint16_t* bytes) {
    *bytes = sizeof(s_nx_full);
    return s_nx_full;
}

static uint8_t class_initialize(USBD_HandleTypeDef* device, uint8_t index) {
    (void)index;
    if (USBD_LL_OpenEP(device, DATA_IN, USBD_EP_TYPE_BULK, PACKET_BYTES) !=
        USBD_OK) {
        return USBD_FAIL;
    }
    return USBD_LL_OpenEP(device, DATA_OUT, USBD_EP_TYPE_BULK, PACKET_BYTES);
}

static uint8_t class_deinitialize(USBD_HandleTypeDef* device, uint8_t index) {
    (void)index;
    const USBD_StatusTypeDef in = USBD_LL_CloseEP(device, DATA_IN);
    const USBD_StatusTypeDef out = USBD_LL_CloseEP(device, DATA_OUT);
    if (in != USBD_OK || out != USBD_OK) {
        return USBD_FAIL;
    }
    s_nx_echo_state = WAITING;
    return USBD_OK;
}

static uint8_t class_setup(USBD_HandleTypeDef* device,
                           USBD_SetupReqTypedef* setup) {
    USBD_CtlError(device, setup);
    return USBD_FAIL;
}

static uint8_t class_output(USBD_HandleTypeDef* device, uint8_t endpoint) {
    if (endpoint != DATA_OUT || s_nx_echo_state != RECEIVING) {
        return USBD_FAIL;
    }
    const uint32_t bytes = USBD_LL_GetRxDataSize(device, DATA_OUT);
    s_nx_echo_state = TRANSMITTING;
    return USBD_LL_Transmit(device, DATA_IN, s_nx_echo, bytes);
}

static uint8_t class_input(USBD_HandleTypeDef* device, uint8_t endpoint) {
    (void)device;
    if (endpoint != DATA_OUT || s_nx_echo_state != TRANSMITTING) {
        return USBD_FAIL;
    }
    s_nx_echo_state = WAITING;
    return USBD_OK;
}

static const USBD_DescriptorsTypeDef s_nx_descriptors = {
    .GetDeviceDescriptor = device_descriptor,
};
static const USBD_ClassTypeDef s_nx_class = {
    .Init = class_initialize,
    .DeInit = class_deinitialize,
    .Setup = class_setup,
    .DataOut = class_output,
    .DataIn = class_input,
    .GetFSConfigDescriptor = full_descriptor,
};

nx_result_t nx_test_pma_st_start(const nx_usb_device_profile_t* profile,
                                 const nx_usb_activation_port_t* activation) {
    if (s_nx_core.config != NULL) {
        return NX_ERROR_BUSY;
    }
    s_nx_configuration = (nx_st_usb_device_stack_config_t){
        .profile = profile,
        .activation = activation,
        .slots = s_nx_slots,
        .slot_count = ENDPOINTS,
        .transfer_timeout_us = 1000000U,
    };
    for (size_t i = 0U; i < ENDPOINTS; ++i) {
        s_nx_slots[i] = (nx_st_usb_device_slot_t){
            .storage = s_nx_staging[i],
            .capacity = PACKET_BYTES,
        };
    }
    return nx_st_usb_device_stack_initialize(&s_nx_core, &s_nx_configuration,
                                             &s_nx_descriptors, &s_nx_class);
}

nx_result_t nx_test_pma_st_step(void) {
    nx_result_t result = nx_st_usb_device_stack_step(&s_nx_core);
    if (result != NX_SUCCESS || s_nx_core.latest.control_pending ||
        s_nx_core.latest.suspended ||
        s_nx_core.middleware.dev_state != USBD_STATE_CONFIGURED ||
        s_nx_echo_state != WAITING) {
        return result;
    }
    /* Prime only after STATUS ACK. The same executor progresses Core and class.
     */
    if (USBD_LL_PrepareReceive(&s_nx_core.middleware, DATA_OUT, s_nx_echo,
                               PACKET_BYTES) != USBD_OK) {
        return NX_ERROR_IO;
    }
    s_nx_echo_state = RECEIVING;
    return NX_SUCCESS;
}

nx_result_t nx_test_pma_st_stop(void) {
    return nx_st_usb_device_stack_close(&s_nx_core);
}
