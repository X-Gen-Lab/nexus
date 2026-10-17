/**
 * \file            g474_adc_group_model.h
 * \brief           External ADC sequencer and finite DMA event boundaries
 * \author          Nexus Team
 */
#ifndef NX_G474_ADC_GROUP_MODEL_H
#define NX_G474_ADC_GROUP_MODEL_H
#include "nexus/arch/arch.h"
#include "nexus/core/time.h"
#include "stm32g474xx.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    NX_AG_RANK,
    NX_AG_EOS,
    NX_AG_JEOS,
    NX_AG_DMA_BEAT,
    NX_AG_TC,
    NX_AG_TIMER_EDGE
} nx_ag_event_t;
typedef struct {
    RCC_TypeDef rcc;
    GPIO_TypeDef gpio;
    ADC_TypeDef adc[2];
    ADC_Common_TypeDef common;
    DMA_TypeDef dma;
    DMA_Channel_TypeDef channel;
    DMAMUX_Channel_TypeDef mux;
    TIM_TypeDef timer;
    DBGMCU_TypeDef debug;
    uint16_t flash_kib;
    uint64_t cycles;
    uint64_t due;
    uint64_t calibration_due;
    uint64_t masked_run;
    uint64_t max_masked_run;
    uint64_t enabled_at;
    uint64_t trigger_due;
    uint64_t internal_enabled_at[3];
    uint64_t last_conversion;
    uint32_t active_psc;
    uint32_t active_arr;
    uint32_t divider;
    uint32_t dma_initial;
    uint32_t mask;
    uint16_t values[19];
    unsigned rank;
    unsigned count;
    unsigned frames;
    unsigned beats;
    unsigned trigger_edges;
    unsigned writes;
    unsigned inaccessible_reads;
    unsigned forbidden_writes;
    unsigned last_channel;
    bool injected;
    bool converting;
    bool limited;
    bool long_idle_bad;
    bool needs_reenable;
    bool duplicate_disturbance;
    bool hold_conversion;
    bool hold_trigger_sync;
    bool trigger_pending;
    bool hold_dma;
    bool hold_adc_stop;
    bool hold_adc_disable;
    bool hold_dma_disable;
    bool hold_counter_stop;
    bool hold_irq_disable;
    bool privileged;
    bool in_isr;
    bool enabled[102];
    bool pending[102];
    bool active[102];
    uint8_t priority[102];
    volatile uint32_t* refused;
    uint32_t held_bits;
    void (*event)(nx_ag_event_t event, unsigned rank);
    void (*before_read)(volatile uint32_t* reg);
    void (*before_write)(volatile uint32_t* reg, uint32_t value);
} nx_ag_model_t;
extern nx_ag_model_t g_ag;
void nx_ag_reset(uint16_t revision);
void nx_ag_advance(uint64_t cycles);
uint32_t nx_ag_read(volatile uint32_t* reg);
void nx_ag_write(volatile uint32_t* reg, uint32_t value);
void nx_ag_poll(void* unused);
nx_result_t nx_ag_irq_control(int irq, bool enable, uint8_t priority);
void nx_ag_priority_write(int irq, uint32_t priority);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef GPIOA
#undef ADC1
#undef ADC2
#undef ADC12_COMMON
#undef DMA1
#undef DMA1_Channel1
#undef DMAMUX1_Channel0
#undef TIM6
#undef DBGMCU
#define RCC                         (&g_ag.rcc)
#define GPIOA                       (&g_ag.gpio)
#define ADC1                        (&g_ag.adc[0])
#define ADC2                        (&g_ag.adc[1])
#define ADC12_COMMON                (&g_ag.common)
#define DMA1                        (&g_ag.dma)
#define DMA1_Channel1               (&g_ag.channel)
#define DMAMUX1_Channel0            (&g_ag.mux)
#define TIM6                        (&g_ag.timer)
#define DBGMCU                      (&g_ag.debug)
#define NX_G474_READ(reg)           nx_ag_read(reg)
#define NX_G474_WRITE(reg, value)   nx_ag_write((reg), (uint32_t)(value))
#define NX_G474_ADC_POLL(port)      nx_ag_poll(port)
#define NX_G474_SYSTEM_POLL(system) nx_ag_poll(system)
#define NX_G474_BOOST_DELAY(system) ((void)(system))
#define NX_G474_IRQ_AVAILABLE(irq)                                             \
    (!g_ag.enabled[(irq)] && !g_ag.pending[(irq)] && !g_ag.active[(irq)])
#define NX_G474_IRQ_CONTROL(irq, enable, priority)                             \
    nx_ag_irq_control((irq), (enable), (priority))
#define NX_G474_IRQ_PRIORITY(irq) g_ag.priority[(irq)]
#define NX_G474_IRQ_PRIORITY_WRITE(irq, priority)                              \
    nx_ag_priority_write((irq), (priority))
#define NX_PLATFORM_REGISTER_WRITE(reg, value) nx_ag_write(&(reg), (value))
#endif
