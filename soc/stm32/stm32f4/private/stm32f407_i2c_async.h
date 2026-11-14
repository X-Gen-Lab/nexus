/**
 * \file            stm32f407_i2c_async.h
 * \brief           Private exact I2C IRQ/DMA selection and immutable DMA facts
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_I2C_ASYNC_H
#define NEXUS_STM32F407_I2C_ASYNC_H
#include "i2c_v1_async.h"
#include "stm32f407_provider.h"
#include "stm32f4_dma_route.h"

/** \brief Validate supplied stream facts without choosing a default route.
 * \note Exact request routes belong to reviewed SoC facts. This cold check
 *       keeps register layout, direction, selector and publisher coherent.
 */
static inline bool
nx_stm32_i2c_dma_engine_valid(I2C_TypeDef* controller,
                              const nx_i2c_v1_dma_engine_t* engine,
                              bool receive) {
    uintptr_t address = (uintptr_t)engine->control;
    uintptr_t first = (uintptr_t)DMA1_Stream0;
    uintptr_t stride = DMA1_Stream1_BASE - DMA1_Stream0_BASE;
    if (address < first || (address - first) % stride != 0U ||
        (address - first) / stride > 7U)
        return false;
    uint8_t lane = (uint8_t)((address - first) / stride);
    DMA_Stream_TypeDef* stream = (DMA_Stream_TypeDef*)address;
    uint8_t channel = controller == I2C1 ? 1U : controller == I2C2 ? 7U : 3U;
    nx_stm32_dma_route_t selected = {DMA1, stream, (IRQn_Type)engine->publisher,
                                     lane, channel};
    uint32_t configuration = ((uint32_t)channel << 25U) | DMA_SxCR_PL_0 |
                             DMA_SxCR_MINC | DMA_SxCR_TCIE | DMA_SxCR_TEIE |
                             DMA_SxCR_DMEIE;
    if (!receive)
        configuration |= DMA_SxCR_DIR_0;
    uint8_t shift = (uint8_t)((lane % 2U) * 6U + (lane % 4U >= 2U ? 16U : 0U));
    return nx_stm32_dma_route_valid(&selected) &&
           engine->count == &stream->NDTR &&
           engine->peripheral == &stream->PAR &&
           engine->memory == &stream->M0AR && engine->fifo == &stream->FCR &&
           engine->flags == (lane < 4U ? &DMA1->LISR : &DMA1->HISR) &&
           engine->clear == (lane < 4U ? &DMA1->LIFCR : &DMA1->HIFCR) &&
           engine->configuration == configuration &&
           engine->flag_shift == shift;
}

/** \brief DMA publishers and stream storage are distinct within an owner. */
static inline bool nx_stm32_i2c_dma_valid(I2C_TypeDef* controller,
                                          const nx_i2c_v1_dma_t* selected) {
    return selected != NULL &&
           nx_stm32_i2c_dma_engine_valid(controller, &selected->tx, false) &&
           nx_stm32_i2c_dma_engine_valid(controller, &selected->rx, true) &&
           selected->tx.control != selected->rx.control &&
           selected->tx.publisher != selected->rx.publisher;
}

/** \brief Build-only aliases preserve typed assembly without runtime layers. */
typedef nx_i2c_v1_state_t nx_stm32_i2c_async_state_t;
typedef nx_i2c_v1_endpoint_t nx_stm32_i2c_async_endpoint_state_t;
#define nx_stm32_i2c_async_ops          nx_i2c_v1_async_ops
#define nx_stm32_i2c_async_endpoint_ops nx_i2c_v1_async_endpoint_ops

#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Initialize only the reviewed controller after clock/pin acquisition
 * \param[in,out] state: Zero-initialized exact register/line facts; dma NULL
 *                      selects IRQ messages, otherwise exact immutable route
 * \param[in] priority: Unshifted EV/ER and selected DMA publisher priority
 * \return SUCCESS, INVALID, CONTEXT, BUSY or UNSUPPORTED
 * \note No GPIO, clock, pool or worker is silently acquired. All selected
 *       NVIC publishers are withdrawn on successful public port stop.
 */
nx_result_t nx_stm32_i2c_async_initialize(nx_i2c_v1_state_t* state,
                                          uint8_t priority);
#ifdef __cplusplus
}
#endif
#endif
