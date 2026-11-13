/**
 * \file            traits.h
 * \brief           Public readonly traits of a selected I/O implementation
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_TRAITS_H
#define NEXUS_IO_TRAITS_H

#include "nexus/io/observation.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Selected execution features, without physical qualification. */
enum nx_io_capabilities {
    NX_IO_CAP_MODEL = 1U << 0,
    NX_IO_CAP_POLLING = 1U << 1,
    NX_IO_CAP_IRQ = 1U << 2,
    NX_IO_CAP_TX_DMA = 1U << 3,
    NX_IO_CAP_RX_DMA = 1U << 4,
    NX_IO_CAP_BLOCK_STREAM = 1U << 5,
    NX_IO_CAP_STREAM_GAPS = 1U << 6,
    NX_IO_CAP_BUS_MASTER_DMA = 1U << 7
};

/**
 * \brief Readonly transfer bounds of one selected implementation.
 * \note Zero bounds mean unspecified, never unlimited. The typed interface
 *       defines transfer units; zero unit_bytes means no fixed byte width.
 *       For Flash, unit_bytes constrains destination offset and length while
 *       buffer_alignment describes the caller's storage. Board wiring, memory
 *       domains, current state and deadlines remain separate contracts.
 */
typedef struct {
    uint32_t features;
    uint32_t max_transfer_units;
    uint16_t unit_bytes;
    uint16_t buffer_alignment;
    uint16_t max_messages;
} nx_io_capability_t;

/**
 * \brief Selected SPI transaction bounds in numeric words, not bytes.
 * \note Bit N of supported_word_bits permits N-bit words. Zero length bounds
 *       are unspecified. These facts do not replace provider admission.
 */
typedef struct {
    uint32_t supported_word_bits;
    uint32_t max_total_words;
    uint32_t max_segment_words;
    uint32_t max_segments;
} nx_spi_transaction_traits_t;

/** \brief Abort effects never release borrowed request or payload storage. */
typedef enum {
    NX_IO_ABORT_UNKNOWN = 0,
    NX_IO_ABORT_REQUEST,
    NX_IO_ABORT_CONTROLLER_PENDING,
    NX_IO_ABORT_NOT_APPLICABLE
} nx_io_abort_scope_t;

/** \brief Selected receive mechanism, without a throughput guarantee. */
typedef enum {
    NX_IO_CONTINUITY_UNKNOWN = 0,
    NX_IO_CONTINUITY_NOT_APPLICABLE,
    NX_IO_CONTINUITY_IRQ_BUFFERED,
    NX_IO_CONTINUITY_REARM_GAP,
    NX_IO_CONTINUITY_DOUBLE_BUFFER,
    NX_IO_CONTINUITY_MODEL
} nx_io_continuity_t;

/** \brief Provider owns actual DMA admission and cache maintenance. */
typedef enum {
    NX_IO_DMA_POLICY_NONE = 0,
    NX_IO_DMA_POLICY_PROVIDER_DOMAIN,
    NX_IO_DMA_POLICY_DOMAIN_CACHE_LINE
} nx_io_dma_policy_t;

/**
 * \brief Readonly uart facts for one selected implementation.
 * \note Facts do not establish readiness, measured timing or physical
 *       qualification. Cache line bytes describe the selected provider policy,
 *       not whether cache is enabled. UNKNOWN is not an unlimited guarantee.
 *       Parity zero means none; stop_half_bits counts half-bit periods.
 */
typedef struct {
    uint32_t data_bits;
    uint32_t parity;
    uint32_t stop_half_bits;
    uint32_t requested_baud;
    nx_io_abort_scope_t abort_scope;
    nx_io_continuity_t continuity;
    nx_io_timestamp_source_t timestamp_source;
    nx_io_dma_policy_t dma_policy;
    uint32_t cache_line_bytes;
} nx_uart_traits_t;

/**
 * \brief Readonly spi facts for one selected implementation.
 * \note Facts do not establish readiness, measured timing or physical
 *       qualification. Cache line bytes describe the selected provider policy,
 *       not whether cache is enabled. UNKNOWN is not an unlimited guarantee.
 *       configured_max_hz is a requested ceiling, not a measured SCK rate.
 */
typedef struct {
    uint32_t word_bits;
    uint32_t configured_max_hz;
    nx_io_abort_scope_t abort_scope;
    nx_io_continuity_t continuity;
    nx_io_timestamp_source_t timestamp_source;
    nx_io_dma_policy_t dma_policy;
    uint32_t cache_line_bytes;
} nx_spi_traits_t;

/**
 * \brief Readonly i2c facts for one selected implementation.
 * \note Facts do not establish readiness, measured timing or physical
 *       qualification. Cache line bytes describe the selected provider policy,
 *       not whether cache is enabled. UNKNOWN is not an unlimited guarantee.
 *       address_bits zero means endpoint-selected width. nominal_hz is the
 *       selected divider result; zero means no known physical rate.
 */
typedef struct {
    uint32_t address_bits;
    uint32_t configured_max_hz;
    uint32_t nominal_hz;
    nx_io_abort_scope_t abort_scope;
    nx_io_continuity_t continuity;
    nx_io_timestamp_source_t timestamp_source;
    nx_io_dma_policy_t dma_policy;
    uint32_t cache_line_bytes;
} nx_i2c_traits_t;

/**
 * \brief Readonly adc facts for one selected implementation.
 * \note Facts do not establish readiness, measured timing or physical
 *       qualification. Cache line bytes describe the selected provider policy,
 *       not whether cache is enabled. UNKNOWN is not an unlimited guarantee.
 *       Storage width is distinct from effective conversion resolution.
 */
typedef struct {
    uint32_t storage_bits;
    uint32_t signed_samples;
    uint32_t scan_channels;
    nx_io_abort_scope_t abort_scope;
    nx_io_continuity_t continuity;
    nx_io_timestamp_source_t timestamp_source;
    nx_io_dma_policy_t dma_policy;
    uint32_t cache_line_bytes;
} nx_adc_traits_t;

/**
 * \brief Readonly can facts for one selected implementation.
 * \note Facts do not establish readiness, measured timing or physical
 *       qualification. Cache line bytes describe the selected provider policy,
 *       not whether cache is enabled. UNKNOWN is not an unlimited guarantee.
 */
typedef struct {
    uint32_t classic_payload_bytes;
    uint32_t hardware_tx_mailboxes;
    nx_io_abort_scope_t abort_scope;
    nx_io_continuity_t continuity;
    nx_io_timestamp_source_t timestamp_source;
    nx_io_dma_policy_t dma_policy;
    uint32_t cache_line_bytes;
} nx_can_traits_t;

/**
 * \brief Readonly eth facts for one selected implementation.
 * \note Facts do not establish readiness, measured timing or physical
 *       qualification. Cache line bytes describe the selected provider policy,
 *       not whether cache is enabled. UNKNOWN is not an unlimited guarantee.
 */
typedef struct {
    uint32_t vlan;
    uint32_t scatter_gather;
    nx_io_abort_scope_t abort_scope;
    nx_io_continuity_t continuity;
    nx_io_timestamp_source_t timestamp_source;
    nx_io_dma_policy_t dma_policy;
    uint32_t cache_line_bytes;
} nx_eth_traits_t;

/**
 * \brief Readonly eth_frames facts for one selected implementation.
 * \note Facts do not establish readiness, measured timing or physical
 *       qualification. Cache line bytes describe the selected provider policy,
 *       not whether cache is enabled. UNKNOWN is not an unlimited guarantee.
 */
typedef struct {
    uint32_t vlan;
    uint32_t scatter_gather;
    uint32_t clock_domain;
    nx_io_abort_scope_t abort_scope;
    nx_io_continuity_t continuity;
    nx_io_timestamp_source_t timestamp_source;
    nx_io_dma_policy_t dma_policy;
    uint32_t cache_line_bytes;
} nx_eth_frames_traits_t;

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_IO_TRAITS_H */
