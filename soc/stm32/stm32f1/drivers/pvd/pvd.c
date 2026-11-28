/**
 * \file            pvd.c
 * \brief           F103xB raw PVD polling with retained PWR ownership.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/pvd/instance.h"
#include "private/hardware.h"

#ifndef STM32F103xB
#error "STM32F1 PVD polling needs the reviewed F103xB device"
#endif

#define PVD_LINE    (UINT32_C(1) << 16U)
#define PVD_CONTROL (PWR_CR_PLS | PWR_CR_PVDE)

enum { OWN_GATE = 1U, OWN_COMPARATOR = 2U, READY = 4U };

_Static_assert(PVD_IRQn == 1 && PWR_CR_PLS_Pos == 5U,
               "Exact F103xB PVD source and threshold layout");
_Static_assert(sizeof(nx_stm32f1_pvd_t) == 3U,
               "Polling retains no event state");

/** \brief Validate the provider's byte objects without dereferencing MMIO. */
static bool storage(const void* pointer, size_t size) {
    const uintptr_t first = (uintptr_t)pointer;
    if (pointer == NULL || size > UINTPTR_MAX - first)
        return false;
    const uintptr_t end = first + size;
    return !(first < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(first < UINT32_C(0xF0000000) && end > UINT32_C(0xE0000000));
}

/** \brief Polling must not touch another EXTI or PVD interrupt owner. */
static bool source_idle(void) {
    return ((EXTI->IMR | EXTI->EMR | EXTI->RTSR | EXTI->FTSR | EXTI->PR |
             EXTI->SWIER) &
            PVD_LINE) == 0U &&
           NVIC_GetEnableIRQ(PVD_IRQn) == 0U &&
           NVIC_GetPendingIRQ(PVD_IRQn) == 0U && NVIC_GetActive(PVD_IRQn) == 0U;
}

static bool gate_on(void) {
    return (RCC->APB1ENR & RCC_APB1ENR_PWREN) != 0U;
}

static uint32_t threshold(uint8_t code) {
    return (uint32_t)code << PWR_CR_PLS_Pos;
}

nx_result_t nx_stm32f1_pvd_admit(const nx_stm32f1_pvd_t* port, uint8_t code) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port)) || code > 7U)
        return NX_ERROR_INVALID;
    if (port->effects != 0U || gate_on() ||
        (RCC->APB1RSTR & RCC_APB1RSTR_PWRRST) != 0U || !source_idle())
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

static nx_result_t release_gate(nx_stm32f1_pvd_t* port) {
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_PWREN, 0U))
        return NX_ERROR_IO;
    *port = (nx_stm32f1_pvd_t){0};
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_pvd_prepare(nx_stm32f1_pvd_t* port, uint8_t code) {
    const nx_result_t result = nx_stm32f1_pvd_admit(port, code);
    if (result != NX_SUCCESS)
        return result;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_PWREN,
                                 RCC_APB1ENR_PWREN))
        return NX_ERROR_IO;
    port->effects = OWN_GATE;

    /* Check PWR only with the gate visible. PVDO is invalid while PVDE is off.
     * WUF/SBF and wake or backup-domain controls are outside this owner. */
    const uint32_t original = PWR->CR;
    if ((original & ~(uint32_t)PWR_CR_PLS) != 0U ||
        (PWR->CSR & ~(uint32_t)PWR_CSR_PVDO) != 0U || !source_idle())
        return release_gate(port) == NX_SUCCESS ? NX_ERROR_BUSY : NX_ERROR_IO;

    port->saved_code = (uint8_t)(original >> PWR_CR_PLS_Pos);
    port->selected_code = code;
    port->effects |= OWN_COMPARATOR;
    const uint32_t selected = threshold(code);
    if (!nx_stm32f1_write_masked(&PWR->CR, PVD_CONTROL, selected) ||
        !nx_stm32f1_write_masked(&PWR->CR, PVD_CONTROL, selected | PWR_CR_PVDE))
        return NX_ERROR_IO;
    port->effects |= READY;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_pvd_close(nx_stm32f1_pvd_t* port) {
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port)))
        return NX_ERROR_INVALID;
    if (port->effects == 0U)
        return NX_SUCCESS;
    port->effects &= (uint8_t)~READY;
    if ((port->effects & OWN_COMPARATOR) != 0U) {
        if (!gate_on() || (RCC->APB1RSTR & RCC_APB1RSTR_PWRRST) != 0U)
            return NX_ERROR_STATE;
        const uint32_t original = threshold(port->saved_code);
        const uint32_t current = PWR->CR;
        if (!source_idle() || (PWR->CSR & PWR_CSR_EWUP) != 0U ||
            (current != original && (current & ~(uint32_t)PWR_CR_PVDE) !=
                                        threshold(port->selected_code)))
            return NX_ERROR_BUSY;
        if (!nx_stm32f1_write_masked(&PWR->CR, PVD_CONTROL, original))
            return NX_ERROR_IO;
        port->effects &= (uint8_t)~OWN_COMPARATOR;
    }
    return release_gate(port);
}

/** \brief Read the raw comparator output with no event or settling contract. */
static nx_result_t poll_level(const void* context, bool* high) {
    const nx_stm32f1_pvd_t* port = context;
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!storage(port, sizeof(*port)) || !storage(high, sizeof(*high)) ||
        ((uintptr_t)high >= (uintptr_t)port &&
         (uintptr_t)high - (uintptr_t)port < sizeof(*port)))
        return NX_ERROR_INVALID;
    if ((port->effects & READY) == 0U)
        return NX_ERROR_STATE;
    if (!gate_on() || PWR->CR != (threshold(port->selected_code) | PWR_CR_PVDE))
        return NX_ERROR_IO;
    *high = (PWR->CSR & PWR_CSR_PVDO) != 0U;
    return NX_SUCCESS;
}

static nx_result_t poll_stop(void* context) {
    return nx_stm32f1_pvd_close(context);
}

const nx_exti_ops_t nx_stm32f1_pvd_ops = {.stop = poll_stop,
                                          .level = poll_level};
