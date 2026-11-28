/**
 * \file            cec.c
 * \brief           STM32F100RB fixed-route, interrupt-driven CEC frames.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "instance.h"

#include "dispatch_checks.h"
#include "nexus_config.h"
#include "private/hardware.h"
#include "system/platform.h"
#include <string.h>

#if !defined(STM32F100xB) ||                                                   \
    (!defined(NEXUS_CMSIS_HOST_MODEL) && NEXUS_CLOCK_APB1_HZ != 8000000U)
#error "F100 CEC runs only with the reviewed APB1 8 MHz plan"
#endif

#ifndef NX_STM32F1_IRQ_ENABLED
#define NX_STM32F1_IRQ_ENABLED(irq)             NVIC_GetEnableIRQ(irq)
#define NX_STM32F1_IRQ_PENDING(irq)             NVIC_GetPendingIRQ(irq)
#define NX_STM32F1_IRQ_ACTIVE(irq)              NVIC_GetActive(irq)
#define NX_STM32F1_IRQ_PRIORITY(irq)            NVIC_GetPriority(irq)
#define NX_STM32F1_IRQ_SET_PRIORITY(irq, value) NVIC_SetPriority((irq), (value))
#define NX_STM32F1_IRQ_ENABLE(irq)              NVIC_EnableIRQ(irq)
#define NX_STM32F1_IRQ_DISABLE(irq)             NVIC_DisableIRQ(irq)
#define NX_STM32F1_IRQ_CLEAR(irq)               NVIC_ClearPendingIRQ(irq)
#endif

#define CEC_PAD       (UINT32_C(1) << 8U)
#define CEC_PAD_MODE  (GPIO_CRH_CNF8 | GPIO_CRH_MODE8_1)
#define CEC_DIVIDER   (50U * 8U - 1U)
#define CEC_ENABLE    (CEC_CFGR_PE | CEC_CFGR_IE)
#define CEC_RX_EVENTS ((uint32_t)(CEC_CSR_RBTF | CEC_CSR_RERR))
#define CEC_RX_FLAGS  (CEC_RX_EVENTS | CEC_CSR_RSOM | CEC_CSR_REOM)
#define CEC_TX_EVENTS ((uint32_t)(CEC_CSR_TBTRF | CEC_CSR_TERR))
#define CEC_EVENTS    (CEC_RX_FLAGS | CEC_TX_EVENTS)
#define DRAIN_LIMIT   262144U

/** \brief Preserve asynchronous rc_w0 status for the other direction.
 * \note TSOM is set only on submission. Mirroring a read TSOM can requeue
 *       a request that acquired the bus between the read and write.
 */
static void acknowledge(uint32_t flags, uint32_t tx_control) {
    NX_STM32F1_WRITE(&CEC->CSR, (CEC_EVENTS & ~flags) | tx_control);
}

static void service_receive(nx_stm32f1_cec_t* state, uint32_t status) {
    const bool byte_ready = (status & CEC_CSR_RBTF) != 0U;
    if ((status & CEC_CSR_RERR) != 0U) {
        state->receiving = false;
        if (!state->rx_ready) {
            state->rx_size = 0U;
            state->rx_result = NX_ERROR_IO;
            state->rx_ready = true;
        }
    } else if (byte_ready) {
        const uint8_t byte = (uint8_t)(CEC->RXD & CEC_RXD_RXD);
        if ((status & CEC_CSR_RSOM) != 0U) {
            state->receiving = !state->rx_ready && !state->stopping;
            if (state->receiving)
                state->rx_size = 0U;
        }
        if (state->receiving) {
            if (state->rx_size == sizeof(state->rx)) {
                state->rx_size = 0U;
                state->rx_result = NX_ERROR_OVERFLOW;
                state->rx_ready = true;
                state->receiving = false;
            } else {
                state->rx[state->rx_size++] = byte;
                if ((status & CEC_CSR_REOM) != 0U) {
                    state->rx_result = NX_SUCCESS;
                    state->rx_ready = true;
                    state->receiving = false;
                }
            }
        }
    }
    /* RM0041 requires observing RBTF before clearing REOM. */
    const uint32_t handled =
        status & (byte_ready ? CEC_RX_FLAGS : CEC_CSR_RERR | CEC_CSR_RSOM);
    acknowledge(handled, status & CEC_CSR_TEOM);
}

static void service_transmit(nx_stm32f1_cec_t* state, uint32_t status) {
    const uint32_t events = status & CEC_TX_EVENTS;
    if (events == 0U || (status & CEC_RX_EVENTS) != 0U)
        return;
    if ((events & CEC_CSR_TERR) != 0U) {
        const uint32_t error = CEC->ESR;
        acknowledge(events, 0U);
        if (state->tx_result == NX_ERROR_BUSY)
            state->tx_result =
                (error & CEC_ESR_ACKE) != 0U ? NX_ERROR_NACK : NX_ERROR_IO;
        return;
    }
    if (state->tx_result == NX_ERROR_BUSY && state->tx_next < state->tx_size &&
        (CEC->CFGR & CEC_CFGR_PE) != 0U) {
        NX_STM32F1_WRITE(&CEC->TXD, state->tx[state->tx_next++]);
        acknowledge(events,
                    state->tx_next == state->tx_size ? CEC_CSR_TEOM : 0U);
        return;
    }
    if (state->tx_result == NX_ERROR_BUSY) {
        state->tx_result =
            (status & CEC_CSR_TEOM) != 0U && state->tx_next == state->tx_size
                ? NX_SUCCESS
                : NX_ERROR_CANCELLED;
    }
    acknowledge(events, 0U);
}

void nx_stm32f1_cec_irq(nx_stm32f1_cec_t* state) {
    if (state == NULL || !state->vector_owned)
        return;
    const uint32_t status = CEC->CSR;
    if ((status & CEC_RX_EVENTS) != 0U)
        service_receive(state, status);
    service_transmit(state, CEC->CSR);
    nx_arch_dsb();
}

static nx_result_t check_route(nx_stm32f1_cec_t* state) {
    const uint32_t gate = RCC_APB2ENR_AFIOEN;
    state->afio_added = (RCC->APB2ENR & gate) == 0U;
    if (state->afio_added &&
        !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, gate))
        return NX_ERROR_IO;
    const bool remapped = (AFIO->MAPR2 & AFIO_MAPR2_CEC_REMAP) != 0U;
    if (state->afio_added && !nx_stm32f1_write_masked(&RCC->APB2ENR, gate, 0U))
        return NX_ERROR_IO;
    state->afio_added = false;
    return remapped ? NX_ERROR_BUSY : NX_SUCCESS;
}

nx_result_t nx_stm32f1_cec_prepare(nx_stm32f1_cec_t* state, uint8_t address,
                                   uint8_t priority) {
    if (state == NULL || address >= 15U || priority >= (1U << __NVIC_PRIO_BITS))
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (state->clock_owned || state->afio_added ||
        state->pad.registers != NULL || !nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    if ((RCC->APB1ENR & RCC_APB1ENR_CECEN) != 0U ||
        (RCC->APB1RSTR & RCC_APB1RSTR_CECRST) != 0U ||
        NX_STM32F1_IRQ_ENABLED(CEC_IRQn) || NX_STM32F1_IRQ_PENDING(CEC_IRQn) ||
        NX_STM32F1_IRQ_ACTIVE(CEC_IRQn))
        return NX_ERROR_BUSY;
    nx_result_t result = check_route(state);
    if (result != NX_SUCCESS)
        return result;
    state->clock_owned = true;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_CECEN,
                                 RCC_APB1ENR_CECEN))
        return NX_ERROR_IO;
    if (CEC->CFGR != 0U || CEC->CSR != 0U || CEC->ESR != 0U || CEC->OAR != 0U ||
        CEC->PRES != 0U)
        return NX_ERROR_BUSY;
    result = nx_stm32f1_pads_capture(&state->pad, GPIOB, CEC_PAD);
    if (result == NX_SUCCESS)
        result = nx_stm32f1_pads_apply(&state->pad, 0U, CEC_PAD_MODE,
                                       state->pad.latch | CEC_PAD);
    if (result != NX_SUCCESS)
        return result;
    state->address = address;
    state->tx_result = NX_ERROR_EMPTY;
    state->controls_owned = true;
    if (!nx_stm32f1_write_masked(&CEC->PRES, CEC_PRES_PRES, CEC_DIVIDER) ||
        !nx_stm32f1_write_masked(&CEC->OAR, CEC_OAR_OA, address))
        return NX_ERROR_IO;
    state->saved_priority = (uint8_t)NX_STM32F1_IRQ_PRIORITY(CEC_IRQn);
    state->vector_owned = true;
    NX_STM32F1_IRQ_SET_PRIORITY(CEC_IRQn, priority);
    if (NX_STM32F1_IRQ_PRIORITY(CEC_IRQn) != priority)
        return NX_ERROR_IO;
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    const bool enabled =
        nx_stm32f1_write_masked(&CEC->CFGR, CEC_ENABLE, CEC_ENABLE);
    if (enabled) {
        NX_STM32F1_IRQ_ENABLE(CEC_IRQn);
        nx_arch_dsb();
        state->live = NX_STM32F1_IRQ_ENABLED(CEC_IRQn);
    }
    nx_arch_irq_restore(mask);
    return state->live ? NX_SUCCESS : NX_ERROR_IO;
}

nx_result_t nx_stm32f1_cec_send(nx_stm32f1_cec_t* state, uint8_t destination,
                                const void* payload, size_t bytes) {
    if (state == NULL || destination > 15U || bytes >= sizeof(state->tx) ||
        (bytes == 0U && destination == 15U) ||
        (bytes != 0U &&
         (!nx_io_dispatch_span(payload, bytes, 1U) ||
          nx_io_dispatch_overlap(payload, bytes, state, sizeof(*state)))))
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!state->live || state->stopping)
        return NX_ERROR_STATE;
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (state->tx_result == NX_ERROR_BUSY || state->receiving ||
        (CEC->CSR &
         (CEC_CSR_TSOM | CEC_CSR_TEOM | CEC_RX_EVENTS | CEC_TX_EVENTS)) != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    if (bytes != 0U)
        memcpy(state->tx, payload, bytes);
    state->tx_size = (uint8_t)bytes;
    state->tx_next = 0U;
    state->tx_result = NX_ERROR_BUSY;
    NX_STM32F1_WRITE(&CEC->TXD, ((uint32_t)state->address << 4U) | destination);
    acknowledge(0U, CEC_CSR_TSOM | (bytes == 0U ? CEC_CSR_TEOM : 0U));
    nx_arch_dsb();
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_cec_transmit_status(const nx_stm32f1_cec_t* state) {
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    const nx_result_t result = state->tx_result;
    nx_arch_irq_restore(mask);
    return result;
}

nx_result_t nx_stm32f1_cec_receive(nx_stm32f1_cec_t* state, void* frame,
                                   size_t capacity, size_t* bytes) {
    if (state == NULL || !nx_io_dispatch_span(frame, capacity, 1U) ||
        !nx_io_dispatch_span(bytes, sizeof(*bytes), _Alignof(size_t)) ||
        nx_io_dispatch_overlap(frame, capacity, state, sizeof(*state)) ||
        nx_io_dispatch_overlap(bytes, sizeof(*bytes), state, sizeof(*state)) ||
        nx_io_dispatch_overlap(frame, capacity, bytes, sizeof(*bytes)))
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (!state->live || state->stopping)
        return NX_ERROR_STATE;
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    *bytes = 0U;
    nx_result_t result = NX_ERROR_EMPTY;
    if (state->rx_ready) {
        result = state->rx_result;
        if (capacity < state->rx_size) {
            result = NX_ERROR_OVERFLOW;
        } else {
            *bytes = state->rx_size;
            memcpy(frame, state->rx, state->rx_size);
            state->rx_ready = false;
        }
    }
    nx_arch_irq_restore(mask);
    return result;
}

/** \brief Request PE clear and leave IRQ available until the wire is idle. */
static nx_result_t stop_wire(nx_stm32f1_cec_t* state, nx_time_us_t deadline) {
    const nx_arch_irq_state_t mask = nx_arch_irq_save();
    state->stopping = true;
    NX_STM32F1_WRITE(&CEC->CFGR, CEC->CFGR & ~(uint32_t)CEC_CFGR_PE);
    nx_arch_dsb();
    nx_arch_irq_restore(mask);
    for (unsigned remaining = DRAIN_LIMIT; (CEC->CFGR & CEC_CFGR_PE) != 0U;
         --remaining) {
        if (remaining == 0U || nx_deadline_expired(deadline, nx_time_now_us()))
            return NX_ERROR_TIMEOUT;
    }
    if (!nx_stm32f1_write_masked(&CEC->CFGR, CEC_CFGR_IE, 0U))
        return NX_ERROR_IO;
    if (state->vector_owned) {
        NX_STM32F1_IRQ_DISABLE(CEC_IRQn);
        nx_arch_dsb();
        if (NX_STM32F1_IRQ_ENABLED(CEC_IRQn) || NX_STM32F1_IRQ_ACTIVE(CEC_IRQn))
            return NX_ERROR_IO;
        nx_stm32f1_cec_irq(state);
        NX_STM32F1_IRQ_CLEAR(CEC_IRQn);
        NX_STM32F1_IRQ_SET_PRIORITY(CEC_IRQn, state->saved_priority);
        if (NX_STM32F1_IRQ_PENDING(CEC_IRQn) ||
            NX_STM32F1_IRQ_PRIORITY(CEC_IRQn) != state->saved_priority)
            return NX_ERROR_IO;
        state->vector_owned = false;
    }
    if (state->tx_result == NX_ERROR_BUSY)
        state->tx_result = NX_ERROR_CANCELLED;
    state->live = false;
    const uint32_t reset = RCC_APB1RSTR_CECRST;
    if (!nx_stm32f1_write_masked(&RCC->APB1RSTR, reset, reset) ||
        !nx_stm32f1_write_masked(&RCC->APB1RSTR, reset, 0U) ||
        CEC->CFGR != 0U || CEC->CSR != 0U || CEC->OAR != 0U || CEC->PRES != 0U)
        return NX_ERROR_IO;
    state->controls_owned = false;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_cec_close(nx_stm32f1_cec_t* state,
                                 nx_time_us_t deadline) {
    if (state == NULL || deadline == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    if (!nx_stm32f1_cold_context())
        return NX_ERROR_CONTEXT;
    if (nx_deadline_expired(deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    if (state->controls_owned) {
        if ((RCC->APB1ENR & RCC_APB1ENR_CECEN) == 0U)
            return NX_ERROR_IO;
        const nx_result_t stopped = stop_wire(state, deadline);
        if (stopped != NX_SUCCESS)
            return stopped;
    }
    const nx_result_t restored = nx_stm32f1_pads_restore(&state->pad);
    if (restored != NX_SUCCESS)
        return restored;
    if ((state->clock_owned &&
         !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_CECEN, 0U)) ||
        (state->afio_added &&
         !nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)))
        return NX_ERROR_IO;
    const nx_result_t last_tx = state->tx_result;
    *state = (nx_stm32f1_cec_t){.tx_result = last_tx};
    return NX_SUCCESS;
}
