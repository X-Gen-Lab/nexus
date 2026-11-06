/**
 * \file            hardware.h
 * \brief           Compile-time H747 hardware leaves for the AMP attachment
 * \author          Nexus Team
 */
#ifndef NX_H747_AMP_HARDWARE_H
#define NX_H747_AMP_HARDWARE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observe selected SRAM4, enabled HSEM and the local MPU region. */
bool nx_h747_amp_hw_ready(const void* address, size_t bytes, uint8_t region);
/** \brief Try one official LL two-step claim; caller masks local preemption. */
bool nx_h747_amp_hw_take(uint8_t semaphore, uint8_t process);
/** \brief Release and prove the old core/process no longer owns this claim. */
bool nx_h747_amp_hw_release(uint8_t semaphore, uint8_t process);
#ifdef __cplusplus
}
#endif
#endif
