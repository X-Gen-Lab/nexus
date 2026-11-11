/**
 * \file            stm32f407_silicon.h
 * \brief           Read-only reviewed F407 silicon-cut admission
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_SILICON_H
#define NEXUS_STM32F407_SILICON_H
#include "nexus/core/status.h"

/**
 * \brief Classify one actually observed IDCODE before any peripheral effect.
 * \param[in] idcode: Copied actual DBGMCU_IDCODE, not a configured revision.
 * \return Success for the reviewed DEV413 cuts; IO for another die group,
 *         UNSUPPORTED for missing, legacy or unreviewed revision identifiers.
 * \note ES0182 Rev19 Table2 has eight markings and six distinct REV_IDs.
 *       Revision A is not admitted: section2.2.2 reports its actual DEV411 /
 *       REV2000 identity. No register read/write, initialized RAM, allocation,
 *       policy override or reference retention is needed, including pre-C.
 *       Software permission does not prove package, PCB, physical behavior or
 *       that any hardware has been observed by the development environment.
 */
static inline nx_result_t nx_stm32f407_silicon_admit(uint32_t idcode) {
    if ((idcode & UINT32_C(0xfff)) != UINT32_C(0x413))
        return NX_ERROR_IO;
    switch (idcode >> 16U) {
        case 0x1001U:
        case 0x1003U:
        case 0x1007U:
        case 0x100fU:
        case 0x101fU:
            return NX_SUCCESS;
        default:
            return NX_ERROR_UNSUPPORTED;
    }
}
#endif /* NEXUS_STM32F407_SILICON_H */
