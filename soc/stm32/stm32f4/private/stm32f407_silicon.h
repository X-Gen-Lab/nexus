/**
 * \file            stm32f407_silicon.h
 * \brief           Read-only reviewed F407 silicon-cut admission
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_SILICON_H
#define NEXUS_STM32F407_SILICON_H
#include "nexus/core/status.h"
#include "stm32f4_traits.h"

/**
 * \brief Classify one observed IDCODE using the selected SDK device group.
 * \param[in] idcode: Copied actual DBGMCU_IDCODE, never a configured revision.
 * \return Success for the selected documented cuts; IO for a different die,
 *         UNSUPPORTED for missing, legacy or unreviewed revision identifiers.
 * \note ES0182 Table2 supplies the five admitted DEV413 cuts. ES0287 Table2,
 *       ES0298 Table2 and ES0321 Table2 each identify their group with REV1000.
 *       These rules do not establish part, mode, PCB or physical qualification;
 *       the host must separately admit the complete selected software review.
 *       No RAM, allocation, register write or policy override is required.
 */
static inline nx_result_t nx_stm32f407_silicon_admit(uint32_t idcode) {
    if ((idcode & UINT32_C(0xfff)) != NX_STM32_DEVICE_GROUP_ID)
        return NX_ERROR_IO;
#if NX_STM32_SINGLE_CUT
    return (idcode >> 16U) == 0x1000U ? NX_SUCCESS : NX_ERROR_UNSUPPORTED;
#else
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
#endif
}
#endif /* NEXUS_STM32F407_SILICON_H */
