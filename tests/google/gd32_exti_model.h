/**
 * \file            gd32_exti_model.h
 * \brief           External EXTI write-one-to-clear register effect
 * \author          Nexus Team
 */
#ifndef NX_GD32_EXTI_MODEL_H
#define NX_GD32_EXTI_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "gd32f4xx.h"
/** \brief Model selected W1C writes; a stuck bit remains observable. */
void nx_gd32_exti_model_clear(uint32_t mask);
/** \brief Read the mapped INTEN register with injected rejected enable. */
volatile uint32_t* nx_gd32_exti_model_inten(void);
/** \brief Model NVIC enable and capture state-publication ordering. */
void nx_gd32_exti_model_enable_irq(unsigned irq);
/** \brief Model NVIC disable without overriding the production decision. */
void nx_gd32_exti_model_disable_irq(unsigned irq);
/** \brief Model selected NVIC pending clear, including refusal. */
void nx_gd32_exti_model_clear_irq(unsigned irq);
#ifdef __cplusplus
}
#endif
#undef EXTI_INTEN
#define EXTI_INTEN                (*nx_gd32_exti_model_inten())
#define NVIC_EnableIRQ(irq)       nx_gd32_exti_model_enable_irq((unsigned)(irq))
#define NX_GD32_EXTI_CLEAR(mask)  nx_gd32_exti_model_clear(mask)
#define NVIC_DisableIRQ(irq)      nx_gd32_exti_model_disable_irq((unsigned)(irq))
#define NVIC_ClearPendingIRQ(irq) nx_gd32_exti_model_clear_irq((unsigned)(irq))
#endif
