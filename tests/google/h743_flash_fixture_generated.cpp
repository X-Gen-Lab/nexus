/**
 * \file            h743_flash_fixture_generated.cpp
 * \brief           Observe generated RAM constructor and call it unchanged
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_fixture.h"
extern "C" nx_result_t
__real_nx_stm32h743_flash_ram_initialize(nx_stm32h743_flash_state_t*,
                                         nx_stm32h743_flash_ram_workspace_t*);
extern "C" nx_result_t __wrap_nx_stm32h743_flash_ram_initialize(
    nx_stm32h743_flash_state_t* port,
    nx_stm32h743_flash_ram_workspace_t* execution) {
    /* Only external address-map binding is observed. The original constructor
     * performs every copy, vector, CPU and protection admission itself. */
    nexus_test::h743_ram::cpu.execution = execution;
    nexus_test::h743_ram::cpu.owner_port = port;
    nexus_test::h743_mpu::mpu.guard = &execution->guard;
    g_h743_model.physical_flash = port;
    return __real_nx_stm32h743_flash_ram_initialize(port, execution);
}
