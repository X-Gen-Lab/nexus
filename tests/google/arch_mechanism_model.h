/**
 * \file            arch_mechanism_model.h
 *
 * \brief           Host-only local CPU memory-mapped register boundary.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_ARCH_MECHANISM_MODEL_H
#define NEXUS_ARCH_MECHANISM_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observe a CPU MMIO read without replacing the production algorithm.
 */
uint32_t nx_arch_model_mmio_read(uintptr_t address);
/** \brief Observe a CPU MMIO write in program order. */
void nx_arch_model_mmio_write(uintptr_t address, uint32_t value);
#ifdef __cplusplus
}
#endif
#endif
