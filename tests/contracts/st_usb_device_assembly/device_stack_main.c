/**
 * \file            device_stack_main.c
 * \brief           Private Device Core wiring, compile/link evidence only
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory.h"
#include "usb_device_stack.h"

static nx_st_usb_device_stack_t s_nx_stack;
static nx_st_usb_device_slot_t s_nx_slots[4];
_Alignas(4) static uint8_t s_nx_staging[4][512];
static nx_st_usb_device_stack_config_t s_nx_config;
/* Unassigned identities are deliberate placeholders, never a USB product. */
static uint8_t s_nx_device[18] = {18U, 1U, 0U, 2U, 0U, 0U, 0U, 64U};
static uint8_t s_nx_qualifier[10] = {10U, 6U, 0U, 2U, 0U, 0U, 0U, 64U, 1U, 0U};
static uint8_t s_nx_full[32] = {9U,  2U, 32U, 0U, 1U,    1U,  0U,    0x80U,
                                50U, 9U, 4U,  0U, 0U,    2U,  0xffU, 0U,
                                0U,  0U, 7U,  5U, 0x81U, 2U,  64U,   0U,
                                0U,  7U, 5U,  1U, 2U,    64U, 0U,    0U};
static uint8_t s_nx_high[32] = {
    9U, 2U, 32U, 0U, 1U,    1U, 0U, 0x80U, 50U, 9U, 4U, 0U, 0U, 2U, 0xffU, 0U,
    0U, 0U, 7U,  5U, 0x81U, 2U, 0U, 2U,    0U,  7U, 5U, 1U, 2U, 0U, 2U,    0U};
/** \brief Stable private descriptor bytes, no registry or allocation. */
static uint8_t* device_descriptor(USBD_SpeedTypeDef speed, uint16_t* bytes) {
    (void)speed;
    *bytes = sizeof(s_nx_device);
    return s_nx_device;
}
/** \brief Full-speed class geometry remains explicit. */
static uint8_t* full_descriptor(uint16_t* bytes) {
    *bytes = sizeof(s_nx_full);
    return s_nx_full;
}
/** \brief High-speed class geometry remains explicit. */
static uint8_t* high_descriptor(uint16_t* bytes) {
    *bytes = sizeof(s_nx_high);
    return s_nx_high;
}
/** \brief Explicit HS-capable private qualifier, with unassigned identity. */
static uint8_t* qualifier_descriptor(uint16_t* bytes) {
    *bytes = sizeof(s_nx_qualifier);
    return s_nx_qualifier;
}
/** \brief A link-only class opens only the exact selected bulk directions. */
static uint8_t class_initialize(USBD_HandleTypeDef* device, uint8_t index) {
    (void)index;
    uint16_t maximum = device->dev_speed == USBD_SPEED_HIGH ? 512U : 64U;
    USBD_StatusTypeDef first =
        USBD_LL_OpenEP(device, 0x81U, USBD_EP_TYPE_BULK, maximum);
    USBD_StatusTypeDef second =
        USBD_LL_OpenEP(device, 1U, USBD_EP_TYPE_BULK, maximum);
    return first == USBD_OK && second == USBD_OK ? USBD_OK : USBD_FAIL;
}
/** \brief The bridge calls class DeInit only after every accepted loan joins.
 */
static uint8_t class_deinitialize(USBD_HandleTypeDef* device, uint8_t index) {
    (void)index;
    USBD_StatusTypeDef first = USBD_LL_CloseEP(device, 0x81U);
    USBD_StatusTypeDef second = USBD_LL_CloseEP(device, 1U);
    return first == USBD_OK && second == USBD_OK ? USBD_OK : USBD_FAIL;
}
/** \brief No external vendor/class protocol is implemented by this fixture. */
static uint8_t class_setup(USBD_HandleTypeDef* device,
                           USBD_SetupReqTypedef* setup) {
    (void)device;
    (void)setup;
    return USBD_FAIL;
}
static const USBD_DescriptorsTypeDef s_nx_descriptors = {.GetDeviceDescriptor =
                                                             device_descriptor};
static const USBD_ClassTypeDef s_nx_class = {
    .Init = class_initialize,
    .DeInit = class_deinitialize,
    .Setup = class_setup,
    .GetFSConfigDescriptor = full_descriptor,
    .GetHSConfigDescriptor = high_descriptor,
    .GetDeviceQualifierDescriptor = qualifier_descriptor,
    .GetOtherSpeedConfigDescriptor = full_descriptor};

int main(void) {
    nx_platform_start_result_t started = nx_platform_start();
    if (started.primary != NX_SUCCESS)
        return (int)started.primary;
    s_nx_config.profile = nx_factory_usb_profile(NX_USB_DEVICE_ID_FS);
    s_nx_config.activation = nx_factory_usb_activation(NX_USB_DEVICE_ID_FS);
    s_nx_config.slots = s_nx_slots;
    s_nx_config.slot_count = 4U;
    s_nx_config.transfer_timeout_us = 1000000U;
    for (size_t i = 0U; i < 4U; ++i) {
        s_nx_slots[i].storage = s_nx_staging[i];
        s_nx_slots[i].capacity = sizeof(s_nx_staging[i]);
    }
    nx_result_t result = nx_st_usb_device_stack_initialize(
        &s_nx_stack, &s_nx_config, &s_nx_descriptors, &s_nx_class);
    if (result == NX_SUCCESS)
        result = nx_st_usb_device_stack_step(&s_nx_stack);
    nx_result_t closed = nx_st_usb_device_stack_close(&s_nx_stack);
    if (closed != NX_SUCCESS)
        return (int)closed;
    closed = nx_platform_stop();
    return (int)(closed == NX_SUCCESS ? result : closed);
}
