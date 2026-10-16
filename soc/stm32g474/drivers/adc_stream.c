/**
 * \file            adc_stream.c
 * \brief           TIM6 triggered normal DMA ADC blocks with explicit gaps
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "stm32g474_acquisition.h"
#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx_ll_dmamux.h"
#ifndef NX_G474_DMAMUX_SELECT
#define NX_G474_DMAMUX_SELECT(mux)                                             \
    LL_DMAMUX_SetRequestID((mux), LL_DMAMUX_CHANNEL_0, LL_DMAMUX_REQ_ADC1)
#endif
static bool task(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
static bool valid(const nx_stm32g474_adc_stream_state_t* p) {
    return p != NULL && p->timer != NULL && p->dma != NULL &&
           p->channel != NULL && p->mux != NULL && p->regions != NULL &&
           p->region_count != 0U;
}
static bool overlap(const void* a, size_t as, const void* b, size_t bs) {
    return (uintptr_t)a < (uintptr_t)b + bs && (uintptr_t)b < (uintptr_t)a + as;
}
/** \brief Disable trigger first; a final in-flight sample remains borrowed. */
static void withdraw(nx_stm32g474_adc_stream_state_t* p) {
    NX_G474_WRITE(&p->timer->CR1, 0U);
    NX_G474_WRITE(&p->timer->CR2, 0U);
    NX_G474_WRITE(&p->channel->CCR,
                  NX_G474_READ(&p->channel->CCR) & ~(uint32_t)DMA_CCR_EN);
    if ((NX_G474_READ(&p->adc.registers->CR) & ADC_CR_ADSTART) != 0U)
        NX_G474_WRITE(&p->adc.registers->CR,
                      NX_G474_READ(&p->adc.registers->CR) | ADC_CR_ADSTP);
    nx_arch_dsb();
}
static bool quiescent(nx_stm32g474_adc_stream_state_t* p) {
    if ((NX_G474_READ(&p->timer->CR1) & TIM_CR1_CEN) != 0U ||
        (NX_G474_READ(&p->timer->CR2) & TIM_CR2_MMS) != 0U ||
        (NX_G474_READ(&p->channel->CCR) & DMA_CCR_EN) != 0U)
        return false;
    /* Bulb sampling starts again after EOC even with no following trigger.
     * Disable the ADC before releasing a block or leaving backpressure idle.
     */
    return nx_stm32g474_adc_quiesce(&p->adc) == NX_SUCCESS;
}
nx_result_t
nx_stm32g474_adc_stream_initialize(nx_stm32g474_adc_stream_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->running || p->stream != NULL || p->filling || p->wake != NULL ||
        p->wake_publishers != 0U || NX_G474_READ(&p->timer->CR1) != 0U ||
        NX_G474_READ(&p->timer->CR2) != 0U ||
        NX_G474_READ(&p->timer->DIER) != 0U ||
        NX_G474_READ(&p->timer->SMCR) != 0U ||
        NX_G474_READ(&p->channel->CCR) != 0U ||
        NX_G474_READ(&p->mux->CCR) != 0U ||
        (NX_G474_READ(&p->dma->ISR) & 15U) != 0U ||
        !NX_G474_IRQ_AVAILABLE(DMA1_Channel1_IRQn))
        return NX_ERROR_BUSY;
    nx_irq_source_t source = {DMA1_Channel1_IRQn, 5U, 0U};
    nx_result_t result = nx_irq_wake_validate(NULL, &p->irq_policy, &source);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32g474_adc_initialize(&p->adc);
    if (result != NX_SUCCESS)
        return result;
    NX_G474_DMAMUX_SELECT(p->mux);
    nx_arch_dsb();
    if (NX_G474_READ(&p->mux->CCR) != 5U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
static nx_result_t validate_stream(nx_stm32g474_adc_stream_state_t* p,
                                   nx_stream_t* stream) {
    if (stream == NULL || stream->slots == NULL || stream->count == 0U ||
        stream->count > SIZE_MAX / sizeof(*stream->slots) || stream->stopping ||
        stream->outstanding != 0U)
        return NX_ERROR_INVALID;
    for (size_t i = 0U; i < stream->count; ++i) {
        const nx_stream_slot_t* slot = &stream->slots[i];
        if (slot->capacity == 0U || (slot->capacity & 1U) != 0U ||
            slot->capacity > 131070U)
            return NX_ERROR_INVALID;
        nx_result_t result =
            nx_dma_buffer_validate(p->regions, p->region_count, slot->data,
                                   slot->capacity, 2U, NX_DMA_FROM_DEVICE);
        if (result != NX_SUCCESS)
            return result;
        if (overlap(slot->data, slot->capacity, p, sizeof(*p)) ||
            overlap(slot->data, slot->capacity, stream, sizeof(*stream)) ||
            overlap(slot->data, slot->capacity, stream->slots,
                    stream->count * sizeof(*stream->slots)) ||
            overlap(slot->data, slot->capacity, p->adc.registers,
                    sizeof(*p->adc.registers)) ||
            overlap(slot->data, slot->capacity, p->timer, sizeof(*p->timer)) ||
            overlap(slot->data, slot->capacity, p->channel,
                    sizeof(*p->channel)))
            return NX_ERROR_INVALID;
    }
    return NX_SUCCESS;
}
static nx_result_t arm(nx_stm32g474_adc_stream_state_t* p) {
    if (!nx_stream_can_reserve(p->stream))
        return NX_ERROR_BUSY;
    nx_stream_fill_t fill;
    nx_result_t result = nx_stream_reserve(p->stream, &fill);
    if (result != NX_SUCCESS)
        return result;
    p->fill = fill;
    p->filling = true;
    p->completed = false;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    uint16_t discarded;
    nx_time_us_t arming_deadline = nx_deadline_after(nx_time_now_us(), 100U);
    result = nx_stm32g474_adc_prime(&p->adc, arming_deadline, &discarded);
    if (result != NX_SUCCESS) {
        p->adc.fault = true;
        p->stopping = true;
        withdraw(p);
        nx_arch_irq_restore(saved);
        return result;
    }
    NX_G474_WRITE(&p->timer->CR2, TIM_CR2_MMS_0);
    NX_G474_WRITE(&p->timer->PSC, 169U);
    NX_G474_WRITE(&p->timer->ARR, p->timer_period - 1U);
    NX_G474_WRITE(&p->timer->CNT, 0U);
    /* MMS=enable follows disabled CEN while UG reloads PSC/ARR. */
    NX_G474_WRITE(&p->timer->EGR, TIM_EGR_UG);
    NX_G474_WRITE(&p->timer->SR, 0U);
    /* UG resets CNT. Load it afterwards: first TRGO is ten1MHz ticks away. */
    NX_G474_WRITE(&p->timer->CNT, p->timer_period - 10U);
    NX_G474_WRITE(&p->dma->IFCR, 14U);
    NX_G474_WRITE(&p->channel->CPAR,
                  (uint32_t)(uintptr_t)&p->adc.registers->DR);
    NX_G474_WRITE(&p->channel->CMAR, (uint32_t)(uintptr_t)fill.data);
    NX_G474_WRITE(&p->channel->CNDTR, (uint32_t)(fill.capacity / 2U));
    NX_G474_WRITE(&p->channel->CCR, DMA_CCR_MINC | DMA_CCR_PSIZE_0 |
                                        DMA_CCR_MSIZE_0 | DMA_CCR_TCIE |
                                        DMA_CCR_TEIE | DMA_CCR_EN);
    NX_G474_WRITE(&p->adc.registers->ISR,
                  ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR);
    nx_arch_dsb();
    if ((NX_G474_READ(&p->adc.registers->ISR) &
         (ADC_ISR_EOC | ADC_ISR_EOS | ADC_ISR_OVR)) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_IO;
    }
    NX_G474_WRITE(&p->adc.registers->CFGR, ADC_CFGR_DMAEN | ADC_CFGR_DMACFG);
    LL_ADC_REG_SetTriggerSource(p->adc.registers,
                                LL_ADC_REG_TRIG_EXT_TIM6_TRGO);
    result = NX_G474_IRQ_CONTROL(DMA1_Channel1_IRQn, true, 5U);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return result;
    }
    uint32_t dma_config = DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 |
                          DMA_CCR_TCIE | DMA_CCR_TEIE | DMA_CCR_EN;
    uint32_t adc_config =
        ADC_CFGR_DMAEN | ADC_CFGR_DMACFG | LL_ADC_REG_TRIG_EXT_TIM6_TRGO;
    /* Count and address readbacks precede CEN: a legitimate first trigger must
     * not turn a decreasing DMA counter into a false admission failure.
     */
    bool prepared =
        NX_G474_READ(&p->timer->PSC) == 169U &&
        NX_G474_READ(&p->timer->ARR) == p->timer_period - 1U &&
        NX_G474_READ(&p->timer->CNT) == p->timer_period - 10U &&
        NX_G474_READ(&p->channel->CCR) == dma_config &&
        NX_G474_READ(&p->channel->CNDTR) == fill.capacity / 2U &&
        NX_G474_READ(&p->channel->CMAR) == (uint32_t)(uintptr_t)fill.data &&
        NX_G474_READ(&p->channel->CPAR) ==
            (uint32_t)(uintptr_t)&p->adc.registers->DR &&
        NX_G474_READ(&p->adc.registers->CFGR) == adc_config &&
        NX_G474_READ(&p->mux->CCR) == 5U;
    if (!prepared || nx_deadline_expired(arming_deadline, nx_time_now_us())) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_IO;
    }
    NX_G474_WRITE(&p->adc.registers->CR,
                  NX_G474_READ(&p->adc.registers->CR) | ADC_CR_ADSTART);
    NX_G474_WRITE(&p->timer->CR2, TIM_CR2_MMS_1);
    nx_arch_dsb();
    NX_G474_WRITE(&p->timer->CR1, TIM_CR1_CEN);
    nx_arch_dsb();
    bool started =
        (NX_G474_READ(&p->timer->CR1) & TIM_CR1_CEN) != 0U &&
        (NX_G474_READ(&p->timer->CR2) & TIM_CR2_MMS) == TIM_CR2_MMS_1 &&
        (NX_G474_READ(&p->adc.registers->CR) & ADC_CR_ADSTART) != 0U &&
        !nx_deadline_expired(arming_deadline, nx_time_now_us());
    nx_arch_irq_restore(saved);
    if (!started)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}
static nx_result_t start(void* context, nx_stream_t* stream,
                         uint32_t trigger_hz) {
    nx_stm32g474_adc_stream_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->adc.initialized || p->adc.fault)
        return NX_ERROR_STATE;
    if (p->running || p->adc.active || p->stream != NULL)
        return NX_ERROR_BUSY;
    /* SYSCLK/128 gives1.328125MHz. At20..400us trigger periods, bulb
     * apertures after12.5-cycle conversion are14.0625..518.75ADCcycles,
     * within the datasheet2.5..640.5 bounds. No rounded rate is accepted.
     */
    if (trigger_hz < 2500U || trigger_hz > 50000U ||
        1000000U % trigger_hz != 0U || 1000000U / trigger_hz > 65536U)
        return NX_ERROR_UNSUPPORTED;
    nx_result_t result = validate_stream(p, stream);
    if (result != NX_SUCCESS)
        return result;
    p->stream = stream;
    p->timer_period = 1000000U / trigger_hz;
    p->running = true;
    p->stopping = false;
    p->adc.active = true;
    result = arm(p);
    if (result != NX_SUCCESS) {
        /* Start has acquired a producer loan. Never pretend rejection released
         * it if hardware withdrawal failed: retain state for explicit stop.
         */
        p->stopping = true;
        p->adc.fault = true;
        withdraw(p);
        if (!quiescent(p) ||
            NX_G474_IRQ_CONTROL(DMA1_Channel1_IRQn, false, 5U) != NX_SUCCESS) {
            (void)nx_arch_irq_save();
            nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__,
                             __LINE__);
            for (;;) {
            }
        }
        if (p->filling) {
            (void)nx_stream_abort(p->stream, &p->fill, true);
            p->filling = false;
        }
        p->stream = NULL;
        p->running = false;
        p->adc.active = false;
    }
    return result;
}
void nx_stm32g474_adc_stream_irq(nx_stm32g474_adc_stream_state_t* p) {
    if (!valid(p) || !p->running || !p->filling)
        return;
    uint32_t flags = NX_G474_READ(&p->dma->ISR) & 15U;
    if ((flags & (DMA_ISR_TCIF1 | DMA_ISR_TEIF1)) == 0U)
        return;
    p->adc.fault = p->adc.fault || (flags & DMA_ISR_TEIF1) != 0U ||
                   NX_G474_READ(&p->channel->CNDTR) != 0U ||
                   (NX_G474_READ(&p->adc.registers->ISR) & ADC_ISR_OVR) != 0U;
    p->completed = true;
    withdraw(p);
    NX_G474_WRITE(&p->dma->IFCR, 14U);
    const nx_irq_wake_t* wake;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    wake = p->wake;
    if (wake != NULL)
        ++p->wake_publishers;
    nx_arch_irq_restore(saved);
    (void)nx_irq_wake_signal(wake);
    if (wake != NULL) {
        saved = nx_arch_irq_save();
        --p->wake_publishers;
        nx_arch_irq_restore(saved);
    }
}
static nx_result_t service(void* context) {
    nx_stm32g474_adc_stream_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->running || p->stream == NULL || p->stopping)
        return NX_ERROR_STATE;
    if ((NX_G474_READ(&p->adc.registers->ISR) & ADC_ISR_OVR) != 0U) {
        p->adc.fault = true;
        p->completed = true;
        withdraw(p);
    }
    if (p->filling && !p->completed)
        return NX_ERROR_BUSY;
    if (p->filling) {
        if (!quiescent(p)) {
            p->adc.fault = true;
            return NX_ERROR_BUSY;
        }
        nx_result_t result;
        if (p->adc.fault) {
            nx_stream_note_loss(p->stream, 1U);
            result = nx_stream_abort(p->stream, &p->fill, true);
        } else {
            result = nx_stream_publish(p->stream, &p->fill, p->fill.capacity,
                                       NX_STREAM_BOUNDARY_TRIGGER);
        }
        if (result != NX_SUCCESS)
            return result;
        p->filling = false;
        p->completed = false;
        /* Explicit second service rearms: a completed block has a trigger gap.
         * Publication itself never races the consumer with a new DMA target.
         */
        return p->adc.fault ? NX_ERROR_IO : NX_SUCCESS;
    }
    if (p->adc.fault)
        return NX_ERROR_IO;
    nx_result_t result = arm(p);
    if (result != NX_SUCCESS && result != NX_ERROR_BUSY) {
        p->adc.fault = true;
        p->stopping = true;
        withdraw(p);
    }
    return result;
}
static nx_result_t stop(void* context) {
    nx_stm32g474_adc_stream_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (p->stream == NULL)
        return NX_SUCCESS;
    p->stopping = true;
    withdraw(p);
    if (!quiescent(p) ||
        NX_G474_IRQ_CONTROL(DMA1_Channel1_IRQn, false, 5U) != NX_SUCCESS ||
        p->wake_publishers != 0U) {
        p->adc.fault = true;
        return NX_ERROR_BUSY;
    }
    NX_G474_WRITE(&p->dma->IFCR, 14U);
    if (p->filling) {
        nx_result_t result = nx_stream_abort(p->stream, &p->fill, true);
        if (result != NX_SUCCESS)
            return result;
        p->filling = false;
    }
    nx_result_t result = nx_stream_stop(p->stream);
    if (result == NX_SUCCESS) {
        p->stream = NULL;
        p->running = false;
        p->adc.active = false;
        p->wake = NULL;
    }
    return result;
}
nx_result_t nx_stm32g474_adc_stream_close(nx_stm32g474_adc_stream_state_t* p) {
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p))
        return NX_ERROR_INVALID;
    if (!p->adc.retained)
        return NX_SUCCESS;
    nx_result_t result = stop(p);
    if (result != NX_SUCCESS)
        return result;
    if (NX_G474_IRQ_CONTROL(DMA1_Channel1_IRQn, false, 5U) != NX_SUCCESS)
        return NX_ERROR_IO;
    if (nx_stm32g474_adc_quiesce(&p->adc) != NX_SUCCESS)
        return NX_ERROR_BUSY;
    NX_G474_WRITE(&p->channel->CCR, 0U);
    NX_G474_WRITE(&p->channel->CNDTR, 0U);
    NX_G474_WRITE(&p->channel->CPAR, 0U);
    NX_G474_WRITE(&p->channel->CMAR, 0U);
    NX_G474_WRITE(&p->dma->IFCR, 14U);
    NX_G474_WRITE(&p->timer->CR1, 0U);
    NX_G474_WRITE(&p->timer->CR2, TIM_CR2_MMS_0);
    NX_G474_WRITE(&p->timer->DIER, 0U);
    NX_G474_WRITE(&p->timer->PSC, 0U);
    NX_G474_WRITE(&p->timer->ARR, 0U);
    NX_G474_WRITE(&p->timer->EGR, TIM_EGR_UG);
    NX_G474_WRITE(&p->timer->CNT, 0U);
    NX_G474_WRITE(&p->timer->SR, 0U);
    NX_G474_WRITE(&p->timer->CR2, 0U);
    NX_G474_WRITE(&p->mux->CCR, 0U);
    nx_arch_dsb();
    if (NX_G474_READ(&p->channel->CCR) != 0U ||
        NX_G474_READ(&p->channel->CNDTR) != 0U ||
        NX_G474_READ(&p->channel->CPAR) != 0U ||
        NX_G474_READ(&p->channel->CMAR) != 0U ||
        (NX_G474_READ(&p->dma->ISR) & 15U) != 0U ||
        NX_G474_READ(&p->timer->CR1) != 0U ||
        NX_G474_READ(&p->timer->CR2) != 0U ||
        NX_G474_READ(&p->timer->DIER) != 0U ||
        NX_G474_READ(&p->timer->PSC) != 0U ||
        NX_G474_READ(&p->timer->ARR) != 0U ||
        NX_G474_READ(&p->timer->CNT) != 0U ||
        NX_G474_READ(&p->timer->SR) != 0U || NX_G474_READ(&p->mux->CCR) != 0U)
        return NX_ERROR_IO;
    return nx_stm32g474_adc_close(&p->adc);
}
static nx_result_t info(const void* context, nx_adc_info_t* output) {
    const nx_stm32g474_adc_stream_state_t* p = context;
    return p != NULL ? nx_stm32g474_adc_ops.info(&p->adc, output)
                     : NX_ERROR_INVALID;
}
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t ceiling) {
    nx_stm32g474_adc_stream_state_t* p = context;
    if (!task())
        return NX_ERROR_CONTEXT;
    if (!valid(p) || !p->adc.initialized)
        return NX_ERROR_STATE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = NX_SUCCESS;
    if (p->wake_publishers != 0U || p->stopping)
        result = NX_ERROR_BUSY;
    else {
        nx_irq_policy_t policy = p->irq_policy;
        if (wake != NULL && wake->calls_kernel) {
            if (policy.kernel == NX_IRQ_KERNEL_BASEPRI &&
                ceiling > policy.syscall_ceiling)
                policy.syscall_ceiling = ceiling;
            else if (policy.kernel == NX_IRQ_KERNEL_PRIMASK && ceiling != 0U)
                result = NX_ERROR_PERMISSION;
        }
        nx_irq_source_t source = {
            DMA1_Channel1_IRQn,
            (uint8_t)NX_G474_IRQ_PRIORITY(DMA1_Channel1_IRQn),
            (uint8_t)NX_G474_IRQ_GROUP()};
        if (result == NX_SUCCESS)
            result = nx_irq_wake_validate(wake, &policy, &source);
        if (result == NX_SUCCESS)
            p->wake = wake;
    }
    nx_arch_irq_restore(saved);
    return result;
}
const nx_adc_ops_t nx_stm32g474_adc_stream_ops = {.info = info,
                                                  .stream_start = start,
                                                  .stream_stop = stop,
                                                  .stream_service = service,
                                                  .attach_wake = attach_wake};
