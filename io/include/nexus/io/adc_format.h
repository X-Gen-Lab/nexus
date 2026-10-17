/**
 * \file            adc_format.h
 * \brief           Explicit integer sample representation and scan layout
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_ADC_FORMAT_H
#define NEXUS_IO_ADC_FORMAT_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Integer encoding; no implicit voltage or floating-point conversion.
 */
typedef enum {
    NX_ADC_ENCODING_UNKNOWN,
    NX_ADC_UNSIGNED,
    NX_ADC_TWOS_COMPLEMENT
} nx_adc_encoding_t;
/** \brief Byte order is explicit, independently of the executing CPU. */
typedef enum {
    NX_ADC_BYTE_ORDER_UNKNOWN,
    NX_ADC_LITTLE_ENDIAN,
    NX_ADC_BIG_ENDIAN
} nx_adc_byte_order_t;
/** \brief Hardware conversion group; descriptive facts do not enable it. */
typedef enum {
    NX_ADC_GROUP_UNKNOWN,
    NX_ADC_REGULAR,
    NX_ADC_INJECTED
} nx_adc_group_t;
/** \brief Trigger identity is not a sample aperture timestamp. */
typedef enum {
    NX_ADC_TRIGGER_UNKNOWN,
    NX_ADC_TRIGGER_SOFTWARE,
    NX_ADC_TRIGGER_TIMER,
    NX_ADC_TRIGGER_EVENT
} nx_adc_trigger_t;
/**
 * \brief Immutable uniform-channel integer layout, independent of ADC faces.
 * \note storage_bytes is1..4; significant_bits is1..32 within that storage.
 *       lsb_bits names discarded low bits, not an arithmetic scaling factor.
 *       sample_stride and scan_stride include explicit padding in bytes.
 *       channel_count retains the provider's separately declared channel order.
 *       Software triggers use identity0; hardware trigger identities are
 * nonzero assembly-local IDs, never addresses or a promise of physical timing.
 *       Group/trigger declarations describe an already supported provider mode;
 *       validating a layout does not add support to a device or configure it.
 */
typedef struct {
    size_t sample_stride;
    size_t scan_stride;
    uint32_t trigger_identity;
    uint16_t channel_count;
    uint8_t storage_bytes;
    uint8_t significant_bits;
    uint8_t lsb_bits;
    nx_adc_encoding_t encoding;
    nx_adc_byte_order_t byte_order;
    nx_adc_group_t group;
    nx_adc_trigger_t trigger;
} nx_adc_format_t;
/**
 * \brief Validate a complete immutable layout without reading sample storage.
 * \param[in] format: Readable naturally aligned layout, retained by caller.
 * \return SUCCESS or INVALID; no allocation, hardware access or retained loan.
 */
nx_result_t nx_adc_format_validate(const nx_adc_format_t* format);
/**
 * \brief Decode one integer from a caller-owned or consumer-acquired scan.
 * \param[in] format: Valid immutable uniform-channel scan description.
 * \param[in] samples: Readable storage; any producer/DMA access must be joined.
 * \param[in] bytes: Positive exact multiple of scan_stride, including padding.
 * \param[in] scan: Zero-based complete scan index.
 * \param[in] channel: Zero-based channel position in provider order.
 * \param[out] value: Separate aligned output, unchanged on rejection.
 * \return SUCCESS or INVALID; all32-bit unsigned/signed raw values fit int64_t.
 * \note No pointer is retained. Input/output/format overlap and wrapping spans
 *       reject before access. Sample loads have bounded1..4 byte work. This
 *       operation performs neither calibrated engineering-unit conversion nor
 *       synchronization with an active producer. The consumer owns that join.
 */
nx_result_t nx_adc_format_decode(const nx_adc_format_t* format,
                                 const void* samples, size_t bytes, size_t scan,
                                 size_t channel, int64_t* value);
#ifdef __cplusplus
}
#endif
#endif
