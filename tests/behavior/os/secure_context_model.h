/**
 * \file            secure_context_model.h
 * \brief           Test-only Secure and Nonsecure CPU register boundary
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NEXUS_SECURE_CONTEXT_MODEL_H
#define NEXUS_SECURE_CONTEXT_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    NX_SECURE_REG_PRIMASK,
    NX_SECURE_REG_PRIMASK_NS,
    NX_SECURE_REG_PSP,
    NX_SECURE_REG_PSPLIM,
    NX_SECURE_REG_CONTROL,
    NX_SECURE_REG_AIRCR,
    NX_SECURE_REG_NSACR,
    NX_SECURE_REG_FPCCR
} nx_secure_register_t;
bool nx_secure_model_nonsecure_caller(void);
uintptr_t nx_secure_model_read(nx_secure_register_t reg);
void nx_secure_model_write(nx_secure_register_t reg, uintptr_t value);
bool nx_secure_model_secure_writable(void* pointer, size_t bytes);
void nx_secure_model_reset(void);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_SECURE_CONTEXT_MODEL_H */
