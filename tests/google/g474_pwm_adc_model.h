/**
 * \file            g474_pwm_adc_model.h
 * \brief           External G474 timer, ADC and DMA event model
 * \author          Nexus Team
 */
#ifndef NX_G474_PWM_ADC_MODEL_H
#define NX_G474_PWM_ADC_MODEL_H
#include "nexus/arch/arch.h"
#include "nexus/core/status.h"
#include "stm32g474xx.h"
#ifdef __cplusplus
extern "C" {
#endif
struct nx_stm32g474_adc_state;
typedef enum {
    NX_PA_PRIME_EOS,
    NX_PA_COUNTER_STARTED,
    NX_PA_TIMER_EDGE,
    NX_PA_DMA_WRITE
} nx_pa_event_t;
typedef struct {
    RCC_TypeDef rcc;
    GPIO_TypeDef gpio;
    TIM_TypeDef timer;
    TIM_TypeDef peer_timer;
    COMP_TypeDef comparator;
    ADC_TypeDef adc[2];
    ADC_Common_TypeDef common;
    DMA_TypeDef dma;
    DMA_Channel_TypeDef channel;
    DMAMUX_Channel_TypeDef mux;
    uint64_t cycles;
    uint64_t conversion_done;
    uint64_t calibration_done;
    uint64_t enabled_at;
    uint32_t counter_divider;
    uint32_t active_psc;
    uint32_t active_arr;
    uint32_t active_ccr[4];
    uint32_t dma_initial;
    unsigned primes;
    unsigned enables;
    unsigned edges;
    unsigned ignored_edges;
    unsigned beats;
    unsigned writes;
    unsigned unsupported_reads;
    uint16_t analog_value;
    bool limited_complete;
    bool converting;
    bool software_conversion;
    bool trgo;
    bool privileged;
    bool in_isr;
    bool hold_conversion;
    bool hold_dma_beat;
    bool hold_adc_stop;
    bool hold_adc_disable;
    bool hold_dma_disable;
    bool hold_counter_stop;
    bool hold_addresses;
    bool global_clear_while_active;
    uint32_t mask;
    bool irq_enabled[102];
    bool irq_pending[102];
    bool irq_active[102];
    uint8_t priority[102];
    volatile uint32_t* refused;
    uint32_t held_bits;
    void (*event)(nx_pa_event_t event);
    void (*poll)(void);
    void (*after_read)(volatile uint32_t* address);
} nx_pa_model_t;
extern nx_pa_model_t g_pa;
void nx_pa_reset(void);
void nx_pa_advance_cycles(uint64_t cycles);
void nx_pa_break(bool asserted);
uint32_t nx_pa_read(volatile uint32_t* reg);
void nx_pa_write(volatile uint32_t* reg, uint32_t value);
void nx_pa_poll(void* unused);
nx_result_t nx_pa_irq_control(int irq, bool enable, uint8_t priority);
void nx_pa_priority_write(int irq, uint32_t priority);
#ifdef __cplusplus
}
#endif
#undef RCC
#undef GPIOA
#undef TIM1
#undef TIM15
#undef COMP1
#undef ADC1
#undef ADC2
#undef ADC12_COMMON
#undef DMA1
#undef DMA1_Channel1
#undef DMAMUX1_Channel0
#define RCC                       (&g_pa.rcc)
#define GPIOA                     (&g_pa.gpio)
#define TIM1                      (&g_pa.timer)
#define TIM15                     (&g_pa.peer_timer)
#define COMP1                     (&g_pa.comparator)
#define ADC1                      (&g_pa.adc[0])
#define ADC2                      (&g_pa.adc[1])
#define ADC12_COMMON              (&g_pa.common)
#define DMA1                      (&g_pa.dma)
#define DMA1_Channel1             (&g_pa.channel)
#define DMAMUX1_Channel0          (&g_pa.mux)
#define NX_G474_GROUP_TIM1        TIM1
#define NX_G474_GROUP_COMP1       COMP1
#define NX_G474_GROUP_RCC         RCC
#define NX_G474_READ(reg)         nx_pa_read(reg)
#define NX_G474_WRITE(reg, value) nx_pa_write((reg), (uint32_t)(value))
#define NX_G474_PWM_ADC_TRIGGER_SELECT(adc, source)                            \
    nx_pa_write(&(adc)->CFGR,                                                  \
                (nx_pa_read(&(adc)->CFGR) &                                    \
                 ~(uint32_t)(ADC_CFGR_EXTSEL | ADC_CFGR_EXTEN)) |              \
                    (uint32_t)(source))
#define NX_G474_PWM_ADC_DMAMUX_SELECT(mux) nx_pa_write(&(mux)->CCR, 5U)
#define NX_G474_ADC_POLL(port)             nx_pa_poll(port)
#define NX_G474_SYSTEM_POLL(system)        nx_pa_poll(system)
#define NX_G474_BOOST_DELAY(system)        ((void)(system))
#define NX_G474_IRQ_AVAILABLE(irq)                                             \
    (!g_pa.irq_enabled[(irq)] && !g_pa.irq_pending[(irq)] &&                   \
     !g_pa.irq_active[(irq)])
#define NX_G474_IRQ_CONTROL(irq, enable, priority)                             \
    nx_pa_irq_control((irq), (enable), (priority))
#define NX_G474_IRQ_PRIORITY(irq) g_pa.priority[(irq)]
#define NX_G474_IRQ_PRIORITY_WRITE(irq, priority)                              \
    nx_pa_priority_write((irq), (priority))
#define NX_PLATFORM_REGISTER_WRITE(reg, value) nx_pa_write(&(reg), (value))
#endif
