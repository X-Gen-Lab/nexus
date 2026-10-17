/**
 * \file            h743_flash_fixture.h
 * \brief           External CPU and timer setup for existing Flash programs
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_FIXTURE_H
#define NX_H743_FLASH_FIXTURE_H
#include "h743_flash_mpu_support.h"
namespace nexus_test::h743_fixture {
/** \brief Reset external CPU/load/vector/MPU facts, never owner state. */
void ResetExternalCpu();
/** \brief Establish only the isolated fixture's explicit timebase SFRs. */
void SetIsolatedTimer(uint32_t input_hz);
/** \brief Join the explicit isolated timer through actual System stop. */
void StopIsolatedTimer();
/** \brief Map exact storage, then execute the actual cold RAM constructor. */
nx_result_t Prepare(nx_stm32h743_flash_state_t* port,
                    nx_stm32h743_flash_ram_workspace_t* execution);
/** \brief Stop through production only after real idle and owner release.
 *  \note Unsafe retention aborts before fixture storage can be reclaimed.
 */
void CheckedStop(nx_stm32h743_flash_ram_workspace_t* execution);
} /* namespace nexus_test::h743_fixture */
#endif
