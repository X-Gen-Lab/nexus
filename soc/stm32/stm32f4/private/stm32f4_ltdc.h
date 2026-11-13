/**
 * \file            stm32f4_ltdc.h
 * \brief           Private shared-layer LTDC HAL lifetime transport
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_LTDC_H
#define NX_STM32F4_LTDC_H
#include "nexus/arch/arch.h"
#include "nexus/io/display.h"
#include "stm32f407_system.h"
#include "stm32f4_graphics_memory.h"
#if !defined(USE_HAL_LTDC_REGISTER_CALLBACKS)
#define USE_HAL_LTDC_REGISTER_CALLBACKS 0U
#elif USE_HAL_LTDC_REGISTER_CALLBACKS != 0U
#error "Nexus graphics requires USE_HAL_LTDC_REGISTER_CALLBACKS=0"
#endif
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_ltdc.h"

#ifndef NX_GFX_READ
#define NX_GFX_READ(reg)         (*(reg))
#define NX_GFX_WRITE(reg, value) (*(reg) = (value))
#define NX_GFX_NOW()             nx_time_now_us()
#define NX_GFX_TASK()                                                          \
    (nx_arch_is_privileged() && !nx_arch_in_isr() && !nx_arch_irq_is_masked())
#define NX_GFX_IRQ_ENABLED(irq)             NVIC_GetEnableIRQ(irq)
#define NX_GFX_IRQ_PENDING(irq)             NVIC_GetPendingIRQ(irq)
#define NX_GFX_IRQ_ACTIVE(irq)              NVIC_GetActive(irq)
#define NX_GFX_IRQ_ENABLE(irq)              NVIC_EnableIRQ(irq)
#define NX_GFX_IRQ_DISABLE(irq)             NVIC_DisableIRQ(irq)
#define NX_GFX_IRQ_CLEAR(irq)               NVIC_ClearPendingIRQ(irq)
#define NX_GFX_IRQ_PRIORITY(irq)            NVIC_GetPriority(irq)
#define NX_GFX_IRQ_SET_PRIORITY(irq, value) NVIC_SetPriority((irq), (value))
#define NX_GFX_IRQ_GROUP()                  NVIC_GetPriorityGrouping()
#endif
#ifndef NX_GFX_ACTIVE_LAYER
#define NX_GFX_ACTIVE_LAYER(handle, layer) LTDC_LAYER((handle), (layer))
#endif

/** \brief Readonly selected layer identity and window/alpha facts.
 * \note Records are borrowed through controller close. Public face context
 *       is the one static controller state, so repeated stop reads no record.
 */
typedef struct {
    nx_display_constraints_t limits;
    uint16_t x;
    uint16_t y;
    uint8_t alpha;
    bool pixel_alpha;
} nx_stm32_ltdc_layer_t;

/** \brief Exact AF14 route fact; Platform owns its original pad snapshot. */
typedef struct {
    GPIO_TypeDef* registers;
    uint8_t bank;
    uint8_t pin;
} nx_stm32_ltdc_pin_t;

/** \brief Generated values join one System and one Platform pad snapshot owner.
 * \note System owns the bus and pixel domains; this controller acquires its
 *       LTDC gate and AF14 pad leases. Platform keeps GPIO gates and the
 *       original snapshots until controller close, then restores pads once.
 *       Expected clock words are equality facts, not register scripts.
 *       Board routes and selected memory domains are independently reviewed.
 */
typedef struct {
    LTDC_TypeDef* registers;
    RCC_TypeDef* rcc;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
    const nx_stm32_system_t* system;
    LTDC_InitTypeDef timing;
    const nx_stm32_ltdc_layer_t* layers;
    size_t layer_count;
    const nx_stm32_ltdc_pin_t* pins;
    const uint16_t* snapshots;
    size_t pin_count;
    nx_irq_policy_t irq_policy;
    uint32_t system_cfgr;
    uint32_t pll_source;
    uint32_t pllsai_cfgr;
    uint32_t pllsai_divr;
    nx_time_us_t drain_us;
    uint8_t priority;
    nx_stm32_sdram_graphics_t* graphics_memory;
} nx_stm32_ltdc_config_t;

/** \brief Exact active/pending storage, HAL context and retryable receipt. */
typedef struct nx_stm32_ltdc_state {
    LTDC_HandleTypeDef handle;
    const nx_stm32_ltdc_config_t* config;
    nx_display_request_t* volatile active[2];
    nx_display_request_t* volatile pending;
    const nx_irq_wake_t* wake;
    uint64_t epoch;
    nx_time_us_t drain_deadline;
    volatile uint64_t stage_generation;
    volatile uint64_t proof_generation;
    volatile uint32_t events;
    uint32_t publishers;
    uint32_t pins_owned;
    uint32_t priorities[2];
    nx_result_t first_failure;
    volatile uint8_t phase;
    volatile uint8_t retire_mask;
    volatile uint8_t proof_mask;
    bool vectors[2];
    volatile bool owned;
    volatile bool initialized;
    bool gate_owned;
    bool controller_touched;
    bool closing;
    volatile bool fault;
    bool draining;
    bool executing;
    bool memory_attached;
} nx_stm32_ltdc_state_t;

/** \brief Pure pre-System admission; no gate, pad, snapshot or state effect.
 * \note Validates static extents and rejects foreign gates/vectors while
 *       System and GPIO domains may still be stopped. No references retained.
 */
nx_result_t nx_stm32_ltdc_admit(nx_stm32_ltdc_state_t* state,
                                const nx_stm32_ltdc_config_t* config);
/** \brief Acquire cold IRQ/HAL responsibility after effect-free admission.
 * \note Before prepare, System owns the live bus/pixel domains and Platform
 *       owns powered admitted pads and their snapshots. The LTDC gate is off
 *       after trusted reset or this same owner's successful close. Foreign
 *       gates/vectors reject before effects. A partial failure retains the
 *       exact config/receipt; keep them and retry close before reclamation.
 */
nx_result_t nx_stm32_ltdc_prepare(nx_stm32_ltdc_state_t* state,
                                  const nx_stm32_ltdc_config_t* config);
/** \brief Retry controller close; successful repeated close is idempotent. */
nx_result_t nx_stm32_ltdc_close(nx_stm32_ltdc_state_t* state);
/** \brief Actual HAL IRQ body latches facts; never settles a frame in IRQ. */
void nx_stm32_ltdc_irq(nx_stm32_ltdc_state_t* state);
/** \brief One readonly method table shared by selected static instances. */
extern const nx_display_ops_t nx_stm32_ltdc_ops;
#endif /* NX_STM32F4_LTDC_H */
