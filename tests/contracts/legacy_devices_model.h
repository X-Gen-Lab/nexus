/**
 * \file            legacy_devices_model.h
 * \brief           External legacy MMIO pulse and clock boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_LEGACY_DEVICES_MODEL_H
#define NX_LEGACY_DEVICES_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_legacy_model_read(volatile uint32_t* reg);
void nx_legacy_model_write(volatile uint32_t* reg, uint32_t value);
void nx_legacy_model_poll(unsigned kind, void* port);
void nx_legacy_model_program(volatile uint16_t* address, uint16_t value);
#define NX_LEGACY_READ(reg)         nx_legacy_model_read(reg)
#define NX_LEGACY_WRITE(reg, value) nx_legacy_model_write(reg, value)
#define NX_LEGACY_POLL(kind, port)  nx_legacy_model_poll(kind, port)
#define NX_LEGACY_PROGRAM(pointer, value)                                      \
    nx_legacy_model_program(pointer, value)
#ifdef __cplusplus
}
#endif
#endif
