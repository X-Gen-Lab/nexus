/**
 * \file            h747_amp_hardware_model.h
 * \brief           External MMIO boundary for actual H747 LL/MPU production C
 * \author          Nexus Team
 */
#ifndef NX_H747_AMP_HARDWARE_MODEL_H
#define NX_H747_AMP_HARDWARE_MODEL_H
#include <stdint.h>
/* Native C++ excludes only unused CMSIS cache helpers that narrow64-bit
 * pointers. Exact device/register types remain, as in the H743 host model.
 * Production C and actual ARM consumers retain the complete vendor header. */
#ifdef __cplusplus
#define ARM_ARMV7M_CACHEL1_H
#endif
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observe the selected hardware register, including banked MPU state.
 */
uint32_t nx_h747_amp_model_read(volatile const uint32_t* address);
/** \brief Apply MMIO effects or refuse a selected external hardware write. */
void nx_h747_amp_model_write(volatile uint32_t* address, uint32_t value);
#ifdef __cplusplus
}
#endif
#endif
