/**
 * \file            stm32g474_acquisition.h
 * \brief           Exact private ADC1/TIM6/DMA and dual-bank physical Flash
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_STM32G474_ACQUISITION_H
#define NX_STM32G474_ACQUISITION_H
#include "nexus/io/adc.h"
#include "nexus/io/dma.h"
#include "nexus/io/flash.h"
#include "stm32g474_system.h"
#ifndef NX_G474_IRQ_PRIORITY
#define NX_G474_IRQ_PRIORITY(irq) NVIC_GetPriority((IRQn_Type)(irq))
#endif
#ifndef NX_G474_IRQ_GROUP
#define NX_G474_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Selected PA0/ADC1_IN1, single-ended, code0 (2.5 cycles).
 * \note Finite normal DMA uses one producer loan, never overwrites consumers.
 */
typedef struct {
    ADC_TypeDef* registers;
    ADC_TypeDef* peer;
    ADC_Common_TypeDef* common;
    uint32_t reference_mv;
    uint32_t poll_limit;
    bool initialized;
    bool retained;
    volatile bool active;
    volatile bool fault;
} nx_stm32g474_adc_state_t;
typedef struct {
    nx_stm32g474_adc_state_t adc;
    TIM_TypeDef* timer;
    DMA_TypeDef* dma;
    DMA_Channel_TypeDef* channel;
    DMAMUX_Channel_TypeDef* mux;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    nx_stream_t* stream;
    nx_stream_fill_t fill;
    nx_irq_policy_t irq_policy;
    const nx_irq_wake_t* wake;
    uint16_t wake_publishers;
    uint32_t timer_period;
    bool running;
    bool filling;
    volatile bool completed;
    bool stopping;
} nx_stm32g474_adc_stream_state_t;
extern const nx_adc_ops_t nx_stm32g474_adc_ops;
extern const nx_adc_ops_t nx_stm32g474_adc_stream_ops;
/** \brief Calibrate the cold ADC; its bulb sampler remains disabled.
 * \param[in,out] port: Static exact channel state, Board supplies analog input.
 * \return Success or error; retained requires close, no reference is inferred.
 */
nx_result_t nx_stm32g474_adc_initialize(nx_stm32g474_adc_state_t* port);
/** \brief Disable and restore independent ADC12 common clock after drain.
 * \param[in,out] port: Exclusive task owner; failed cleanup retains storage.
 * \return Success only after observed conversion and enable withdrawal.
 */
nx_result_t nx_stm32g474_adc_close(nx_stm32g474_adc_state_t* port);
/** \brief Stop conversion and disable the bulb sampler without releasing state.
 * \param[in,out] port: Exclusive initialized owner; failed drain retains it.
 * \return Success only after ADSTART, ADSTP, ADEN and ADDIS withdraw.
 */
nx_result_t nx_stm32g474_adc_quiesce(nx_stm32g474_adc_state_t* port);
/** \brief Internal masked two-shot errata prime; only second raw result lives.
 * \param[in,out] port: Exclusive initialized ADC owner, finite deadline.
 * \param[in] deadline: Absolute deadline, at most100us from the caller.
 * \param[out] value: Second conversion only; no retained reference on success.
 * \return Success or failure; caller must stop any in-flight conversion.
 */
nx_result_t nx_stm32g474_adc_prime(nx_stm32g474_adc_state_t* port,
                                   nx_time_us_t deadline, uint16_t* value);
/** \brief Admit exact unused TIM6, DMA1 channel1, DMAMUX1 channel0/request5.
 * \param[in,out] port: Static stream state and immutable DMA domains.
 * \return Success or error; no timer, DMA or acquisition starts here.
 */
nx_result_t
nx_stm32g474_adc_stream_initialize(nx_stm32g474_adc_stream_state_t* port);
/** \brief Latch terminal DMA facts; task service detaches before publishing.
 * \param[in,out] port: Exact DMA1 channel1 vector instance.
 */
void nx_stm32g474_adc_stream_irq(nx_stm32g474_adc_stream_state_t* port);
/** \brief Join trigger/DMA/vector and ADC before returning private storage.
 * \param[in,out] port: External stream consumers have their own loan lifetime.
 * \return Success or BUSY/IO with recovery responsibility retained.
 */
nx_result_t
nx_stm32g474_adc_stream_close(nx_stm32g474_adc_stream_state_t* port);
typedef struct {
    FLASH_TypeDef* registers;
    volatile uint8_t* memory;
    uint32_t supply_mv;
    uint32_t saved_cache;
    bool active;
    bool restore_pending;
} nx_stm32g474_flash_state_t;
extern const nx_flash_geometry_t g_nx_stm32g474_flash_geometry;
extern const nx_flash_ops_t nx_stm32g474_flash_ops;
/** \brief Retry failed idle Flash relock/cache restoration before release.
 * \param[in,out] port: Static exact provider; no active pulse may be abandoned.
 * \return Success only after observed restoration; failed leases remain live.
 */
nx_result_t nx_stm32g474_flash_close(nx_stm32g474_flash_state_t* port);
#ifndef NX_G474_ADC_POLL
#define NX_G474_ADC_POLL(port) ((void)(port))
#endif
#ifndef NX_G474_FLASH_REMAP
#define NX_G474_FLASH_REMAP() NX_G474_READ(&SYSCFG->MEMRMP)
#endif
#ifndef NX_G474_FLASH_POLL
#define NX_G474_FLASH_POLL(port) ((void)(port))
#endif
#ifndef NX_G474_FLASH_WRITE_WORD
#define NX_G474_FLASH_WRITE_WORD(address, word) (*(address) = (word))
#endif
#ifdef __cplusplus
}
#endif
#endif
