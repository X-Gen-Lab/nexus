/**
 * \file            stm32_i2c_dma_fixture.h
 * \brief           Independent immutable routes for F4 I2C register models
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32_I2C_DMA_FIXTURE_H
#define NEXUS_STM32_I2C_DMA_FIXTURE_H
#include "stm32f407_i2c_async.h"

static inline nx_i2c_v1_dma_engine_t I2CDmaEngine(DMA_Stream_TypeDef* stream,
                                                  uint8_t lane, uint8_t channel,
                                                  IRQn_Type irq, bool receive) {
    uint32_t configuration = ((uint32_t)channel << 25U) | DMA_SxCR_PL_0 |
                             DMA_SxCR_MINC | DMA_SxCR_TCIE | DMA_SxCR_TEIE |
                             DMA_SxCR_DMEIE;
    if (!receive)
        configuration |= DMA_SxCR_DIR_0;
    return {&stream->CR,
            &stream->NDTR,
            &stream->PAR,
            &stream->M0AR,
            &stream->FCR,
            lane < 4U ? &DMA1->LISR : &DMA1->HISR,
            lane < 4U ? &DMA1->LIFCR : &DMA1->HIFCR,
            configuration,
            (uint8_t)((lane % 2U) * 6U + (lane % 4U >= 2U ? 16U : 0U)),
            (uint16_t)irq};
}

static inline nx_i2c_v1_dma_t I2C1Dma(const nx_dma_memory_region_t* regions,
                                      size_t count) {
    return {I2CDmaEngine(DMA1_Stream6, 6U, 1U, DMA1_Stream6_IRQn, false),
            I2CDmaEngine(DMA1_Stream0, 0U, 1U, DMA1_Stream0_IRQn, true),
            regions, count};
}

static inline nx_i2c_v1_dma_t I2C2Dma(const nx_dma_memory_region_t* regions,
                                      size_t count) {
    return {I2CDmaEngine(DMA1_Stream7, 7U, 7U, DMA1_Stream7_IRQn, false),
            I2CDmaEngine(DMA1_Stream2, 2U, 7U, DMA1_Stream2_IRQn, true),
            regions, count};
}

static inline nx_i2c_v1_dma_t I2C3Dma(const nx_dma_memory_region_t* regions,
                                      size_t count) {
    return {I2CDmaEngine(DMA1_Stream4, 4U, 3U, DMA1_Stream4_IRQn, false),
            I2CDmaEngine(DMA1_Stream2, 2U, 3U, DMA1_Stream2_IRQn, true),
            regions, count};
}
#endif /* NEXUS_STM32_I2C_DMA_FIXTURE_H */
