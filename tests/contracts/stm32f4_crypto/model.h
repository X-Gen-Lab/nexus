/**
 * \file            model.h
 * \brief           External MMIO effects for the actual CRYP HAL and owner
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_CRYPTO_MODEL_H
#define NEXUS_TEST_CRYPTO_MODEL_H
#include "../../cmake/stm32_scs_model.h"
#include "stm32f4xx_hal.h"
#ifdef __cplusplus
extern "C" {
#endif
void nx_crypto_model_write(volatile uint32_t* reg, uint32_t value);
void nx_crypto_model_cryp_key(volatile uint32_t* reg, uint32_t word);
void nx_crypto_model_cryp_input(void* instance, uint32_t word);
uint32_t nx_crypto_model_cryp_output(void* instance);
void nx_crypto_model_hash_input(uint32_t word);
uint32_t nx_crypto_model_hash_digest(unsigned index);
void nx_crypto_model_irq_clear(IRQn_Type irq);
#ifdef __cplusplus
}
#endif
#undef WRITE_REG
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define WRITE_REG(REG, VAL) nx_crypto_model_write(&(REG), (uint32_t)(VAL))
#define SET_BIT(REG, BIT)   WRITE_REG(REG, READ_REG(REG) | (BIT))
#define CLEAR_BIT(REG, BIT) WRITE_REG(REG, READ_REG(REG) & ~(BIT))
#define MODIFY_REG(REG, CLEAR, SET)                                            \
    WRITE_REG(REG, (READ_REG(REG) & ~(CLEAR)) | (SET))
#undef NVIC_ClearPendingIRQ
#define NVIC_ClearPendingIRQ(irq) nx_crypto_model_irq_clear(irq)
#endif
