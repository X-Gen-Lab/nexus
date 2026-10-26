/**
 * \file            gd32f470_i2c_async.h
 * \brief           Private exact I2C IRQ/DMA selection and immutable DMA facts
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_I2C_ASYNC_H
#define NEXUS_GD32F470_I2C_ASYNC_H
#include "../../../../drivers/i2c/i2c_v1_async/i2c_v1_async.h"
#include "gd32f470_provider.h"

/** \brief Exact reviewed physical routes; caller supplies immutable domains. */
#define NX_GD32_I2C0_DMA(memory_regions, memory_count)                         \
    {                                                                          \
        {&DMA_CH6CTL(DMA0),                                                    \
         &DMA_CH6CNT(DMA0),                                                    \
         &DMA_CH6PADDR(DMA0),                                                  \
         &DMA_CH6M0ADDR(DMA0),                                                 \
         &DMA_CH6FCTL(DMA0),                                                   \
         &DMA_INTF1(DMA0),                                                     \
         &DMA_INTC1(DMA0),                                                     \
         (1U << 25U) | (1U << 17U) | DMA_CHXCTL_MNAGA | (1U << 6U) |           \
             DMA_CHXCTL_FTFIE | DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE,           \
         16U},                                                                 \
            {&DMA_CH0CTL(DMA0),                                                \
             &DMA_CH0CNT(DMA0),                                                \
             &DMA_CH0PADDR(DMA0),                                              \
             &DMA_CH0M0ADDR(DMA0),                                             \
             &DMA_CH0FCTL(DMA0),                                               \
             &DMA_INTF0(DMA0),                                                 \
             &DMA_INTC0(DMA0),                                                 \
             (1U << 25U) | (1U << 17U) | DMA_CHXCTL_MNAGA | DMA_CHXCTL_FTFIE | \
                 DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE,                          \
             0U},                                                              \
            (memory_regions), (memory_count)                                   \
    }

/** \brief Independent second-controller DMA engines and selector tuple. */
#define NX_GD32_I2C1_DMA(memory_regions, memory_count)                         \
    {                                                                          \
        {&DMA_CH7CTL(DMA0),                                                    \
         &DMA_CH7CNT(DMA0),                                                    \
         &DMA_CH7PADDR(DMA0),                                                  \
         &DMA_CH7M0ADDR(DMA0),                                                 \
         &DMA_CH7FCTL(DMA0),                                                   \
         &DMA_INTF1(DMA0),                                                     \
         &DMA_INTC1(DMA0),                                                     \
         (7U << 25U) | (1U << 17U) | DMA_CHXCTL_MNAGA | (1U << 6U) |           \
             DMA_CHXCTL_FTFIE | DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE,           \
         22U},                                                                 \
            {&DMA_CH2CTL(DMA0),                                                \
             &DMA_CH2CNT(DMA0),                                                \
             &DMA_CH2PADDR(DMA0),                                              \
             &DMA_CH2M0ADDR(DMA0),                                             \
             &DMA_CH2FCTL(DMA0),                                               \
             &DMA_INTF0(DMA0),                                                 \
             &DMA_INTC0(DMA0),                                                 \
             (7U << 25U) | (1U << 17U) | DMA_CHXCTL_MNAGA | DMA_CHXCTL_FTFIE | \
                 DMA_CHXCTL_TAEIE | DMA_CHXCTL_SDEIE,                          \
             16U},                                                             \
            (memory_regions), (memory_count)                                   \
    }

/** \brief Build-only aliases preserve typed assembly without runtime layers. */
typedef nx_i2c_v1_state_t nx_gd32_i2c_async_state_t;
typedef nx_i2c_v1_endpoint_t nx_gd32_i2c_async_endpoint_state_t;
#define nx_gd32_i2c_async_ops          nx_i2c_v1_async_ops
#define nx_gd32_i2c_async_endpoint_ops nx_i2c_v1_async_endpoint_ops

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
nx_result_t nx_gd32_i2c_async_initialize(nx_i2c_v1_state_t* state,
                                         uint8_t priority);
#ifdef __cplusplus
}
#endif
#endif
