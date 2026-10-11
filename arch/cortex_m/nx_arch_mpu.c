/**
 * \file            nx_arch_mpu.c
 *
 * \brief           Validated MPU encoding and current-domain register writes.
 *
 * \author          Nexus Team
 *
 * \version         1.0.0
 *
 * \date            2026-10-11
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/arch/mpu.h"
#include "private/mechanisms.h"

#define NX_MPU_TYPE ((uintptr_t)0xe000ed90U)
#define NX_MPU_CTRL (NX_MPU_TYPE + 4U)
#define NX_MPU_RNR  (NX_MPU_TYPE + 8U)
#define NX_MPU_RBAR (NX_MPU_TYPE + 12U)
#define NX_MPU_ATTR (NX_MPU_TYPE + 16U)
#define NX_MPU_MAIR (NX_MPU_TYPE + 48U)

/** \brief Reject undefined/implementation-defined v7 memory encodings. */
static bool nx_arch_mpu_v7_attributes_valid(uint32_t tex, uint32_t cb) {
    return tex == 0U || tex >= 4U || (tex == 1U && (cb == 0U || cb == 3U)) ||
           (tex == 2U && cb == 0U);
}

nx_arch_result_t nx_arch_mpu_v7_encode(const nx_arch_mpu_v7_region_t* region,
                                       nx_arch_mpu_v7_encoding_t* encoding) {
    if (region == 0 || encoding == 0 || region->size_log2 < 5U ||
        region->size_log2 > 32U || region->access > 6U ||
        region->access == 4U || region->tex > 7U ||
        region->subregion_disable > 255U ||
        (region->size_log2 < 8U && region->subregion_disable != 0U)) {
        return NX_ARCH_INVALID;
    }
    /* Shifting a 32-bit one by 32 is undefined. A full-address-space region
     * has a uniquely valid zero base; smaller regions use their exact mask. */
    if ((region->size_log2 == 32U && region->base != 0U) ||
        (region->size_log2 < 32U &&
         (region->base & ((1U << region->size_log2) - 1U)) != 0U)) {
        return NX_ARCH_INVALID;
    }
    uint32_t cb =
        ((uint32_t)region->cacheable << 1U) | (uint32_t)region->bufferable;
    if (!nx_arch_mpu_v7_attributes_valid(region->tex, cb)) {
        return NX_ARCH_INVALID;
    }
    nx_arch_mpu_v7_encoding_t result = {
        region->base, ((uint32_t)region->execute_never << 28U) |
                          (region->access << 24U) | (region->tex << 19U) |
                          ((uint32_t)region->shareable << 18U) | (cb << 16U) |
                          (region->subregion_disable << 8U) |
                          ((region->size_log2 - 1U) << 1U) | 1U};
    *encoding = result;
    return NX_ARCH_OK;
}

nx_arch_result_t nx_arch_mpu_v8_encode(const nx_arch_mpu_v8_region_t* region,
                                       nx_arch_mpu_v8_encoding_t* encoding) {
    if (region == 0 || encoding == 0 || (region->base & 31U) != 0U ||
        (region->limit & 31U) != 31U || region->base > region->limit ||
        region->access > 3U || region->shareability > 3U ||
        region->shareability == 1U || region->attribute_index > 7U) {
        return NX_ARCH_INVALID;
    }
    nx_arch_mpu_v8_encoding_t result = {
        region->base | (region->shareability << 3U) | (region->access << 1U) |
            (uint32_t)region->execute_never,
        (region->limit & ~31U) | (region->attribute_index << 1U) | 1U};
    *encoding = result;
    return NX_ARCH_OK;
}

#if NEXUS_ARCH_MPU_VERSION == 7 || NEXUS_ARCH_MPU_VERSION == 8
/** \brief Caller owns serialization; priority masking alone is insufficient. */
static bool nx_arch_mpu_context_valid(void) {
    return nx_arch_is_privileged() && nx_arch_exception_number() == 0U &&
           (nx_arch_irq_masks().primask & 1U) != 0U;
}

/** \brief Check actual region geometry and disabled MPU before any writes. */
static nx_arch_result_t nx_arch_mpu_program(uint32_t index, uint32_t base,
                                            uint32_t attributes) {
    if (!nx_arch_mpu_context_valid()) {
        return NX_ARCH_CONTEXT;
    }
    uint32_t count = (nx_arch_hw_mmio_read(NX_MPU_TYPE) >> 8U) & 255U;
    if (count == 0U) {
        return NX_ARCH_UNSUPPORTED;
    }
    if (index >= count) {
        return NX_ARCH_INVALID;
    }
    if ((nx_arch_hw_mmio_read(NX_MPU_CTRL) & 1U) != 0U) {
        return NX_ARCH_CONTEXT;
    }
    uint32_t incoming_region = nx_arch_hw_mmio_read(NX_MPU_RNR);
    nx_arch_dmb();
    nx_arch_hw_mmio_write(NX_MPU_RNR, index);
    /* CTRL remains disabled throughout; no partially changed region can
     * become active. RBAR.VALID is zero on v7, so only this RNR is selected. */
    nx_arch_hw_mmio_write(NX_MPU_ATTR, 0U);
    nx_arch_hw_mmio_write(NX_MPU_RBAR, base);
    nx_arch_hw_mmio_write(NX_MPU_ATTR, attributes);
    nx_arch_hw_mmio_write(NX_MPU_RNR, incoming_region);
    nx_arch_dsb();
    nx_arch_isb();
    return NX_ARCH_OK;
}
#endif

nx_arch_result_t nx_arch_mpu_v7_program(uint32_t index,
                                        const nx_arch_mpu_v7_region_t* region) {
#if NEXUS_ARCH_MPU_VERSION == 7
    nx_arch_mpu_v7_encoding_t encoding;
    nx_arch_result_t result = nx_arch_mpu_v7_encode(region, &encoding);
    return result == NX_ARCH_OK
               ? nx_arch_mpu_program(index, encoding.rbar, encoding.rasr)
               : result;
#else
    (void)index;
    (void)region;
    return NX_ARCH_UNSUPPORTED;
#endif
}

nx_arch_result_t nx_arch_mpu_v8_program(uint32_t index,
                                        const nx_arch_mpu_v8_region_t* region) {
#if NEXUS_ARCH_MPU_VERSION == 8
    nx_arch_mpu_v8_encoding_t encoding;
    nx_arch_result_t result = nx_arch_mpu_v8_encode(region, &encoding);
    return result == NX_ARCH_OK
               ? nx_arch_mpu_program(index, encoding.rbar, encoding.rlar)
               : result;
#else
    (void)index;
    (void)region;
    return NX_ARCH_UNSUPPORTED;
#endif
}

#if NEXUS_ARCH_MPU_VERSION == 8
/** \brief Reject reserved Device/Normal MAIR encodings, without truncation. */
static bool nx_arch_mpu_v8_attribute_valid(uint32_t attribute) {
    if (attribute > 255U) {
        return false;
    }
    uint32_t outer = attribute >> 4U;
    uint32_t inner = attribute & 15U;
    return outer == 0U ? (inner & 3U) == 0U
                       : outer != 8U && inner != 0U && inner != 8U;
}
#endif

nx_arch_result_t nx_arch_mpu_v8_attribute_set(uint32_t index,
                                              uint32_t attribute) {
#if NEXUS_ARCH_MPU_VERSION == 8
    if (index > 7U || !nx_arch_mpu_v8_attribute_valid(attribute)) {
        return NX_ARCH_INVALID;
    }
    if (!nx_arch_mpu_context_valid()) {
        return NX_ARCH_CONTEXT;
    }
    if (((nx_arch_hw_mmio_read(NX_MPU_TYPE) >> 8U) & 255U) == 0U) {
        return NX_ARCH_UNSUPPORTED;
    }
    if ((nx_arch_hw_mmio_read(NX_MPU_CTRL) & 1U) != 0U) {
        return NX_ARCH_CONTEXT;
    }
    uintptr_t address = NX_MPU_MAIR + ((uintptr_t)index / 4U) * 4U;
    uint32_t shift = (index % 4U) * 8U;
    uint32_t incoming = nx_arch_hw_mmio_read(address);
    uint32_t updated = (incoming & ~(255U << shift)) | (attribute << shift);
    nx_arch_dmb();
    nx_arch_hw_mmio_write(address, updated);
    nx_arch_dsb();
    nx_arch_isb();
    return NX_ARCH_OK;
#else
    (void)index;
    (void)attribute;
    return NX_ARCH_UNSUPPORTED;
#endif
}
