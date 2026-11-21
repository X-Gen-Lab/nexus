/**
 * \file            crc.h
 * \brief           Streaming software CRC with explicit catalogue parameters
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-21
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_CRC_H
#define NEXUS_ALGORITHMS_CRC_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Width 8..32, normal polynomial without its leading x^width term.
 * \note initial/xor_out must fit width. reflect_input reverses each input
 *       byte; reflect_output reverses the register before xor_out. This is
 *       byte-oriented software math, not an MCU hardware CRC word contract.
 */
typedef struct {
    uint32_t polynomial;
    uint32_t initial;
    uint32_t xor_out;
    uint8_t width;
    bool reflect_input;
    bool reflect_output;
} nx_crc_model_t;
/** \brief CRC-32/ISO-HDLC model; immutable shared data. */
extern const nx_crc_model_t nx_crc32_iso_hdlc;
/** \brief CRC-16/MODBUS model; immutable shared data. */
extern const nx_crc_model_t nx_crc16_modbus;
/** \brief CRC-8/SMBUS model; immutable shared data. */
extern const nx_crc_model_t nx_crc8_smbus;
/**
 * \brief           Fold bytes into an unfinalized register.
 * \param[in]       model: Immutable valid model, borrowed until return.
 * \param[in]       data: Readable length bytes; NULL only when length is zero.
 * \param[in]       length: Number of bytes to consume.
 * \param[in,out]   state: Register initialized to model.initial; separate from
 *                  model/data. Unchanged on argument failure.
 * \return          SUCCESS or INVALID; bounded by length*width, no heap/OS.
 */
nx_result_t nx_crc_update(const nx_crc_model_t* model, const void* data,
                          size_t length, uint32_t* state);
/**
 * \brief           Finalize a register without changing streaming state.
 * \param[in]       model: Same immutable model used for update.
 * \param[in]       state: Unfinalized register within the selected width.
 * \param[out]      out: Distinct result, unchanged on failure.
 * \return          SUCCESS or INVALID; no allocation or retained references.
 */
nx_result_t nx_crc_finish(const nx_crc_model_t* model, uint32_t state,
                          uint32_t* out);
#ifdef __cplusplus
}
#endif
#endif
