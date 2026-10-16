/**
 * \file            legacy_watchdog_gd32_model.h
 * \brief           External GD470 register and write-refusal model boundary
 * \author          Nexus Team
 */
#ifndef NX_LEGACY_WATCHDOG_GD32_MODEL_H
#define NX_LEGACY_WATCHDOG_GD32_MODEL_H
#include "gd32f4xx.h"
#ifdef __cplusplus
extern "C" {
#endif
volatile uint32_t* nx_legacy_gd32_watchdog_address(uint32_t address);
void nx_legacy_gd32_watchdog_write(volatile uint32_t* address, uint32_t value);
void nx_legacy_gd32_watchdog_poll(void* port);
#ifdef __cplusplus
}
#endif
#undef REG32
#define REG32(address) (*nx_legacy_gd32_watchdog_address((uint32_t)(address)))
#define NX_GD32_WATCHDOG_WRITE(address, value)                                 \
    nx_legacy_gd32_watchdog_write((address), (uint32_t)(value))
#define NX_GD32_WATCHDOG_POLL(port) nx_legacy_gd32_watchdog_poll(port)
#define NX_GD32_WATCHDOG_POLL_LIMIT 3U
#endif
