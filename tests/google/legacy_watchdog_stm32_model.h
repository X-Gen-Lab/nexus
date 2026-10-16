/**
 * \file            legacy_watchdog_stm32_model.h
 * \brief           External register-write and poll faults for real F407 LL
 * \author          Nexus Team
 */
#ifndef NX_LEGACY_WATCHDOG_STM32_MODEL_H
#define NX_LEGACY_WATCHDOG_STM32_MODEL_H
#include "stm32_official_model.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_legacy_stm32_watchdog_write(volatile uint32_t* address, uint32_t value);
void nx_legacy_stm32_watchdog_poll(void* port);
#ifdef __cplusplus
}
#endif
#undef WRITE_REG
#define WRITE_REG(reg, value)                                                  \
    nx_legacy_stm32_watchdog_write(&(reg), (uint32_t)(value))
#define NX_STM32_WATCHDOG_WRITE(address, value)                                \
    nx_legacy_stm32_watchdog_write((address), (uint32_t)(value))
#define NX_STM32_IO_POLL(kind, port)                                           \
    ((void)(kind), nx_legacy_stm32_watchdog_poll(port))
#endif
