/**
 * \file            g474_pin_state_model.h
 * \brief           External actual-layout GPIO and bank-access observations
 * \author          Nexus Team
 */
#ifndef NX_G474_PIN_STATE_MODEL_H
#define NX_G474_PIN_STATE_MODEL_H
#include "nexus/arch/arch.h"
#include "stm32g474xx.h"
#ifdef NX_PIN_STATE_FACTORY_MODEL
#include "g474_factory_model.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    RCC_TypeDef rcc;
    GPIO_TypeDef gpio;
    bool privileged;
    bool isr;
    bool unsafe_electrical;
    bool unsafe_drive;
    bool unselected_change;
    uint32_t pins;
    uint32_t expected_output_mask;
    uint32_t expected_levels;
    uint32_t reads;
    uint32_t writes;
    uint32_t gpio_reads;
    uint32_t gpio_writes;
    uint32_t forbidden;
    uint32_t unmasked_writes;
    uint32_t rcc_writes;
    volatile uint32_t* refuse_write;
    void (*after_read)(volatile uint32_t*);
    void (*after_write)(volatile uint32_t*, uint32_t);
} nx_pin_state_model_t;
extern nx_pin_state_model_t g_pin_state_model;
void nx_pin_state_model_reset(void);
void nx_pin_state_model_counters(void);
uint32_t nx_pin_state_model_read(volatile uint32_t* reg);
void nx_pin_state_model_write(volatile uint32_t* reg, uint32_t value);
bool nx_pin_state_model_privileged(void);
bool nx_pin_state_model_isr(void);
#ifdef __cplusplus
}
#endif
#define NX_PIN_STATE_MODEL 1
#define NX_PIN_READ(reg)   nx_pin_state_model_read(reg)
#define NX_PIN_WRITE(reg, value)                                               \
    nx_pin_state_model_write(reg, (uint32_t)(value))
#ifdef NX_PIN_STATE_FACTORY_MODEL
#undef NX_G474_READ
#undef NX_G474_WRITE
#undef NX_PLATFORM_REGISTER_WRITE
#define NX_G474_READ(reg) nx_pin_state_model_read(reg)
#define NX_G474_WRITE(reg, value)                                              \
    nx_pin_state_model_write(reg, (uint32_t)(value))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_pin_state_model_write(&(reg), (uint32_t)(value))
#endif
#if defined(NX_PIN_STATE_MODEL_C) && !defined(NX_PIN_STATE_FACTORY_MODEL)
#define nx_arch_is_privileged nx_pin_state_model_privileged
#define nx_arch_in_isr        nx_pin_state_model_isr
#endif
#endif
