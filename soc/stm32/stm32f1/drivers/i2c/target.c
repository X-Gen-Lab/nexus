/**
 * \file            target.c
 * \brief           F103 I2C1 finite target with IRQ settlement.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "drivers/i2c/target.h"
#include "dispatch_checks.h"
#include "private/hardware.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "I2C1 target operation is reviewed only for STM32F103xB"
#endif

#define I2C_FREQ_MHZ 8U
#define I2C_BAUD     80000U
#define LINES        (GPIO_IDR_IDR6 | GPIO_IDR_IDR7)
#define TX_IRQS      (I2C_CR2_ITEVTEN | I2C_CR2_ITERREN)
#define PUBLISHERS   (TX_IRQS | I2C_CR2_ITBUFEN)
#if defined(NX_STM32F1_I2C_TARGET_TX)
#define TARGET_DIRECTION NX_I2C_TARGET_TRANSMIT
#define ARMED_IRQS       TX_IRQS
#define AF_ERROR         0U
#elif defined(NX_STM32F1_I2C_TARGET_WRITE_READ)
#define TARGET_DIRECTION NX_I2C_TARGET_WRITE_READ
#define ARMED_IRQS       PUBLISHERS
#define AF_ERROR         0U
#else
#define TARGET_DIRECTION NX_I2C_TARGET_RECEIVE
#define ARMED_IRQS       PUBLISHERS
#define AF_ERROR         I2C_SR1_AF
#endif
#define I2C_ERRORS                                                             \
    (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR | AF_ERROR | I2C_SR1_TIMEOUT)

_Static_assert(I2C1_EV_IRQn == 31 && I2C1_ER_IRQn == 32,
               "Exact STM32F103 SVT vector identities");

static bool gate_live(void) {
    return (READ_REG(RCC->APB1ENR) & RCC_APB1ENR_I2C1EN) != 0U;
}

static bool clock_profile(void) {
    const uint32_t hsi = RCC_CR_HSION | RCC_CR_HSIRDY;
    const uint32_t bus = RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1;
    return nx_stm32f1_system_live() && (READ_REG(RCC->CR) & hsi) == hsi &&
           (READ_REG(RCC->CFGR) & bus) == 0U;
}

static bool clock_live(void) {
    return clock_profile() && gate_live();
}

static bool vector_idle(IRQn_Type irq) {
    return NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetActive(irq) == 0U &&
           NVIC_GetPendingIRQ(irq) == 0U;
}

/** \brief Hold reset after masking producers, without waiting for peer wires.
 */
static bool withdraw(void) {
    if (!gate_live()) {
        return false;
    }
    WRITE_REG(I2C1->CR2, I2C_FREQ_MHZ);
    WRITE_REG(I2C1->CR1, I2C_CR1_SWRST);
    NVIC_DisableIRQ(I2C1_EV_IRQn);
    NVIC_DisableIRQ(I2C1_ER_IRQn);
    nx_arch_dsb();
    nx_arch_isb();
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
    NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    nx_arch_dsb();
    return READ_REG(I2C1->CR1) == I2C_CR1_SWRST &&
           (READ_REG(I2C1->CR2) & PUBLISHERS) == 0U &&
           vector_idle(I2C1_EV_IRQn) && vector_idle(I2C1_ER_IRQn);
}

nx_result_t nx_stm32f1_i2c_target_open(nx_stm32f1_i2c_target_t* state,
                                       uint32_t address) {
    if (state == NULL || address < 8U || address > 119U) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!clock_profile()) {
        return NX_ERROR_STATE;
    }
    if (state->live || state->vectors_owned || state->bus.effects != 0U ||
        !vector_idle(I2C1_EV_IRQn) || !vector_idle(I2C1_ER_IRQn)) {
        return NX_ERROR_BUSY;
    }
    if (state->generation == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    nx_result_t result =
        nx_stm32f1_i2c_prepare(&state->bus, 8000000U, I2C_BAUD);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->prior_event_priority = (uint8_t)NVIC_GetPriority(I2C1_EV_IRQn);
    state->prior_error_priority = (uint8_t)NVIC_GetPriority(I2C1_ER_IRQn);
    state->vectors_owned = true;
    NVIC_SetPriority(I2C1_EV_IRQn, 0U);
    NVIC_SetPriority(I2C1_ER_IRQn, 0U);
    if (NVIC_GetPriority(I2C1_EV_IRQn) != 0U ||
        NVIC_GetPriority(I2C1_ER_IRQn) != 0U || !withdraw()) {
        return NX_ERROR_IO;
    }
    ++state->generation;
    state->address = (uint8_t)address;
    state->closing = false;
    state->live = true;
    return NX_SUCCESS;
}

/** \brief Validate independent request storage, then publish READY metadata. */
static bool independent(const nx_stm32f1_i2c_target_t* state, const void* ptr,
                        size_t size, size_t alignment) {
    return nx_io_dispatch_span(ptr, size, alignment) &&
           !nx_io_dispatch_overlap(ptr, size, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(ptr, size, (const void*)UINT32_C(0x40000000),
                                   UINT32_C(0x20000000));
}

static bool request_shape(const nx_stm32f1_i2c_target_t* state,
                          const nx_i2c_target_request_t* request,
                          const uint8_t* rx, size_t rx_bytes, const uint8_t* tx,
                          size_t tx_bytes) {
#if defined(NX_STM32F1_I2C_TARGET_WRITE_READ)
    return rx_bytes != 0U && rx_bytes <= NX_I2C_TARGET_MAX_BYTES &&
           tx_bytes != 0U && tx_bytes <= NX_I2C_TARGET_MAX_BYTES &&
           independent(state, request, sizeof(*request),
                       _Alignof(nx_i2c_target_request_t)) &&
           independent(state, rx, rx_bytes, 1U) &&
           independent(state, tx, tx_bytes, 1U) &&
           !nx_io_dispatch_overlap(request, sizeof(*request), rx, rx_bytes) &&
           !nx_io_dispatch_overlap(request, sizeof(*request), tx, tx_bytes) &&
           !nx_io_dispatch_overlap(rx, rx_bytes, tx, tx_bytes);
#else
#if defined(NX_STM32F1_I2C_TARGET_TX)
    if (rx != NULL || rx_bytes != 0U)
        return false;
    const uint8_t* payload = tx;
    const size_t bytes = tx_bytes;
#else
    if (tx != NULL || tx_bytes != 0U)
        return false;
    const uint8_t* payload = rx;
    const size_t bytes = rx_bytes;
#endif
    return bytes != 0U && bytes <= NX_I2C_TARGET_MAX_BYTES &&
           independent(state, request, sizeof(*request),
                       _Alignof(nx_i2c_target_request_t)) &&
           independent(state, payload, bytes, 1U) &&
           !nx_io_dispatch_overlap(request, sizeof(*request), payload, bytes);
#endif
}

static nx_result_t
prepare_request(void* context, nx_i2c_target_request_t* request, uint8_t* rx,
                size_t length, const uint8_t* tx, size_t tx_length,
                nx_i2c_target_direction_t direction, nx_time_us_t deadline) {
    nx_stm32f1_i2c_target_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || !state->live || state->closing || !clock_live()) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (direction != TARGET_DIRECTION) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (deadline == NX_DEADLINE_NEVER ||
        !request_shape(state, request, rx, length, tx, tx_length)) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    const nx_result_t prepared = nx_request_prepare(&request->base, deadline);
    if (prepared != NX_SUCCESS) {
        return prepared;
    }
    request->owner = state;
    request->generation = state->generation;
    request->time_epoch = state->generation;
    request->rx = rx;
    request->rx_capacity = length;
    request->tx = tx;
    request->tx_length = tx_length;
    request->direction = direction;
    request->rx_received = request->tx_completed = 0U;
    request->faults = request->matched_address = request->address_phases = 0U;
    return NX_SUCCESS;
}

/** \brief Record a terminal outcome before disabling further publication. */
static void end_request(nx_stm32f1_i2c_target_t* state, nx_result_t result) {
    if (state->terminal != NX_ERROR_BUSY) {
        return;
    }
    state->terminal = result;
    (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    if (gate_live()) {
        WRITE_REG(I2C1->CR2, I2C_FREQ_MHZ);
        /* EV4 clears STOPF with an SR1 read immediately before the CR1 write.
         */
        (void)READ_REG(I2C1->SR1);
        WRITE_REG(I2C1->CR1, I2C_CR1_PE);
        nx_arch_dsb();
    }
}

/** \brief Use clock stretching and the highest IRQ priority for ES096 byte
 * windows. */
static bool arm(uint8_t address) {
    const uint32_t oar = (1U << 14U) | ((uint32_t)address << 1U);
    WRITE_REG(I2C1->CR1, 0U);
    WRITE_REG(I2C1->CR2, I2C_FREQ_MHZ);
    WRITE_REG(I2C1->OAR1, oar);
    WRITE_REG(I2C1->CCR, 50U);
    WRITE_REG(I2C1->TRISE, 9U);
    WRITE_REG(I2C1->CR1, I2C_CR1_PE);
    if (READ_REG(I2C1->CR1) != I2C_CR1_PE ||
        READ_REG(I2C1->CR2) != I2C_FREQ_MHZ || READ_REG(I2C1->OAR1) != oar) {
        return false;
    }
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
    nx_arch_dsb();
    if (NVIC_GetEnableIRQ(I2C1_EV_IRQn) == 0U ||
        NVIC_GetEnableIRQ(I2C1_ER_IRQn) == 0U) {
        return false;
    }
    WRITE_REG(I2C1->CR2, I2C_FREQ_MHZ | ARMED_IRQS);
    WRITE_REG(I2C1->CR1, I2C_CR1_PE | I2C_CR1_ACK);
    nx_arch_dsb();
    return READ_REG(I2C1->CR2) == (I2C_FREQ_MHZ | ARMED_IRQS) &&
           READ_REG(I2C1->CR1) == (I2C_CR1_PE | I2C_CR1_ACK);
}

static nx_result_t submit(void* context, nx_i2c_target_request_t* request) {
    nx_stm32f1_i2c_target_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || !state->live || state->closing || !clock_live()) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        return NX_ERROR_BUSY;
    }
    if (!independent(state, request, sizeof(*request),
                     _Alignof(nx_i2c_target_request_t))) {
        return NX_ERROR_INVALID;
    }
    if (request->owner != state || request->generation != state->generation ||
        request->time_epoch != state->generation) {
        return NX_ERROR_STATE;
    }
    if (request->direction != TARGET_DIRECTION ||
        request->base.deadline == NX_DEADLINE_NEVER ||
        !request_shape(state, request, request->rx, request->rx_capacity,
                       request->tx, request->tx_length)) {
        return NX_ERROR_INVALID;
    }
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    if (READ_REG(I2C1->CR1) != I2C_CR1_SWRST ||
        (READ_REG(GPIOB->IDR) & LINES) != LINES || !vector_idle(I2C1_EV_IRQn) ||
        !vector_idle(I2C1_ER_IRQn)) {
        return NX_ERROR_BUSY;
    }

    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    const nx_result_t accepted =
        nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (accepted == NX_SUCCESS) {
        state->active = request;
        state->count = state->phases = 0U;
#if defined(NX_STM32F1_I2C_TARGET_WRITE_READ)
        state->received = 0U;
#endif
        state->faults = 0U;
        state->terminal = NX_ERROR_BUSY;
        if (!arm(state->address)) {
            end_request(state, NX_ERROR_IO);
        }
    }
    nx_arch_irq_restore(saved);
    return accepted;
}

/** \brief Reject a stretched or malformed pipeline after its valid prefix. */
static void receive_fault(nx_stm32f1_i2c_target_t* state, uint32_t flag,
                          nx_result_t result) {
    state->faults |= flag;
    end_request(state, result);
}

#if defined(NX_STM32F1_I2C_TARGET_TX) ||                                       \
    defined(NX_STM32F1_I2C_TARGET_WRITE_READ)
/** \brief Keep one byte in the pipeline until BTF or terminal AF proves it. */
static void transmit_irq(nx_stm32f1_i2c_target_t* state, uint32_t status) {
    const nx_i2c_target_request_t* request = state->active;
#if defined(NX_STM32F1_I2C_TARGET_TX)
    if ((status & I2C_SR1_ADDR) != 0U) {
        const uint32_t address_status = READ_REG(I2C1->SR2);
        if ((address_status & I2C_SR2_TRA) == 0U || state->phases != 0U ||
            (status &
             (I2C_SR1_STOPF | I2C_SR1_RXNE | I2C_SR1_AF | I2C_SR1_BTF)) != 0U) {
            receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
            return;
        }
        /* EV3_1 stretches SCL until the first DR store after clearing ADDR. */
        state->phases = 1U;
        WRITE_REG(I2C1->DR, request->tx[0]);
        return;
    }
#endif
    if ((status & I2C_SR1_RXNE) != 0U ||
        ((status & I2C_SR1_STOPF) != 0U && (status & I2C_SR1_AF) == 0U)) {
        receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
        return;
    }
    if ((status & (I2C_SR1_AF | I2C_SR1_BTF)) == 0U)
        return;
    const bool nack = (status & I2C_SR1_AF) != 0U;
    if (nack)
        WRITE_REG(I2C1->SR1, ~(uint32_t)I2C_SR1_AF);
    if (state->phases == 0U || state->count >= request->tx_length) {
        receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
        return;
    }
    ++state->count;
    const bool complete = state->count == request->tx_length;
    if (nack && complete) {
        end_request(state, NX_SUCCESS);
    } else if (nack || complete) {
        receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
    } else {
        /* No TXE prefetch. Empty DR at BTF stretches the next ACK window. */
        WRITE_REG(I2C1->DR, request->tx[state->count]);
    }
}
#endif

#if defined(NX_STM32F1_I2C_TARGET_WRITE_READ)
/** \brief Admit only W then R at the local address, with EV2 already drained.
 */
static void exchange_address(nx_stm32f1_i2c_target_t* state, uint32_t status) {
    const bool transmit = (READ_REG(I2C1->SR2) & I2C_SR2_TRA) != 0U;
    if ((status & (I2C_SR1_RXNE | I2C_SR1_BTF | I2C_SR1_STOPF | I2C_SR1_AF)) !=
            0U ||
        (!transmit && (status & I2C_SR1_TXE) != 0U)) {
        receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
        return;
    }
    if (state->phases == 0U && !transmit) {
        state->phases = 1U;
        return;
    }
    if (state->phases != 1U || !transmit || state->count == 0U) {
        receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
        return;
    }
    /* The first DR write ends address stretching. Mask TXE before release. */
    WRITE_REG(I2C1->CR2, I2C_FREQ_MHZ | TX_IRQS);
    if (READ_REG(I2C1->CR2) != (I2C_FREQ_MHZ | TX_IRQS)) {
        end_request(state, NX_ERROR_IO);
        return;
    }
    state->received = state->count;
    state->count = 0U;
    state->phases = 2U;
    WRITE_REG(I2C1->DR, state->active->tx[0]);
}
#endif

void nx_stm32f1_i2c_target_irq(nx_stm32f1_i2c_target_t* state) {
    if (state == NULL || state->active == NULL ||
        state->terminal != NX_ERROR_BUSY) {
        return;
    }
    if (!clock_live()) {
        end_request(state, NX_ERROR_IO);
        return;
    }
    if (nx_deadline_expired(state->active->base.deadline, nx_time_now_us())) {
        end_request(state, NX_ERROR_TIMEOUT);
        return;
    }
    const uint32_t status = READ_REG(I2C1->SR1);
    if ((status & I2C_ERRORS) != 0U) {
        state->faults |=
            (((status & I2C_SR1_BERR) != 0U) ? NX_I2C_TARGET_BUS_ERROR : 0U) |
            (((status & I2C_SR1_OVR) != 0U) ? NX_I2C_TARGET_OVERRUN : 0U) |
            (((status & I2C_SR1_ARLO) != 0U) ? NX_I2C_TARGET_ARBITRATION : 0U) |
            (((status & I2C_SR1_TIMEOUT) != 0U) ? NX_I2C_TARGET_CLOCK_TIMEOUT
                                                : 0U) |
            (((status & AF_ERROR) != 0U) ? NX_I2C_TARGET_PROTOCOL_ERROR : 0U);
        end_request(state, NX_ERROR_IO);
        return;
    }
#if defined(NX_STM32F1_I2C_TARGET_TX)
    transmit_irq(state, status);
#else
#if defined(NX_STM32F1_I2C_TARGET_WRITE_READ)
    if ((status & I2C_SR1_ADDR) != 0U) {
        exchange_address(state, status);
        return;
    }
    if (state->phases == 2U) {
        transmit_irq(state, status);
        return;
    }
    if ((status & I2C_SR1_AF) != 0U) {
        receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
        return;
    }
#else
    if ((status & I2C_SR1_ADDR) != 0U) {
        const uint32_t address_status = READ_REG(I2C1->SR2);
        if ((address_status & I2C_SR2_TRA) != 0U || state->phases != 0U ||
            (status & (I2C_SR1_STOPF | I2C_SR1_RXNE)) != 0U) {
            receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
            return;
        }
        state->phases = 1U;
    }
#endif
    if ((status & I2C_SR1_TXE) != 0U) {
        receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
        return;
    }
    if ((status & I2C_SR1_BTF) != 0U) {
        /* ES096 2.8.1: a missed EV2 window cannot certify buffered data. */
        receive_fault(state, NX_I2C_TARGET_OVERRUN, NX_ERROR_IO);
        return;
    }
    if ((status & I2C_SR1_RXNE) != 0U) {
        if (state->phases == 0U) {
            receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
            return;
        }
        if (state->count == state->active->rx_capacity) {
            receive_fault(state, NX_I2C_TARGET_OVERRUN, NX_ERROR_OVERFLOW);
            return;
        }
        state->active->rx[state->count++] = (uint8_t)READ_REG(I2C1->DR);
    }
    if ((status & I2C_SR1_STOPF) != 0U) {
#if defined(NX_STM32F1_I2C_TARGET_WRITE_READ)
        receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
#else
        if (state->phases == 1U) {
            end_request(state, NX_SUCCESS);
        } else {
            receive_fault(state, NX_I2C_TARGET_PROTOCOL_ERROR, NX_ERROR_IO);
        }
#endif
    }
#endif
}

/** \brief Thread alone can settle after both publishers and reset agree. */
static void service(void* context) {
    nx_stm32f1_i2c_target_t* state = context;
    if (!nx_stm32f1_cold_context() || state == NULL || state->active == NULL) {
        return;
    }
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!clock_live()) {
        end_request(state, NX_ERROR_IO);
    } else if (nx_deadline_expired(state->active->base.deadline,
                                   nx_time_now_us())) {
        end_request(state, NX_ERROR_TIMEOUT);
    }
    if (state->terminal != NX_ERROR_BUSY) {
        if (withdraw()) {
            nx_i2c_target_request_t* request = state->active;
            nx_result_t result = state->terminal;
            if (result == NX_SUCCESS) {
                if (!clock_live()) {
                    result = NX_ERROR_IO;
                } else if (nx_deadline_expired(request->base.deadline,
                                               nx_time_now_us())) {
                    result = NX_ERROR_TIMEOUT;
                }
            }
#if defined(NX_STM32F1_I2C_TARGET_WRITE_READ)
            const bool switched = state->phases == 2U;
            request->rx_received = switched ? state->received : state->count;
            request->tx_completed = switched ? state->count : 0U;
#elif defined(NX_STM32F1_I2C_TARGET_TX)
            request->tx_completed = state->count;
#else
            request->rx_received = state->count;
#endif
            request->faults = state->faults;
            request->matched_address =
                state->phases != 0U ? state->address : 0U;
            request->address_phases = state->phases;
            state->active = NULL;
#if defined(NX_STM32F1_I2C_TARGET_WRITE_READ)
            nx_request_settle(&request->base, result,
                              request->rx_received + request->tx_completed);
#else
            nx_request_settle(&request->base, result, state->count);
#endif
        } else {
            (void)nx_request_transition(&state->active->base,
                                        NX_REQUEST_QUARANTINED);
        }
    }
    nx_arch_irq_restore(saved);
}

static nx_result_t cancel(void* context, nx_i2c_target_request_t* request) {
    nx_stm32f1_i2c_target_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || state->active == NULL || state->active != request) {
        return NX_ERROR_STATE;
    }
    const nx_arch_irq_state_t saved = nx_arch_irq_save();
    end_request(state, NX_ERROR_CANCELLED);
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

static nx_result_t stop(void* context) {
    nx_stm32f1_i2c_target_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || !state->live) {
        return NX_ERROR_STATE;
    }
    state->closing = true;
    if (state->active != NULL) {
        (void)cancel(state, state->active);
        service(state);
    }
    return state->active != NULL || !withdraw() ? NX_ERROR_BUSY : NX_SUCCESS;
}

static nx_result_t recover(void* context) {
    nx_stm32f1_i2c_target_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || !state->live || state->closing || !clock_live()) {
        return NX_ERROR_STATE;
    }
    if (state->active != NULL || !withdraw()) {
        return NX_ERROR_BUSY;
    }
    if (state->generation == UINT64_MAX) {
        return NX_ERROR_EXHAUSTED;
    }
    ++state->generation;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_i2c_target_close(nx_stm32f1_i2c_target_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->live) {
        nx_result_t result = stop(state);
        if (result != NX_SUCCESS) {
            return result;
        }
        state->live = false;
    }
    if (state->vectors_owned) {
        if (!vector_idle(I2C1_EV_IRQn) || !vector_idle(I2C1_ER_IRQn)) {
            return NX_ERROR_BUSY;
        }
        NVIC_SetPriority(I2C1_EV_IRQn, state->prior_event_priority);
        NVIC_SetPriority(I2C1_ER_IRQn, state->prior_error_priority);
        if (NVIC_GetPriority(I2C1_EV_IRQn) != state->prior_event_priority ||
            NVIC_GetPriority(I2C1_ER_IRQn) != state->prior_error_priority) {
            return NX_ERROR_IO;
        }
        state->vectors_owned = false;
    }
    return nx_stm32f1_i2c_close(&state->bus);
}

const nx_i2c_target_ops_t nx_stm32f1_i2c_target_ops = {.prepare =
                                                           prepare_request,
                                                       .submit = submit,
                                                       .cancel = cancel,
                                                       .service = service,
                                                       .stop = stop,
                                                       .recover = recover};
