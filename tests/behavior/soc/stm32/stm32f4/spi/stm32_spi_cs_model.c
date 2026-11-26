/**
 * \file            stm32_spi_cs_model.c
 * \brief           GPIO command latch effects and selected hardware refusal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-08
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32_spi_cs_model.h"
#include <stddef.h>

volatile uint32_t* g_nx_spi_cs_fault_odr;
uint32_t g_nx_spi_cs_hold_low;
uint32_t g_nx_spi_cs_hold_high;

void nx_stm32_spi_cs_model_reset(void) {
    g_nx_spi_cs_fault_odr = NULL;
    g_nx_spi_cs_hold_low = 0U;
    g_nx_spi_cs_hold_high = 0U;
}

uint32_t nx_stm32_spi_cs_model_read(volatile uint32_t* odr,
                                    volatile uint32_t* command) {
    uint32_t set = *command & UINT32_C(0xffff);
    uint32_t reset = *command >> 16U;
    if (odr == g_nx_spi_cs_fault_odr) {
        set &= ~g_nx_spi_cs_hold_low;
        reset &= ~g_nx_spi_cs_hold_high;
    }
    *odr = (*odr | set) & ~reset;
    return *odr;
}
