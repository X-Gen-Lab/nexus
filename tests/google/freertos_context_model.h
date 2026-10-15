/**
 * \file            freertos_context_model.h
 * \brief           Test-only NVIC register read boundary for the real adapter
 * \author          Nexus Team
 */
#ifndef NEXUS_FREERTOS_CONTEXT_MODEL_H
#define NEXUS_FREERTOS_CONTEXT_MODEL_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Read only the injected priority-grouping register field. */
uint32_t nx_freertos_test_priority_group(void);

/** \brief Read the injected external exception's priority register. */
uint8_t nx_freertos_test_irq_priority(uint32_t exception);

#ifdef __cplusplus
}
#endif

#define NX_FREERTOS_PRIORITY_GROUP() nx_freertos_test_priority_group()
#define NX_FREERTOS_IRQ_PRIORITY(exception)                                    \
    nx_freertos_test_irq_priority(exception)

#endif /* NEXUS_FREERTOS_CONTEXT_MODEL_H */
