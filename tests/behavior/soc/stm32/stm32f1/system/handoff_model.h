/**
 * \file            handoff_model.h
 * \brief           Immutable hardware sampling and nonreturning tail model.
 * \author          Nexus Team
 */
#ifndef NEXUS_F103_HANDOFF_MODEL_H
#define NEXUS_F103_HANDOFF_MODEL_H
#include "nexus/soc/stm32f103rb_handoff.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t f103_read(uintptr_t address);
uint16_t f103_flash_kib(void);
uint32_t f103_control(void);
uint32_t f103_vector_word(uint32_t address);
nx_result_t f103_tail(uint32_t vector, uint32_t stack, uint32_t pc);
#ifdef __cplusplus
}
#endif
#endif
