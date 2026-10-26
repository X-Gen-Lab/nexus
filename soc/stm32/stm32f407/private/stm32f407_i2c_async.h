/**
 * \file            stm32f407_i2c_async.h
 * \brief           Private exact I2C IRQ/DMA selection and immutable DMA facts
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_I2C_ASYNC_H
#define NEXUS_STM32F407_I2C_ASYNC_H
#include "../../../../drivers/i2c/i2c_v1_async/i2c_v1_async.h"
#include "stm32f407_provider.h"

/** \brief Exact reviewed physical routes; caller supplies immutable domains. */
#define NX_STM32_I2C1_DMA(memory_regions, memory_count)                        \
    {                                                                          \
        {&DMA1_Stream6->CR,                                                    \
         &DMA1_Stream6->NDTR,                                                  \
         &DMA1_Stream6->PAR,                                                   \
         &DMA1_Stream6->M0AR,                                                  \
         &DMA1_Stream6->FCR,                                                   \
         &DMA1->HISR,                                                          \
         &DMA1->HIFCR,                                                         \
         (1U << 25U) | (1U << 16U) | DMA_SxCR_MINC | DMA_SxCR_DIR_0 |          \
             DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE,                   \
         16U},                                                                 \
            {&DMA1_Stream0->CR,                                                \
             &DMA1_Stream0->NDTR,                                              \
             &DMA1_Stream0->PAR,                                               \
             &DMA1_Stream0->M0AR,                                              \
             &DMA1_Stream0->FCR,                                               \
             &DMA1->LISR,                                                      \
             &DMA1->LIFCR,                                                     \
             (1U << 25U) | (1U << 16U) | DMA_SxCR_MINC | DMA_SxCR_TCIE |       \
                 DMA_SxCR_TEIE | DMA_SxCR_DMEIE,                               \
             0U},                                                              \
            (memory_regions), (memory_count)                                   \
    }

/** \brief Independent second-controller DMA engines and selector tuple. */
#define NX_STM32_I2C2_DMA(memory_regions, memory_count)                        \
    {                                                                          \
        {&DMA1_Stream7->CR,                                                    \
         &DMA1_Stream7->NDTR,                                                  \
         &DMA1_Stream7->PAR,                                                   \
         &DMA1_Stream7->M0AR,                                                  \
         &DMA1_Stream7->FCR,                                                   \
         &DMA1->HISR,                                                          \
         &DMA1->HIFCR,                                                         \
         (7U << 25U) | (1U << 16U) | DMA_SxCR_MINC | DMA_SxCR_DIR_0 |          \
             DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE,                   \
         22U},                                                                 \
            {&DMA1_Stream2->CR,                                                \
             &DMA1_Stream2->NDTR,                                              \
             &DMA1_Stream2->PAR,                                               \
             &DMA1_Stream2->M0AR,                                              \
             &DMA1_Stream2->FCR,                                               \
             &DMA1->LISR,                                                      \
             &DMA1->LIFCR,                                                     \
             (7U << 25U) | (1U << 16U) | DMA_SxCR_MINC | DMA_SxCR_TCIE |       \
                 DMA_SxCR_TEIE | DMA_SxCR_DMEIE,                               \
             16U},                                                             \
            (memory_regions), (memory_count)                                   \
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
