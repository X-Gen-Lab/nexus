/**
 * \file            i2c.c
 * \brief           STM32F103 standard-mode I2C1 polling and repeated START
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "drivers/i2c/instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_i2c.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "The selected polling mode requires the reviewed STM32F103xB route"
#endif

#define SCL_PIN       (1U << 6U)
#define SDA_PIN       (1U << 7U)
#define LINES         (SCL_PIN | SDA_PIN)
#define SCL_FIELD     (UINT32_C(0xf) << 24U)
#define SCL_OUTPUT_OD (UINT32_C(0x6) << 24U)
#define SCL_AF_OD     (UINT32_C(0xe) << 24U)
#define PADS_AF_OD    (UINT32_C(0xee) << 24U)
#define ERROR_FLAGS   (I2C_SR1_AF | I2C_SR1_ARLO | I2C_SR1_BERR | I2C_SR1_OVR)

enum {
    AFIO_EFFECT = 1U,
    CLOCK_EFFECT = 2U,
    CONFIG_EFFECT = 4U,
    LIVE = 8U,
    ACTIVE = 16U,
    FAULT = 32U,
    SCL_HELD = 64U,
    STOP_SETTLE_US = 1000U
};

static bool lines_high(void) {
    return (READ_REG(GPIOB->IDR) & LINES) == LINES;
}

static bool clock_profile(void) {
    const uint32_t masks = RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1;
    return (READ_REG(RCC->CFGR) & masks) == RCC_CFGR_SWS_HSI;
}

static bool local_reset(nx_stm32f1_i2c_t* state);

static bool configure(void) {
    /* 80 kHz leaves HSI margin under the ES096 2.8.5 88 kHz ceiling. */
    LL_I2C_ConfigSpeed(I2C1, 8000000U, 80000U, LL_I2C_DUTYCYCLE_2);
    WRITE_REG(I2C1->OAR1, UINT32_C(1) << 14U);
    LL_I2C_Enable(I2C1);
    nx_arch_dsb();
    return READ_REG(I2C1->CR1) == I2C_CR1_PE && READ_REG(I2C1->CR2) == 8U &&
           READ_REG(I2C1->CCR) == 50U && READ_REG(I2C1->TRISE) == 9U &&
           READ_REG(I2C1->OAR1) == (UINT32_C(1) << 14U);
}

static bool restore_afio(nx_stm32f1_i2c_t* state) {
    if ((state->effects & AFIO_EFFECT) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN, 0U)) {
            return false;
        }
        state->effects &= (uint8_t)~AFIO_EFFECT;
    }
    return true;
}

nx_result_t nx_stm32f1_i2c_prepare(nx_stm32f1_i2c_t* state, uint32_t clock_hz,
                                   uint32_t max_hz) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (clock_hz != 8000000U || max_hz != 80000U || !nx_stm32f1_system_live() ||
        !clock_profile()) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (state->effects != 0U || state->pads.registers != NULL ||
        (READ_REG(RCC->APB1ENR) & RCC_APB1ENR_I2C1EN) != 0U ||
        (READ_REG(RCC->APB1RSTR) & RCC_APB1RSTR_I2C1RST) != 0U) {
        return NX_ERROR_BUSY;
    }
    if ((READ_REG(RCC->APB2ENR) & RCC_APB2ENR_AFIOEN) == 0U) {
        state->effects |= AFIO_EFFECT;
        if (!nx_stm32f1_write_masked(&RCC->APB2ENR, RCC_APB2ENR_AFIOEN,
                                     RCC_APB2ENR_AFIOEN)) {
            return NX_ERROR_IO;
        }
    }
    const bool remapped = (READ_REG(AFIO->MAPR) & AFIO_MAPR_I2C1_REMAP) != 0U;
    if (!restore_afio(state)) {
        return NX_ERROR_IO;
    }
    if (remapped) {
        return NX_ERROR_BUSY;
    }
    state->effects |= CLOCK_EFFECT;
    if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_I2C1EN,
                                 RCC_APB1ENR_I2C1EN)) {
        return NX_ERROR_IO;
    }
    if (READ_REG(I2C1->CR1) != 0U || READ_REG(I2C1->CR2) != 0U) {
        return NX_ERROR_BUSY;
    }
    nx_result_t result =
        nx_stm32f1_pads_capture(&state->pads, GPIOB, (uint16_t)LINES);
    if (result != NX_SUCCESS) {
        return result;
    }
    result = nx_stm32f1_pads_apply(&state->pads, PADS_AF_OD, 0U, LINES);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->effects |= CONFIG_EFFECT;
    if (!configure()) {
        return NX_ERROR_IO;
    }
    state->effects |= LIVE;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_i2c_close(nx_stm32f1_i2c_t* state) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if ((state->effects & ACTIVE) != 0U) {
        return NX_ERROR_BUSY;
    }
    state->effects &= (uint8_t)~LIVE;
    if ((state->effects & CONFIG_EFFECT) != 0U) {
        if (!local_reset(state)) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~CONFIG_EFFECT;
    }
    nx_result_t result = nx_stm32f1_pads_restore(&state->pads);
    if (result != NX_SUCCESS) {
        return result;
    }
    if ((state->effects & CLOCK_EFFECT) != 0U) {
        if (!nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_I2C1EN, 0U)) {
            return NX_ERROR_IO;
        }
        state->effects &= (uint8_t)~CLOCK_EFFECT;
    }
    if (!restore_afio(state)) {
        return NX_ERROR_IO;
    }
    state->effects = 0U;
    return NX_SUCCESS;
}

/** \brief Acknowledge only errors in the observed SR1 snapshot. */
static nx_result_t acknowledge_error(uint32_t status) {
    const uint32_t errors = status & ERROR_FLAGS;
    if (errors == 0U) {
        return NX_SUCCESS;
    }
    WRITE_REG(I2C1->SR1, ~errors);
    if ((errors & I2C_SR1_ARLO) != 0U) {
        return NX_ERROR_ARBITRATION;
    }
    return (errors & (I2C_SR1_BERR | I2C_SR1_OVR)) != 0U ? NX_ERROR_IO
                                                         : NX_ERROR_NACK;
}

static nx_result_t wait_event(uint32_t mask, nx_time_us_t deadline) {
    while (!nx_deadline_expired(deadline, nx_time_now_us())) {
        const uint32_t status = READ_REG(I2C1->SR1);
        const nx_result_t error = acknowledge_error(status);
        if (error != NX_SUCCESS) {
            return error;
        }
        if ((status & mask) == mask) {
            return NX_SUCCESS;
        }
    }
    return NX_ERROR_TIMEOUT;
}

static void command(uint32_t controls) {
    /* Never copy a hardware-cleared START/STOP from a stale CR1 snapshot. */
    WRITE_REG(I2C1->CR1, I2C_CR1_PE | controls);
}

static void clear_address(void) {
    /* ADDR is acknowledged by an ordered SR1/SR2 read pair. */
    (void)READ_REG(I2C1->SR1);
    (void)READ_REG(I2C1->SR2);
}

static bool hold_scl(nx_stm32f1_i2c_t* state) {
    /* The F1 errata's byte-critical windows need an actual electrical hold. */
    state->effects |= SCL_HELD;
    WRITE_REG(GPIOB->BSRR, SCL_PIN << 16U);
    nx_arch_dsb();
    return (READ_REG(GPIOB->ODR) & SCL_PIN) == 0U &&
           nx_stm32f1_write_masked(&GPIOB->CRL, SCL_FIELD, SCL_OUTPUT_OD) &&
           (READ_REG(GPIOB->IDR) & SCL_PIN) == 0U;
}

static bool release_scl(nx_stm32f1_i2c_t* state) {
    if ((state->effects & SCL_HELD) == 0U) {
        return true;
    }
    if (!nx_stm32f1_write_masked(&GPIOB->CRL, SCL_FIELD, SCL_AF_OD)) {
        return false;
    }
    WRITE_REG(GPIOB->BSRR, SCL_PIN);
    nx_arch_dsb();
    if ((READ_REG(GPIOB->ODR) & SCL_PIN) == 0U) {
        return false;
    }
    state->effects &= (uint8_t)~SCL_HELD;
    return true;
}

static bool local_reset(nx_stm32f1_i2c_t* state) {
    /* ES096 2.8.4 calls this CR2, but RM0008 locates SWRST in CR1. */
    WRITE_REG(I2C1->CR1, I2C_CR1_SWRST);
    nx_arch_dsb();
    if ((READ_REG(I2C1->CR1) & I2C_CR1_SWRST) == 0U || !release_scl(state) ||
        !lines_high()) {
        return false;
    }
    /* RM0008 requires both wires released before withdrawing SWRST. */
    WRITE_REG(I2C1->CR1, 0U);
    nx_arch_dsb();
    return READ_REG(I2C1->CR1) == 0U &&
           (READ_REG(I2C1->SR2) & I2C_SR2_MSL) == 0U;
}

static void read_byte(uint8_t* data, size_t* moved) {
    *data = LL_I2C_ReceiveData8(I2C1);
    ++*moved;
}

static nx_result_t receive(nx_stm32f1_i2c_t* state, uint8_t* data,
                           size_t remaining, uint32_t end,
                           nx_time_us_t deadline, size_t* moved) {
    if (remaining == 1U) {
        command(0U);
        if (!hold_scl(state)) {
            return NX_ERROR_IO;
        }
        clear_address();
        command(end);
    } else if (remaining == 2U) {
        if (!hold_scl(state)) {
            return NX_ERROR_IO;
        }
        clear_address();
        command(I2C_CR1_POS);
        if (!release_scl(state)) {
            return NX_ERROR_IO;
        }
        nx_result_t ready = wait_event(I2C_SR1_BTF, deadline);
        if (ready != NX_SUCCESS) {
            return ready;
        }
        if (!hold_scl(state)) {
            return NX_ERROR_IO;
        }
        command(end | I2C_CR1_POS);
        read_byte(data++, moved);
    } else {
        clear_address();
        /* BTF stretches the clock if task or IRQ latency outlasts RXNE. */
        while (remaining > 3U) {
            nx_result_t ready = wait_event(I2C_SR1_BTF, deadline);
            if (ready != NX_SUCCESS) {
                return ready;
            }
            read_byte(data++, moved);
            --remaining;
        }
        nx_result_t ready = wait_event(I2C_SR1_BTF, deadline);
        if (ready != NX_SUCCESS) {
            return ready;
        }
        command(0U);
        if (!hold_scl(state)) {
            return NX_ERROR_IO;
        }
        read_byte(data++, moved);
        command(end);
        read_byte(data++, moved);
    }
    if (!release_scl(state)) {
        return NX_ERROR_IO;
    }
    nx_result_t ready = wait_event(I2C_SR1_RXNE, deadline);
    if (ready == NX_SUCCESS) {
        read_byte(data, moved);
    }
    return ready;
}

static nx_result_t transmit(const uint8_t* data, size_t length, uint32_t end,
                            nx_time_us_t deadline, size_t* moved) {
    clear_address();
    nx_result_t ready = wait_event(I2C_SR1_TXE, deadline);
    for (size_t index = 0U; index < length && ready == NX_SUCCESS; ++index) {
        WRITE_REG(I2C1->DR, data[index]);
        ++*moved;
        /* A completed byte stretches SCL until the next software write. */
        ready = wait_event(I2C_SR1_BTF, deadline);
    }
    if (ready == NX_SUCCESS) {
        command(end);
    }
    return ready;
}

static bool master_active(void) {
    return (READ_REG(I2C1->CR1) & I2C_CR1_STOP) != 0U ||
           (READ_REG(I2C1->SR2) & I2C_SR2_MSL) != 0U;
}

/** \brief Preserve an earlier error while honoring loss of bus ownership. */
static nx_result_t settle_error(nx_result_t earlier) {
    const nx_result_t latest = acknowledge_error(READ_REG(I2C1->SR1));
    return (earlier == NX_SUCCESS || latest == NX_ERROR_ARBITRATION) ? latest
                                                                     : earlier;
}

static nx_result_t settle(nx_stm32f1_i2c_t* state, nx_result_t result,
                          bool awaiting_start) {
    if (result != NX_ERROR_ARBITRATION) {
        result = settle_error(result);
    }
    if (result == NX_ERROR_ARBITRATION) {
        if (!release_scl(state)) {
            state->effects |= FAULT;
            return NX_ERROR_IO;
        }
        return result;
    }
    if (awaiting_start || (state->effects & SCL_HELD) != 0U ||
        (result != NX_SUCCESS && ((READ_REG(I2C1->CR1) & I2C_CR1_START) != 0U ||
                                  (READ_REG(I2C1->SR1) & I2C_SR1_SB) != 0U))) {
        state->effects |= FAULT;
        return local_reset(state) ? result : NX_ERROR_IO;
    }
    if (result != NX_SUCCESS && (READ_REG(I2C1->CR1) & I2C_CR1_STOP) == 0U &&
        master_active()) {
        command(I2C_CR1_STOP);
    }
    const nx_time_us_t start = nx_time_now_us();
    while (master_active()) {
        result = settle_error(result);
        if (result == NX_ERROR_ARBITRATION) {
            return result;
        }
        if (nx_time_now_us() - start >= STOP_SETTLE_US) {
            state->effects |= FAULT;
            return local_reset(state) && result != NX_SUCCESS ? result
                                                              : NX_ERROR_IO;
        }
    }
    /* STOP retirement alone cannot prove the last payload was error-free. */
    result = settle_error(result);
    if (result != NX_ERROR_ARBITRATION) {
        command(0U);
    }
    return result;
}

static bool outside_controls(const nx_stm32f1_i2c_endpoint_t* endpoint,
                             const void* storage, size_t bytes) {
    return !nx_io_dispatch_overlap(storage, bytes, endpoint,
                                   sizeof(*endpoint)) &&
           !nx_io_dispatch_overlap(storage, bytes, endpoint->port,
                                   sizeof(*endpoint->port)) &&
           !nx_io_dispatch_overlap(storage, bytes, I2C1, sizeof(*I2C1)) &&
           !nx_io_dispatch_overlap(storage, bytes, GPIOB, sizeof(*GPIOB)) &&
           !nx_io_dispatch_overlap(storage, bytes, RCC, sizeof(*RCC)) &&
           !nx_io_dispatch_overlap(storage, bytes, AFIO, sizeof(*AFIO)) &&
           !nx_io_dispatch_overlap(storage, bytes, &nx_stm32f1_i2c_ops,
                                   sizeof(nx_stm32f1_i2c_ops)) &&
           !nx_io_dispatch_overlap(storage, bytes, &nx_stm32f1_i2c_endpoint_ops,
                                   sizeof(nx_stm32f1_i2c_endpoint_ops));
}

static nx_result_t check_storage(const nx_stm32f1_i2c_endpoint_t* endpoint,
                                 nx_i2c_message_t* messages, size_t count,
                                 size_t* moved) {
    if (count > NX_I2C_BOUNDED_MAX_MESSAGES) {
        return NX_ERROR_UNSUPPORTED;
    }
    const size_t bytes = sizeof(*messages) * count;
    if (!nx_io_dispatch_span(messages, bytes, _Alignof(nx_i2c_message_t)) ||
        !nx_io_dispatch_span(moved, sizeof(*moved), _Alignof(size_t)) ||
        !outside_controls(endpoint, moved, sizeof(*moved)) ||
        !outside_controls(endpoint, messages, bytes) ||
        nx_io_dispatch_overlap(moved, sizeof(*moved), messages, bytes)) {
        return NX_ERROR_INVALID;
    }
    const nx_i2c_request_t view = {.messages = messages, .count = count};
    nx_result_t result =
        nx_i2c_request_validate_limits(&view, &nx_i2c_bounded_limits);
    if (result != NX_SUCCESS) {
        return result;
    }
    for (size_t index = 0U; index < count; ++index) {
        const void* data = nx_i2c_message_data(&messages[index]);
        const size_t length = messages[index].length;
        if (!outside_controls(endpoint, data, length) ||
            nx_io_dispatch_overlap(moved, sizeof(*moved), data, length)) {
            return NX_ERROR_INVALID;
        }
    }
    return NX_SUCCESS;
}

static bool valid_endpoint(const nx_stm32f1_i2c_endpoint_t* endpoint) {
    return endpoint != NULL && endpoint->port != NULL &&
           ((endpoint->address_bits == 7U && endpoint->address >= 0x08U &&
             endpoint->address <= 0x77U) ||
            (endpoint->address_bits == 10U && endpoint->address <= 0x3ffU));
}

/** \brief Select the endpoint and leave ADDR for the common data phase.
 * \note RM0008 EV5/EV9 followed by a writer ADDR. A standalone 10-bit
 * read needs an internal repeated START. An already addressed same target
 * accepts the read header after the previous message's repeated START.
 */
static nx_result_t address_phase(const nx_stm32f1_i2c_endpoint_t* endpoint,
                                 const nx_i2c_message_t* message,
                                 bool already_selected, nx_time_us_t deadline,
                                 bool* awaiting_start) {
    const uint32_t mode = message->read && message->length == 2U
                              ? I2C_CR1_ACK | I2C_CR1_POS
                              : I2C_CR1_ACK;
    const uint32_t direction = message->read ? 1U : 0U;
    if (endpoint->address_bits == 7U) {
        command(mode);
        WRITE_REG(I2C1->DR, ((uint32_t)endpoint->address << 1U) | direction);
        return wait_event(I2C_SR1_ADDR, deadline);
    }

    const uint32_t header = 0xf0U | (((uint32_t)endpoint->address >> 7U) & 6U);
    if (message->read && already_selected) {
        command(mode);
        WRITE_REG(I2C1->DR, header | 1U);
        return wait_event(I2C_SR1_ADDR, deadline);
    }
    command(I2C_CR1_ACK);
    WRITE_REG(I2C1->DR, header);
    nx_result_t result = wait_event(I2C_SR1_ADD10, deadline);
    if (result != NX_SUCCESS)
        return result;
    WRITE_REG(I2C1->DR, endpoint->address & 0xffU);
    result = wait_event(I2C_SR1_ADDR, deadline);
    if (result != NX_SUCCESS || !message->read)
        return result;

    /* The preliminary EV6 was transmitter-side. Release it before START,
     * then configure ACK/POS after SB for the actual receiver EV6. */
    clear_address();
    *awaiting_start = true;
    command(I2C_CR1_START | I2C_CR1_ACK);
    result = wait_event(I2C_SR1_SB, deadline);
    if (result != NX_SUCCESS)
        return result;
    *awaiting_start = false;
    command(mode);
    WRITE_REG(I2C1->DR, header | 1U);
    return wait_event(I2C_SR1_ADDR, deadline);
}

static nx_result_t transaction(void* context, nx_i2c_message_t* messages,
                               size_t count, nx_time_us_t deadline,
                               size_t* moved) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    const nx_stm32f1_i2c_endpoint_t* endpoint = context;
    if (!valid_endpoint(endpoint)) {
        return NX_ERROR_INVALID;
    }
    nx_result_t result = check_storage(endpoint, messages, count, moved);
    if (result != NX_SUCCESS) {
        return result;
    }
    *moved = 0U;
    nx_stm32f1_i2c_t* state = endpoint->port;
    if ((state->effects & (LIVE | FAULT)) != LIVE) {
        return NX_ERROR_STATE;
    }
    if ((state->effects & ACTIVE) != 0U ||
        (READ_REG(I2C1->SR2) & I2C_SR2_BUSY) != 0U || !lines_high()) {
        return NX_ERROR_BUSY;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }

    state->effects |= ACTIVE;
    command(I2C_CR1_START);
    bool awaiting_start = true;
    for (size_t index = 0U; index < count; ++index) {
        result = wait_event(I2C_SR1_SB, deadline);
        if (result != NX_SUCCESS) {
            break;
        }
        awaiting_start = false;
        const nx_i2c_message_t* message = &messages[index];
        result = address_phase(endpoint, message, index != 0U, deadline,
                               &awaiting_start);
        if (result != NX_SUCCESS) {
            break;
        }
        const uint32_t end = index + 1U == count ? I2C_CR1_STOP : I2C_CR1_START;
        result = message->read ? receive(state, message->buffer.rx,
                                         message->length, end, deadline, moved)
                               : transmit(message->buffer.tx, message->length,
                                          end, deadline, moved);
        if (result != NX_SUCCESS) {
            break;
        }
        awaiting_start = end == I2C_CR1_START;
    }
    result = settle(state, result, awaiting_start);
    state->effects &= (uint8_t)~ACTIVE;
    return result == NX_SUCCESS &&
                   nx_deadline_expired(deadline, nx_time_now_us())
               ? NX_ERROR_TIMEOUT
               : result;
}

static nx_result_t recover(void* context) {
    nx_stm32f1_i2c_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state == NULL || (state->effects & LIVE) == 0U) {
        return NX_ERROR_STATE;
    }
    if ((state->effects & ACTIVE) != 0U ||
        ((state->effects & SCL_HELD) == 0U && !lines_high())) {
        return NX_ERROR_BUSY;
    }
    state->effects |= FAULT;
    /* ES096 2.8.7 requires real line transitions to unlock an analog filter.
     * This local recovery does not drive such transitions on a caller's bus.
     */
    if (!local_reset(state) || !configure() ||
        (READ_REG(I2C1->SR2) & I2C_SR2_BUSY) != 0U || !lines_high()) {
        return NX_ERROR_IO;
    }
    state->effects &= (uint8_t)~FAULT;
    return NX_SUCCESS;
}

static bool on_port(const void* context, const nx_i2c_port_t* port) {
    const nx_stm32f1_i2c_endpoint_t* endpoint = context;
    return valid_endpoint(endpoint) && port != NULL &&
           port->ops == &nx_stm32f1_i2c_ops && endpoint->port == port->context;
}

const nx_i2c_ops_t nx_stm32f1_i2c_ops = {.recover = recover};
const nx_i2c_endpoint_ops_t nx_stm32f1_i2c_endpoint_ops = {
    .transaction = transaction,
    .on_port = on_port,
};
