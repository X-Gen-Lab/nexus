/**
 * \file            ethernet_frames.c
 * \brief           Explicit Native enhanced DMA and independent PTP counter
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "eth_frames_validation.h"
#include "ethernet_frames_access.h"
#include <string.h>
#ifndef NX_NATIVE_ETH_FRAMES_BEFORE_LOAN_READ
#define NX_NATIVE_ETH_FRAMES_BEFORE_LOAN_READ(mac, request) ((void)0)
#endif
#define OWN  UINT32_C(0x80000000)
#define RATE UINT64_C(1000000000)
static nx_native_eth_frames_hardware_t* hardware(uintptr_t base) {
    return (nx_native_eth_frames_hardware_t*)base;
}
uint32_t nx_native_eth_frames_read(uintptr_t base, unsigned offset) {
    if (base == 0U || offset >= 0x1060U || offset % 4U != 0U)
        return 0U;
    return hardware(base)->registers[offset / 4U];
}
void nx_native_eth_frames_write(uintptr_t base, unsigned offset,
                                uint32_t value) {
    if (base == 0U || offset >= 0x1060U || offset % 4U != 0U)
        return;
    nx_native_eth_frames_hardware_t* model = hardware(base);
    if (offset == 0x1014U) {
        if (!model->hold_interrupt_clear)
            model->registers[offset / 4U] &= ~(value & UINT32_C(0x0001e7ff));
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
void nx_native_eth_frames_poll(nx_eth_frames_mac_t* mac) {
    if (mac == NULL || mac->registers == 0U)
        return;
    nx_native_eth_frames_hardware_t* model = hardware(mac->registers);
    uint32_t* regs = model->registers;
    if ((regs[0x1000U / 4U] & 1U) != 0U && !model->hold_reset) {
        memset(regs, 0, sizeof(model->registers));
        model->hardware_nanoseconds = 0U;
        model->tx_cursor = model->rx_cursor = 0U;
    }
    if (!model->hold_engine) {
        if ((regs[0x1018U / 4U] & 0x2000U) == 0U)
            regs[0x1014U / 4U] &= ~UINT32_C(0x00700000);
        if ((regs[0x1018U / 4U] & 2U) == 0U)
            regs[0x1014U / 4U] &= ~UINT32_C(0x000e0000);
        if ((regs[0x1014U / 4U] & UINT32_C(0x00700000)) == 0U &&
            (regs[0x34U / 4U] & UINT32_C(0x01770000)) == 0U)
            regs[0x1018U / 4U] &= ~UINT32_C(0x00100000);
    }
    if (!model->hold_ptp) {
        if ((regs[0x700U / 4U] & 4U) != 0U) {
            model->hardware_nanoseconds =
                (uint64_t)regs[0x710U / 4U] * RATE + regs[0x714U / 4U];
            regs[0x708U / 4U] = regs[0x710U / 4U];
            regs[0x70cU / 4U] = regs[0x714U / 4U];
        }
        regs[0x700U / 4U] &= ~UINT32_C(0x2c);
    }
    if ((regs[0x10U / 4U] & 1U) != 0U) {
        uint32_t control = regs[0x10U / 4U];
        bool present = model->phy_registers != NULL &&
                       ((control >> 11U) & 31U) == model->phy_address;
        unsigned reg = (control >> 6U) & 31U;
        if ((control & 2U) != 0U) {
            if (present)
                model->phy_registers[reg] = (uint16_t)regs[0x14U / 4U];
        } else {
            regs[0x14U / 4U] = present ? model->phy_registers[reg] : UINT16_MAX;
        }
        regs[0x10U / 4U] &= ~1U;
    }
}
static nx_result_t control_irq(nx_eth_frames_mac_t* mac, bool enabled) {
    nx_native_eth_frames_hardware_t* model = hardware(mac->registers);
    model->irq_enabled = enabled;
    if (enabled) {
        model->priority = mac->source.priority;
        model->priority_group = mac->source.priority_group;
    } else {
        model->irq_pending = false;
    }
    return NX_SUCCESS;
}
static void pend_irq(nx_eth_frames_mac_t* mac) {
    hardware(mac->registers)->irq_pending = true;
}
static nx_result_t validate_wake(nx_eth_frames_mac_t* mac,
                                 const nx_irq_wake_t* wake, uint8_t ceiling) {
    if (ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_native_eth_frames_hardware_t* model = hardware(mac->registers);
    nx_irq_source_t actual = {mac->source.irq_number, model->priority,
                              model->priority_group};
    return nx_irq_wake_validate(wake, &mac->irq_policy, &actual);
}
nx_result_t nx_native_eth_frames_initialize(nx_native_eth_frames_t* instance,
                                            nx_time_us_t deadline) {
    if (instance == NULL || instance->hardware.phy_address > 31U)
        return NX_ERROR_INVALID;
    if (instance->mac.retained || instance->mac.snapshot.mac.started)
        return NX_ERROR_BUSY;
    instance->mac.registers = (uintptr_t)&instance->hardware;
    instance->mac.irq_control = control_irq;
    instance->mac.pend_irq = pend_irq;
    instance->mac.validate_wake = validate_wake;
    nx_result_t result = nx_eth_frames_mac_initialize(&instance->mac, deadline);
    if (result == NX_SUCCESS)
        instance->hardware.tx_cursor = instance->hardware.rx_cursor = 0U;
    return result;
}
static bool valid_instance(nx_native_eth_frames_t* instance) {
    return instance != NULL &&
           instance->mac.registers == (uintptr_t)&instance->hardware;
}
static size_t next(size_t index, size_t count) {
    return index + 1U == count ? 0U : index + 1U;
}
static bool output_disjoint(nx_native_eth_frames_t* instance, const void* data,
                            size_t length) {
    nx_eth_frames_mac_t* mac = &instance->mac;
    if (!nx_eth_frames_span(data, length) ||
        nx_eth_frames_overlap(data, length, instance, sizeof(*instance)) ||
        nx_eth_frames_overlap(data, length, mac->regions,
                              mac->region_count * sizeof(*mac->regions)) ||
        nx_eth_frames_overlap(data, length, mac->tx_slots,
                              mac->tx_count * sizeof(*mac->tx_slots)) ||
        nx_eth_frames_overlap(data, length, mac->rx_slots,
                              mac->rx_count * sizeof(*mac->rx_slots)) ||
        nx_eth_frames_overlap(data, length, mac->tx_descriptors,
                              mac->tx_count * sizeof(*mac->tx_descriptors)) ||
        nx_eth_frames_overlap(data, length, mac->rx_descriptors,
                              mac->rx_count * sizeof(*mac->rx_descriptors)))
        return false;
    for (size_t i = 0U; i < mac->rx_count; ++i)
        if (nx_eth_frames_overlap(data, length, mac->rx_slots[i].data,
                                  mac->rx_slots[i].capacity))
            return false;
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        const nx_eth_frames_tx_request_t* request = mac->tx_slots[i].request;
        if (request == NULL)
            continue;
        NX_NATIVE_ETH_FRAMES_BEFORE_LOAN_READ(mac, request);
        if (nx_eth_frames_overlap(data, length, request, sizeof(*request)) ||
            nx_eth_frames_overlap(data, length, request->fragments,
                                  request->fragment_count *
                                      sizeof(*request->fragments)))
            return false;
        for (size_t j = 0U; j < request->fragment_count; ++j)
            if (nx_eth_frames_overlap(data, length, request->fragments[j].data,
                                      request->fragments[j].length))
                return false;
    }
    return true;
}
nx_result_t nx_native_eth_frames_transmit(nx_native_eth_frames_t* instance,
                                          void* output, size_t capacity,
                                          size_t* length,
                                          nx_result_t wire_result,
                                          bool captured) {
    if (!valid_instance(instance) ||
        (wire_result != NX_SUCCESS && wire_result != NX_ERROR_IO))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    /* Validation follows the same exclusion as the hardware event. A loan
     * reader cannot outlive provider settlement and consumer reclamation. */
    if (!output_disjoint(instance, output, capacity) ||
        !output_disjoint(instance, length, sizeof(*length)) ||
        nx_eth_frames_overlap(output, capacity, length, sizeof(*length))) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_INVALID;
    }
    nx_eth_frames_mac_t* mac = &instance->mac;
    nx_native_eth_frames_hardware_t* model = &instance->hardware;
    if (!mac->snapshot.mac.started || mac->snapshot.mac.closing ||
        (model->registers[0x1018U / 4U] & 0x2000U) == 0U ||
        (model->registers[0U] & 8U) == 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    size_t index = model->tx_cursor;
    if (index >= mac->tx_count) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_IO;
    }
    nx_eth_frames_tx_slot_t* slot = &mac->tx_slots[index];
    nx_eth_frames_tx_request_t* request = slot->request;
    if (request == NULL || slot->phase != NX_ETH_FRAMES_TX_DMA ||
        (mac->tx_descriptors[index].status & OWN) == 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_EMPTY;
    }
    size_t total = 0U;
    bool valid = request->fragment_count == slot->count;
    for (size_t i = 0U, cursor = index; valid && i < slot->count; ++i) {
        nx_eth_frames_descriptor_t* descriptor = &mac->tx_descriptors[cursor];
        const nx_eth_fragment_t* piece = &request->fragments[i];
        /* Full pointer authority is the exact accepted request of this
         * controller; an equal truncated foreign DMA address is not proof. */
        valid = (descriptor->status & OWN) != 0U &&
                descriptor->buffer == (uint32_t)(uintptr_t)piece->data &&
                (descriptor->control & 0x1fffU) == piece->length &&
                descriptor->next ==
                    (uint32_t)(uintptr_t)&mac
                        ->tx_descriptors[next(cursor, mac->tx_count)] &&
                piece->length <= SIZE_MAX - total;
        total += piece->length;
        cursor = next(cursor, mac->tx_count);
    }
    if (!valid || total != slot->length) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_IO;
    }
    if (total > capacity) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_OVERFLOW;
    }
    size_t copied = 0U;
    for (size_t i = 0U, cursor = index; i < slot->count; ++i) {
        const nx_eth_fragment_t* piece = &request->fragments[i];
        memcpy((uint8_t*)output + copied, piece->data, piece->length);
        copied += piece->length;
        bool last = i + 1U == slot->count;
        nx_eth_frames_descriptor_t* descriptor = &mac->tx_descriptors[cursor];
        uint32_t bits = wire_result == NX_SUCCESS ? 0U : 0x8002U;
        if (last && captured && request->timestamp_requested) {
            descriptor->timestamp_high =
                (uint32_t)(model->hardware_nanoseconds / RATE);
            descriptor->timestamp_low =
                (uint32_t)(model->hardware_nanoseconds % RATE);
            bits |= 0x20000U;
        }
        nx_arch_dmb();
        descriptor->status = bits;
        cursor = next(cursor, mac->tx_count);
        if (last)
            model->tx_cursor = cursor;
    }
    *length = total;
    model->registers[0x1014U / 4U] |= 0x10001U;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
nx_result_t nx_native_eth_frames_receive(nx_native_eth_frames_t* instance,
                                         const void* frame, size_t length,
                                         bool captured) {
    nx_eth_fragment_t fragment = {frame, length};
    if (!valid_instance(instance) ||
        !nx_eth_frames_valid(&fragment, 1U, NULL, NULL))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_eth_frames_mac_t* mac = &instance->mac;
    nx_native_eth_frames_hardware_t* model = &instance->hardware;
    if (!mac->snapshot.mac.started || mac->snapshot.mac.closing ||
        (model->registers[0x1018U / 4U] & 2U) == 0U ||
        (model->registers[0U] & 4U) == 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    size_t first = model->rx_cursor, remaining = length + 4U, descriptors = 0U;
    size_t cursor = first;
    if (first >= mac->rx_count) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_IO;
    }
    while (remaining != 0U && descriptors < mac->rx_count) {
        nx_eth_frames_rx_slot_t* slot = &mac->rx_slots[cursor];
        nx_eth_frames_descriptor_t* descriptor = &mac->rx_descriptors[cursor];
        if ((descriptor->status & OWN) == 0U ||
            slot->state != NX_ETH_FRAMES_RX_DMA) {
            model->registers[0x1014U / 4U] |= 0x8080U;
            nx_arch_irq_restore(guard);
            return NX_ERROR_OVERFLOW;
        }
        if (descriptor->buffer != (uint32_t)(uintptr_t)slot->data ||
            (descriptor->control & 0x1fffU) != slot->capacity ||
            descriptor->next !=
                (uint32_t)(uintptr_t)&mac
                    ->rx_descriptors[next(cursor, mac->rx_count)]) {
            nx_arch_irq_restore(guard);
            return NX_ERROR_IO;
        }
        size_t bytes = slot->capacity < remaining ? slot->capacity : remaining;
        remaining -= bytes;
        ++descriptors;
        cursor = next(cursor, mac->rx_count);
    }
    if (remaining != 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_OVERFLOW;
    }
    size_t copied = 0U;
    cursor = first;
    remaining = length + 4U;
    for (size_t i = 0U; i < descriptors; ++i) {
        nx_eth_frames_rx_slot_t* slot = &mac->rx_slots[cursor];
        nx_eth_frames_descriptor_t* descriptor = &mac->rx_descriptors[cursor];
        size_t bytes = slot->capacity < remaining ? slot->capacity : remaining;
        size_t payload = copied < length ? length - copied : 0U;
        if (payload > bytes)
            payload = bytes;
        if (payload != 0U)
            memmove(slot->data, (const uint8_t*)frame + copied, payload);
        if (bytes > payload)
            memset(slot->data + payload, 0, bytes - payload);
        uint32_t bits = i == 0U ? 0x200U : 0U;
        if (i + 1U == descriptors) {
            bits |= ((uint32_t)(length + 4U) << 16U) | 0x120U;
            if (((const uint8_t*)frame)[12] == 0x81U)
                bits |= 0x400U;
            if (captured) {
                descriptor->timestamp_high =
                    (uint32_t)(model->hardware_nanoseconds / RATE);
                descriptor->timestamp_low =
                    (uint32_t)(model->hardware_nanoseconds % RATE);
                bits |= 0x80U;
            }
        }
        nx_arch_dmb();
        descriptor->status = bits;
        copied += bytes;
        remaining -= bytes;
        cursor = next(cursor, mac->rx_count);
    }
    model->rx_cursor = cursor;
    model->registers[0x1014U / 4U] |= 0x10040U;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
nx_result_t nx_native_eth_frames_tick(nx_native_eth_frames_t* instance,
                                      uint64_t nanoseconds) {
    if (!valid_instance(instance))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_native_eth_frames_hardware_t* model = &instance->hardware;
    if (!instance->mac.snapshot.mac.started ||
        (model->registers[0x700U / 4U] & 1U) == 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    if (model->hardware_nanoseconds > UINT64_MAX - nanoseconds) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_EXHAUSTED;
    }
    model->hardware_nanoseconds += nanoseconds;
    model->registers[0x708U / 4U] =
        (uint32_t)(model->hardware_nanoseconds / RATE);
    model->registers[0x70cU / 4U] =
        (uint32_t)(model->hardware_nanoseconds % RATE);
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
