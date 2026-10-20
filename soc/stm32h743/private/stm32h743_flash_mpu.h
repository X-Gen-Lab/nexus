/**
 * \file            stm32h743_flash_mpu.h
 * \brief           Private RAM maintenance and MPU responsibility contract
 * \author          Nexus Team
 */
#ifndef NX_STM32H743_FLASH_MPU_H
#define NX_STM32H743_FLASH_MPU_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct nx_stm32h743_flash_ram_workspace
    nx_stm32h743_flash_ram_workspace_t;
/** \brief Effects belong only to the explicitly selected RAM workspace. */
enum nx_stm32h743_flash_mpu_effect {
    NX_H743_FLASH_MPU_EFFECT_SELECTOR = 1U,
    NX_H743_FLASH_MPU_EFFECT_REGION = 2U,
    NX_H743_FLASH_MPU_EFFECT_CONTROL = 4U
};
typedef enum {
    NX_H743_FLASH_MPU_FAULT_NONE = 0,
    NX_H743_FLASH_MPU_FAULT_RESTORE = 1,
    NX_H743_FLASH_MPU_FAULT_LOCK = 2
} nx_stm32h743_flash_mpu_fault_t;
/**
 * \brief Private sidecar, not a public authored safe-execution assertion.
 * \note Save the exact observed values before the first possible effect.
 *       remaining_effects is cleared only after matching hardware readback.
 *       This type has no default instance, registry, callback or heap owner.
 *       The selected workspace embeds one exact guard subobject. An internal
 *       disjoint sidecar is also admitted after whole-span alias validation.
 *       No arbitrary overlapping subobject is accepted; sizeof is not cost.
 */
typedef struct nx_stm32h743_flash_mpu_state {
    uint32_t saved_control;
    uint32_t saved_selector;
    uint32_t saved_base;
    uint32_t saved_attributes;
    uint32_t region;
    uint32_t remaining_effects;
    nx_stm32h743_flash_mpu_fault_t fault;
    bool timed_out;
} nx_stm32h743_flash_mpu_state_t;
/**
 * \brief Complete one internal RAM maintenance transaction under MPU guard.
 * \param[in,out] execution: Already-admitted private RAM command, MSP and VTOR.
 * \param[in,out] guard: The exact embedded guard or a disjoint selected RAM
 *       sidecar, initially without effects.
 * \param[in] deadline: Absolute time in the existing System TIM2/epoch domain.
 * \return SUCCESS, IO or TIMEOUT only after both physical banks are really
 *       BSY/QW/WBNE-clear, the target is LOCKed, and all MPU state is restored.
 *       Input/context rejection performs no MPU writes. Foreign-policy
 *       rejection may select and exactly restore RNR, but changes no RBAR,
 *       RASR or CTRL and starts no pulse. This already-admitted internal
 *       maintenance may install and restore protection even when its deadline
 *       suppresses the physical trigger.
 * \note The real bridge has installed private MSP, PRIMASK and VTOR. Guard
 *       enable/disable and every post-trigger instruction execute in AXI RAM.
 *       Expiry latches TIMEOUT; it does not cancel a pulse, withdraw protection
 *       or authorize return. RAM polling continues until actual idle. Permanent
 *       busy therefore does not return. An unprovable LOCK/MPU restore parks
 *       in RAM with effects retained; no reset/watchdog/RTOS policy is invoked.
 *       An internal return still uses the private RAM caller; the independently
 *       checked bridge restores VTOR/CPU only after this success-to-return
 *       proof. The caller must independently withdraw/join other bus masters;
 *       this guard controls CPU accesses only. Source implementation alone
 *       does not establish installed RAM closure or hardware qualification.
 */
nx_result_t nx_stm32h743_flash_ram_guarded_transaction(
    nx_stm32h743_flash_ram_workspace_t* execution,
    nx_stm32h743_flash_mpu_state_t* guard, nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
