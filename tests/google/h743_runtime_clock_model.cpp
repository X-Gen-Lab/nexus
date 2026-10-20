/**
 * \file            h743_runtime_clock_model.cpp
 * \brief           Observe the real domain without implementing lifecycle
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-18
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_runtime_clock_model.h"
#include "clock_domain_impl.h"
#include "nexus_bindings.h"
extern "C" {
bool nx_runtime_watch;
bool nx_runtime_hold_resume_cleanup;
unsigned nx_runtime_unleased_writes;
void nx_runtime_register_write(volatile uint32_t* reg, uint32_t value) {
    if (nx_runtime_watch && nx_platform_runtime_domain()->count == 0U)
        ++nx_runtime_unleased_writes;
    nx_h743_spi_dma_factory_write(reg, value);
    if (nx_runtime_hold_resume_cleanup &&
        g_h743_spi_dma_factory_model.trim_events != 0U)
        g_h743_model.ignore_write = &RCC->AHB1ENR;
}
}
