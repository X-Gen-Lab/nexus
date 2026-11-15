/**
 * \file            arch_cortex_model.h
 *
 * \brief           Host-only CPU instruction and register hardware boundary.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_CORTEX_MODEL_H
#define NEXUS_ARCH_CORTEX_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Architected registers observed by the production CPU primitives. */
typedef enum {
    NX_ARCH_MODEL_PRIMASK,
    NX_ARCH_MODEL_BASEPRI,
    NX_ARCH_MODEL_FAULTMASK,
    NX_ARCH_MODEL_IPSR,
    NX_ARCH_MODEL_CONTROL,
    NX_ARCH_MODEL_DEMCR,
    NX_ARCH_MODEL_DWT_CONTROL,
    NX_ARCH_MODEL_CYCCNT
} nx_arch_model_register_t;
/** \brief Instructions whose ordering is observed independently of state. */
typedef enum {
    NX_ARCH_MODEL_CPSID_I,
    NX_ARCH_MODEL_DMB,
    NX_ARCH_MODEL_DSB,
    NX_ARCH_MODEL_ISB
} nx_arch_model_instruction_t;
/** \brief Read a hardware register without replacing the CPU algorithm. */
uint32_t nx_arch_model_read(nx_arch_model_register_t reg);
/** \brief Write the modeled PRIMASK register. */
void nx_arch_model_write(nx_arch_model_register_t reg, uint32_t value);
/** \brief Observe a hardware instruction in program order. */
void nx_arch_model_instruction(nx_arch_model_instruction_t instruction);
#ifdef __cplusplus
}
#endif
#endif
