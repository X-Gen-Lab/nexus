/**
 * \file            u575_security_model.h
 * \brief           U575 security MMIO and CPU instruction model boundary
 * \author          Nexus Team
 */
#ifndef NX_U575_SECURITY_MODEL_H
#define NX_U575_SECURITY_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_u575_tz_model_read(uintptr_t address);
void nx_u575_tz_model_write(uintptr_t address, uint32_t value);
uint32_t nx_u575_tz_model_msp(void);
uint32_t nx_u575_tz_model_control(void);
bool nx_u575_tz_model_clear(uint32_t address, uint32_t bytes);
void nx_u575_tz_model_enter(uint32_t stack, uint32_t limit, uint32_t entry);
#ifdef __cplusplus
}
#endif
#endif /* NX_U575_SECURITY_MODEL_H */
