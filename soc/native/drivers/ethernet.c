/**
 * \file            ethernet.c
 * \brief           Explicit Native DMA, W1C, finite MDIO and ingress/egress
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "ethernet.h"
#include <string.h>

static nx_native_eth_hardware_t* hardware(uintptr_t base) {
    return (nx_native_eth_hardware_t*)base;
}
uint32_t nx_native_eth_read(uintptr_t base, unsigned offset) {
    if (base == 0U || offset >= 0x1060U || offset % 4U != 0U)
        return 0U;
    return hardware(base)->registers[offset / 4U];
}
void nx_native_eth_write(uintptr_t base, unsigned offset, uint32_t value) {
    if (base == 0U || offset >= 0x1060U || offset % 4U != 0U)
        return;
    nx_native_eth_hardware_t* model = hardware(base);
    if (offset == 0x1014U) {
        if (!model->hold_interrupt_clear)
            model->registers[offset / 4U] &= ~value;
        return;
    }
    if (offset == 0x1018U && model->hold_engine)
        value |= model->registers[offset / 4U] & 0x2002U;
    model->registers[offset / 4U] = value;
    if (offset == 0x1018U) {
        if ((value & 0x2000U) != 0U)
            model->registers[0x1014U / 4U] |= 6U << 20U;
        if ((value & 2U) != 0U)
            model->registers[0x1014U / 4U] |= 3U << 17U;
    }
}
void nx_native_eth_poll(nx_eth_mac_t* mac) {
    if (mac == NULL || mac->registers == 0U)
        return;
    nx_native_eth_hardware_t* model = hardware(mac->registers);
    if (!model->hold_reset)
        model->registers[0x1000U / 4U] &= ~1U;
    if (!model->hold_engine) {
        if ((model->registers[0x1018U / 4U] & 0x2000U) == 0U)
            model->registers[0x1014U / 4U] &= ~UINT32_C(0x00700000);
        if ((model->registers[0x1018U / 4U] & 2U) == 0U)
            model->registers[0x1014U / 4U] &= ~UINT32_C(0x000E0000);
    }
    if (!model->hold_flush &&
        (model->registers[0x1014U / 4U] & UINT32_C(0x00700000)) == 0U)
        model->registers[0x1018U / 4U] &= ~UINT32_C(0x00100000);
    if (model->phy_registers != NULL && !model->hold_phy_reset)
        model->phy_registers[0] &= (uint16_t)~0x8000U;
    uint32_t control = model->registers[0x10U / 4U];
    if (!model->hold_mdio && (control & 1U) != 0U) {
        unsigned phy = (control >> 11U) & 31U;
        unsigned reg = (control >> 6U) & 31U;
        bool present =
            model->phy_registers != NULL && phy == model->phy_address;
        if ((control & 2U) != 0U) {
            if (present)
                model->phy_registers[reg] =
                    (uint16_t)model->registers[0x14U / 4U];
        } else {
            model->registers[0x14U / 4U] =
                present ? model->phy_registers[reg] : UINT16_MAX;
        }
        model->registers[0x10U / 4U] &= ~1U;
    }
}
static nx_result_t irq_control(nx_eth_mac_t* mac, bool enabled) {
    nx_native_eth_hardware_t* model = hardware(mac->registers);
    if (enabled && !model->hold_irq_enable)
        model->enabled_irq = true;
    if (!enabled && !model->hold_irq_disable)
        model->enabled_irq = false;
    if (!enabled && !model->enabled_irq)
        model->pending_irq = false;
    if (enabled) {
        model->priority = mac->source.priority;
        model->priority_group = mac->source.priority_group;
    }
    return model->enabled_irq == enabled ? NX_SUCCESS : NX_ERROR_IO;
}
static void pend_irq(nx_eth_mac_t* mac) {
    hardware(mac->registers)->pending_irq = true;
}
static nx_result_t validate_wake(nx_eth_mac_t* mac, const nx_irq_wake_t* wake,
                                 uint8_t ceiling) {
    nx_native_eth_hardware_t* model = hardware(mac->registers);
    if (ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_irq_source_t actual = {mac->source.irq_number, model->priority,
                              model->priority_group};
    return nx_irq_wake_validate(wake, &mac->irq_policy, &actual);
}
nx_result_t nx_native_eth_initialize(nx_native_eth_t* instance,
                                     nx_time_us_t deadline) {
    if (instance == NULL)
        return NX_ERROR_INVALID;
    if (instance->hardware.phy_address > 31U)
        return NX_ERROR_INVALID;
    nx_eth_mac_t* mac = &instance->mac;
    if (mac->snapshot.started || mac->retained)
        return NX_ERROR_BUSY;
    mac->registers = (uintptr_t)&instance->hardware;
    mac->irq_control = irq_control;
    mac->pend_irq = pend_irq;
    mac->validate_wake = validate_wake;
    nx_result_t result = nx_eth_mac_initialize(mac, deadline);
    if (result == NX_SUCCESS) {
        instance->hardware.tx_cursor = 0U;
        instance->hardware.rx_cursor = 0U;
    }
    return result;
}
nx_result_t nx_native_eth_transmit(nx_native_eth_t* instance, void* frame,
                                   size_t capacity, size_t* length,
                                   nx_result_t status) {
    if (instance == NULL || frame == NULL || length == NULL ||
        (status != NX_SUCCESS && status != NX_ERROR_IO))
        return NX_ERROR_INVALID;
    nx_eth_mac_t* mac = &instance->mac;
    nx_native_eth_hardware_t* model = &instance->hardware;
    if (!mac->snapshot.started ||
        (model->registers[0x1018U / 4U] & 0x2000U) == 0U ||
        (model->registers[0U] & 8U) == 0U)
        return NX_ERROR_STATE;
    size_t index = model->tx_cursor;
    nx_eth_descriptor_t* descriptor = &mac->tx_descriptors[index];
    if ((descriptor->status & UINT32_C(0x80000000)) == 0U)
        return NX_ERROR_EMPTY;
    nx_eth_tx_request_t* request = mac->tx_slots[index].request;
    size_t bytes = descriptor->control & 0x1FFFU;
    if (request == NULL ||
        descriptor->buffer != (uint32_t)(uintptr_t)request->data ||
        bytes != request->length)
        return NX_ERROR_IO;
    if (bytes > capacity)
        return NX_ERROR_OVERFLOW;
    memcpy(frame, request->data, bytes);
    *length = bytes;
    descriptor->status = (descriptor->status & ~UINT32_C(0x80000000)) |
                         (status == NX_SUCCESS ? 0U : 0x8002U);
    model->registers[0x1014U / 4U] |= 0x10001U;
    model->tx_cursor = (index + 1U) % mac->tx_count;
    return NX_SUCCESS;
}
nx_result_t nx_native_eth_receive(nx_native_eth_t* instance, const void* frame,
                                  size_t length, uint32_t status) {
    if (instance == NULL || frame == NULL || length < 14U || length > 1514U ||
        status > UINT16_MAX)
        return NX_ERROR_INVALID;
    nx_eth_mac_t* mac = &instance->mac;
    nx_native_eth_hardware_t* model = &instance->hardware;
    if (!mac->snapshot.started || (model->registers[0x1018U / 4U] & 2U) == 0U ||
        (model->registers[0U] & 4U) == 0U)
        return NX_ERROR_STATE;
    size_t index = model->rx_cursor;
    nx_eth_descriptor_t* descriptor = &mac->rx_descriptors[index];
    nx_eth_rx_slot_t* slot = &mac->rx_slots[index];
    if ((descriptor->status & UINT32_C(0x80000000)) == 0U) {
        model->registers[0x1014U / 4U] |= 0x8080U;
        return NX_ERROR_OVERFLOW;
    }
    if (descriptor->buffer != (uint32_t)(uintptr_t)slot->data ||
        (descriptor->control & 0x1FFFU) < length + 4U)
        return NX_ERROR_IO;
    memmove(slot->data, frame, length);
    memset(slot->data + length, 0, 4U);
    descriptor->status = ((uint32_t)(length + 4U) << 16U) | status;
    model->registers[0x1014U / 4U] |= 0x10040U;
    model->rx_cursor = (index + 1U) % mac->rx_count;
    return NX_SUCCESS;
}
