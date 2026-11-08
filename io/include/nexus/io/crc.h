/**
 * \file            crc.h
 * \brief           Serialized CRC-32 of an explicit sequence of 32-bit words
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_CRC_H
#define NEXUS_IO_CRC_H
#include "nexus/core/time.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Shared immutable methods receive the selected controller context. */
typedef struct {
    nx_result_t (*compute)(void* context, const uint32_t* words, size_t count,
                           nx_time_us_t deadline, uint32_t* result);
} nx_crc_ops_t;
/** \brief Stable controller identity; lookup never resets the calculator. */
struct nx_crc_port {
    const nx_crc_ops_t* ops;
    void* context;
};
/**
 * \brief Compute fixed CRC-32 without byte-order or padding inference.
 * \param[in] port: Initialized face with one serialized privileged task owner.
 * \param[in] words: Aligned immutable 32-bit values, consumed MSB first.
 *                  NULL is permitted only for an empty sequence.
 * \param[in] count: Number of words, not bytes; no implicit trailing padding.
 * \param[in] deadline: Absolute live Core deadline, including final readback.
 * \param[out] result: Independent aligned output, changed only on success.
 * \return SUCCESS, INVALID, STATE, CONTEXT, TIMEOUT, IO or UNSUPPORTED.
 * \note Polynomial is 0x04C11DB7, initial state 0xffffffff, no reflection and
 *       no final XOR. An empty sequence returns the initial state. This is a
 *       word sequence contract, not an arbitrary CRC or Ethernet byte stream.
 *       No allocation, IRQ, DMA or retained references; inputs, output and face
 *       remain stable through this synchronous call. A timeout discards the
 *       partial calculation; each new call begins with a fresh initial state.
 *       Caller storage is disjoint from provider state and hardware controls.
 */
nx_result_t nx_crc_port_compute(const nx_crc_port_t* port,
                                const uint32_t* words, size_t count,
                                nx_time_us_t deadline, uint32_t* result);
#ifdef __cplusplus
}
#endif
#endif
