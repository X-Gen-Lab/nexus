/**
 * \file            cipher_dma.c
 * \brief           Official basic CRYP DMA with separate optional state
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "crypto_validation.h"
#include "nexus/arch/arch.h"
#include "stm32f4_cipher.h"
#if !NEXUS_STM32_CIPHER_DMA
#error "Cipher DMA must be explicitly selected"
#endif
nx_result_t
nx_stm32f4_cipher_dma_admit(const nx_stm32f4_cipher_state_t* state,
                            const nx_stm32f4_cipher_config_t* config) {
    const nx_stm32f4_cipher_dma_config_t* dma = config->dma;
    if (dma == NULL)
        return NX_SUCCESS;
    if (!nx_crypto_span_valid(dma, sizeof(*dma),
                              _Alignof(nx_stm32f4_cipher_dma_config_t), false))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_stm32_crypto_dma_metadata(
        state, sizeof(*state), config, sizeof(*config), dma, sizeof(*dma),
        dma->state, sizeof(*dma->state), dma->regions, dma->region_count);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32_crypto_dma_admit(&dma->state->input,
                                       NX_STM32_CRYPTO_DMA_CIPHER_INPUT);
    return result == NX_SUCCESS
               ? nx_stm32_crypto_dma_admit(&dma->state->output,
                                           NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT)
               : result;
}
nx_result_t nx_stm32f4_cipher_dma_prepare(nx_stm32f4_cipher_state_t* state) {
    if (state->config->dma == NULL)
        return NX_SUCCESS;
    nx_stm32f4_cipher_dma_state_t* dma = state->config->dma->state;
    nx_result_t result = nx_stm32_crypto_dma_prepare(
        &dma->output, NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT, &state->handle);
    return result == NX_SUCCESS
               ? nx_stm32_crypto_dma_prepare(&dma->input,
                                             NX_STM32_CRYPTO_DMA_CIPHER_INPUT,
                                             &state->handle)
               : result;
}
nx_result_t nx_stm32f4_cipher_dma_submit(nx_stm32f4_cipher_state_t* state) {
    nx_stm32f4_cipher_dma_state_t* dma = state->config->dma->state;
    const nx_cipher_operation_t* operation = &state->active->operation;
    state->handle.hdmain = &dma->input.handle;
    state->handle.hdmaout = &dma->output.handle;
    dma->input.handle.Parent = dma->output.handle.Parent = &state->handle;
    uint32_t* input = (uint32_t*)(uintptr_t)operation->input;
    HAL_StatusTypeDef status =
        operation->direction == NX_CIPHER_ENCRYPT
            ? HAL_CRYP_Encrypt_DMA(&state->handle, input,
                                   (uint16_t)operation->bytes,
                                   operation->output)
            : HAL_CRYP_Decrypt_DMA(&state->handle, input,
                                   (uint16_t)operation->bytes,
                                   operation->output);
    if (status != HAL_OK || state->handle.ErrorCode != HAL_CRYP_ERROR_NONE)
        return NX_ERROR_IO;
    nx_arch_dsb();
    nx_result_t result = nx_stm32_crypto_dma_verify(
        &dma->input, NX_STM32_CRYPTO_DMA_CIPHER_INPUT, operation->input,
        operation->bytes / 4U);
    return result == NX_SUCCESS
               ? nx_stm32_crypto_dma_verify(
                     &dma->output, NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT,
                     operation->output, operation->bytes / 4U)
               : result;
}
nx_result_t nx_stm32f4_cipher_dma_observe(nx_stm32f4_cipher_state_t* state) {
    nx_stm32f4_cipher_dma_state_t* dma = state->config->dma->state;
    nx_result_t result =
        nx_stm32_crypto_dma_poll(&dma->input, NX_STM32_CRYPTO_DMA_CIPHER_INPUT);
    if (result == NX_SUCCESS)
        result = nx_stm32_crypto_dma_poll(&dma->output,
                                          NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT);
    if (result != NX_SUCCESS || state->handle.ErrorCode != HAL_CRYP_ERROR_NONE)
        return NX_ERROR_IO;
    return state->handle.State == HAL_CRYP_STATE_READY &&
                   dma->input.handle.State == HAL_DMA_STATE_READY &&
                   dma->output.handle.State == HAL_DMA_STATE_READY &&
                   READ_REG(DMA2_Stream6->NDTR) == 0U &&
                   READ_REG(DMA2_Stream5->NDTR) == 0U &&
                   (READ_REG(CRYP->SR) & (CRYP_SR_BUSY | CRYP_SR_OFNE |
                                          CRYP_SR_IFEM)) == CRYP_SR_IFEM
               ? NX_SUCCESS
               : NX_ERROR_BUSY;
}
nx_result_t nx_stm32f4_cipher_dma_withdraw(nx_stm32f4_cipher_state_t* state) {
    if (state->config == NULL || state->config->dma == NULL)
        return NX_SUCCESS;
    WRITE_REG(CRYP->DMACR, 0U);
    nx_arch_dsb();
    if (READ_REG(CRYP->DMACR) != 0U)
        return NX_ERROR_IO;
    nx_stm32f4_cipher_dma_state_t* dma = state->config->dma->state;
    nx_result_t result = nx_stm32_crypto_dma_withdraw(
        &dma->input, NX_STM32_CRYPTO_DMA_CIPHER_INPUT);
    nx_result_t output = nx_stm32_crypto_dma_withdraw(
        &dma->output, NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT);
    if (output != NX_SUCCESS)
        return output;
    if (state->active != NULL) {
        size_t block =
            state->active->operation.algorithm <= NX_CIPHER_AES_CTR ? 4U : 2U;
        state->handle.CrypOutCount =
            (uint16_t)(dma->output.completed_words / block * block);
    }
    return result;
}
nx_result_t nx_stm32f4_cipher_dma_stop(nx_stm32f4_cipher_state_t* state) {
    if (state->config == NULL || state->config->dma == NULL)
        return NX_SUCCESS;
    nx_stm32f4_cipher_dma_state_t* dma = state->config->dma->state;
    nx_result_t result =
        nx_stm32_crypto_dma_stop(&dma->input, NX_STM32_CRYPTO_DMA_CIPHER_INPUT);
    return result == NX_SUCCESS
               ? nx_stm32_crypto_dma_stop(&dma->output,
                                          NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT)
               : result;
}
