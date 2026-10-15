/**
 * \file            m4_model.h
 * \brief           Test-only CPU register and bootstrap mask observations
 * \author          Nexus Team
 */
#ifndef NEXUS_THREADX_M4_MODEL_H
#define NEXUS_THREADX_M4_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
volatile uint32_t* nx_threadx_m4_model_register(uintptr_t address);
void nx_threadx_m4_model_disable(void);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_THREADX_M4_MODEL_H */
