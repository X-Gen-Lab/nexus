/**
 * \file            mpu.h
 *
 * \brief           Distinct Armv7-M and Armv8-M MPU encoding mechanisms.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_MPU_H
#define NEXUS_ARCH_MPU_H
#include "nexus/arch/features.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Armv7-M region fields, without an implicit memory policy. */
typedef struct {
    uint32_t base;
    uint32_t size_log2;
    uint32_t access;
    uint32_t tex;
    uint32_t subregion_disable;
    bool shareable;
    bool cacheable;
    bool bufferable;
    bool execute_never;
} nx_arch_mpu_v7_region_t;

/** \brief Armv7-M words targeting the caller-selected RNR, not RBAR.VALID. */
typedef struct {
    uint32_t rbar;
    uint32_t rasr;
} nx_arch_mpu_v7_encoding_t;

/** \brief Armv8-M fields; limit is the exact inclusive final byte. */
typedef struct {
    uint32_t base;
    uint32_t limit;
    uint32_t access;
    uint32_t shareability;
    uint32_t attribute_index;
    bool execute_never;
} nx_arch_mpu_v8_region_t;

/** \brief Armv8-M words retaining its separate MAIR indirection. */
typedef struct {
    uint32_t rbar;
    uint32_t rlar;
} nx_arch_mpu_v8_encoding_t;

/**
 * \brief           Encode an exact Armv7-M region without accessing hardware.
 *
 * \param[in]       region: Size exponent 5..32, aligned base, AP 0/1/2/3/5/6,
 *                  TEX 0..7 and raw S/C/B bits. Subregion mask is at most 255
 *                  and must be zero for sizes below 256 bytes. A 4 GiB region
 *                  has base zero. Reserved/implementation-defined TEX/C/B
 *                  encodings are rejected, never truncated or rounded.
 * \param[out]      encoding: RBAR and enabled RASR; unchanged on failure.
 *
 * \return          NX_ARCH_OK or NX_ARCH_INVALID, independent of CPU profile.
 *
 * \note            Pure bounded task/IRQ function. Inputs and output must not
 *                  overlap. This encodes fields, not SoC memory reachability,
 *                  cache ownership, DMA coherency or product protection policy.
 */
nx_arch_result_t nx_arch_mpu_v7_encode(const nx_arch_mpu_v7_region_t* region,
                                       nx_arch_mpu_v7_encoding_t* encoding);

/**
 * \brief           Encode an exact Armv8-M region without accessing hardware.
 *
 * \param[in]       region: 32-byte aligned base, inclusive limit ending in
 *                  0x1f, base not above limit, AP 0..3, SH 0/2/3 and MAIR
 *                  index 0..7. The caller owns the corresponding MAIR policy.
 * \param[out]      encoding: RBAR and enabled RLAR; unchanged on failure.
 *
 * \return          NX_ARCH_OK or NX_ARCH_INVALID, independent of CPU profile.
 *
 * \note            Pure bounded task/IRQ function. Inputs and output must not
 *                  overlap. No address rounding or v7/v8 policy conversion.
 */
nx_arch_result_t nx_arch_mpu_v8_encode(const nx_arch_mpu_v8_region_t* region,
                                       nx_arch_mpu_v8_encoding_t* encoding);

/**
 * \brief           Program one disabled-MPU Armv7-M region in this world.
 *
 * \param[in]       index: Region number below actual MPU_TYPE.DREGION.
 * \param[in]       region: Fields validated by nx_arch_mpu_v7_encode.
 *
 * \return          NX_ARCH_UNSUPPORTED before MMIO for a different/absent
 *                  compiled MPU; NX_ARCH_INVALID for invalid fields or index;
 *                  NX_ARCH_CONTEXT for an unprivileged, Handler, unmasked or
 *                  enabled-MPU caller. Rejection performs no register writes.
 *
 * \note            Privileged Thread only with PRIMASK set and exclusive
 *                  caller ownership of this MPU. NMI/HardFault/another security
 *                  world must not modify it. Disables only the selected region,
 *                  writes its fields, restores incoming RNR and leaves CTRL
 *                  unchanged. No allocation, OS calls, implicit MPU enable,
 *                  bulk clear or Secure access to the Non-secure alias.
 */
nx_arch_result_t nx_arch_mpu_v7_program(uint32_t index,
                                        const nx_arch_mpu_v7_region_t* region);

/**
 * \brief           Program one disabled-MPU Armv8-M region in this world.
 *
 * \param[in]       index: Region number below actual MPU_TYPE.DREGION.
 * \param[in]       region: Fields validated by nx_arch_mpu_v8_encode.
 *
 * \return          Same rejection and ownership rules as v7_program; the
 *                  compiled MPU must specifically implement Armv8-M.
 *
 * \note            Privileged masked Thread, disabled MPU and exclusive local
 *                  ownership required. No MAIR updates or cross-world alias
 *                  access. Incoming RNR and CTRL are preserved.
 */
nx_arch_result_t nx_arch_mpu_v8_program(uint32_t index,
                                        const nx_arch_mpu_v8_region_t* region);

/**
 * \brief           Update one Armv8-M MAIR byte without changing its neighbors.
 *
 * \param[in]       index: Attribute slot 0..7.
 * \param[in]       attribute: Architectural MAIR byte, at most 255. Device
 *                  encodings are 0x00/0x04/0x08/0x0c; Normal memory uses
 *                  nonzero outer/inner nibbles other than reserved 0x8.
 *
 * \return          NX_ARCH_OK, NX_ARCH_UNSUPPORTED for absent/v7 MPU,
 *                  NX_ARCH_INVALID for an out-of-range field, or
 *                  NX_ARCH_CONTEXT under the programming context rules.
 *
 * \note            Caller owns valid memory attributes and all regions using
 *                  this slot. Privileged masked Thread, disabled MPU and
 *                  exclusive local ownership required. No CTRL or RNR writes,
 *                  no cache maintenance, and no cross-world register access.
 */
nx_arch_result_t nx_arch_mpu_v8_attribute_set(uint32_t index,
                                              uint32_t attribute);
#ifdef __cplusplus
}
#endif
#endif
