/**
 * \file            pvd.c
 * \brief           PVD comparator and EXTI16 lifecycle without voltage policy
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_pvd.h"
#include "stm32f4xx_ll_pwr.h"

#define OWN_COMPARATOR 1U
#define OWN_SOURCE     2U
#define READY          4U
#define LINE           (UINT32_C(1) << 16U)

#ifndef NX_STM32_PVD_WRITE
#define NX_STM32_PVD_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_STM32_PVD_SET_LEVEL
#define NX_STM32_PVD_SET_LEVEL(value) LL_PWR_SetPVDLevel(value)
#define NX_STM32_PVD_ENABLE()         LL_PWR_EnablePVD()
#define NX_STM32_PVD_DISABLE()        LL_PWR_DisablePVD()
#endif
#ifndef NX_STM32_CLEAR_FLAGS
#define NX_STM32_CLEAR_FLAGS(reg, mask) ((reg) = (mask))
#endif

_Static_assert(PVD_IRQn == 1, "Exact F407 internal comparator publisher");
_Static_assert(sizeof(nx_stm32_pvd_state_t) == 3U,
               "Polling retains comparator ownership, not an event ring");

/** \brief Cold controls use the serialized privileged Thread owner. */
static bool cold_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Caller storage cannot alias MMIO or wrap its address range. */
static bool storage(const void* pointer, size_t bytes, size_t alignment) {
    const uintptr_t begin = (uintptr_t)pointer;
    if (pointer == NULL || begin % alignment != 0U ||
        bytes > UINTPTR_MAX - begin)
        return false;
    const uintptr_t end = begin + bytes;
    return !(begin < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(begin < UINT32_C(0xF0000000) && end > UINT32_C(0xE0000000));
}

/** \brief A result byte cannot overwrite retained provider or ring storage. */
static bool aliases(const bool* output, const void* object, size_t bytes) {
    return (uintptr_t)output >= (uintptr_t)object &&
           (uintptr_t)output - (uintptr_t)object < bytes;
}

/** \brief Validated extents must not overwrite their own ownership/config. */
static bool overlaps(const void* left, size_t left_bytes, const void* right,
                     size_t right_bytes) {
    return (uintptr_t)left < (uintptr_t)right + right_bytes &&
           (uintptr_t)right < (uintptr_t)left + left_bytes;
}

static bool vector_idle(void) {
    return NVIC_GetEnableIRQ(PVD_IRQn) == 0U &&
           NVIC_GetPendingIRQ(PVD_IRQn) == 0U && NVIC_GetActive(PVD_IRQn) == 0U;
}

/** \brief No polling owner adopts another interrupt or event source. */
static bool source_idle(void) {
    return ((EXTI->IMR | EXTI->EMR | EXTI->RTSR | EXTI->FTSR | EXTI->PR |
             EXTI->SWIER) &
            LINE) == 0U &&
           vector_idle();
}

nx_result_t nx_stm32_pvd_admit(const nx_stm32_pvd_state_t* port,
                               uint8_t threshold_code) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port), _Alignof(nx_stm32_pvd_state_t)) ||
        threshold_code > 7U)
        return NX_ERROR_INVALID;
    if (port->effects != 0U || port->saved_level != 0U ||
        port->selected_level != 0U || !source_idle() ||
        ((RCC->APB1ENR & RCC_APB1ENR_PWREN) != 0U && LL_PWR_IsEnabledPVD()))
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_pvd_prepare(nx_stm32_pvd_state_t* port,
                                 uint8_t threshold_code) {
    nx_result_t result = nx_stm32_pvd_admit(port, threshold_code);
    if (result != NX_SUCCESS)
        return result;
    if ((RCC->APB1ENR & RCC_APB1ENR_PWREN) == 0U)
        return NX_ERROR_STATE;
    port->saved_level = (uint8_t)(LL_PWR_GetPVDLevel() >> PWR_CR_PLS_Pos);
    port->selected_level = threshold_code;
    port->effects = OWN_COMPARATOR;
    const uint32_t selected = (uint32_t)threshold_code << PWR_CR_PLS_Pos;
    NX_STM32_PVD_SET_LEVEL(selected);
    nx_arch_dsb();
    if (LL_PWR_GetPVDLevel() != selected)
        return NX_ERROR_IO;
    NX_STM32_PVD_ENABLE();
    nx_arch_dsb();
    if (!LL_PWR_IsEnabledPVD())
        return NX_ERROR_IO;
    port->effects |= READY;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_pvd_stop(nx_stm32_pvd_state_t* port) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port), _Alignof(nx_stm32_pvd_state_t)))
        return NX_ERROR_INVALID;
    port->effects &= (uint8_t)~READY;
    if ((port->effects & OWN_SOURCE) != 0U)
        return NX_ERROR_BUSY;
    if ((port->effects & OWN_COMPARATOR) == 0U)
        return NX_SUCCESS;
    if ((RCC->APB1ENR & RCC_APB1ENR_PWREN) == 0U)
        return NX_ERROR_STATE;
    if (!source_idle())
        return NX_ERROR_BUSY;
    NX_STM32_PVD_DISABLE();
    nx_arch_dsb();
    if (LL_PWR_IsEnabledPVD())
        return NX_ERROR_IO;
    const uint32_t original = (uint32_t)port->saved_level << PWR_CR_PLS_Pos;
    NX_STM32_PVD_SET_LEVEL(original);
    nx_arch_dsb();
    if (LL_PWR_GetPVDLevel() != original)
        return NX_ERROR_IO;
    *port = (nx_stm32_pvd_state_t){0};
    return NX_SUCCESS;
}

/** \brief Instantaneous raw PVDO, without an accuracy or settling promise. */
static nx_result_t read_level(const nx_stm32_pvd_state_t* port, bool* high) {
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port), _Alignof(nx_stm32_pvd_state_t)) ||
        !storage(high, sizeof(*high), _Alignof(bool)) ||
        aliases(high, port, sizeof(*port)))
        return NX_ERROR_INVALID;
    if ((port->effects & READY) == 0U)
        return NX_ERROR_STATE;
    if ((RCC->APB1ENR & RCC_APB1ENR_PWREN) == 0U || !LL_PWR_IsEnabledPVD() ||
        LL_PWR_GetPVDLevel() !=
            ((uint32_t)port->selected_level << PWR_CR_PLS_Pos))
        return NX_ERROR_IO;
    *high = LL_PWR_IsActiveFlag_PVDO() != 0U;
    return NX_SUCCESS;
}

static nx_result_t poll_level(const void* context, bool* high) {
    return read_level(context, high);
}
static nx_result_t poll_stop(void* context) {
    return nx_stm32_pvd_stop(context);
}

nx_result_t
nx_stm32_pvd_events_admit(const nx_stm32_pvd_events_state_t* port,
                          const nx_stm32_pvd_events_config_t* config) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port), _Alignof(nx_stm32_pvd_events_state_t)) ||
        !storage(config, sizeof(*config),
                 _Alignof(nx_stm32_pvd_events_config_t)) ||
        config->capacity == 0U || config->capacity > 4096U ||
        !storage(config->storage, config->capacity * sizeof(*config->storage),
                 _Alignof(nx_exti_event_t)) ||
        config->edge < NX_EXTI_RISING || config->edge > NX_EXTI_BOTH ||
        config->priority > 15U)
        return NX_ERROR_INVALID;
    const size_t bytes = config->capacity * sizeof(*config->storage);
    if (overlaps(port, sizeof(*port), config->storage, bytes) ||
        overlaps(config, sizeof(*config), config->storage, bytes) ||
        overlaps(port, sizeof(*port), config, sizeof(*config)))
        return NX_ERROR_INVALID;
    if (port->events.initialized || port->events.wake != NULL ||
        port->events.storage != NULL)
        return NX_ERROR_BUSY;
    return nx_stm32_pvd_admit(&port->pvd, config->threshold_code);
}

nx_result_t
nx_stm32_pvd_events_prepare(nx_stm32_pvd_events_state_t* port,
                            const nx_stm32_pvd_events_config_t* config) {
    nx_result_t result = nx_stm32_pvd_events_admit(port, config);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32_pvd_prepare(&port->pvd, config->threshold_code);
    if (result != NX_SUCCESS)
        return result;
    port->pvd.effects &= (uint8_t)~READY;
    port->saved_priority = (uint8_t)NVIC_GetPriority(PVD_IRQn);
    port->events = (nx_stm32_exti_state_t){.registers = EXTI,
                                           .storage = config->storage,
                                           .capacity = config->capacity,
                                           .line = 16U,
                                           .edge = config->edge,
                                           .initialized = true};
    port->pvd.effects |= OWN_SOURCE;
    const uint32_t rising = (config->edge & NX_EXTI_RISING) != 0 ? LINE : 0U;
    const uint32_t falling = (config->edge & NX_EXTI_FALLING) != 0 ? LINE : 0U;
    NX_STM32_PVD_WRITE(&EXTI->RTSR, (EXTI->RTSR & ~LINE) | rising);
    NX_STM32_PVD_WRITE(&EXTI->FTSR, (EXTI->FTSR & ~LINE) | falling);
    NX_STM32_CLEAR_FLAGS(EXTI->PR, LINE);
    NVIC_ClearPendingIRQ(PVD_IRQn);
    nx_arch_dsb();
    if ((EXTI->RTSR & LINE) != rising || (EXTI->FTSR & LINE) != falling ||
        (EXTI->PR & LINE) != 0U || !vector_idle())
        return NX_ERROR_IO;
    NVIC_SetPriority(PVD_IRQn, config->priority);
    NX_STM32_PVD_WRITE(&EXTI->IMR, EXTI->IMR | LINE);
    NVIC_EnableIRQ(PVD_IRQn);
    nx_arch_dsb();
    nx_arch_isb();
    if (NVIC_GetPriority(PVD_IRQn) != config->priority ||
        NVIC_GetEnableIRQ(PVD_IRQn) == 0U || (EXTI->IMR & LINE) == 0U)
        return NX_ERROR_IO;
    port->pvd.effects |= READY;
    return NX_SUCCESS;
}

nx_result_t nx_stm32_pvd_events_stop(nx_stm32_pvd_events_state_t* port) {
    if (!cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port), _Alignof(nx_stm32_pvd_events_state_t)))
        return NX_ERROR_INVALID;
    port->pvd.effects &= (uint8_t)~READY;
    if ((port->pvd.effects & OWN_SOURCE) != 0U) {
        nx_result_t result = nx_stm32_exti_stop(&port->events);
        if (result != NX_SUCCESS)
            return result;
        NVIC_SetPriority(PVD_IRQn, port->saved_priority);
        nx_arch_dsb();
        if (NVIC_GetPriority(PVD_IRQn) != port->saved_priority)
            return NX_ERROR_IO;
        port->pvd.effects &= (uint8_t)~OWN_SOURCE;
    }
    nx_result_t result = nx_stm32_pvd_stop(&port->pvd);
    if (result == NX_SUCCESS)
        *port = (nx_stm32_pvd_events_state_t){0};
    return result;
}

static nx_result_t events_level(const void* context, bool* high) {
    const nx_stm32_pvd_events_state_t* port = context;
    if (!storage(port, sizeof(*port), _Alignof(nx_stm32_pvd_events_state_t)) ||
        aliases(high, port, sizeof(*port)) ||
        (port->events.storage != NULL &&
         aliases(high, port->events.storage,
                 port->events.capacity * sizeof(*port->events.storage))) ||
        (port->events.wake != NULL &&
         aliases(high, port->events.wake, sizeof(*port->events.wake))))
        return NX_ERROR_INVALID;
    return read_level(&port->pvd, high);
}
static nx_result_t events_stop(void* context) {
    return nx_stm32_pvd_events_stop(context);
}

const nx_exti_ops_t nx_stm32_pvd_ops = {.stop = poll_stop, .level = poll_level};
const nx_exti_ops_t nx_stm32_pvd_events_ops = {
    .read = nx_stm32_exti_read,
    .stop = events_stop,
    .attach_wake = nx_stm32_exti_attach_wake,
    .level = events_level,
};
