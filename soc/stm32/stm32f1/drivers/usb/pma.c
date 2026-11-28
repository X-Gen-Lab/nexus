/**
 * \file            pma.c
 *
 * \brief           STM32F1 USB Device buffer table and halfword accesses.
 *
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "pma.h"

enum {
    PMA_ROW_BYTES = 8U,
    PMA_TABLE_BYTES = NX_STM32F1_USB_PMA_ENDPOINTS * PMA_ROW_BYTES,
    PMA_TX_ADDRESS = 0U,
    PMA_TX_COUNT = 2U,
    PMA_RX_ADDRESS = 4U,
    PMA_RX_COUNT = 6U,
    PMA_PACKET_LIMIT = 64U,
    PMA_SMALL_RX_LIMIT = 62U,
    PMA_RX_TWO_BLOCKS_OF_32 = 0x8400U,
    PMA_RECEIVED_MASK = 0x03ffU
};

/** \brief Round a writable packet area to the physical halfword width. */
static uint16_t even_bytes(uint16_t capacity) {
    return (uint16_t)((capacity + 1U) & ~1U);
}

/** \brief Include every byte hardware can write to the RX allocation. */
static uint16_t rx_bytes(uint16_t capacity) {
    return capacity <= PMA_SMALL_RX_LIMIT ? even_bytes(capacity) : 64U;
}

/** \brief Encode the selected allocation and clear its old software count. */
static uint16_t rx_allocation(uint16_t capacity) {
    return capacity <= PMA_SMALL_RX_LIMIT
               ? (uint16_t)((even_bytes(capacity) / 2U) << 10)
               : PMA_RX_TWO_BLOCKS_OF_32;
}

/** \brief Check one word-aligned payload outside the fixed descriptor table. */
static bool range_valid(uint16_t address, uint16_t bytes) {
    if (bytes == 0U) {
        return address == 0U;
    }
    return address >= PMA_TABLE_BYTES && (address & 1U) == 0U &&
           address <= NX_STM32F1_USB_PMA_BYTES - bytes;
}

/** \brief Disabled directions claim no bytes. */
static bool overlap(uint16_t left, uint16_t left_bytes, uint16_t right,
                    uint16_t right_bytes) {
    return left_bytes != 0U && right_bytes != 0U &&
           (unsigned)left < (unsigned)right + right_bytes &&
           (unsigned)right < (unsigned)left + left_bytes;
}

/** \brief Check row fields before touching the buffer table. */
static bool row_valid(const nx_stm32f1_usb_pma_endpoint_t* row) {
    return row->tx_capacity <= PMA_PACKET_LIMIT &&
           row->rx_capacity <= PMA_PACKET_LIMIT &&
           range_valid(row->tx_address, even_bytes(row->tx_capacity)) &&
           range_valid(row->rx_address, rx_bytes(row->rx_capacity)) &&
           !overlap(row->tx_address, even_bytes(row->tx_capacity),
                    row->rx_address, rx_bytes(row->rx_capacity));
}

/** \brief Check two complete endpoint rows for cross-direction alias. */
static bool rows_overlap(const nx_stm32f1_usb_pma_endpoint_t* left,
                         const nx_stm32f1_usb_pma_endpoint_t* right) {
    const uint16_t left_tx = even_bytes(left->tx_capacity);
    const uint16_t left_rx = rx_bytes(left->rx_capacity);
    const uint16_t right_tx = even_bytes(right->tx_capacity);
    const uint16_t right_rx = rx_bytes(right->rx_capacity);
    return overlap(left->tx_address, left_tx, right->tx_address, right_tx) ||
           overlap(left->tx_address, left_tx, right->rx_address, right_rx) ||
           overlap(left->rx_address, left_rx, right->tx_address, right_tx) ||
           overlap(left->rx_address, left_rx, right->rx_address, right_rx);
}

nx_result_t
nx_stm32f1_usb_pma_prepare(volatile uint16_t* pma,
                           const nx_stm32f1_usb_pma_endpoint_t* endpoints,
                           size_t count) {
    if (pma == NULL || endpoints == NULL || count == 0U ||
        count > NX_STM32F1_USB_PMA_ENDPOINTS) {
        return NX_ERROR_INVALID;
    }
    for (size_t i = 0U; i < count; ++i) {
        if (!row_valid(&endpoints[i])) {
            return NX_ERROR_INVALID;
        }
        for (size_t previous = 0U; previous < i; ++previous) {
            if (rows_overlap(&endpoints[i], &endpoints[previous])) {
                return NX_ERROR_INVALID;
            }
        }
    }
    for (size_t i = 0U; i < count; ++i) {
        /* RM0008 §23.5.3 doubles logical byte addresses on the APB
         * window. Each 16-bit pointer index therefore equals one logical
         * byte offset, and hardware exposes only even indexes. */
        volatile uint16_t* row = &pma[i * PMA_ROW_BYTES];
        row[PMA_TX_ADDRESS] = endpoints[i].tx_address;
        row[PMA_TX_COUNT] = 0U;
        row[PMA_RX_ADDRESS] = endpoints[i].rx_address;
        row[PMA_RX_COUNT] = rx_allocation(endpoints[i].rx_capacity);
    }
    return NX_SUCCESS;
}

nx_result_t
nx_stm32f1_usb_pma_transmit(volatile uint16_t* pma, uint8_t endpoint,
                            const nx_stm32f1_usb_pma_endpoint_t* layout,
                            const uint8_t* bytes, size_t length) {
    if (pma == NULL || layout == NULL ||
        endpoint >= NX_STM32F1_USB_PMA_ENDPOINTS || layout->tx_capacity == 0U ||
        layout->tx_capacity > PMA_PACKET_LIMIT ||
        length > layout->tx_capacity || (length != 0U && bytes == NULL) ||
        !range_valid(layout->tx_address, even_bytes(layout->tx_capacity))) {
        return NX_ERROR_INVALID;
    }
    volatile uint16_t* payload = &pma[layout->tx_address];
    size_t i = 0U;
    for (; i + 1U < length; i += 2U) {
        payload[i] =
            (uint16_t)((uint16_t)bytes[i] | ((uint16_t)bytes[i + 1U] << 8));
    }
    if (i < length) {
        payload[i] = bytes[i];
    }
    pma[(size_t)endpoint * PMA_ROW_BYTES + PMA_TX_COUNT] = (uint16_t)length;
    return NX_SUCCESS;
}

nx_result_t
nx_stm32f1_usb_pma_receive(const volatile uint16_t* pma, uint8_t endpoint,
                           const nx_stm32f1_usb_pma_endpoint_t* layout,
                           uint8_t* bytes, size_t capacity, size_t* count) {
    if (pma == NULL || layout == NULL || count == NULL ||
        endpoint >= NX_STM32F1_USB_PMA_ENDPOINTS || layout->rx_capacity == 0U ||
        layout->rx_capacity > PMA_PACKET_LIMIT ||
        capacity > layout->rx_capacity || (capacity != 0U && bytes == NULL) ||
        !range_valid(layout->rx_address, rx_bytes(layout->rx_capacity))) {
        return NX_ERROR_INVALID;
    }
    const size_t actual = pma[(size_t)endpoint * PMA_ROW_BYTES + PMA_RX_COUNT] &
                          PMA_RECEIVED_MASK;
    if (actual > capacity) {
        return NX_ERROR_OVERFLOW;
    }
    const volatile uint16_t* payload = &pma[layout->rx_address];
    size_t i = 0U;
    for (; i + 1U < actual; i += 2U) {
        const uint16_t pair = payload[i];
        bytes[i] = (uint8_t)pair;
        bytes[i + 1U] = (uint8_t)(pair >> 8);
    }
    if (i < actual) {
        bytes[i] = (uint8_t)payload[i];
    }
    *count = actual;
    return NX_SUCCESS;
}

#if defined(NX_STM32F1_USB_PMA_EP1)
nx_result_t
nx_stm32f1_usb_pma_prepare_ep1(volatile uint16_t* pma,
                               const nx_stm32f1_usb_pma_endpoint_t* control,
                               const nx_stm32f1_usb_pma_endpoint_t* ep1) {
    if (pma == NULL || control == NULL || ep1 == NULL ||
        ep1->tx_capacity != 64U || ep1->rx_capacity != 64U ||
        !row_valid(control) || !row_valid(ep1) || rows_overlap(control, ep1))
        return NX_ERROR_INVALID;

    /* Re-admission of EP1 must not touch the live control descriptor row. */
    volatile uint16_t* row = &pma[PMA_ROW_BYTES];
    row[PMA_TX_ADDRESS] = ep1->tx_address;
    row[PMA_TX_COUNT] = 0U;
    row[PMA_RX_ADDRESS] = ep1->rx_address;
    row[PMA_RX_COUNT] = rx_allocation(ep1->rx_capacity);
    return NX_SUCCESS;
}
#endif
