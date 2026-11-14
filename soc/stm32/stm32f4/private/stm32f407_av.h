/**
 * \file            stm32f407_av.h
 * \brief           Private F407 capture/audio DMA and resource observations
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_AV_H
#define NX_STM32F407_AV_H

#include "nexus/arch/arch.h"
#include "nexus/core/time.h"
#include "nexus/io/frame_stream.h"
#include "stm32f407_clock.h"
#include "stm32f407_platform_cold.h"
#include "stm32f4_dma_route.h"

#ifndef NX_AV_READ
#define NX_AV_READ(reg)                   (*(reg))
#define NX_AV_WRITE(reg, value)           (*(reg) = (value))
#define NX_AV_POLL()                      ((void)0)
#define NX_AV_IRQ_ENABLE(irq)             NVIC_EnableIRQ(irq)
#define NX_AV_IRQ_DISABLE(irq)            NVIC_DisableIRQ(irq)
#define NX_AV_IRQ_CLEAR(irq)              NVIC_ClearPendingIRQ(irq)
#define NX_AV_IRQ_ACTIVE(irq)             NVIC_GetActive(irq)
#define NX_AV_IRQ_ENABLED(irq)            NVIC_GetEnableIRQ(irq)
#define NX_AV_IRQ_PENDING(irq)            NVIC_GetPendingIRQ(irq)
#define NX_AV_IRQ_PRIORITY(irq, priority) NVIC_SetPriority((irq), (priority))
#endif
#ifndef NX_AV_IRQ_PRIORITY_READ
#define NX_AV_IRQ_PRIORITY_READ(irq) NVIC_GetPriority(irq)
#endif
#ifndef NX_AV_NOW
#define NX_AV_NOW() nx_time_now_us()
#endif
#ifndef NX_AV_PRIVILEGED
#define NX_AV_PRIVILEGED() nx_arch_is_privileged()
#define NX_AV_IN_ISR()     nx_arch_in_isr()
#define NX_AV_MASKED()     nx_arch_irq_is_masked()
#endif

/** \brief One reviewed pad; snapshot is owned by the shared Platform lease. */
typedef struct {
    GPIO_TypeDef* registers;
    uint16_t* snapshot;
    uint8_t bank;
    uint8_t pin;
    uint8_t af;
} nx_stm32_av_pin_t;

/** \brief Exact independent DMA stream; no runtime engine discovery. */
typedef nx_stm32_dma_route_t nx_stm32_av_dma_t;

/** \brief Selected pad/gate/readback budget, not a register script. */
typedef struct {
    const nx_stm32_av_pin_t* pins;
    size_t pin_count;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    uint32_t poll_limit;
    uint8_t priority;
} nx_stm32_av_config_t;

/** \brief The System owns monotonic epochs; no old borrow crosses a restart. */
static inline uint64_t nx_stm32_av_epoch(void) {
#ifdef NX_STM32_SYSTEM_HAS_TIME_EPOCH
    return g_nx_stm32_system.time_epoch;
#else
    return 0U;
#endif
}
/** \brief Restore only the IRQ priority that this exact owner captured. */
static inline bool nx_stm32_av_irq_priority_restore(IRQn_Type irq,
                                                    uint32_t priority) {
    NX_AV_IRQ_PRIORITY(irq, priority);
    nx_arch_dsb();
    return NX_AV_IRQ_PRIORITY_READ(irq) == priority;
}
/** \brief Require one serialized privileged unmasked task executor. */
static inline bool nx_stm32_av_task(void) {
    return NX_AV_PRIVILEGED() && !NX_AV_IN_ISR() && !NX_AV_MASKED();
}
/** \brief Observe the actual microsecond source before trusting its history. */
static inline bool nx_stm32_av_timebase(void) {
    RCC_TypeDef* rcc = g_nx_stm32_system.rcc;
    TIM_TypeDef* timer = g_nx_stm32_system.timer;
    return rcc != NULL && timer != NULL &&
           (NX_AV_READ(&rcc->APB1ENR) & NEXUS_STM32_TIMEBASE_GATE) != 0U &&
           (NX_AV_READ(&rcc->APB1RSTR) & NEXUS_STM32_TIMEBASE_RESET) == 0U &&
           NX_AV_READ(&timer->PSC) == NEXUS_APB1_TIMER_HZ / 1000000U - 1U &&
           NX_AV_READ(&timer->ARR) == UINT32_MAX &&
           (NX_AV_READ(&timer->CR1) &
            (TIM_CR1_CEN | TIM_CR1_UDIS | TIM_CR1_OPM | TIM_CR1_DIR |
             TIM_CR1_CMS)) == TIM_CR1_CEN &&
           (NX_AV_READ(&timer->DIER) & TIM_DIER_UIE) != 0U;
}
/** \brief Write then independently observe only the owned register fields. */
static inline bool nx_stm32_av_write(volatile uint32_t* reg, uint32_t value,
                                     uint32_t mask) {
    NX_AV_WRITE(reg, value);
    nx_arch_dsb();
    return (NX_AV_READ(reg) & mask) == (value & mask);
}
/** \brief Preserve unrelated owner fields while observing our own update. */
static inline bool nx_stm32_av_bits(volatile uint32_t* reg, uint32_t mask,
                                    uint32_t value) {
    return nx_stm32_av_write(reg, (NX_AV_READ(reg) & ~mask) | (value & mask),
                             mask);
}
/** \brief DMA flags stay in their physical lane's W1C field. */
static inline uint32_t nx_stm32_av_dma_flags(const nx_stm32_av_dma_t* dma) {
    const uint8_t shifts[4] = {0U, 6U, 16U, 22U};
    return (NX_AV_READ(dma->lane < 4U ? &dma->dma->LISR : &dma->dma->HISR) >>
            shifts[dma->lane & 3U]) &
           0x3dU;
}
/** \brief Acknowledge only observed selected-stream flags. */
static inline void nx_stm32_av_dma_ack(const nx_stm32_av_dma_t* dma,
                                       uint32_t flags) {
    const uint8_t shifts[4] = {0U, 6U, 16U, 22U};
    NX_AV_WRITE(dma->lane < 4U ? &dma->dma->LIFCR : &dma->dma->HIFCR,
                (flags & 0x3dU) << shifts[dma->lane & 3U]);
    nx_arch_dsb();
}
/** \brief Mask and observe actual IRQ observers before releasing payload. */
static inline bool nx_stm32_av_irq_detach(IRQn_Type irq) {
    NX_AV_IRQ_DISABLE(irq);
    NX_AV_IRQ_CLEAR(irq);
    nx_arch_dsb();
    nx_arch_isb();
    return NX_AV_IRQ_ACTIVE(irq) == 0U && NX_AV_IRQ_ENABLED(irq) == 0U &&
           NX_AV_IRQ_PENDING(irq) == 0U;
}
/** \brief A stopped stream has no interrupt source, memory bus or IRQ owner. */
static inline bool nx_stm32_av_dma_detach(const nx_stm32_av_dma_t* dma,
                                          uint32_t limit) {
    (void)nx_stm32_av_bits(&dma->stream->CR,
                           DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE |
                               DMA_SxCR_TEIE | DMA_SxCR_DMEIE,
                           0U);
    (void)nx_stm32_av_bits(&dma->stream->FCR, DMA_SxFCR_FEIE, 0U);
    for (uint32_t i = 0U; i < limit; ++i) {
        if ((NX_AV_READ(&dma->stream->CR) &
             (DMA_SxCR_EN | DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_TEIE |
              DMA_SxCR_DMEIE)) == 0U &&
            (NX_AV_READ(&dma->stream->FCR) & DMA_SxFCR_FEIE) == 0U &&
            ((NX_AV_READ(&dma->stream->FCR) & DMA_SxFCR_DMDIS) == 0U ||
             (NX_AV_READ(&dma->stream->FCR) & DMA_SxFCR_FS) ==
                 DMA_SxFCR_FS_2)) {
            nx_arch_dsb();
            nx_stm32_av_dma_ack(dma, nx_stm32_av_dma_flags(dma));
            return nx_stm32_av_irq_detach(dma->irq);
        }
        NX_AV_POLL();
    }
    return false;
}
/** \brief Detach both target addresses only after the engine is disabled. */
static inline bool nx_stm32_av_dma_addresses(const nx_stm32_av_dma_t* dma) {
    if ((NX_AV_READ(&dma->stream->CR) & DMA_SxCR_EN) != 0U)
        return false;
    bool ok = nx_stm32_av_write(&dma->stream->M0AR, 0U, UINT32_MAX);
    ok = nx_stm32_av_write(&dma->stream->M1AR, 0U, UINT32_MAX) && ok;
    return nx_stm32_av_write(&dma->stream->PAR, 0U, UINT32_MAX) && ok;
}
/** \brief Entire intervals, including unused capacity, participate in aliasing.
 */
static inline bool nx_stm32_av_overlap(const void* a, size_t an, const void* b,
                                       size_t bn) {
    uintptr_t ap = (uintptr_t)a, bp = (uintptr_t)b;
    return a == NULL || b == NULL || an == 0U || bn == 0U ||
           an > UINTPTR_MAX - ap || bn > UINTPTR_MAX - bp ||
           (ap < bp + bn && bp < ap + an);
}
/** \brief No caller storage may overlap the shared clock/resource owner. */
static inline bool nx_stm32_av_system_disjoint(const void* data, size_t bytes) {
    return !nx_stm32_av_overlap(data, bytes, &g_nx_stm32_system,
                                sizeof(g_nx_stm32_system));
}
/** \brief Validate all producer buffers before any request is accepted. */
nx_result_t nx_stm32_av_stream_validate(const nx_stm32_av_config_t* config,
                                        nx_stream_t* stream, size_t bytes,
                                        size_t alignment, const void* state,
                                        size_t state_bytes, const void* request,
                                        size_t request_bytes);
/** \brief Apply exact AF fields only after shared pin capture and admission. */
nx_result_t nx_stm32_av_pins(const nx_stm32_av_config_t* config);

#endif /* NX_STM32F407_AV_H */
