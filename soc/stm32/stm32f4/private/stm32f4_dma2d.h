/**
 * \file            stm32f4_dma2d.h
 * \brief           Private single-shot HAL image-memory transport
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_DMA2D_H
#define NX_STM32F4_DMA2D_H
#include "nexus/arch/arch.h"
#include "nexus/io/dma.h"
#include "nexus/io/image.h"
#include "stm32f407_system.h"
#if !defined(USE_HAL_DMA2D_REGISTER_CALLBACKS)
#define USE_HAL_DMA2D_REGISTER_CALLBACKS 0U
#elif USE_HAL_DMA2D_REGISTER_CALLBACKS != 0U
#error "Nexus graphics requires USE_HAL_DMA2D_REGISTER_CALLBACKS=0"
#endif
#include "stm32f4xx_hal_dma2d.h"

#ifndef NX_IMG_READ
#define NX_IMG_READ(reg)         (*(reg))
#define NX_IMG_WRITE(reg, value) (*(reg) = (value))
#define NX_IMG_NOW()             nx_time_now_us()
#define NX_IMG_TASK()                                                          \
    (nx_arch_is_privileged() && !nx_arch_in_isr() && !nx_arch_irq_is_masked())
#define NX_IMG_IRQ_ENABLED()           NVIC_GetEnableIRQ(DMA2D_IRQn)
#define NX_IMG_IRQ_PENDING()           NVIC_GetPendingIRQ(DMA2D_IRQn)
#define NX_IMG_IRQ_ACTIVE()            NVIC_GetActive(DMA2D_IRQn)
#define NX_IMG_IRQ_ENABLE()            NVIC_EnableIRQ(DMA2D_IRQn)
#define NX_IMG_IRQ_DISABLE()           NVIC_DisableIRQ(DMA2D_IRQn)
#define NX_IMG_IRQ_CLEAR()             NVIC_ClearPendingIRQ(DMA2D_IRQn)
#define NX_IMG_IRQ_PRIORITY()          NVIC_GetPriority(DMA2D_IRQn)
#define NX_IMG_IRQ_SET_PRIORITY(value) NVIC_SetPriority(DMA2D_IRQn, (value))
#define NX_IMG_IRQ_GROUP()             NVIC_GetPriorityGrouping()
#endif

/** \brief Reviewed static DMA domains and live System ownership, not scripts.
 * \note Parent System owns the AHB domain; this controller owns its DMA2D
 *       gate through close. Config is kept alive until successful close.
 *       This ignored candidate has no production factory integration.
 */
typedef struct {
    DMA2D_TypeDef* registers;
    RCC_TypeDef* rcc;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    const nx_stm32_system_t* system;
    nx_irq_policy_t irq_policy;
    nx_time_us_t drain_us;
    uint32_t system_cfgr;
    uint8_t priority;
} nx_stm32_image_config_t;

/** \brief One HAL context, active loan and exact retryable vector receipt. */
typedef struct {
    DMA2D_HandleTypeDef handle;
    const nx_stm32_image_config_t* config;
    nx_image_request_t* active;
    const nx_irq_wake_t* wake;
    uint64_t epoch;
    nx_time_us_t drain_deadline;
    volatile uint32_t events;
    uint32_t publishers;
    uint32_t priority;
    bool owned;
    bool initialized;
    bool vector_owned;
    bool draining;
    bool gate_owned;
    bool controller_touched;
    bool closing;
    bool executing;
    bool clock_fault;
} nx_stm32_image_state_t;

/** \brief Pure pre-System admission; no gate, pad, snapshot or state effect.
 * \note Validates static extents and rejects foreign gates/vectors while
 *       System and GPIO domains may still be stopped. No references retained.
 */
nx_result_t nx_stm32_image_admit(nx_stm32_image_state_t* state,
                                 const nx_stm32_image_config_t* config);
/** \brief Acquire a trusted-reset controller gate and a disabled vector.
 * \note System owns a live AHB domain. A foreign enabled gate or IRQ rejects
 *       before effects. Partial prepare retains its attempted gate receipt
 *       and config; retry close before returning their storage.
 */
nx_result_t nx_stm32_image_prepare(nx_stm32_image_state_t* state,
                                   const nx_stm32_image_config_t* config);
/** \brief Finite retryable withdrawal; success returns config responsibility.
 */
nx_result_t nx_stm32_image_close(nx_stm32_image_state_t* state);
/** \brief Actual HAL IRQ callbacks only latch facts and issue bounded hints. */
void nx_stm32_image_irq(nx_stm32_image_state_t* state);
/** \brief Shared readonly native fill/copy/convert/blend operations. */
extern const nx_image_ops_t nx_stm32_image_ops;
#endif /* NX_STM32F4_DMA2D_H */
