/**
 * \file            eth_frames_mac.c
 * \brief           Selected enhanced DMA frame loans and hardware MAC clock
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "../private/eth_frames_mac.h"
#include "../private/eth_frames_validation.h"
#ifdef NX_ETH_FRAMES_NATIVE
#include "../native/ethernet_frames_access.h"
#endif
#include <string.h>

enum {
    MAC = 0U,
    FILTER = 4U,
    MDIO = 0x10U,
    MDIO_DATA = 0x14U,
    FLOW = 0x18U,
    VLAN = 0x1cU,
    DEBUG = 0x34U,
    MASK = 0x3cU,
    ADDRESS_HIGH = 0x40U,
    ADDRESS_LOW = 0x44U,
    PTP = 0x700U,
    INCREMENT = 0x704U,
    SECONDS = 0x708U,
    NANOSECONDS = 0x70cU,
    UPDATE_SECONDS = 0x710U,
    UPDATE_NANOSECONDS = 0x714U,
    ADDEND = 0x718U,
    BUS = 0x1000U,
    TX_POLL = 0x1004U,
    RX_POLL = 0x1008U,
    RX_TABLE = 0x100cU,
    TX_TABLE = 0x1010U,
    STATUS = 0x1014U,
    CONTROL = 0x1018U,
    INTERRUPTS = 0x101cU
};
#define OWN              UINT32_C(0x80000000)
#define CHAIN            UINT32_C(0x00100000)
#define FS               UINT32_C(0x10000000)
#define LS               UINT32_C(0x20000000)
#define TTSE             UINT32_C(0x02000000)
#define TTSS             UINT32_C(0x00020000)
#define TX_START         UINT32_C(0x00002000)
#define RX_START         UINT32_C(0x00000002)
#define FLUSH            UINT32_C(0x00100000)
#define TX_PROCESS       UINT32_C(0x00700000)
#define RX_PROCESS       UINT32_C(0x000e0000)
#define TX_DEBUG         UINT32_C(0x01770000)
#define RX_DEBUG         UINT32_C(0x00000077)
#define SOURCES          UINT32_C(0x0001a1e3)
#define W1C              UINT32_C(0x0001e7ff)
#define NANOSECOND_RATE  UINT64_C(1000000000)
#define WRAP_NANOSECONDS ((UINT64_C(1) << 32U) * NANOSECOND_RATE)
#define AMBIGUOUS_US     ((UINT64_C(1) << 31U) * UINT64_C(1000000))
_Static_assert(sizeof(nx_eth_frames_descriptor_t) == 32U,
               "enhanced hardware descriptor stride");
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static size_t next(size_t index, size_t count) {
    return index + 1U == count ? 0U : index + 1U;
}
static size_t advance(size_t index, size_t amount, size_t count) {
    return amount >= count - index ? amount - (count - index) : index + amount;
}
static uint32_t encoded(const void* pointer) {
    return (uint32_t)(uintptr_t)pointer;
}
static uint32_t read_reg(nx_eth_frames_mac_t* mac, unsigned offset) {
    return NX_ETH_FRAMES_READ(mac->registers, offset);
}
static void write_reg(nx_eth_frames_mac_t* mac, unsigned offset,
                      uint32_t value) {
    NX_ETH_FRAMES_WRITE(mac->registers, offset, value);
    if (offset == MAC || offset == FILTER || offset == FLOW || offset == VLAN ||
        offset == ADDRESS_HIGH || offset == ADDRESS_LOW || offset == BUS ||
        offset == CONTROL || offset == PTP) {
        /* Four 2.5 MHz MAC cycles at 10 Mbit/s require 1.6 us. Each
         * uncached Device read consumes at least one stable leased HCLK;
         * ceil(HCLK/500000) reads guarantee >=2 us without a core timer. */
        uint32_t cycles =
            mac->hclk_hz / 500000U + (mac->hclk_hz % 500000U != 0U ? 1U : 0U);
        while (cycles-- != 0U)
            (void)read_reg(mac, offset);
    }
}
static bool checked(nx_eth_frames_mac_t* mac, unsigned offset, uint32_t value,
                    uint32_t mask) {
    write_reg(mac, offset, value);
    return (read_reg(mac, offset) & mask) == (value & mask);
}
static void saturate(uint64_t* value) {
    if (*value != UINT64_MAX)
        ++*value;
}
static nx_result_t domain(nx_eth_frames_mac_t* mac, const void* data,
                          size_t length, nx_dma_direction_t direction) {
    if (!NX_ETH_FRAMES_HOST_ADDRESSES &&
        ((uintptr_t)data > UINT32_MAX || length > UINT32_MAX - (uintptr_t)data))
        return NX_ERROR_PERMISSION;
    return nx_dma_buffer_validate(mac->regions, mac->region_count, data, length,
                                  4U, direction);
}
static bool control_overlap(nx_eth_frames_mac_t* mac, const void* data,
                            size_t length) {
    return nx_eth_frames_overlap(data, length, mac, sizeof(*mac)) ||
           nx_eth_frames_overlap(data, length, (const void*)mac->registers,
                                 0x1060U) ||
           nx_eth_frames_overlap(data, length, &nx_eth_frames_mac_ops,
                                 sizeof(nx_eth_frames_mac_ops)) ||
           nx_eth_frames_overlap(data, length, &nx_eth_frames_mdio_ops,
                                 sizeof(nx_eth_frames_mdio_ops)) ||
           nx_eth_frames_overlap(data, length, mac->regions,
                                 mac->region_count * sizeof(*mac->regions)) ||
           nx_eth_frames_overlap(data, length, mac->tx_descriptors,
                                 mac->tx_count *
                                     sizeof(*mac->tx_descriptors)) ||
           nx_eth_frames_overlap(data, length, mac->rx_descriptors,
                                 mac->rx_count *
                                     sizeof(*mac->rx_descriptors)) ||
           nx_eth_frames_overlap(data, length, mac->tx_slots,
                                 mac->tx_count * sizeof(*mac->tx_slots)) ||
           nx_eth_frames_overlap(data, length, mac->rx_slots,
                                 mac->rx_count * sizeof(*mac->rx_slots)) ||
           (mac->wake != NULL &&
            nx_eth_frames_overlap(data, length, mac->wake, sizeof(*mac->wake)));
}
static bool request_overlap(const nx_eth_frames_tx_request_t* request,
                            const void* data, size_t length) {
    if (nx_eth_frames_overlap(data, length, request, sizeof(*request)) ||
        nx_eth_frames_overlap(data, length, request->fragments,
                              request->fragment_count *
                                  sizeof(*request->fragments)))
        return true;
    for (size_t i = 0U; i < request->fragment_count; ++i)
        if (nx_eth_frames_overlap(data, length, request->fragments[i].data,
                                  request->fragments[i].length))
            return true;
    return false;
}
static bool output_overlap(nx_eth_frames_mac_t* mac, const void* output,
                           size_t length) {
    if (!nx_eth_frames_span(output, length) ||
        control_overlap(mac, output, length))
        return true;
    for (size_t i = 0U; i < mac->rx_count; ++i)
        if (nx_eth_frames_overlap(output, length, mac->rx_slots[i].data,
                                  mac->rx_slots[i].capacity))
            return true;
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        const nx_eth_frames_tx_request_t* request = mac->tx_slots[i].request;
        bool conflict =
            request != NULL && request_overlap(request, output, length);
        nx_arch_irq_restore(guard);
        if (conflict)
            return true;
    }
    return false;
}
static nx_result_t validate_storage(nx_eth_frames_mac_t* mac) {
    if (mac == NULL || mac->registers == 0U || mac->clock_domain == 0U ||
        mac->clock_epoch == UINT32_MAX || mac->rx_epoch == UINT64_MAX ||
        mac->tx_count == 0U || mac->rx_count == 0U || mac->region_count == 0U ||
        mac->tx_count > SIZE_MAX / sizeof(*mac->tx_descriptors) ||
        mac->tx_count > SIZE_MAX / sizeof(*mac->tx_slots) ||
        mac->rx_count > SIZE_MAX / sizeof(*mac->rx_descriptors) ||
        mac->rx_count > SIZE_MAX / sizeof(*mac->rx_slots) ||
        mac->region_count > SIZE_MAX / sizeof(*mac->regions) ||
        mac->drain_timeout_us == 0U ||
        (mac->hclk_hz != 168000000U && mac->hclk_hz != 200000000U) ||
        (mac->address[0] & 1U) != 0U)
        return NX_ERROR_INVALID;
    unsigned nonzero = 0U;
    for (size_t i = 0U; i < 6U; ++i)
        nonzero |= mac->address[i];
    if (nonzero == 0U)
        return NX_ERROR_INVALID;
    const void* pointers[] = {
        mac,           mac->regions, mac->tx_descriptors, mac->rx_descriptors,
        mac->tx_slots, mac->rx_slots};
    const size_t sizes[] = {sizeof(*mac),
                            mac->region_count * sizeof(*mac->regions),
                            mac->tx_count * sizeof(*mac->tx_descriptors),
                            mac->rx_count * sizeof(*mac->rx_descriptors),
                            mac->tx_count * sizeof(*mac->tx_slots),
                            mac->rx_count * sizeof(*mac->rx_slots)};
    for (size_t i = 0U; i < 6U; ++i) {
        if (!nx_eth_frames_span(pointers[i], sizes[i]))
            return NX_ERROR_INVALID;
        for (size_t j = 0U; j < i; ++j)
            if (nx_eth_frames_overlap(pointers[i], sizes[i], pointers[j],
                                      sizes[j]))
                return NX_ERROR_INVALID;
    }
    nx_result_t result =
        nx_irq_wake_validate(NULL, &mac->irq_policy, &mac->source);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 2U; i < 4U; ++i) {
        result = domain(mac, pointers[i], sizes[i], NX_DMA_FROM_DEVICE);
        if (result == NX_SUCCESS)
            result = domain(mac, pointers[i], sizes[i], NX_DMA_TO_DEVICE);
        if (result != NX_SUCCESS)
            return result;
    }
    for (size_t i = 0U; i < mac->tx_count; ++i)
        if (mac->tx_slots[i].request != NULL)
            return NX_ERROR_BUSY;
    size_t capacity = 0U;
    for (size_t i = 0U; i < mac->rx_count; ++i) {
        nx_eth_frames_rx_slot_t* slot = &mac->rx_slots[i];
        if (!nx_eth_frames_span(slot->data, slot->capacity) ||
            slot->capacity < 64U || slot->capacity > 8191U ||
            control_overlap(mac, slot->data, slot->capacity) ||
            slot->capacity > SIZE_MAX - capacity)
            return NX_ERROR_INVALID;
        capacity += slot->capacity;
        if (slot->state != NX_ETH_FRAMES_RX_DMA)
            return NX_ERROR_BUSY;
        result = domain(mac, slot->data, slot->capacity, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
        for (size_t j = 0U; j < i; ++j)
            if (nx_eth_frames_overlap(slot->data, slot->capacity,
                                      mac->rx_slots[j].data,
                                      mac->rx_slots[j].capacity))
                return NX_ERROR_INVALID;
    }
    return capacity < 1522U ? NX_ERROR_INVALID : NX_SUCCESS;
}
static void rearm(nx_eth_frames_mac_t* mac, size_t index) {
    nx_eth_frames_rx_slot_t* slot = &mac->rx_slots[index];
    nx_eth_frames_descriptor_t* descriptor = &mac->rx_descriptors[index];
    slot->state = NX_ETH_FRAMES_RX_DMA;
    slot->length = slot->count = slot->flags = 0U;
    slot->armed_us = nx_time_now_us();
    descriptor->extended = descriptor->reserved = 0U;
    descriptor->timestamp_low = descriptor->timestamp_high = 0U;
    nx_arch_dmb();
    NX_ETH_FRAMES_DESCRIPTOR_STATUS(descriptor, OWN);
    nx_arch_dsb();
}
static nx_result_t wait_clear(nx_eth_frames_mac_t* mac, unsigned offset,
                              uint32_t mask, nx_time_us_t deadline) {
    while ((read_reg(mac, offset) & mask) != 0U) {
        if (nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
        NX_ETH_FRAMES_POLL(mac);
    }
    return nx_deadline_expired(deadline, nx_time_now_us()) ? NX_ERROR_TIMEOUT
                                                           : NX_SUCCESS;
}
static nx_result_t start_clock(nx_eth_frames_mac_t* mac,
                               nx_time_us_t deadline) {
    if (mac->clock_epoch == UINT32_MAX)
        return NX_ERROR_EXHAUSTED;
    ++mac->clock_epoch;
    mac->clock_observed = false;
    mac->clock_wrap_nanoseconds = 0U;
    mac->clock_last_seconds = 0U;
    mac->clock_last_nanoseconds = 0U;
    mac->snapshot.clock_epoch = mac->clock_epoch;
    mac->snapshot.timestamp_resolution_ns = 20U;
    /* Digital rollover, timestamp every received frame, fine update. Desired
     * accumulator overflow is 50 MHz; input is HCLK, never RMII REF_CLK. */
    if (!checked(mac, INCREMENT, 20U, 0xffU) ||
        !checked(mac, ADDEND,
                 mac->hclk_hz == 168000000U ? 1278264076U : 1073741824U,
                 UINT32_MAX) ||
        !checked(mac, PTP, 0x323U, 0x303U))
        return NX_ERROR_IO;
    nx_result_t result = wait_clear(mac, PTP, 0x20U, deadline);
    if (result != NX_SUCCESS)
        return result;
    write_reg(mac, UPDATE_SECONDS, 0U);
    write_reg(mac, UPDATE_NANOSECONDS, 0U);
    write_reg(mac, PTP, 0x307U);
    return wait_clear(mac, PTP, 4U, deadline);
}
nx_result_t nx_eth_frames_mac_initialize(nx_eth_frames_mac_t* mac,
                                         nx_time_us_t deadline) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    nx_result_t result = validate_storage(mac);
    if (result != NX_SUCCESS)
        return result;
    if (mac->snapshot.mac.started || mac->retained)
        return NX_ERROR_BUSY;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if ((read_reg(mac, CONTROL) & (TX_START | RX_START)) != 0U ||
        (read_reg(mac, STATUS) & (TX_PROCESS | RX_PROCESS)) != 0U ||
        (read_reg(mac, MAC) & 12U) != 0U || (read_reg(mac, MDIO) & 1U) != 0U ||
        read_reg(mac, TX_TABLE) != 0U || read_reg(mac, RX_TABLE) != 0U)
        return NX_ERROR_BUSY;
    for (size_t i = 0U; i < mac->tx_count; ++i)
        if ((mac->tx_descriptors[i].status & OWN) != 0U)
            return NX_ERROR_BUSY;
    for (size_t i = 0U; i < mac->rx_count; ++i)
        if ((mac->rx_descriptors[i].status & OWN) != 0U)
            return NX_ERROR_BUSY;
    mac->retained = true;
    write_reg(mac, BUS, 1U);
    result = wait_clear(mac, BUS, 1U, deadline);
    if (result != NX_SUCCESS)
        return result;
    bool valid = checked(mac, BUS, (32U << 8U) | 0x80U, UINT32_MAX) &&
                 checked(mac, MAC, 0U, 0x02ffffffU) &&
                 checked(mac, FILTER, 0U, UINT32_MAX) &&
                 checked(mac, FLOW, 0U, UINT32_MAX) &&
                 checked(mac, VLAN, 0U, UINT32_MAX) &&
                 checked(mac, MASK, 0x208U, 0x208U) &&
                 checked(mac, INTERRUPTS, 0U, SOURCES) &&
                 checked(mac, CONTROL, 0x02200080U, UINT32_MAX);
    uint32_t low =
        (uint32_t)mac->address[0] | ((uint32_t)mac->address[1] << 8U) |
        ((uint32_t)mac->address[2] << 16U) | ((uint32_t)mac->address[3] << 24U);
    uint32_t high =
        (uint32_t)mac->address[4] | ((uint32_t)mac->address[5] << 8U);
    if (!valid || !checked(mac, ADDRESS_LOW, low, UINT32_MAX) ||
        !checked(mac, ADDRESS_HIGH, high | OWN, 0xffffU))
        return NX_ERROR_IO;
    mac->snapshot = (nx_eth_frames_state_t){0};
    result = start_clock(mac, deadline);
    if (result != NX_SUCCESS)
        return result;
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        nx_eth_frames_descriptor_t* descriptor = &mac->tx_descriptors[i];
        descriptor->buffer = descriptor->control = 0U;
        descriptor->extended = descriptor->reserved = 0U;
        descriptor->timestamp_low = descriptor->timestamp_high = 0U;
        descriptor->next =
            encoded(&mac->tx_descriptors[next(i, mac->tx_count)]);
        NX_ETH_FRAMES_DESCRIPTOR_STATUS(descriptor, CHAIN);
        mac->tx_slots[i] = (nx_eth_frames_tx_slot_t){0};
    }
    for (size_t i = 0U; i < mac->rx_count; ++i) {
        nx_eth_frames_descriptor_t* descriptor = &mac->rx_descriptors[i];
        descriptor->buffer = encoded(mac->rx_slots[i].data);
        descriptor->control = 0x4000U | (uint32_t)mac->rx_slots[i].capacity;
        descriptor->next =
            encoded(&mac->rx_descriptors[next(i, mac->rx_count)]);
        rearm(mac, i);
    }
    nx_arch_dsb();
    if (!checked(mac, TX_TABLE, encoded(mac->tx_descriptors), UINT32_MAX) ||
        !checked(mac, RX_TABLE, encoded(mac->rx_descriptors), UINT32_MAX))
        return NX_ERROR_IO;
    mac->tx_producer = mac->tx_consumer = mac->tx_in_use = mac->rx_producer =
        0U;
    mac->rx_ready_head = mac->rx_ready_tail = SIZE_MAX;
    mac->tx_closing = false;
    mac->flush_started = false;
    mac->drain_deadline = NX_DEADLINE_NEVER;
    mac->mdio_divider = 16U; /* HCLK/102 at the two selected clock profiles. */
    write_reg(mac, STATUS, W1C);
    mac->snapshot.mac.started = true;
    if (mac->irq_control != NULL) {
        result = mac->irq_control(mac, true);
        if (result != NX_SUCCESS) {
            mac->snapshot.mac.closing = true;
            return result;
        }
    }
    if (!checked(mac, INTERRUPTS, SOURCES, SOURCES)) {
        mac->snapshot.mac.closing = true;
        return NX_ERROR_IO;
    }
    return NX_SUCCESS;
}
static nx_result_t observe_clock(nx_eth_frames_mac_t* mac,
                                 nx_io_timestamp_t* stamp) {
    nx_time_us_t now = nx_time_now_us();
    if (mac->clock_observed &&
        (now < mac->clock_last_sample_us ||
         now - mac->clock_last_sample_us >= AMBIGUOUS_US))
        return NX_ERROR_STATE;
    uint32_t seconds = 0U, nanos = 0U;
    bool coherent = false;
    for (unsigned i = 0U; i < 4U; ++i) {
        seconds = read_reg(mac, SECONDS);
        nanos = read_reg(mac, NANOSECONDS);
        if (read_reg(mac, SECONDS) == seconds) {
            coherent = true;
            break;
        }
    }
    if (!coherent || nanos >= 1000000000U)
        return NX_ERROR_IO;
    if (mac->clock_observed && seconds == mac->clock_last_seconds &&
        nanos < mac->clock_last_nanoseconds)
        return NX_ERROR_STATE;
    uint64_t wrap = mac->clock_wrap_nanoseconds;
    if (mac->clock_observed && seconds < mac->clock_last_seconds) {
        if (mac->clock_last_seconds - seconds <= UINT32_MAX / 2U ||
            wrap > UINT64_MAX - WRAP_NANOSECONDS)
            return NX_ERROR_STATE;
        wrap += WRAP_NANOSECONDS;
    }
    uint64_t low = (uint64_t)seconds * NANOSECOND_RATE + nanos;
    if (wrap > UINT64_MAX - low)
        return NX_ERROR_EXHAUSTED;
    mac->clock_observed = true;
    mac->clock_last_seconds = seconds;
    mac->clock_last_nanoseconds = nanos;
    mac->clock_last_sample_us = now;
    mac->clock_wrap_nanoseconds = wrap;
    *stamp = (nx_io_timestamp_t){.ticks = wrap + low,
                                 .ticks_per_second = NANOSECOND_RATE,
                                 .domain = mac->clock_domain,
                                 .epoch = mac->clock_epoch,
                                 .source = NX_IO_TIMESTAMP_SERVICE_OBSERVATION};
    return NX_SUCCESS;
}
static nx_io_timestamp_t capture(nx_eth_frames_mac_t* mac,
                                 nx_eth_frames_descriptor_t* descriptor,
                                 bool valid, nx_time_us_t admitted,
                                 uint32_t* flags) {
    nx_io_timestamp_t stamp = {0};
    if (!valid)
        return stamp;
    uint32_t seconds = descriptor->timestamp_high;
    uint32_t nanos = descriptor->timestamp_low;
    nx_io_timestamp_t current;
    if ((seconds == UINT32_MAX && nanos == UINT32_MAX) ||
        nanos >= 1000000000U || nx_time_now_us() < admitted ||
        nx_time_now_us() - admitted >= AMBIGUOUS_US ||
        observe_clock(mac, &current) != NX_SUCCESS) {
        *flags |= NX_ETH_FRAMES_CAPTURE_CORRUPT;
        return stamp;
    }
    uint64_t wrap = mac->clock_wrap_nanoseconds;
    if (seconds > mac->clock_last_seconds &&
        seconds - mac->clock_last_seconds > UINT32_MAX / 2U) {
        if (wrap < WRAP_NANOSECONDS) {
            *flags |= NX_ETH_FRAMES_CAPTURE_CORRUPT;
            return stamp;
        }
        wrap -= WRAP_NANOSECONDS;
    }
    uint64_t low = (uint64_t)seconds * NANOSECOND_RATE + nanos;
    if (wrap > UINT64_MAX - low || wrap + low > current.ticks) {
        *flags |= NX_ETH_FRAMES_CAPTURE_CORRUPT;
        return stamp;
    }
    stamp = current;
    stamp.ticks = wrap + low;
    stamp.source = NX_IO_TIMESTAMP_HARDWARE_EVENT;
    return stamp;
}
static void close_tx(nx_eth_frames_mac_t* mac, nx_time_us_t now) {
    if (!mac->tx_closing) {
        mac->tx_closing = true;
        mac->flush_started = false;
        mac->drain_deadline = nx_deadline_after(now, mac->drain_timeout_us);
    }
    write_reg(mac, CONTROL, read_reg(mac, CONTROL) & ~TX_START);
    write_reg(mac, MAC, read_reg(mac, MAC) & ~8U);
    nx_arch_dsb();
}
static bool tx_quiet(nx_eth_frames_mac_t* mac) {
    NX_ETH_FRAMES_POLL(mac);
    if ((read_reg(mac, CONTROL) & TX_START) != 0U ||
        (read_reg(mac, STATUS) & TX_PROCESS) != 0U ||
        (read_reg(mac, MAC) & 8U) != 0U ||
        (read_reg(mac, DEBUG) & TX_DEBUG) != 0U)
        return false;
    if (!mac->flush_started) {
        write_reg(mac, CONTROL, read_reg(mac, CONTROL) | FLUSH);
        mac->flush_started = true;
    }
    NX_ETH_FRAMES_POLL(mac);
    return (read_reg(mac, CONTROL) & FLUSH) == 0U;
}
static void progress_tx(nx_eth_frames_mac_t* mac, nx_time_us_t now) {
    if (mac->snapshot.mac.pending_tx == 0U)
        return;
    nx_eth_frames_tx_slot_t* slot = &mac->tx_slots[mac->tx_consumer];
    if (slot->request == NULL || slot->phase == NX_ETH_FRAMES_TX_PREPARING)
        return;
    nx_eth_frames_tx_request_t* request = slot->request;
    size_t index = advance(slot->first, slot->returned, mac->tx_count);
    nx_eth_frames_descriptor_t* descriptor = &mac->tx_descriptors[index];
    uint32_t bits = descriptor->status;
    nx_arch_dmb();
    if (slot->terminal == NX_SUCCESS && (bits & OWN) != 0U &&
        nx_deadline_expired(request->base.deadline, now)) {
        slot->terminal = NX_ERROR_TIMEOUT;
        close_tx(mac, now);
    }
    if ((bits & OWN) != 0U) {
        if (!mac->tx_closing || !tx_quiet(mac))
            return;
        if (slot->terminal == NX_SUCCESS)
            slot->terminal = NX_ERROR_CANCELLED;
        NX_ETH_FRAMES_DESCRIPTOR_STATUS(descriptor, bits & ~OWN);
        nx_arch_dsb();
        if ((descriptor->status & OWN) != 0U)
            return;
    } else if ((bits & 0x8000U) != 0U && slot->terminal == NX_SUCCESS) {
        slot->terminal = NX_ERROR_IO;
    }
    bool last = slot->returned + 1U == slot->count;
    if (last && slot->terminal == NX_ERROR_CANCELLED && (bits & OWN) == 0U &&
        (bits & 0x8000U) == 0U && !mac->snapshot.mac.fatal_bus_error &&
        !mac->snapshot.mac.transmit_underflow)
        slot->terminal = NX_SUCCESS;
    if (last && slot->terminal == NX_SUCCESS && request->timestamp_requested)
        request->stamp = capture(mac, descriptor, (bits & TTSS) != 0U,
                                 slot->admitted_us, &request->capture_flags);
    descriptor->buffer = descriptor->control = 0U;
    NX_ETH_FRAMES_DESCRIPTOR_STATUS(descriptor, CHAIN);
    nx_arch_dsb();
    ++slot->returned;
    if (!last)
        return;
    nx_result_t result = slot->terminal;
    size_t length = slot->length;
    size_t descriptors = slot->count;
    size_t successor = advance(slot->first, slot->count, mac->tx_count);
    slot->request = NULL;
    slot->phase = NX_ETH_FRAMES_TX_FREE;
    mac->tx_in_use -= descriptors;
    --mac->snapshot.mac.pending_tx;
    mac->tx_consumer = successor;
    nx_request_settle(&request->base, result,
                      result == NX_SUCCESS ? length : 0U);
}
static uint32_t receive_flags(uint32_t bits) {
    uint32_t flags = 0U;
    if ((bits & 2U) != 0U)
        flags |= NX_ETH_RX_CRC;
    if ((bits & 0x5000U) != 0U)
        flags |= NX_ETH_RX_LENGTH;
    if ((bits & 0x8000U) != 0U)
        flags |= NX_ETH_RX_DMA_ERROR;
    return flags;
}
static void progress_rx(nx_eth_frames_mac_t* mac) {
    if (mac->snapshot.mac.closing || !mac->snapshot.mac.link.up)
        return;
    size_t index = mac->rx_producer;
    nx_eth_frames_rx_slot_t* slot = &mac->rx_slots[index];
    uint32_t bits = mac->rx_descriptors[index].status;
    if (slot->state != NX_ETH_FRAMES_RX_DMA || (bits & OWN) != 0U)
        return;
    nx_arch_dmb();
    size_t first = index;
    size_t previous = index == 0U ? mac->rx_count - 1U : index - 1U;
    if ((bits & 0x200U) == 0U &&
        mac->rx_slots[previous].state == NX_ETH_FRAMES_RX_PARTIAL)
        first = mac->rx_slots[previous].first;
    nx_eth_frames_rx_slot_t* head = &mac->rx_slots[first];
    if (first == index) {
        head->count = 0U;
        head->length = 0U;
        head->flags = (bits & 0x200U) == 0U ? NX_ETH_RX_FRAGMENT : 0U;
        head->stamp = (nx_io_timestamp_t){0};
        head->capture_flags = 0U;
    }
    slot->state = NX_ETH_FRAMES_RX_PARTIAL;
    slot->first = first;
    size_t preceding = head->length;
    head->length += slot->capacity;
    ++head->count;
    head->flags |= receive_flags(bits);
    mac->rx_producer = next(index, mac->rx_count);
    if ((bits & 0x100U) == 0U)
        return;
    size_t wire = (bits >> 16U) & 0x3fffU;
    if (wire < 18U || wire > 1522U || wire <= preceding ||
        wire - preceding > slot->capacity || (bits & 0x20U) == 0U)
        head->flags |= NX_ETH_RX_LENGTH;
    head->length = wire >= 4U ? wire - 4U : 0U;
    /* RX buffers have at least 64 bytes, so the complete L2 header is in
     * the first descriptor. No frame-sized scratch or hidden copy is used. */
    nx_eth_fragment_t header = {head->data, head->length};
    if (!nx_eth_frames_valid(&header, 1U, NULL, &head->vlan))
        head->flags |= NX_ETH_RX_UNSUPPORTED;
    if (mac->rx_epoch == UINT64_MAX)
        head->flags |= NX_ETH_RX_BACKPRESSURE;
    if (head->flags == 0U) {
        head->epoch = ++mac->rx_epoch;
        head->stamp =
            capture(mac, &mac->rx_descriptors[index], (bits & 0x80U) != 0U,
                    head->armed_us, &head->capture_flags);
        saturate(&mac->snapshot.mac.received);
    } else {
        saturate(&mac->snapshot.mac.discarded);
        mac->snapshot.mac.receive_flags |= head->flags;
    }
    head->state = NX_ETH_FRAMES_RX_READY;
    head->ready_next = SIZE_MAX;
    if (mac->rx_ready_tail == SIZE_MAX)
        mac->rx_ready_head = first;
    else
        mac->rx_slots[mac->rx_ready_tail].ready_next = first;
    mac->rx_ready_tail = first;
}
static void progress(nx_eth_frames_mac_t* mac, nx_time_us_t now) {
    uint32_t status = read_reg(mac, STATUS);
    if ((status & 0x2020U) != 0U) {
        mac->snapshot.mac.fatal_bus_error |= (status & 0x2000U) != 0U;
        mac->snapshot.mac.transmit_underflow |= (status & 0x20U) != 0U;
        close_tx(mac, now);
    }
    if ((status & 0x90U) != 0U) {
        saturate(&mac->snapshot.mac.unavailable_events);
        mac->snapshot.mac.receive_flags |= NX_ETH_RX_BACKPRESSURE;
    }
    write_reg(mac, STATUS, status & W1C);
    progress_tx(mac, now);
    progress_rx(mac);
}
void nx_eth_frames_mac_irq(nx_eth_frames_mac_t* mac) {
    if (mac == NULL)
        return;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!mac->snapshot.mac.started) {
        nx_arch_irq_restore(guard);
        return;
    }
    progress(mac, nx_time_now_us());
    nx_eth_frames_tx_slot_t* pending = &mac->tx_slots[mac->tx_consumer];
    bool more = mac->snapshot.mac.pending_tx != 0U && !mac->tx_closing &&
                pending->phase == NX_ETH_FRAMES_TX_DMA &&
                (mac->tx_descriptors[advance(pending->first, pending->returned,
                                             mac->tx_count)]
                     .status &
                 OWN) == 0U;
    more |= !mac->snapshot.mac.closing &&
            mac->rx_slots[mac->rx_producer].state == NX_ETH_FRAMES_RX_DMA &&
            (mac->rx_descriptors[mac->rx_producer].status & OWN) == 0U;
    if (more && mac->pend_irq != NULL)
        mac->pend_irq(mac);
    const nx_irq_wake_t* wake = mac->wake;
    if (wake != NULL && mac->wake_publishers != UINT16_MAX)
        ++mac->wake_publishers;
    else
        wake = NULL;
    nx_arch_irq_restore(guard);
    (void)nx_irq_wake_signal(wake);
    if (wake != NULL) {
        guard = nx_arch_irq_save();
        --mac->wake_publishers;
        nx_arch_irq_restore(guard);
    }
}
static nx_result_t submit(void* context, nx_eth_frames_tx_request_t* request) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || !nx_eth_frames_span(request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (!mac->snapshot.mac.started || mac->snapshot.mac.closing ||
        mac->tx_closing)
        return NX_ERROR_STATE;
    if (!mac->snapshot.mac.link.up)
        return NX_ERROR_IO;
    if (output_overlap(mac, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    /* Prove metadata and DMA spans before interpreting either the fragment
     * array or Ethernet header. Rejection must not read a control window. */
    if (request->fragment_count == 0U ||
        request->fragment_count > mac->tx_count ||
        request->fragment_count > SIZE_MAX / sizeof(*request->fragments) ||
        output_overlap(mac, request->fragments,
                       request->fragment_count * sizeof(*request->fragments)) ||
        nx_eth_frames_overlap(request, sizeof(*request), request->fragments,
                              request->fragment_count *
                                  sizeof(*request->fragments)))
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < request->fragment_count; ++i) {
        const nx_eth_fragment_t* piece = &request->fragments[i];
        if (output_overlap(mac, piece->data, piece->length) ||
            nx_eth_frames_overlap(request, sizeof(*request), piece->data,
                                  piece->length) ||
            nx_eth_frames_overlap(request->fragments,
                                  request->fragment_count *
                                      sizeof(*request->fragments),
                                  piece->data, piece->length))
            return NX_ERROR_INVALID;
        nx_result_t result =
            domain(mac, piece->data, piece->length, NX_DMA_TO_DEVICE);
        if (result != NX_SUCCESS)
            return result;
    }
    size_t length;
    if (!nx_eth_frames_valid(request->fragments, request->fragment_count,
                             &length, NULL))
        return NX_ERROR_INVALID;
    size_t first = mac->tx_producer;
    for (size_t i = 0U, cursor = first; i < request->fragment_count; ++i) {
        if ((mac->tx_descriptors[cursor].status & OWN) != 0U ||
            mac->tx_slots[cursor].request != NULL)
            return NX_ERROR_BUSY;
        cursor = next(cursor, mac->tx_count);
    }
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_TIMEOUT;
    }
    if (mac->snapshot.mac.closing || mac->tx_closing ||
        !mac->snapshot.mac.link.up || mac->tx_producer != first ||
        request->fragment_count > mac->tx_count - mac->tx_in_use) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(guard);
        return result;
    }
    nx_eth_frames_tx_slot_t* slot = &mac->tx_slots[first];
    *slot = (nx_eth_frames_tx_slot_t){.request = request,
                                      .first = first,
                                      .count = request->fragment_count,
                                      .length = length,
                                      .admitted_us = nx_time_now_us(),
                                      .terminal = NX_SUCCESS,
                                      .phase = NX_ETH_FRAMES_TX_PREPARING};
    ++mac->snapshot.mac.pending_tx;
    mac->tx_in_use += request->fragment_count;
    mac->tx_producer = advance(first, request->fragment_count, mac->tx_count);
    nx_arch_irq_restore(guard);
    /* Variable descriptor preparation is unmasked. PREPARING is not a DMA
     * completion fact. Only the first OWN publication releases the chain. */
    for (size_t i = 0U, cursor = first; i < request->fragment_count; ++i) {
        nx_eth_frames_descriptor_t* descriptor = &mac->tx_descriptors[cursor];
        descriptor->buffer = encoded(request->fragments[i].data);
        descriptor->control = (uint32_t)request->fragments[i].length;
        descriptor->extended = descriptor->reserved = 0U;
        descriptor->timestamp_low = descriptor->timestamp_high = 0U;
        uint32_t bits = CHAIN;
        if (i == 0U)
            bits |= FS | (request->timestamp_requested ? TTSE : 0U);
        if (i + 1U == request->fragment_count)
            bits |= LS | 0x40000000U;
        nx_arch_dmb();
        NX_ETH_FRAMES_DESCRIPTOR_STATUS(descriptor,
                                        bits | (i == 0U ? 0U : OWN));
        cursor = next(cursor, mac->tx_count);
    }
    guard = nx_arch_irq_save();
    slot->phase = NX_ETH_FRAMES_TX_DMA;
    if (!mac->tx_closing && !mac->snapshot.mac.closing) {
        nx_arch_dmb();
        NX_ETH_FRAMES_DESCRIPTOR_STATUS(&mac->tx_descriptors[first],
                                        mac->tx_descriptors[first].status |
                                            OWN);
        nx_arch_dsb();
        write_reg(mac, TX_POLL, 0U);
    } else {
        slot->terminal = NX_ERROR_IO;
        slot->phase = NX_ETH_FRAMES_TX_DRAINING;
        (void)nx_request_transition(&request->base, NX_REQUEST_DRAINING);
    }
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static void close_loans(nx_eth_frames_mac_t* mac, nx_result_t reason,
                        nx_time_us_t now) {
    /* Task-only ring traversal. Every descriptor operation remains bounded in
     * the IRQ path; controller-wide cancellation is explicit to all loans. */
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        nx_eth_frames_tx_slot_t* slot = &mac->tx_slots[i];
        if (slot->request != NULL &&
            slot->phase != NX_ETH_FRAMES_TX_PREPARING) {
            if (slot->terminal == NX_SUCCESS)
                slot->terminal = reason;
            slot->phase = NX_ETH_FRAMES_TX_DRAINING;
            nx_request_state_t state =
                nx_deadline_expired(mac->drain_deadline, now)
                    ? NX_REQUEST_QUARANTINED
                    : NX_REQUEST_DRAINING;
            (void)nx_request_transition(&slot->request->base, state);
            mac->snapshot.quarantined |= state == NX_REQUEST_QUARANTINED;
        }
        nx_arch_irq_restore(guard);
    }
}
static void recycle_frame(nx_eth_frames_mac_t* mac, size_t first, size_t count,
                          bool publish) {
    for (size_t i = 0U, cursor = first; i < count; ++i) {
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        if (publish)
            rearm(mac, cursor);
        else
            mac->rx_slots[cursor].state = NX_ETH_FRAMES_RX_DMA;
        nx_arch_irq_restore(guard);
        cursor = next(cursor, mac->rx_count);
    }
    if (publish)
        write_reg(mac, RX_POLL, 0U);
}
static void discard_bad(nx_eth_frames_mac_t* mac) {
    for (;;) {
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        size_t first = mac->rx_ready_head;
        if (first == SIZE_MAX || mac->rx_slots[first].flags == 0U) {
            nx_arch_irq_restore(guard);
            return;
        }
        nx_eth_frames_rx_slot_t* slot = &mac->rx_slots[first];
        size_t count = slot->count;
        mac->rx_ready_head = slot->ready_next;
        if (mac->rx_ready_head == SIZE_MAX)
            mac->rx_ready_tail = SIZE_MAX;
        nx_arch_irq_restore(guard);
        recycle_frame(mac, first, count, !mac->snapshot.mac.closing);
    }
}
static nx_result_t service(void* context) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL)
        return NX_ERROR_INVALID;
    if (!mac->snapshot.mac.started)
        return NX_ERROR_STATE;
    NX_ETH_FRAMES_POLL(mac);
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_time_us_t now = nx_time_now_us();
    progress(mac, now);
    bool closing = mac->tx_closing;
    nx_arch_irq_restore(guard);
    if (closing)
        close_loans(mac,
                    mac->snapshot.mac.fatal_bus_error ||
                            mac->snapshot.mac.transmit_underflow
                        ? NX_ERROR_IO
                        : NX_ERROR_CANCELLED,
                    now);
    discard_bad(mac);
    return NX_SUCCESS;
}
static nx_result_t cancel(void* context, nx_eth_frames_tx_request_t* request) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || request == NULL)
        return NX_ERROR_INVALID;
    if (!mac->snapshot.mac.started)
        return NX_ERROR_STATE;
    bool found = false;
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        nx_arch_irq_state_t guard = nx_arch_irq_save();
        found |= mac->tx_slots[i].request == request;
        nx_arch_irq_restore(guard);
    }
    if (!found)
        return NX_ERROR_STATE;
    /* Observe returned hardware facts before locking cancellation. */
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_time_us_t now = nx_time_now_us();
    progress(mac, now);
    close_tx(mac, now);
    nx_arch_irq_restore(guard);
    close_loans(mac, NX_ERROR_CANCELLED, now);
    return NX_SUCCESS;
}
static size_t fragment_count(nx_eth_frames_mac_t* mac,
                             nx_eth_frames_rx_slot_t* head) {
    size_t remaining = head->length, count = 0U, cursor = head->first;
    while (remaining != 0U) {
        size_t bytes = mac->rx_slots[cursor].capacity;
        if (bytes > remaining)
            bytes = remaining;
        remaining -= bytes;
        ++count;
        cursor = next(cursor, mac->rx_count);
    }
    return count;
}
static nx_result_t acquire(void* context, nx_eth_frames_rx_loan_t* loan,
                           nx_eth_fragment_t* fragments, size_t capacity) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || capacity == 0U ||
        capacity > SIZE_MAX / sizeof(*fragments) ||
        output_overlap(mac, loan, sizeof(*loan)) ||
        output_overlap(mac, fragments, capacity * sizeof(*fragments)) ||
        nx_eth_frames_overlap(loan, sizeof(*loan), fragments,
                              capacity * sizeof(*fragments)))
        return NX_ERROR_INVALID;
    if (!mac->snapshot.mac.started || mac->snapshot.mac.closing)
        return NX_ERROR_STATE;
    discard_bad(mac);
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    size_t first = mac->rx_ready_head;
    if (first == SIZE_MAX) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_EMPTY;
    }
    nx_eth_frames_rx_slot_t* head = &mac->rx_slots[first];
    size_t count = fragment_count(mac, head);
    if (count > capacity) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_OVERFLOW;
    }
    mac->rx_ready_head = head->ready_next;
    if (mac->rx_ready_head == SIZE_MAX)
        mac->rx_ready_tail = SIZE_MAX;
    head->state = NX_ETH_FRAMES_RX_HELD;
    /* Queue observation is not acquisition. Record the actual consumer grant
     * only after whole-frame capacity admission has succeeded. */
    head->acquired_us = nx_time_now_us();
    ++mac->snapshot.mac.held_rx;
    *loan = (nx_eth_frames_rx_loan_t){.identity = mac,
                                      .epoch = head->epoch,
                                      .first_slot = first,
                                      .descriptor_count = head->count,
                                      .fragment_count = count,
                                      .length = head->length,
                                      .acquired_us = head->acquired_us,
                                      .acquired_source =
                                          NX_IO_TIMESTAMP_ACQUIRE_OBSERVATION,
                                      .vlan = head->vlan,
                                      .stamp = head->stamp,
                                      .capture_flags = head->capture_flags};
    nx_arch_irq_restore(guard);
    size_t remaining = head->length, cursor = first;
    for (size_t i = 0U; i < count; ++i) {
        size_t bytes = mac->rx_slots[cursor].capacity;
        if (bytes > remaining)
            bytes = remaining;
        fragments[i] = (nx_eth_fragment_t){mac->rx_slots[cursor].data, bytes};
        remaining -= bytes;
        cursor = next(cursor, mac->rx_count);
    }
    return NX_SUCCESS;
}
static nx_result_t release(void* context, const nx_eth_frames_rx_loan_t* loan) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || !nx_eth_frames_span(loan, sizeof(*loan)) ||
        loan->first_slot >= mac->rx_count)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_eth_frames_rx_slot_t* slot = &mac->rx_slots[loan->first_slot];
    if (loan->identity != mac || slot->state != NX_ETH_FRAMES_RX_HELD ||
        slot->epoch != loan->epoch || loan->descriptor_count != slot->count ||
        loan->fragment_count != fragment_count(mac, slot) ||
        loan->length != slot->length ||
        loan->acquired_us != slot->acquired_us ||
        loan->acquired_source != NX_IO_TIMESTAMP_ACQUIRE_OBSERVATION ||
        loan->vlan.present != slot->vlan.present ||
        loan->vlan.tci != slot->vlan.tci ||
        loan->capture_flags != slot->capture_flags ||
        loan->stamp.ticks != slot->stamp.ticks ||
        loan->stamp.ticks_per_second != slot->stamp.ticks_per_second ||
        loan->stamp.domain != slot->stamp.domain ||
        loan->stamp.epoch != slot->stamp.epoch ||
        loan->stamp.uncertainty_ticks != slot->stamp.uncertainty_ticks ||
        loan->stamp.source != slot->stamp.source ||
        loan->stamp.uncertainty_known != slot->stamp.uncertainty_known) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_STATE;
    }
    size_t first = loan->first_slot, count = loan->descriptor_count;
    --mac->snapshot.mac.held_rx;
    bool publish = !mac->snapshot.mac.closing;
    nx_arch_irq_restore(guard);
    recycle_frame(mac, first, count, publish);
    return NX_SUCCESS;
}
static nx_result_t state(void* context, nx_eth_frames_state_t* output) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || output_overlap(mac, output, sizeof(*output)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    *output = mac->snapshot;
    nx_arch_irq_restore(guard);
    return NX_SUCCESS;
}
static nx_result_t clock_read(void* context, nx_io_timestamp_t* output) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || output_overlap(mac, output, sizeof(*output)))
        return NX_ERROR_INVALID;
    if (!mac->snapshot.mac.started || mac->snapshot.mac.closing)
        return NX_ERROR_STATE;
    nx_io_timestamp_t stamp;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    nx_result_t result = observe_clock(mac, &stamp);
    nx_arch_irq_restore(guard);
    if (result == NX_SUCCESS)
        *output = stamp;
    return result;
}
static nx_result_t apply_link(void* context, const nx_eth_link_t* link) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || link == NULL ||
        (link->up && link->speed_mbps != 10U && link->speed_mbps != 100U))
        return NX_ERROR_INVALID;
    if (!mac->snapshot.mac.started || mac->snapshot.mac.closing)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (!link->up) {
        mac->snapshot.mac.link = *link;
        close_tx(mac, nx_time_now_us());
        write_reg(mac, CONTROL, read_reg(mac, CONTROL) & ~RX_START);
        write_reg(mac, MAC, read_reg(mac, MAC) & ~4U);
        nx_arch_irq_restore(guard);
        close_loans(mac, NX_ERROR_IO, nx_time_now_us());
        return NX_SUCCESS;
    }
    bool unchanged = mac->snapshot.mac.link.up &&
                     mac->snapshot.mac.link.speed_mbps == link->speed_mbps &&
                     mac->snapshot.mac.link.full_duplex == link->full_duplex;
    if (unchanged && !mac->tx_closing) {
        nx_arch_irq_restore(guard);
        return NX_SUCCESS;
    }
    NX_ETH_FRAMES_POLL(mac);
    bool quiet = unchanged ||
                 ((read_reg(mac, CONTROL) & (TX_START | RX_START)) == 0U &&
                  (read_reg(mac, STATUS) & (TX_PROCESS | RX_PROCESS)) == 0U &&
                  (read_reg(mac, MAC) & 12U) == 0U &&
                  (read_reg(mac, DEBUG) & (TX_DEBUG | RX_DEBUG)) == 0U);
    if (!quiet || mac->snapshot.mac.pending_tx != 0U ||
        (mac->tx_closing && !tx_quiet(mac))) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    uint32_t bits = (link->speed_mbps == 100U ? 0x4000U : 0U) |
                    (link->full_duplex ? 0x800U : 0U) | 12U;
    bool valid =
        checked(mac, MAC, bits, 0x480cU) &&
        checked(mac, CONTROL, read_reg(mac, CONTROL) | TX_START | RX_START,
                TX_START | RX_START);
    if (valid) {
        mac->snapshot.mac.link = *link;
        mac->tx_closing = false;
        mac->flush_started = false;
    } else {
        mac->snapshot.mac.closing = true;
        close_tx(mac, nx_time_now_us());
    }
    nx_arch_irq_restore(guard);
    return valid ? NX_SUCCESS : NX_ERROR_IO;
}
static nx_result_t detach(nx_eth_frames_mac_t* mac) {
    if (mac->irq_control != NULL) {
        nx_result_t result = mac->irq_control(mac, false);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (mac->wake_publishers != 0U || mac->snapshot.mac.held_rx != 0U ||
        mac->snapshot.mac.pending_tx != 0U) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    mac->snapshot.mac.started = false;
    nx_arch_irq_restore(guard);
    for (size_t i = 0U; i < mac->rx_count; ++i) {
        NX_ETH_FRAMES_DESCRIPTOR_STATUS(&mac->rx_descriptors[i], 0U);
        nx_arch_dsb();
        if ((mac->rx_descriptors[i].status & OWN) != 0U)
            return NX_ERROR_BUSY;
        mac->rx_slots[i].state = NX_ETH_FRAMES_RX_DMA;
    }
    write_reg(mac, RX_TABLE, 0U);
    write_reg(mac, TX_TABLE, 0U);
    write_reg(mac, STATUS, W1C);
    nx_arch_dsb();
    if (read_reg(mac, RX_TABLE) != 0U || read_reg(mac, TX_TABLE) != 0U ||
        (read_reg(mac, STATUS) & W1C) != 0U)
        return NX_ERROR_BUSY;
    mac->wake = NULL;
    mac->rx_ready_head = mac->rx_ready_tail = SIZE_MAX;
    mac->snapshot.mac.link.up = false;
    mac->retained = false;
    return NX_SUCCESS;
}
static nx_result_t stop(void* context) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL)
        return NX_ERROR_INVALID;
    if (!mac->retained)
        return NX_SUCCESS;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    mac->snapshot.mac.closing = true;
    close_tx(mac, nx_time_now_us());
    write_reg(mac, CONTROL, read_reg(mac, CONTROL) & ~RX_START);
    write_reg(mac, MAC, read_reg(mac, MAC) & ~4U);
    write_reg(mac, INTERRUPTS, 0U);
    nx_arch_dsb();
    nx_arch_irq_restore(guard);
    close_loans(mac, NX_ERROR_CANCELLED, nx_time_now_us());
    /* Task stop may visit the whole ring, with only one descriptor progress
     * under each metadata exclusion. Failed proof never revokes a borrow. */
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        guard = nx_arch_irq_save();
        progress_tx(mac, nx_time_now_us());
        nx_arch_irq_restore(guard);
    }
    NX_ETH_FRAMES_POLL(mac);
    guard = nx_arch_irq_save();
    bool quiet =
        tx_quiet(mac) &&
        (read_reg(mac, CONTROL) & (TX_START | RX_START | FLUSH)) == 0U &&
        (read_reg(mac, STATUS) & (TX_PROCESS | RX_PROCESS)) == 0U &&
        (read_reg(mac, MAC) & 12U) == 0U &&
        (read_reg(mac, DEBUG) & RX_DEBUG) == 0U &&
        (read_reg(mac, INTERRUPTS) & SOURCES) == 0U &&
        (read_reg(mac, MDIO) & 1U) == 0U && (read_reg(mac, BUS) & 1U) == 0U;
    bool busy = mac->snapshot.mac.pending_tx != 0U ||
                mac->snapshot.mac.held_rx != 0U || mac->wake_publishers != 0U;
    nx_arch_irq_restore(guard);
    return quiet && !busy ? detach(mac) : NX_ERROR_BUSY;
}
static nx_result_t recover(void* context, nx_time_us_t deadline) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (!mac->retained)
        return NX_ERROR_STATE;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    if (mac->snapshot.mac.held_rx != 0U || mac->wake_publishers != 0U ||
        mac->clock_epoch == UINT32_MAX) {
        nx_arch_irq_restore(guard);
        return NX_ERROR_BUSY;
    }
    for (size_t i = 0U; i < mac->tx_count; ++i)
        if (mac->tx_slots[i].phase == NX_ETH_FRAMES_TX_PREPARING) {
            nx_arch_irq_restore(guard);
            return NX_ERROR_BUSY;
        }
    mac->snapshot.mac.closing = true;
    write_reg(mac, INTERRUPTS, 0U);
    nx_arch_irq_restore(guard);
    if (mac->irq_control != NULL) {
        nx_result_t result = mac->irq_control(mac, false);
        if (result != NX_SUCCESS)
            return result;
    }
    guard = nx_arch_irq_save();
    bool publishers = mac->wake_publishers != 0U;
    nx_arch_irq_restore(guard);
    if (publishers)
        return NX_ERROR_BUSY;
    write_reg(mac, BUS, 1U);
    nx_result_t result = wait_clear(mac, BUS, 1U, deadline);
    if (result != NX_SUCCESS)
        return result;
    /* Reset acknowledgement destroys all old DMA/prefetch references. Only
     * now may accepted loans detach; no implicit replay or wire success. */
    ++mac->clock_epoch;
    mac->snapshot.clock_epoch = mac->clock_epoch;
    mac->clock_observed = false;
    saturate(&mac->snapshot.receive_gaps);
    for (size_t i = 0U; i < mac->tx_count; ++i) {
        nx_eth_frames_tx_slot_t* slot = &mac->tx_slots[i];
        nx_eth_frames_tx_request_t* request = slot->request;
        mac->tx_descriptors[i].buffer = mac->tx_descriptors[i].control = 0U;
        NX_ETH_FRAMES_DESCRIPTOR_STATUS(&mac->tx_descriptors[i], CHAIN);
        slot->request = NULL;
        slot->phase = NX_ETH_FRAMES_TX_FREE;
        if (request != NULL) {
            nx_result_t terminal =
                slot->terminal == NX_SUCCESS ? NX_ERROR_IO : slot->terminal;
            nx_request_settle(&request->base, terminal, 0U);
        }
    }
    mac->snapshot.mac.pending_tx = mac->tx_in_use = 0U;
    for (size_t i = 0U; i < mac->rx_count; ++i) {
        NX_ETH_FRAMES_DESCRIPTOR_STATUS(&mac->rx_descriptors[i], 0U);
        mac->rx_slots[i].state = NX_ETH_FRAMES_RX_DMA;
    }
    mac->snapshot.quarantined = false;
    return detach(mac);
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_eth_frames_mac_t* mac = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL)
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t guard = nx_arch_irq_save();
    bool busy = !mac->snapshot.mac.started || mac->snapshot.mac.closing ||
                mac->snapshot.mac.pending_tx != 0U ||
                mac->wake_publishers != 0U;
    nx_arch_irq_restore(guard);
    if (busy)
        return NX_ERROR_BUSY;
    if (wake != NULL && wake != mac->wake &&
        output_overlap(mac, wake, sizeof(*wake)))
        return NX_ERROR_INVALID;
    if (ceiling != mac->irq_policy.syscall_ceiling)
        return NX_ERROR_PERMISSION;
    nx_result_t result =
        mac->validate_wake != NULL
            ? mac->validate_wake(mac, wake, ceiling)
            : nx_irq_wake_validate(wake, &mac->irq_policy, &mac->source);
    if (result == NX_SUCCESS) {
        guard = nx_arch_irq_save();
        if (!mac->snapshot.mac.started || mac->snapshot.mac.closing ||
            mac->snapshot.mac.pending_tx != 0U || mac->wake_publishers != 0U)
            result = NX_ERROR_BUSY;
        else
            mac->wake = wake;
        nx_arch_irq_restore(guard);
    }
    return result;
}
static nx_result_t mdio_transfer(nx_eth_frames_mac_t* mac, uint8_t phy,
                                 uint8_t reg, uint16_t* value, bool write,
                                 nx_time_us_t deadline) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (mac == NULL || value == NULL || phy > 31U || reg > 31U ||
        deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (!mac->snapshot.mac.started || mac->snapshot.mac.closing)
        return NX_ERROR_STATE;
    if (!write && output_overlap(mac, value, sizeof(*value)))
        return NX_ERROR_INVALID;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if ((read_reg(mac, MDIO) & 1U) != 0U)
        return NX_ERROR_BUSY;
    if (write)
        write_reg(mac, MDIO_DATA, *value);
    write_reg(mac, MDIO,
              ((uint32_t)phy << 11U) | ((uint32_t)reg << 6U) |
                  mac->mdio_divider | (write ? 2U : 0U) | 1U);
    nx_result_t result = wait_clear(mac, MDIO, 1U, deadline);
    if (result == NX_SUCCESS && !write)
        *value = (uint16_t)read_reg(mac, MDIO_DATA);
    return result;
}
static nx_result_t mdio_read(void* context, uint8_t phy, uint8_t reg,
                             uint16_t* value, nx_time_us_t deadline) {
    return mdio_transfer(context, phy, reg, value, false, deadline);
}
static nx_result_t mdio_write(void* context, uint8_t phy, uint8_t reg,
                              uint16_t value, nx_time_us_t deadline) {
    return mdio_transfer(context, phy, reg, &value, true, deadline);
}
const nx_eth_frames_ops_t nx_eth_frames_mac_ops = {
    submit,     cancel,     service, acquire, release,    state,
    clock_read, apply_link, stop,    recover, attach_wake};
const nx_mdio_ops_t nx_eth_frames_mdio_ops = {mdio_read, mdio_write};
