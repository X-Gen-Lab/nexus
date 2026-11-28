/**
 * \file            ep0_mmio.c
 *
 * \brief           Hardware halfword store for an STM32F1 USB endpoint.
 *
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "pma_device.h"

void nx_stm32f1_usb_epr_write(volatile uint16_t* endpoint, uint16_t word) {
    *endpoint = word;
}

void nx_stm32f1_usb_istr_write(volatile uint16_t* status, uint16_t word) {
    *status = word;
}

void nx_stm32f1_usb_lp_nvic_enable(NVIC_Type* nvic) {
    nvic->ISER[0] = 1U << NX_STM32F1_USB_LP_IRQ;
}

void nx_stm32f1_usb_lp_nvic_disable(NVIC_Type* nvic) {
    nvic->ICER[0] = 1U << NX_STM32F1_USB_LP_IRQ;
}

void nx_stm32f1_usb_lp_nvic_ack(NVIC_Type* nvic) {
    nvic->ICPR[0] = 1U << NX_STM32F1_USB_LP_IRQ;
}
