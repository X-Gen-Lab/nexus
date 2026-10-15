/**
 * \file            eth_mac.c
 * \brief           Direct-register DMA descriptor ownership and progress
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../private/eth_mac.h"
#ifdef NX_ETH_NATIVE
#include "../native/ethernet_access.h"
#endif
#include <string.h>

enum {
    MAC_CONTROL = 0x00U,
    MAC_FILTER = 0x04U,
    MDIO_CONTROL = 0x10U,
    MDIO_DATA = 0x14U,
    MAC_DEBUG = 0x34U,
    MAC_INTERRUPT_MASK = 0x3CU,
    MAC_ADDRESS_HIGH = 0x40U,
    MAC_ADDRESS_LOW = 0x44U,
    DMA_BUS = 0x1000U,
    DMA_TX_POLL = 0x1004U,
    DMA_RX_POLL = 0x1008U,
    DMA_RX_TABLE = 0x100CU,
    DMA_TX_TABLE = 0x1010U,
    DMA_STATUS = 0x1014U,
    DMA_CONTROL = 0x1018U,
    DMA_INTERRUPTS = 0x101CU
};
#define OWN         UINT32_C(0x80000000)
#define TX_CHAIN    UINT32_C(0x00100000)
#define TX_FRAME    UINT32_C(0x70100000)
#define RX_CHAIN    UINT32_C(0x00004000)
#define DMA_TX      UINT32_C(0x00002000)
#define DMA_RX      UINT32_C(0x00000002)
#define DMA_FLUSH   UINT32_C(0x00100000)
#define DMA_PROCESS UINT32_C(0x007E0000)
#define TX_PROCESS  UINT32_C(0x00700000)
#define TX_DEBUG    UINT32_C(0x01770000)
#define RX_DEBUG    UINT32_C(0x00000077)
#define DMA_SOURCES UINT32_C(0x0001A1E3)
#define DMA_W1C     UINT32_C(0x0001E7FF)
#define DMA_FATAL   UINT32_C(0x00002000)

_Static_assert(sizeof(nx_eth_descriptor_t) == 16U, "normal descriptor stride");

static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool span(const void* pointer, size_t size) {
    return pointer != NULL && size != 0U &&
           (uintptr_t)pointer <= UINTPTR_MAX - size;
}
static bool overlaps(const void* a, size_t asize, const void* b, size_t bsize) {
    return (uintptr_t)a < (uintptr_t)b + bsize &&
           (uintptr_t)b < (uintptr_t)a + asize;
}
static uint32_t address(const void* pointer) {
    return (uint32_t)(uintptr_t)pointer;
}
/** \brief Advance within the validated nonempty descriptor ring. */
static size_t ring_next(size_t index, size_t count) {
    return index + 1U == count ? 0U : index + 1U;
}
static nx_result_t domain(nx_eth_mac_t* mac, const void* data, size_t length,
                          nx_dma_direction_t direction) {
    if (!NX_ETH_HOST_ADDRESSES && ((uintptr_t)data > UINT32_MAX ||
                                   length > UINT32_MAX - (uintptr_t)data)) {
        return NX_ERROR_PERMISSION;
    }
    return nx_dma_buffer_validate(mac->regions, mac->region_count, data, length,
                                  4U, direction);
}
static uint32_t read_reg(nx_eth_mac_t* mac, unsigned offset) {
    return NX_ETH_READ(mac->registers, offset);
}
static void write_reg(nx_eth_mac_t* mac, unsigned offset, uint32_t value) {
    NX_ETH_WRITE(mac->registers, offset, value);
}
/** \brief Ethernet register writes require four HCLK cycles before readback. */
static bool write_checked(nx_eth_mac_t* mac, unsigned offset, uint32_t value,
                          uint32_t mask) {
    write_reg(mac, offset, value);
    (void)read_reg(mac, offset);
    (void)read_reg(mac, offset);
    return (read_reg(mac, offset) & mask) == (value & mask);
}
static void saturate(uint64_t* value) {
    if (*value != UINT64_MAX) {
        ++*value;
    }
}
static bool control_overlap(nx_eth_mac_t* mac, const void* data,
                            size_t length) {
    return (mac->wake != NULL &&
            overlaps(data, length, mac->wake, sizeof(*mac->wake))) ||
           overlaps(data, length, (const void*)mac->registers, 0x1060U) ||
           overlaps(data, length, mac, sizeof(*mac)) ||
           overlaps(data, length, mac->regions,
                    mac->region_count * sizeof(*mac->regions)) ||
           overlaps(data, length, mac->tx_descriptors,
                    mac->tx_count * sizeof(*mac->tx_descriptors)) ||
           overlaps(data, length, mac->rx_descriptors,
                    mac->rx_count * sizeof(*mac->rx_descriptors)) ||
           overlaps(data, length, mac->tx_slots,
                    mac->tx_count * sizeof(*mac->tx_slots)) ||
           overlaps(data, length, mac->rx_slots,
                    mac->rx_count * sizeof(*mac->rx_slots));
}
static bool output_overlap(nx_eth_mac_t* mac, const void* output,
                           size_t length) {
    if (!span(output, length) || control_overlap(mac, output, length))
        return true;
    for (size_t i = 0U; i < mac->rx_count; ++i)
        if (overlaps(output, length, mac->rx_slots[i].data,
                     mac->rx_slots[i].capacity))
            return true;
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        const nx_eth_tx_request_t* request = mac->tx_slots[i].request;
        bool conflict =
            request != NULL &&
            (overlaps(output, length, request, sizeof(*request)) ||
             overlaps(output, length, request->data, request->length));
        nx_arch_irq_restore(guard);
        if (conflict)
            return true;
    }
    return false;
}
static nx_result_t validate_storage(nx_eth_mac_t* mac) {
    if (mac == NULL || mac->registers == 0U || mac->tx_count == 0U ||
        mac->rx_count == 0U || mac->region_count == 0U ||
        mac->tx_count > SIZE_MAX / sizeof(*mac->tx_descriptors) ||
        mac->tx_count > SIZE_MAX / sizeof(*mac->tx_slots) ||
        mac->rx_count > SIZE_MAX / sizeof(*mac->rx_descriptors) ||
        mac->rx_count > SIZE_MAX / sizeof(*mac->rx_slots) ||
        mac->region_count > SIZE_MAX / sizeof(*mac->regions) ||
        mac->drain_timeout_us == 0U || mac->hclk_hz < 20000000U ||
        mac->hclk_hz > 240000000U || (mac->address[0] & 1U) != 0U ||
        mac->epoch == UINT64_MAX) {
        return NX_ERROR_INVALID;
    }
    unsigned nonzero = 0U;
    for (unsigned i = 0U; i < 6U; ++i)
        nonzero |= mac->address[i];
    if (nonzero == 0U)
        return NX_ERROR_INVALID;
    const void* pointers[6] = {
        mac,           mac->regions, mac->tx_descriptors, mac->rx_descriptors,
        mac->tx_slots, mac->rx_slots};
    const size_t sizes[6] = {sizeof(*mac),
                             mac->region_count * sizeof(*mac->regions),
                             mac->tx_count * sizeof(*mac->tx_descriptors),
                             mac->rx_count * sizeof(*mac->rx_descriptors),
                             mac->tx_count * sizeof(*mac->tx_slots),
                             mac->rx_count * sizeof(*mac->rx_slots)};
    for (unsigned i = 0U; i < 6U; ++i) {
        if (!span(pointers[i], sizes[i]))
            return NX_ERROR_INVALID;
        for (unsigned j = 0U; j < i; ++j)
            if (overlaps(pointers[i], sizes[i], pointers[j], sizes[j]))
                return NX_ERROR_INVALID;
    }
    nx_result_t result =
        nx_irq_wake_validate(NULL, &mac->irq_policy, &mac->source);
    if (result != NX_SUCCESS)
        return result;
    for (unsigned i = 2U; i < 4U; ++i) {
        result = domain(mac, pointers[i], sizes[i], NX_DMA_FROM_DEVICE);
        if (result == NX_SUCCESS)
            result = domain(mac, pointers[i], sizes[i], NX_DMA_TO_DEVICE);
        if (result != NX_SUCCESS)
            return result;
    }
    for (size_t i = 0U; i < mac->tx_count; ++i)
        if (mac->tx_slots[i].request != NULL)
            return NX_ERROR_BUSY;
    for (size_t i = 0U; i < mac->rx_count; ++i) {
        nx_eth_rx_slot_t* slot = &mac->rx_slots[i];
        if (!span(slot->data, slot->capacity) ||
            slot->capacity < NX_ETH_RX_STORAGE_MIN || slot->capacity > 8191U ||
            control_overlap(mac, slot->data, slot->capacity))
            return NX_ERROR_INVALID;
        if (slot->state != NX_ETH_RX_DMA)
            return NX_ERROR_BUSY;
        result = domain(mac, slot->data, slot->capacity, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
        for (size_t j = 0U; j < i; ++j)
            if (overlaps(slot->data, slot->capacity, mac->rx_slots[j].data,
                         mac->rx_slots[j].capacity))
                return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
static uint8_t divider(uint32_t hz) {
    if (hz < 35000000U)
        return 8U;
    if (hz < 60000000U)
        return 12U;
    if (hz < 100000000U)
        return 0U;
    if (hz < 150000000U)
        return 4U;
    return 16U;
}
static void rearm(nx_eth_mac_t* mac, size_t index) {
    nx_eth_rx_slot_t* slot = &mac->rx_slots[index];
    slot->state = NX_ETH_RX_DMA;
    slot->length = 0U;
    nx_arch_dmb();
    NX_ETH_DESCRIPTOR_STATUS(&mac->rx_descriptors[index], OWN);
    nx_arch_dsb();
    write_reg(mac, DMA_RX_POLL, 0U);
}

nx_result_t nx_eth_mac_initialize(nx_eth_mac_t* mac, nx_time_us_t deadline) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    nx_result_t result = validate_storage(mac);
    if (result != NX_SUCCESS)
        return result;
    if (mac->snapshot.started || mac->retained)
        return NX_ERROR_BUSY;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if ((read_reg(mac, DMA_CONTROL) & (DMA_TX | DMA_RX)) != 0U ||
        (read_reg(mac, DMA_STATUS) & DMA_PROCESS) != 0U ||
        (read_reg(mac, MAC_CONTROL) & 12U) != 0U ||
        (read_reg(mac, MDIO_CONTROL) & 1U) != 0U ||
        read_reg(mac, DMA_TX_TABLE) != 0U || read_reg(mac, DMA_RX_TABLE) != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < mac->tx_count; ++i)
        if ((mac->tx_descriptors[i].status & OWN) != 0U)
            return NX_ERROR_BUSY;
    for (size_t i = 0U; i < mac->rx_count; ++i)
        if ((mac->rx_descriptors[i].status & OWN) != 0U)
            return NX_ERROR_BUSY;
    mac->retained = true;
    write_reg(mac, DMA_BUS, 1U);
    while ((read_reg(mac, DMA_BUS) & 1U) != 0U) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        NX_ETH_POLL(mac);
    }
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    mac->mdio_divider = divider(mac->hclk_hz);
    bool valid = write_checked(mac, DMA_BUS, 32U << 8U, UINT32_MAX) &&
                 write_checked(mac, MAC_CONTROL, 0U, UINT32_C(0x02FFFFFF)) &&
                 write_checked(mac, MAC_FILTER, 0U, UINT32_MAX) &&
                 write_checked(mac, MAC_INTERRUPT_MASK, 0x208U, 0x208U) &&
                 write_checked(mac, DMA_INTERRUPTS, 0U, DMA_SOURCES) &&
                 write_checked(mac, DMA_CONTROL,
                               (1U << 25U) | (1U << 21U) | 128U, UINT32_MAX);
    uint32_t low =
        (uint32_t)mac->address[0] | ((uint32_t)mac->address[1] << 8U) |
        ((uint32_t)mac->address[2] << 16U) | ((uint32_t)mac->address[3] << 24U);
    uint32_t high =
        (uint32_t)mac->address[4] | ((uint32_t)mac->address[5] << 8U);
    valid = write_checked(mac, MAC_ADDRESS_LOW, low, UINT32_MAX) && valid;
    valid = write_checked(mac, MAC_ADDRESS_HIGH, high | OWN, 0xFFFFU) && valid;
    if (!valid)
        return NX_ERROR_IO;
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        nx_eth_descriptor_t* d = &mac->tx_descriptors[i];
        d->control = 0U;
        d->buffer = 0U;
        d->next = address(&mac->tx_descriptors[ring_next(i, mac->tx_count)]);
        NX_ETH_DESCRIPTOR_STATUS(d, TX_CHAIN);
        mac->tx_slots[i].terminal = NX_SUCCESS;
    }
    for (size_t i = 0U; i < mac->rx_count; ++i) {
        nx_eth_descriptor_t* d = &mac->rx_descriptors[i];
        d->control = RX_CHAIN | (uint32_t)mac->rx_slots[i].capacity;
        d->buffer = address(mac->rx_slots[i].data);
        d->next = address(&mac->rx_descriptors[ring_next(i, mac->rx_count)]);
        mac->rx_slots[i].length = 0U;
        rearm(mac, i);
    }
    nx_arch_dsb();
    valid = write_checked(mac, DMA_RX_TABLE, address(mac->rx_descriptors),
                          UINT32_MAX) &&
            write_checked(mac, DMA_TX_TABLE, address(mac->tx_descriptors),
                          UINT32_MAX);
    if (!valid)
        return NX_ERROR_IO;
    write_reg(mac, DMA_STATUS, DMA_W1C);
    mac->snapshot = (nx_eth_state_t){0};
    mac->snapshot.started = true;
    mac->tx_producer = mac->tx_consumer = mac->rx_producer = 0U;
    mac->rx_consumer = mac->rx_ready_tail = SIZE_MAX;
    mac->tx_closing = false;
    mac->flush_started = false;
    mac->drain_deadline = NX_DEADLINE_NEVER;
    if (mac->irq_control != NULL) {
        result = mac->irq_control(mac, true);
        if (result != NX_SUCCESS) {
            mac->snapshot.closing = true;
            return result;
        }
    }
    if (!write_checked(mac, DMA_INTERRUPTS, DMA_SOURCES, DMA_SOURCES)) {
        mac->snapshot.closing = true;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
/** \brief Close first; descriptor withdrawal only follows engine readback. */
static void close_tx(nx_eth_mac_t* mac, nx_result_t terminal,
                     nx_time_us_t now) {
    if (!mac->tx_closing) {
        mac->tx_closing = true;
        mac->drain_deadline = nx_deadline_after(now, mac->drain_timeout_us);
        mac->flush_started = false;
    }
    if (terminal == NX_ERROR_IO)
        mac->snapshot.link.up = false;
    write_reg(mac, DMA_CONTROL, read_reg(mac, DMA_CONTROL) & ~DMA_TX);
    write_reg(mac, MAC_CONTROL, read_reg(mac, MAC_CONTROL) & ~8U);
    nx_arch_dsb();
}
static bool tx_quiet(nx_eth_mac_t* mac) {
    if ((read_reg(mac, DMA_CONTROL) & DMA_TX) != 0U ||
        (read_reg(mac, DMA_STATUS) & TX_PROCESS) != 0U ||
        (read_reg(mac, MAC_CONTROL) & 8U) != 0U)
        return false;
    if (!mac->flush_started) {
        write_reg(mac, DMA_CONTROL, read_reg(mac, DMA_CONTROL) | DMA_FLUSH);
        nx_arch_dsb();
        mac->flush_started = true;
    }
    return (read_reg(mac, DMA_CONTROL) & DMA_FLUSH) == 0U &&
           (read_reg(mac, MAC_DEBUG) & TX_DEBUG) == 0U;
}
static void progress_tx(nx_eth_mac_t* mac, nx_time_us_t now) {
    if (mac->snapshot.pending_tx == 0U)
        return;
    size_t index = mac->tx_consumer;
    nx_eth_tx_slot_t* slot = &mac->tx_slots[index];
    nx_eth_tx_request_t* request = slot->request;
    if (request == NULL)
        return;
    nx_eth_descriptor_t* d = &mac->tx_descriptors[index];
    uint32_t status = d->status;
    nx_arch_dmb();
    if (slot->terminal == NX_SUCCESS && mac->tx_closing)
        slot->terminal =
            mac->snapshot.link.up ? NX_ERROR_CANCELLED : NX_ERROR_IO;
    if ((status & OWN) != 0U && slot->terminal == NX_SUCCESS &&
        nx_deadline_expired(request->base.deadline, now)) {
        slot->terminal = NX_ERROR_TIMEOUT;
        close_tx(mac, NX_ERROR_CANCELLED, now);
    }
    bool completed = (status & OWN) == 0U && slot->terminal == NX_SUCCESS;
    if (!completed && mac->tx_closing) {
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
        if (tx_quiet(mac)) {
            NX_ETH_DESCRIPTOR_STATUS(d, status & ~OWN);
            nx_arch_dsb();
            completed = (d->status & OWN) == 0U;
        }
        if (!completed && nx_deadline_expired(mac->drain_deadline, now))
            (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
    }
    if (!completed)
        return;
    nx_result_t terminal = slot->terminal;
    if ((status & OWN) == 0U && terminal == NX_ERROR_CANCELLED &&
        !mac->snapshot.fatal_bus_error && !mac->snapshot.transmit_underflow)
        terminal = NX_SUCCESS;
    if (terminal == NX_SUCCESS &&
        ((status & 0x8000U) != 0U || (status & 0x30000000U) != 0x30000000U))
        terminal = NX_ERROR_IO;
    size_t transferred = terminal == NX_SUCCESS ? request->length : 0U;
    d->buffer = 0U;
    d->control = 0U;
    NX_ETH_DESCRIPTOR_STATUS(d, TX_CHAIN);
    nx_arch_dsb();
    slot->request = NULL;
    slot->terminal = NX_SUCCESS;
    mac->snapshot.pending_tx--;
    mac->tx_consumer = ring_next(index, mac->tx_count);
    nx_request_settle(&request->base, terminal, transferred);
}
static void progress_rx(nx_eth_mac_t* mac, nx_time_us_t now) {
    if (mac->snapshot.closing || !mac->snapshot.link.up)
        return;
    size_t index = mac->rx_producer;
    nx_eth_rx_slot_t* slot = &mac->rx_slots[index];
    nx_eth_descriptor_t* d = &mac->rx_descriptors[index];
    uint32_t status = d->status;
    if (slot->state != NX_ETH_RX_DMA || (status & OWN) != 0U)
        return;
    nx_arch_dmb();
    size_t length = (status >> 16U) & 0x3FFFU;
    uint32_t flags = 0U;
    if ((status & 0x300U) != 0x300U)
        flags |= NX_ETH_RX_FRAGMENT;
    if ((status & 2U) != 0U)
        flags |= NX_ETH_RX_CRC;
    if (length < NX_ETH_FRAME_MIN + 4U || length > NX_ETH_RX_STORAGE_MIN ||
        length > slot->capacity || (status & 0x1000U) != 0U)
        flags |= NX_ETH_RX_LENGTH;
    if ((status & 0x8000U) != 0U && flags == 0U)
        flags |= NX_ETH_RX_DMA_ERROR;
    if ((status & 0x400U) != 0U)
        flags |= NX_ETH_RX_UNSUPPORTED;
    if (flags == 0U && !nx_eth_frame_valid(slot->data, length - 4U))
        flags |= NX_ETH_RX_UNSUPPORTED;
    if (flags != 0U) {
        saturate(&mac->snapshot.discarded);
        mac->snapshot.receive_flags |= flags;
        rearm(mac, index);
    } else if (mac->epoch == UINT64_MAX) {
        mac->snapshot.receive_flags |= NX_ETH_RX_BACKPRESSURE;
        mac->snapshot.closing = true;
        close_tx(mac, NX_ERROR_IO, now);
    } else {
        slot->epoch = ++mac->epoch;
        slot->length = length - 4U;
        slot->timestamp_us = now;
        slot->state = NX_ETH_RX_READY;
        slot->ready_next = SIZE_MAX;
        if (mac->rx_consumer == SIZE_MAX)
            mac->rx_consumer = index;
        else
            mac->rx_slots[mac->rx_ready_tail].ready_next = index;
        mac->rx_ready_tail = index;
        saturate(&mac->snapshot.received);
    }
    mac->rx_producer = ring_next(index, mac->rx_count);
}
static void progress(nx_eth_mac_t* mac, nx_time_us_t now) {
    uint32_t status = read_reg(mac, DMA_STATUS);
    if ((status & DMA_FATAL) != 0U) {
        mac->snapshot.fatal_bus_error = true;
        mac->snapshot.closing = true;
        close_tx(mac, NX_ERROR_IO, now);
        write_reg(mac, DMA_CONTROL, read_reg(mac, DMA_CONTROL) & ~DMA_RX);
        write_reg(mac, MAC_CONTROL, read_reg(mac, MAC_CONTROL) & ~4U);
    }
    if ((status & 0x20U) != 0U) {
        mac->snapshot.transmit_underflow = true;
        close_tx(mac, NX_ERROR_IO, now);
    }
    if ((status & 0x90U) != 0U) {
        saturate(&mac->snapshot.unavailable_events);
        mac->snapshot.receive_flags |= NX_ETH_RX_BACKPRESSURE;
    }
    write_reg(mac, DMA_STATUS, status & DMA_W1C);
    progress_tx(mac, now);
    progress_rx(mac, now);
}
void nx_eth_mac_irq(nx_eth_mac_t* mac) {
    if (mac == NULL)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!mac->snapshot.started) {
        nx_arch_irq_restore(guard);
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    progress(mac, now);
    bool more_tx = mac->snapshot.pending_tx != 0U &&
                   mac->tx_slots[mac->tx_consumer].request != NULL &&
                   (mac->tx_descriptors[mac->tx_consumer].status & OWN) == 0U;
    bool more_rx = !mac->snapshot.closing && mac->snapshot.link.up &&
                   mac->rx_slots[mac->rx_producer].state == NX_ETH_RX_DMA &&
                   (mac->rx_descriptors[mac->rx_producer].status & OWN) == 0U;
    if ((more_tx || more_rx) && mac->pend_irq != NULL)
        mac->pend_irq(mac);
    const nx_irq_wake_t* wake = mac->wake;
    if (wake != NULL && mac->wake_publishers != UINT16_MAX)
        mac->wake_publishers++;
    else
        wake = NULL;
    nx_arch_irq_restore(guard);
    (void)nx_irq_wake_signal(wake);
    if (wake != NULL) {
        guard = nx_arch_irq_save();
        mac->wake_publishers--;
        nx_arch_irq_restore(guard);
    }
}
static nx_result_t submit(void* context, nx_eth_tx_request_t* request) {
    nx_eth_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || request == NULL || !span(request, sizeof(*request)) ||
        !nx_eth_frame_valid(request->data, request->length))
        return NX_ERROR_INVALID;
    if (!mac->snapshot.started || mac->snapshot.closing || mac->tx_closing)
        return NX_ERROR_STATE;
    if (!mac->snapshot.link.up)
        return NX_ERROR_IO;
    if (control_overlap(mac, request, sizeof(*request)) ||
        control_overlap(mac, request->data, request->length) ||
        overlaps(request, sizeof(*request), request->data, request->length))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < mac->rx_count; ++i)
        if (overlaps(request, sizeof(*request), mac->rx_slots[i].data,
                     mac->rx_slots[i].capacity) ||
            overlaps(request->data, request->length, mac->rx_slots[i].data,
                     mac->rx_slots[i].capacity))
            return NX_ERROR_INVALID;
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        nx_arch_irq_state_t exclusion = nx_arch_irq_save();
        const nx_eth_tx_request_t* active = mac->tx_slots[i].request;
        bool conflict =
            active != NULL &&
            (overlaps(request, sizeof(*request), active, sizeof(*active)) ||
             overlaps(request, sizeof(*request), active->data,
                      active->length) ||
             overlaps(request->data, request->length, active, sizeof(*active)));
        nx_arch_irq_restore(exclusion);
        if (conflict)
            return NX_ERROR_INVALID;
    }
    nx_result_t result =
        domain(mac, request->data, request->length, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_TIMEOUT;
    }
    if (mac->snapshot.closing || mac->tx_closing || !mac->snapshot.link.up) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    size_t index = mac->tx_producer;
    nx_eth_descriptor_t* d = &mac->tx_descriptors[index];
    nx_eth_tx_slot_t* slot = &mac->tx_slots[index];
    if (slot->request != NULL || (d->status & OWN) != 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result == NX_SUCCESS) {
        slot->request = request;
        slot->terminal = NX_SUCCESS;
        mac->snapshot.pending_tx++;
        mac->tx_producer = ring_next(index, mac->tx_count);
        d->control = (uint32_t)request->length;
        d->buffer = address(request->data);
        nx_arch_dmb();
        NX_ETH_DESCRIPTOR_STATUS(d, TX_FRAME | OWN);
        nx_arch_dsb();
        write_reg(mac, DMA_TX_POLL, 0U);
    }
    nx_arch_irq_restore(guard);
    return result;
}
static nx_result_t service(void* context) {
    nx_eth_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL)
        return NX_ERROR_INVALID;
    if (!mac->snapshot.started)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    progress(mac, nx_time_now_us());
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t cancel(void* context, nx_eth_tx_request_t* request) {
    nx_eth_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || request == NULL)
        return NX_ERROR_INVALID;
    if (!mac->snapshot.started)
        return NX_ERROR_STATE;
    size_t index;
    for (index = 0U; index < mac->tx_count; ++index)
        if (mac->tx_slots[index].request == request)
            break;
    if (index == mac->tx_count)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_time_us_t now = nx_time_now_us();
    progress(mac, now);
    if (mac->tx_slots[index].request == request) {
        mac->tx_slots[index].terminal = NX_ERROR_CANCELLED;
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
        close_tx(mac, NX_ERROR_CANCELLED, now);
        progress_tx(mac, now);
    }
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t acquire(void* context, nx_eth_rx_frame_t* frame) {
    nx_eth_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || frame == NULL || !span(frame, sizeof(*frame)))
        return NX_ERROR_INVALID;
    if (output_overlap(mac, frame, sizeof(*frame)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!mac->snapshot.started || mac->snapshot.closing) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    size_t selected = mac->rx_consumer;
    if (selected == SIZE_MAX) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_EMPTY;
    }
    nx_eth_rx_slot_t* slot = &mac->rx_slots[selected];
    mac->rx_consumer = slot->ready_next;
    if (mac->rx_consumer == SIZE_MAX)
        mac->rx_ready_tail = SIZE_MAX;
    slot->state = NX_ETH_RX_HELD;
    mac->snapshot.held_rx++;
    *frame = (nx_eth_rx_frame_t){.data = slot->data,
                                 .length = slot->length,
                                 .slot = selected,
                                 .identity = mac,
                                 .epoch = slot->epoch,
                                 .timestamp_us = slot->timestamp_us};
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t release(void* context, const nx_eth_rx_frame_t* frame) {
    nx_eth_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || frame == NULL || frame->slot >= mac->rx_count)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_eth_rx_slot_t* slot = &mac->rx_slots[frame->slot];
    if (slot->state != NX_ETH_RX_HELD || frame->identity != mac ||
        slot->epoch != frame->epoch || slot->data != frame->data ||
        slot->length != frame->length ||
        slot->timestamp_us != frame->timestamp_us) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    mac->snapshot.held_rx--;
    if (!mac->snapshot.closing)
        rearm(mac, frame->slot);
    else
        slot->state = NX_ETH_RX_DMA;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t state(void* context, nx_eth_state_t* output) {
    nx_eth_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || output == NULL ||
        output_overlap(mac, output, sizeof(*output)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    *output = mac->snapshot;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t apply_link(void* context, const nx_eth_link_t* link) {
    nx_eth_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || link == NULL ||
        (link->up && link->speed_mbps != 10U && link->speed_mbps != 100U))
        return NX_ERROR_INVALID;
    if (!mac->snapshot.started || mac->snapshot.closing)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!link->up) {
        mac->snapshot.link = *link;
        close_tx(mac, NX_ERROR_IO, nx_time_now_us());
        write_reg(mac, DMA_CONTROL, read_reg(mac, DMA_CONTROL) & ~DMA_RX);
        write_reg(mac, MAC_CONTROL, read_reg(mac, MAC_CONTROL) & ~4U);
        nx_arch_irq_restore(guard);
        return NX_SUCCESS;
    }
    bool unchanged = mac->snapshot.link.up &&
                     mac->snapshot.link.speed_mbps == link->speed_mbps &&
                     mac->snapshot.link.full_duplex == link->full_duplex;
    if (unchanged && !mac->tx_closing) {
        nx_arch_irq_restore(guard);
        return NX_SUCCESS;
    }
    /* A different PHY mode cannot change a frame already in the MAC/FIFO.
     * The owner first applies link-down and observes both engines idle.
     * Reopening drained TX in the same mode leaves live RX untouched. */
    bool mode_quiet =
        unchanged || ((read_reg(mac, DMA_STATUS) & DMA_PROCESS) == 0U &&
                      (read_reg(mac, DMA_CONTROL) & (DMA_TX | DMA_RX)) == 0U &&
                      (read_reg(mac, MAC_CONTROL) & 12U) == 0U &&
                      (read_reg(mac, MAC_DEBUG) & (TX_DEBUG | RX_DEBUG)) == 0U);
    if (!mode_quiet || mac->snapshot.pending_tx != 0U ||
        (mac->tx_closing && !tx_quiet(mac)) ||
        (read_reg(mac, DMA_STATUS) & TX_PROCESS) != 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    uint32_t control = (link->speed_mbps == 100U ? 0x4000U : 0U) |
                       (link->full_duplex ? 0x800U : 0U) | 12U;
    bool valid = write_checked(mac, MAC_CONTROL, control, 0x480CU) &&
                 write_checked(mac, DMA_CONTROL,
                               read_reg(mac, DMA_CONTROL) | DMA_TX | DMA_RX,
                               DMA_TX | DMA_RX);
    if (valid) {
        mac->snapshot.link = *link;
        mac->tx_closing = false;
        mac->flush_started = false;
    } else {
        mac->snapshot.closing = true;
        close_tx(mac, NX_ERROR_IO, nx_time_now_us());
    }
    nx_arch_irq_restore(guard);
    return valid ? NX_SUCCESS : NX_ERROR_IO;
}
static nx_result_t stop(void* context) {
    nx_eth_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL)
        return NX_ERROR_INVALID;
    if (!mac->retained)
        return NX_SUCCESS;
    if (mac->tx_descriptors == NULL || mac->rx_descriptors == NULL)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    mac->snapshot.closing = true;
    close_tx(mac, NX_ERROR_CANCELLED, nx_time_now_us());
    write_reg(mac, DMA_CONTROL, read_reg(mac, DMA_CONTROL) & ~DMA_RX);
    write_reg(mac, MAC_CONTROL, read_reg(mac, MAC_CONTROL) & ~4U);
    write_reg(mac, DMA_INTERRUPTS, 0U);
    nx_arch_dsb();
    progress_tx(mac, nx_time_now_us());
    bool quiet =
        tx_quiet(mac) && (read_reg(mac, DMA_STATUS) & DMA_PROCESS) == 0U &&
        (read_reg(mac, DMA_CONTROL) & (DMA_TX | DMA_RX | DMA_FLUSH)) == 0U &&
        (read_reg(mac, MAC_CONTROL) & 12U) == 0U &&
        (read_reg(mac, MAC_DEBUG) & RX_DEBUG) == 0U &&
        (read_reg(mac, DMA_INTERRUPTS) & DMA_SOURCES) == 0U &&
        (read_reg(mac, MDIO_CONTROL) & 1U) == 0U &&
        (read_reg(mac, DMA_BUS) & 1U) == 0U;
    size_t pending = mac->snapshot.pending_tx;
    size_t held = mac->snapshot.held_rx;
    uint16_t publishers = mac->wake_publishers;
    nx_arch_irq_restore(guard);
    if (!quiet || pending != 0U || held != 0U || publishers != 0U)
        return NX_ERROR_BUSY;
    if (mac->irq_control != NULL) {
        nx_result_t result = mac->irq_control(mac, false);
        if (result != NX_SUCCESS)
            return result;
    }
    /* A pending IRQ may have captured the old sink during NVIC withdrawal.
     * Join its final callback access before releasing any caller storage.
     * Mark dispatch closed under the same guard; variable ring cleanup then
     * runs unmasked after the actual hardware source has been detached. */
    guard = nx_arch_irq_save();
    if (mac->wake_publishers != 0U || mac->snapshot.pending_tx != 0U ||
        mac->snapshot.held_rx != 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    mac->snapshot.started = false;
    nx_arch_irq_restore(guard);
    for (size_t i = 0U; i < mac->rx_count; ++i) {
        NX_ETH_DESCRIPTOR_STATUS(&mac->rx_descriptors[i], 0U);
        nx_arch_dsb();
        if ((mac->rx_descriptors[i].status & OWN) != 0U)
            return NX_ERROR_BUSY;
        if (mac->rx_slots[i].state == NX_ETH_RX_READY)
            saturate(&mac->snapshot.discarded);
        mac->rx_slots[i].state = NX_ETH_RX_DMA;
    }
    write_reg(mac, DMA_RX_TABLE, 0U);
    write_reg(mac, DMA_TX_TABLE, 0U);
    write_reg(mac, DMA_STATUS, DMA_W1C);
    nx_arch_dsb();
    if (read_reg(mac, DMA_RX_TABLE) != 0U ||
        read_reg(mac, DMA_TX_TABLE) != 0U ||
        (read_reg(mac, DMA_STATUS) & DMA_W1C) != 0U)
        return NX_ERROR_BUSY;
    mac->wake = NULL;
    mac->rx_consumer = mac->rx_ready_tail = SIZE_MAX;
    mac->snapshot.started = false;
    mac->snapshot.link.up = false;
    mac->retained = false;
    return NX_SUCCESS;
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_eth_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    bool busy = !mac->snapshot.started || mac->snapshot.closing ||
                mac->snapshot.pending_tx != 0U || mac->wake_publishers != 0U;
    nx_arch_irq_restore(guard);
    if (busy)
        return NX_ERROR_BUSY;
    if (wake != NULL && wake != mac->wake &&
        output_overlap(mac, wake, sizeof(*wake)))
        return NX_ERROR_INVALID;
    if (syscall_ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_result_t result =
        mac->validate_wake != NULL
            ? mac->validate_wake(mac, wake, syscall_ceiling)
            : nx_irq_wake_validate(wake, &mac->irq_policy, &mac->source);
    if (result == NX_SUCCESS) {
        guard = nx_arch_irq_save();
        if (!mac->snapshot.started || mac->snapshot.closing ||
            mac->snapshot.pending_tx != 0U || mac->wake_publishers != 0U)
            result = NX_ERROR_BUSY;
        else
            mac->wake = wake;
        nx_arch_irq_restore(guard);
    }
    return result;
}
static nx_result_t mdio_transfer(nx_eth_mac_t* mac, uint8_t phy, uint8_t reg,
                                 uint16_t* value, bool write,
                                 nx_time_us_t deadline) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || value == NULL || phy > 31U || reg > 31U ||
        deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (!mac->snapshot.started || mac->snapshot.closing)
        return NX_ERROR_STATE;
    if (!write && output_overlap(mac, value, sizeof(*value)))
        return NX_ERROR_INVALID;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if ((read_reg(mac, MDIO_CONTROL) & 1U) != 0U)
        return NX_ERROR_BUSY;
    if (write)
        write_reg(mac, MDIO_DATA, *value);
    write_reg(mac, MDIO_CONTROL,
              ((uint32_t)phy << 11U) | ((uint32_t)reg << 6U) |
                  mac->mdio_divider | (write ? 2U : 0U) | 1U);
    while ((read_reg(mac, MDIO_CONTROL) & 1U) != 0U) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        NX_ETH_POLL(mac);
    }
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (!write)
        *value = (uint16_t)read_reg(mac, MDIO_DATA);
    return NX_SUCCESS;
}
static nx_result_t mdio_read(void* context, uint8_t phy, uint8_t reg,
                             uint16_t* value, nx_time_us_t deadline) {
    return mdio_transfer(context, phy, reg, value, false, deadline);
}
static nx_result_t mdio_write(void* context, uint8_t phy, uint8_t reg,
                              uint16_t value, nx_time_us_t deadline) {
    return mdio_transfer(context, phy, reg, &value, true, deadline);
}
const nx_eth_ops_t nx_eth_mac_ops = {submit,     cancel,  service,
                                     acquire,    release, state,
                                     apply_link, stop,    attach_wake};
const nx_mdio_ops_t nx_eth_mdio_ops = {mdio_read, mdio_write};
