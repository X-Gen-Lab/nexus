/**
 * \file            hash_dma.c
 * \brief           Official whole-word HASH and HMAC DMA with explicit limits
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "crypto_validation.h"
#include "nexus/arch/arch.h"
#include "stm32f4_hash.h"
#if !NEXUS_STM32_HASH_DMA
#error "HASH DMA must be explicitly selected"
#endif
nx_result_t nx_stm32f4_hash_dma_admit(const nx_stm32f4_hash_state_t* state,
                                      const nx_stm32f4_hash_config_t* config) {
    const nx_stm32f4_hash_dma_config_t* dma = config->dma;
    if (dma == NULL)
        return NX_SUCCESS;
    if (!nx_crypto_span_valid(dma, sizeof(*dma),
                              _Alignof(nx_stm32f4_hash_dma_config_t), false))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32_crypto_dma_metadata(
        state, sizeof(*state), config, sizeof(*config), dma, sizeof(*dma),
        dma->state, sizeof(*dma->state), dma->regions, dma->region_count);
    if (result != NX_SUCCESS)
        return result;
    return nx_stm32_crypto_dma_admit(&dma->state->input,
                                     NX_STM32_CRYPTO_DMA_HASH_INPUT);
}
nx_result_t nx_stm32f4_hash_dma_prepare(nx_stm32f4_hash_state_t* state) {
    if (state->config->dma == NULL)
        return NX_SUCCESS;
    nx_stm32f4_hash_dma_state_t* dma = state->config->dma->state;
    return nx_stm32_crypto_dma_prepare(
        &dma->input, NX_STM32_CRYPTO_DMA_HASH_INPUT, &dma->handle);
}
/** \brief Validate each official HMAC phase's live descriptor independently. */
static nx_result_t descriptor(nx_stm32f4_hash_state_t* state) {
    nx_stm32f4_hash_dma_state_t* dma = state->config->dma->state;
    const nx_hash_operation_t* operation = &state->active->operation;
    bool key = dma->handle.Phase == HAL_HASH_PHASE_HMAC_STEP_1 ||
               dma->handle.Phase == HAL_HASH_PHASE_HMAC_STEP_3;
    return nx_stm32_crypto_dma_verify(
        &dma->input, NX_STM32_CRYPTO_DMA_HASH_INPUT,
        key ? operation->key : operation->input,
        (key ? operation->key_bytes : operation->bytes) / 4U);
}
nx_result_t nx_stm32f4_hash_dma_submit(nx_stm32f4_hash_state_t* state) {
    nx_stm32f4_hash_dma_state_t* dma = state->config->dma->state;
    const nx_hash_operation_t* operation = &state->active->operation;
    bool keyed = operation->algorithm == NX_HASH_HMAC_SHA1 ||
                 operation->algorithm == NX_HASH_HMAC_MD5;
    bool md5 = operation->algorithm == NX_HASH_MD5 ||
               operation->algorithm == NX_HASH_HMAC_MD5;
    dma->handle.Init.DataType = HASH_DATATYPE_8B;
    dma->handle.Init.pKey = (uint8_t*)(uintptr_t)operation->key;
    dma->handle.Init.KeySize = (uint32_t)operation->key_bytes;
    dma->handle.hdmain = &dma->input.handle;
    dma->input.handle.Parent = &dma->handle;
    if (HAL_HASH_Init(&dma->handle) != HAL_OK)
        return NX_ERROR_IO;
    HAL_StatusTypeDef status =
        keyed ? md5 ? HAL_HMAC_MD5_Start_DMA(&dma->handle, operation->input,
                                             (uint32_t)operation->bytes)
                    : HAL_HMAC_SHA1_Start_DMA(&dma->handle, operation->input,
                                              (uint32_t)operation->bytes)
        : md5 ? HAL_HASH_MD5_Start_DMA(&dma->handle, operation->input,
                                       (uint32_t)operation->bytes)
              : HAL_HASH_SHA1_Start_DMA(&dma->handle, operation->input,
                                        (uint32_t)operation->bytes);
    if (status != HAL_OK || dma->handle.ErrorCode != HAL_HASH_ERROR_NONE)
        return NX_ERROR_IO;
    WRITE_REG(HASH->IMR, HASH_IMR_DCIE);
    nx_arch_dsb();
    return READ_REG(HASH->IMR) == HASH_IMR_DCIE ? descriptor(state)
                                                : NX_ERROR_IO;
}
nx_result_t nx_stm32f4_hash_dma_observe(nx_stm32f4_hash_state_t* state) {
    nx_stm32f4_hash_dma_state_t* dma = state->config->dma->state;
    uint32_t phase = dma->handle.Phase;
    nx_result_t result =
        nx_stm32_crypto_dma_poll(&dma->input, NX_STM32_CRYPTO_DMA_HASH_INPUT);
    if (result != NX_SUCCESS || dma->handle.ErrorCode != HAL_HASH_ERROR_NONE ||
        dma->handle.State == HAL_HASH_STATE_ERROR)
        return NX_ERROR_IO;
    if (dma->handle.Phase != phase &&
        dma->handle.State == HAL_HASH_STATE_BUSY &&
        descriptor(state) != NX_SUCCESS)
        return NX_ERROR_IO;
    if (dma->handle.State != HAL_HASH_STATE_READY ||
        (READ_REG(HASH->SR) & (HASH_SR_BUSY | HASH_SR_DCIS | HASH_SR_DMAS)) !=
            HASH_SR_DCIS)
        return NX_ERROR_BUSY;
    /* The common provider copies the ready digest as bytes. HAL Finish's
     * aligned uint32_t destination cannot implement that public contract. */
    return NX_SUCCESS;
}
nx_result_t nx_stm32f4_hash_dma_withdraw(nx_stm32f4_hash_state_t* state) {
    if (state->config == NULL || state->config->dma == NULL)
        return NX_SUCCESS;
    CLEAR_BIT(HASH->CR, HASH_CR_DMAE);
    nx_arch_dsb();
    if ((READ_REG(HASH->CR) & HASH_CR_DMAE) != 0U)
        return NX_ERROR_IO;
    nx_stm32f4_hash_dma_state_t* dma = state->config->dma->state;
    nx_result_t result = nx_stm32_crypto_dma_withdraw(
        &dma->input, NX_STM32_CRYPTO_DMA_HASH_INPUT);
    if (result == NX_SUCCESS) {
        volatile uint8_t* bytes = (volatile uint8_t*)&dma->handle;
        for (size_t i = 0U; i < sizeof(dma->handle); ++i)
            bytes[i] = 0U;
    }
    return result;
}
nx_result_t nx_stm32f4_hash_dma_stop(nx_stm32f4_hash_state_t* state) {
    if (state->config == NULL || state->config->dma == NULL)
        return NX_SUCCESS;
    return nx_stm32_crypto_dma_stop(&state->config->dma->state->input,
                                    NX_STM32_CRYPTO_DMA_HASH_INPUT);
}
