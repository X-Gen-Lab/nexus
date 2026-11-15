/**
 * \file            entropy_model.h
 * \brief           Host-only RNG/RCC register boundary for official LL calls
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_ENTROPY_MODEL_H
#define NEXUS_TEST_ENTROPY_MODEL_H

#include "stm32f4xx.h"

#ifdef __cplusplus
extern "C" {
#endif
extern RNG_TypeDef nx_entropy_model_rng;
extern RCC_TypeDef nx_entropy_model_rcc;
/** \brief Model volatile reads and RNG_DR's consume-on-read behavior. */
uint32_t nx_entropy_model_read(const volatile uint32_t* address);
/** \brief Model register writes and RNG status's write-zero-to-clear bits. */
void nx_entropy_model_write(volatile uint32_t* address, uint32_t value);
#ifdef __cplusplus
}
#endif

#undef RNG
#undef RCC
#define RNG (&nx_entropy_model_rng)
#define RCC (&nx_entropy_model_rcc)
#undef READ_REG
#undef WRITE_REG
#undef READ_BIT
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define READ_REG(reg)         nx_entropy_model_read(&(reg))
#define WRITE_REG(reg, value) nx_entropy_model_write(&(reg), (uint32_t)(value))
#define READ_BIT(reg, bits)   (READ_REG(reg) & (uint32_t)(bits))
#define SET_BIT(reg, bits)    WRITE_REG(reg, READ_REG(reg) | (uint32_t)(bits))
#define CLEAR_BIT(reg, bits)  WRITE_REG(reg, READ_REG(reg) & ~(uint32_t)(bits))
#define MODIFY_REG(reg, clear, set)                                            \
    WRITE_REG(reg, (READ_REG(reg) & ~(uint32_t)(clear)) | (uint32_t)(set))

#endif /* NEXUS_TEST_ENTROPY_MODEL_H */
