/**
 * \file            instance.h
 * \brief           Private STM32F103xB physical Flash owner
 * \author          Nexus Team
 */
#ifndef NX_STM32F1_FLASH_INSTANCE_H
#define NX_STM32F1_FLASH_INSTANCE_H

#include "nexus/io/flash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NX_STM32F1_FLASH_IDLE,
    NX_STM32F1_FLASH_ACTIVE,
    NX_STM32F1_FLASH_RETAINED
} nx_stm32f1_flash_phase_t;

/** \brief Caller-owned state, initially zero; never mutate while published. */
typedef struct {
    const nx_flash_geometry_t* geometry;
    nx_stm32f1_flash_phase_t phase;
} nx_stm32f1_flash_state_t;

extern const nx_flash_ops_t nx_stm32f1_flash_ops;

/** \brief Bind the complete geometry for an authorized STM32F103xB part.
 * \param[in,out] port: Unprepared, serialized owner.
 * \param[in] geometry: Stable read-only whole-Flash description.
 * \param[in] supply_mv: Board-supplied programming voltage assumption.
 * \return Success, or rejection with no writes.
 * \note Privileged unmasked Thread context. The platform owns HSI and the
 * timebase, and joins all consumers before closing this instance.
 */
nx_result_t nx_stm32f1_flash_prepare(nx_stm32f1_flash_state_t* port,
                                     const nx_flash_geometry_t* geometry,
                                     uint32_t supply_mv);

/** \brief Release the prepared owner once hardware cleanup is proven.
 * \param[in,out] port: Same serialized owner whose users have joined.
 * \return SUCCESS or a failure retaining the owner for another attempt.
 * \note Privileged unmasked Thread. Never launches or replays a pulse.
 */
nx_result_t nx_stm32f1_flash_close(nx_stm32f1_flash_state_t* port);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F1_FLASH_INSTANCE_H */
