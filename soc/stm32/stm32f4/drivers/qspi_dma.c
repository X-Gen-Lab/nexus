/**
 * \file            qspi_dma.c
 * \brief           Exact task-serviced QSPI program DMA and complete withdrawal
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-13
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f4_qspi.h"

#if !defined(STM32F479xx) || !NEXUS_QSPI_PROGRAM_DMA
#error "QSPI program DMA requires the selected reviewed F479 mode"
#endif

static const uint32_t stream_flags = DMA_HISR_FEIF7 | DMA_HISR_DMEIF7 |
                                     DMA_HISR_TEIF7 | DMA_HISR_HTIF7 |
                                     DMA_HISR_TCIF7;
static const uint32_t stream_sources =
    DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;

static bool vector_quiet(void) {
    return NVIC_GetEnableIRQ(DMA2_Stream7_IRQn) == 0U &&
           NVIC_GetActive(DMA2_Stream7_IRQn) == 0U;
}

nx_result_t nx_stm32_qspi_dma_admit(const nx_stm32_qspi_dma_config_t* config) {
    if (config == NULL)
        return NX_SUCCESS;
    const nx_stm32_qspi_dma_t* engine = config->state;
    return engine->captured || engine->prepared || engine->active ||
                   engine->transport.Instance != NULL || !vector_quiet() ||
                   NVIC_GetPendingIRQ(DMA2_Stream7_IRQn) != 0U
               ? NX_ERROR_BUSY
               : NX_SUCCESS;
}

nx_result_t nx_stm32_qspi_dma_prepare(nx_stm32_qspi_t* owner) {
    if (owner->config->dma == NULL)
        return NX_SUCCESS;
    if ((READ_REG(RCC->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U)
        return NX_ERROR_STATE;
    nx_stm32_qspi_dma_t* engine = owner->config->dma->state;
    if (!vector_quiet() || NVIC_GetPendingIRQ(DMA2_Stream7_IRQn) != 0U ||
        READ_REG(DMA2_Stream7->CR) != 0U ||
        READ_REG(DMA2_Stream7->NDTR) != 0U ||
        READ_REG(DMA2_Stream7->PAR) != 0U ||
        READ_REG(DMA2_Stream7->M0AR) != 0U ||
        READ_REG(DMA2_Stream7->M1AR) != 0U ||
        READ_REG(DMA2_Stream7->FCR) != 0x21U ||
        (READ_REG(DMA2->HISR) & stream_flags) != 0U)
        return NX_ERROR_BUSY;
    engine->captured = true;
    engine->transport.Instance = DMA2_Stream7;
    engine->transport.Init =
        (DMA_InitTypeDef){DMA_CHANNEL_3,        DMA_MEMORY_TO_PERIPH,
                          DMA_PINC_DISABLE,     DMA_MINC_ENABLE,
                          DMA_PDATAALIGN_BYTE,  DMA_MDATAALIGN_BYTE,
                          DMA_NORMAL,           DMA_PRIORITY_LOW,
                          DMA_FIFOMODE_DISABLE, DMA_FIFO_THRESHOLD_1QUARTERFULL,
                          DMA_MBURST_SINGLE,    DMA_PBURST_SINGLE};
    engine->transport.Parent = &owner->transport;
    owner->transport.hdma = &engine->transport;
    if (HAL_DMA_Init(&engine->transport) != HAL_OK)
        return NX_ERROR_IO;
    nx_arch_dsb();
    if (READ_REG(DMA2_Stream7->CR) !=
            (DMA_CHANNEL_3 | DMA_MEMORY_TO_PERIPH | DMA_MINC_ENABLE) ||
        (READ_REG(DMA2_Stream7->FCR) & DMA_SxFCR_DMDIS) != 0U)
        return NX_ERROR_IO;
    engine->prepared = true;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_qspi_dma_transmit(nx_stm32_qspi_t* owner, uint8_t* data) {
    nx_stm32_qspi_dma_t* engine = owner->config->dma->state;
    if (!engine->prepared || engine->active ||
        (READ_REG(DMA2_Stream7->CR) & DMA_SxCR_EN) != 0U ||
        engine->transport.State != HAL_DMA_STATE_READY || !vector_quiet())
        return NX_ERROR_STATE;
    engine->active = true;
    if (HAL_QSPI_Transmit_DMA(&owner->transport, data) != HAL_OK)
        return NX_ERROR_IO;
    nx_arch_dsb();
    uint32_t control = READ_REG(DMA2_Stream7->CR);
    uint32_t remaining = READ_REG(DMA2_Stream7->NDTR);
    /* DMA may already progress. Addresses and mode stay stable; a decreasing
     * count is legal, whereas an extended span or unproved EN0 is not. */
    if ((control & ~(DMA_SxCR_EN | stream_sources)) !=
            (DMA_CHANNEL_3 | DMA_MEMORY_TO_PERIPH | DMA_MINC_ENABLE) ||
        remaining > owner->transport.TxXferSize ||
        READ_REG(DMA2_Stream7->PAR) != (uint32_t)(uintptr_t)&QUADSPI->DR ||
        READ_REG(DMA2_Stream7->M0AR) != (uint32_t)(uintptr_t)data ||
        ((control & DMA_SxCR_EN) == 0U &&
         (remaining != 0U || (READ_REG(DMA2->HISR) & DMA_HISR_TCIF7) == 0U)))
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_qspi_dma_service(nx_stm32_qspi_t* owner) {
    if (owner->config->dma == NULL)
        return NX_SUCCESS;
    if ((READ_REG(RCC->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U || !vector_quiet())
        return NX_ERROR_STATE;
    nx_stm32_qspi_dma_t* engine = owner->config->dma->state;
    if (engine->active)
        HAL_DMA_IRQHandler(&engine->transport);
    return NX_SUCCESS;
}

nx_result_t nx_stm32_qspi_dma_withdraw(nx_stm32_qspi_t* owner) {
    if (owner->config->dma == NULL)
        return NX_SUCCESS;
    nx_stm32_qspi_dma_t* engine = owner->config->dma->state;
    if (!engine->captured)
        return NX_SUCCESS;
    if ((READ_REG(RCC->AHB1ENR) & RCC_AHB1ENR_DMA2EN) == 0U || !vector_quiet())
        return NX_ERROR_STATE;
    CLEAR_BIT(QUADSPI->CR, QUADSPI_CR_DMAEN);
    CLEAR_BIT(DMA2_Stream7->CR, DMA_SxCR_EN | stream_sources);
    CLEAR_BIT(DMA2_Stream7->FCR, DMA_SxFCR_FEIE);
    nx_arch_dsb();
    if ((READ_REG(QUADSPI->CR) & QUADSPI_CR_DMAEN) != 0U ||
        (READ_REG(DMA2_Stream7->CR) & (DMA_SxCR_EN | stream_sources)) != 0U) {
        if (engine->drain_deadline == 0U &&
            nx_deadline_finite_after(owner->last_clock,
                                     owner->config->stop_budget_us,
                                     &engine->drain_deadline) != NX_SUCCESS)
            return NX_ERROR_TIMEOUT;
        return owner->last_clock >= engine->drain_deadline ? NX_ERROR_TIMEOUT
                                                           : NX_ERROR_BUSY;
    }
    WRITE_REG(DMA2_Stream7->PAR, 0U);
    WRITE_REG(DMA2_Stream7->M0AR, 0U);
    WRITE_REG(DMA2_Stream7->M1AR, 0U);
    WRITE_REG(DMA2_Stream7->NDTR, 0U);
    WRITE_REG(DMA2->HIFCR, stream_flags);
    NVIC_ClearPendingIRQ(DMA2_Stream7_IRQn);
    nx_arch_dsb();
    if (READ_REG(DMA2_Stream7->PAR) != 0U ||
        READ_REG(DMA2_Stream7->M0AR) != 0U ||
        READ_REG(DMA2_Stream7->M1AR) != 0U ||
        READ_REG(DMA2_Stream7->NDTR) != 0U ||
        (READ_REG(DMA2->HISR) & stream_flags) != 0U ||
        NVIC_GetPendingIRQ(DMA2_Stream7_IRQn) != 0U)
        return NX_ERROR_IO;
    engine->transport.XferCpltCallback = NULL;
    engine->transport.XferHalfCpltCallback = NULL;
    engine->transport.XferM1CpltCallback = NULL;
    engine->transport.XferM1HalfCpltCallback = NULL;
    engine->transport.XferErrorCallback = NULL;
    engine->transport.XferAbortCallback = NULL;
    engine->transport.State = HAL_DMA_STATE_READY;
    engine->transport.Lock = HAL_UNLOCKED;
    engine->active = false;
    engine->drain_deadline = 0U;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_qspi_dma_stop(nx_stm32_qspi_t* owner) {
    nx_result_t result = nx_stm32_qspi_dma_withdraw(owner);
    if (result != NX_SUCCESS || owner->config->dma == NULL)
        return result;
    nx_stm32_qspi_dma_t* engine = owner->config->dma->state;
    if (!engine->captured)
        return NX_SUCCESS;
    WRITE_REG(DMA2_Stream7->CR, 0U);
    WRITE_REG(DMA2_Stream7->FCR, 0x21U);
    nx_arch_dsb();
    if (READ_REG(DMA2_Stream7->CR) != 0U ||
        READ_REG(DMA2_Stream7->FCR) != 0x21U)
        return NX_ERROR_IO;
    engine->transport.Instance = NULL;
    engine->transport.Parent = NULL;
    engine->transport.State = HAL_DMA_STATE_RESET;
    owner->transport.hdma = NULL;
    engine->prepared = engine->captured = false;
    return NX_SUCCESS;
}
