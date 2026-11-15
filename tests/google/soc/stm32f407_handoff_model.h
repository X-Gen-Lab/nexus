/**
 * \file            stm32f407_handoff_model.h
 * \brief           Host-only F407 MMIO and final transfer instruction boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_HANDOFF_MODEL_H
#define NEXUS_STM32F407_HANDOFF_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Read one official register or admitted vector word. */
uint32_t nx_f407_handoff_model_read(uintptr_t address);
/** \brief Read the physical Flash-density halfword. */
uint32_t nx_f407_handoff_model_read16(uintptr_t address);
/** \brief Read the special CONTROL register. */
uint32_t nx_f407_handoff_model_control(void);
/** \brief Observe the nonreturning VTOR/MSP/PC transfer. */
__attribute__((noreturn)) void
nx_f407_handoff_model_tail(uint32_t vector, uint32_t stack, uint32_t pc);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F407_HANDOFF_MODEL_H */
