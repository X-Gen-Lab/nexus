/**
 * \file            crypto_dma.c
 * \brief           Exact optional DMA2 crypto transports and proven withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "crypto_validation.h"
#include "nexus/arch/arch.h"
#include "stm32f407_system.h"
#include "stm32f4_crypto_dma.h"
#include <string.h>
#if !NEXUS_STM32_CRYPTO_DMA || (!defined(STM32F415xx) && !defined(STM32F417xx))
#error "Crypto DMA requires the reviewed selected F415/F417 mode"
#endif
extern nx_stm32_system_t g_nx_stm32_system;
static const uint32_t sources =
    DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;

/** \brief Fixed reviewed role selection never discovers a controller. */
static DMA_Stream_TypeDef* stream(nx_stm32_crypto_dma_role_t role) {
    return role == NX_STM32_CRYPTO_DMA_CIPHER_INPUT    ? DMA2_Stream6
           : role == NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT ? DMA2_Stream5
                                                       : DMA2_Stream7;
}
/** \brief Every selected vector remains disabled throughout task service. */
static IRQn_Type vector(nx_stm32_crypto_dma_role_t role) {
    return role == NX_STM32_CRYPTO_DMA_CIPHER_INPUT    ? DMA2_Stream6_IRQn
           : role == NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT ? DMA2_Stream5_IRQn
                                                       : DMA2_Stream7_IRQn;
}
/** \brief Exact high-bank flags preserve every sibling DMA stream. */
static uint32_t flags(nx_stm32_crypto_dma_role_t role) {
    return role == NX_STM32_CRYPTO_DMA_CIPHER_INPUT
               ? DMA_HISR_FEIF6 | DMA_HISR_DMEIF6 | DMA_HISR_TEIF6 |
                     DMA_HISR_HTIF6 | DMA_HISR_TCIF6
           : role == NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT
               ? DMA_HISR_FEIF5 | DMA_HISR_DMEIF5 | DMA_HISR_TEIF5 |
                     DMA_HISR_HTIF5 | DMA_HISR_TCIF5
               : DMA_HISR_FEIF7 | DMA_HISR_DMEIF7 | DMA_HISR_TEIF7 |
                     DMA_HISR_HTIF7 | DMA_HISR_TCIF7;
}
/** \brief A disabled but pending vector is still retained cleanup state. */
static bool quiet(nx_stm32_crypto_dma_role_t role) {
    return NVIC_GetEnableIRQ(vector(role)) == 0U &&
           NVIC_GetActive(vector(role)) == 0U;
}
/** \brief The output's higher priority is not an in-place safety promise. */
static uint32_t control(nx_stm32_crypto_dma_role_t role) {
    return DMA_CHANNEL_2 | DMA_MINC_ENABLE | DMA_PDATAALIGN_WORD |
           DMA_MDATAALIGN_WORD |
           (role == NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT
                ? DMA_PERIPH_TO_MEMORY | DMA_PRIORITY_HIGH
                : DMA_MEMORY_TO_PERIPH | DMA_PRIORITY_LOW);
}
/** \brief Reject fixed MMIO metadata while permitting real host test storage.
 */
static bool metadata(const void* data, size_t bytes, size_t alignment) {
    if (!nx_crypto_span_valid(data, bytes, alignment, false))
        return false;
    uint64_t begin = (uintptr_t)data, end = begin + bytes;
    return !(begin < UINT64_C(0x100000000) && end > UINT64_C(0x40000000));
}
nx_result_t nx_stm32_crypto_dma_metadata(
    const void* owner, size_t owner_bytes, const void* config,
    size_t config_bytes, const void* connection, size_t connection_bytes,
    const void* extension, size_t extension_bytes,
    const nx_dma_memory_region_t* regions, size_t count) {
    if (count == 0U || count > 8U ||
        !metadata(connection, connection_bytes, _Alignof(void*)) ||
        !metadata(extension, extension_bytes,
                  _Alignof(nx_stm32_crypto_dma_channel_t)) ||
        !metadata(regions, count * sizeof(*regions),
                  _Alignof(nx_dma_memory_region_t)))
        return NX_ERROR_INVALID;
    const void* mutable[] = {owner, extension, &g_nx_stm32_system};
    const size_t sizes[] = {owner_bytes, extension_bytes,
                            sizeof(g_nx_stm32_system)};
    const void* readonly[] = {config, connection, regions};
    const size_t extents[] = {config_bytes, connection_bytes,
                              count * sizeof(*regions)};
    for (size_t i = 0U; i < 3U; ++i) {
        for (size_t j = 0U; j < 3U; ++j) {
            if (nx_crypto_overlap(mutable[i], sizes[i], readonly[j],
                                  extents[j]) ||
                (i != j &&
                 nx_crypto_overlap(mutable[i], sizes[i], mutable[j], sizes[j])))
                return NX_ERROR_INVALID;
        }
    }
    for (size_t i = 0U; i < count; ++i) {
        const nx_dma_memory_region_t* region = &regions[i];
        if (region->size == 0U || region->size > UINTPTR_MAX - region->begin ||
            region->permissions == 0U ||
            (region->permissions &
             ~(uint32_t)(NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE)) != 0U)
            return NX_ERROR_INVALID;
        uintptr_t end = region->begin + region->size;
        if (!((region->begin >= 0x20000000U && end <= 0x20020000U) ||
              (region->begin >= 0x08000000U && end <= 0x08100000U &&
               region->permissions == NX_DMA_MEMORY_READ)))
            return NX_ERROR_PERMISSION;
    }
    return NX_SUCCESS;
}
nx_result_t
nx_stm32_crypto_dma_admit(const nx_stm32_crypto_dma_channel_t* channel,
                          nx_stm32_crypto_dma_role_t role) {
    if ((unsigned)role > NX_STM32_CRYPTO_DMA_HASH_INPUT ||
        !metadata(channel, sizeof(*channel),
                  _Alignof(nx_stm32_crypto_dma_channel_t)))
        return NX_ERROR_INVALID;
    return channel->captured || channel->prepared ||
                   channel->handle.Instance != NULL || !quiet(role) ||
                   NVIC_GetPendingIRQ(vector(role)) != 0U
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}
nx_result_t nx_stm32_crypto_dma_prepare(nx_stm32_crypto_dma_channel_t* channel,
                                        nx_stm32_crypto_dma_role_t role,
                                        void* parent) {
    nx_result_t result = nx_stm32_crypto_dma_admit(channel, role);
    if (result != NX_SUCCESS)
        return result;
    if ((READ_REG(RCC->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U)
        return NX_ERROR_STATE;
    DMA_Stream_TypeDef* registers = stream(role);
    if (READ_REG(registers->CR) != 0U || READ_REG(registers->NDTR) != 0U ||
        READ_REG(registers->PAR) != 0U || READ_REG(registers->M0AR) != 0U ||
        READ_REG(registers->M1AR) != 0U || READ_REG(registers->FCR) != 0x21U ||
        (READ_REG(DMA2->HISR) & flags(role)) != 0U)
        return NX_ERROR_BUSY;
    channel->captured = true;
    channel->handle.Instance = registers;
    channel->handle.Parent = parent;
    const bool output = role == NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT;
    channel->handle.Init =
        (DMA_InitTypeDef){DMA_CHANNEL_2,
                          output ? DMA_PERIPH_TO_MEMORY : DMA_MEMORY_TO_PERIPH,
                          DMA_PINC_DISABLE,
                          DMA_MINC_ENABLE,
                          DMA_PDATAALIGN_WORD,
                          DMA_MDATAALIGN_WORD,
                          DMA_NORMAL,
                          output ? DMA_PRIORITY_HIGH : DMA_PRIORITY_LOW,
                          DMA_FIFOMODE_DISABLE,
                          DMA_FIFO_THRESHOLD_1QUARTERFULL,
                          DMA_MBURST_SINGLE,
                          DMA_PBURST_SINGLE};
    if (HAL_DMA_Init(&channel->handle) != HAL_OK)
        return NX_ERROR_IO;
    nx_arch_dsb();
    if (READ_REG(registers->CR) != control(role) ||
        (READ_REG(registers->FCR) & (DMA_SxFCR_DMDIS | DMA_SxFCR_FEIE)) != 0U)
        return NX_ERROR_IO;
    channel->prepared = true;
    return NX_SUCCESS;
}
nx_result_t nx_stm32_crypto_dma_verify(nx_stm32_crypto_dma_channel_t* channel,
                                       nx_stm32_crypto_dma_role_t role,
                                       const void* memory, size_t words) {
    DMA_Stream_TypeDef* registers = stream(role);
    uint32_t cr = READ_REG(registers->CR),
             remaining = READ_REG(registers->NDTR);
    uint32_t peripheral =
        (uint32_t)(uintptr_t)(role == NX_STM32_CRYPTO_DMA_CIPHER_INPUT
                                  ? &CRYP->DIN
                              : role == NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT
                                  ? &CRYP->DOUT
                                  : &HASH->DIN);
    uint32_t complete =
        role == NX_STM32_CRYPTO_DMA_CIPHER_INPUT    ? DMA_HISR_TCIF6
        : role == NX_STM32_CRYPTO_DMA_CIPHER_OUTPUT ? DMA_HISR_TCIF5
                                                    : DMA_HISR_TCIF7;
    bool valid = channel->prepared && words != 0U && words <= UINT16_MAX &&
                 quiet(role) &&
                 (cr & ~(DMA_SxCR_EN | sources)) == control(role) &&
                 remaining <= words && READ_REG(registers->PAR) == peripheral &&
                 READ_REG(registers->M0AR) == (uint32_t)(uintptr_t)memory &&
                 READ_REG(registers->M1AR) == 0U &&
                 ((cr & DMA_SxCR_EN) != 0U ||
                  (remaining == 0U && (READ_REG(DMA2->HISR) & complete) != 0U));
    if (!valid)
        return NX_ERROR_IO;
    channel->words = (uint16_t)words;
    channel->completed_words = 0U;
    return NX_SUCCESS;
}
nx_result_t nx_stm32_crypto_dma_poll(nx_stm32_crypto_dma_channel_t* channel,
                                     nx_stm32_crypto_dma_role_t role) {
    if (!channel->prepared || !quiet(role) ||
        (READ_REG(RCC->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U)
        return NX_ERROR_STATE;
    if ((READ_REG(DMA2->HISR) & flags(role)) != 0U)
        HAL_DMA_IRQHandler(&channel->handle);
    nx_arch_dsb();
    return channel->handle.ErrorCode == HAL_DMA_ERROR_NONE ? NX_SUCCESS
                                                           : NX_ERROR_IO;
}
nx_result_t nx_stm32_crypto_dma_withdraw(nx_stm32_crypto_dma_channel_t* channel,
                                         nx_stm32_crypto_dma_role_t role) {
    if (!channel->captured)
        return NX_SUCCESS;
    if (!quiet(role) || (READ_REG(RCC->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U)
        return NX_ERROR_STATE;
    DMA_Stream_TypeDef* registers = stream(role);
    CLEAR_BIT(registers->CR, DMA_SxCR_EN | sources);
    CLEAR_BIT(registers->FCR, DMA_SxFCR_FEIE);
    nx_arch_dsb();
    if ((READ_REG(registers->CR) & (DMA_SxCR_EN | sources)) != 0U ||
        (READ_REG(registers->FCR) & DMA_SxFCR_FEIE) != 0U)
        return NX_ERROR_BUSY;
    if (channel->words != 0U) {
        uint32_t remaining = READ_REG(registers->NDTR);
        if (remaining <= channel->words)
            channel->completed_words = (uint16_t)(channel->words - remaining);
        channel->words = 0U;
    }
    WRITE_REG(registers->PAR, 0U);
    WRITE_REG(registers->M0AR, 0U);
    WRITE_REG(registers->M1AR, 0U);
    WRITE_REG(registers->NDTR, 0U);
    WRITE_REG(DMA2->HIFCR, flags(role));
    NVIC_ClearPendingIRQ(vector(role));
    nx_arch_dsb();
    if (READ_REG(registers->PAR) != 0U || READ_REG(registers->M0AR) != 0U ||
        READ_REG(registers->M1AR) != 0U || READ_REG(registers->NDTR) != 0U ||
        (READ_REG(DMA2->HISR) & flags(role)) != 0U ||
        NVIC_GetPendingIRQ(vector(role)) != 0U)
        return NX_ERROR_IO;
    channel->handle.XferCpltCallback = NULL;
    channel->handle.XferHalfCpltCallback = NULL;
    channel->handle.XferM1CpltCallback = NULL;
    channel->handle.XferM1HalfCpltCallback = NULL;
    channel->handle.XferErrorCallback = NULL;
    channel->handle.XferAbortCallback = NULL;
    channel->handle.Parent = NULL;
    channel->handle.State = HAL_DMA_STATE_READY;
    channel->handle.Lock = HAL_UNLOCKED;
    channel->handle.ErrorCode = HAL_DMA_ERROR_NONE;
    return NX_SUCCESS;
}
nx_result_t nx_stm32_crypto_dma_stop(nx_stm32_crypto_dma_channel_t* channel,
                                     nx_stm32_crypto_dma_role_t role) {
    nx_result_t result = nx_stm32_crypto_dma_withdraw(channel, role);
    if (result != NX_SUCCESS || !channel->captured)
        return result;
    WRITE_REG(stream(role)->CR, 0U);
    WRITE_REG(stream(role)->FCR, 0x21U);
    nx_arch_dsb();
    if (READ_REG(stream(role)->CR) != 0U ||
        READ_REG(stream(role)->FCR) != 0x21U)
        return NX_ERROR_IO;
    memset(channel, 0, sizeof(*channel));
    return NX_SUCCESS;
}
