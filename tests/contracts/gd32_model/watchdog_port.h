/**
 * \file            watchdog_port.h
 * \brief           External watchdog MMIO ports for the legacy host model
 * \author          Nexus Team
 */
#ifndef NX_GD32_LEGACY_WATCHDOG_PORT_H
#define NX_GD32_LEGACY_WATCHDOG_PORT_H
#include <stdint.h>

/** \brief Apply a modeled watchdog write, including shadow-update faults. */
void nx_gd32_watchdog_model_write(volatile uint32_t* address, uint32_t value);
/** \brief Progress only externally modeled watchdog synchronization. */
void nx_gd32_watchdog_model_poll(void* port);

#endif
