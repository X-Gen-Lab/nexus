/**
 * \file            spi.c
 * \brief           F103 SPI2 short-poll, independent CS and verified tail.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "dispatch_checks.h"
#include "instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_spi.h"
#include "system/platform.h"

#ifndef NX_STM32F1_SPI_CS
#define NX_STM32F1_SPI_CS(gpio, mask, active)                                  \
    do {                                                                       \
        if (active) {                                                          \
            LL_GPIO_ResetOutputPin((gpio), (mask));                            \
        } else {                                                               \
            LL_GPIO_SetOutputPin((gpio), (mask));                              \
        }                                                                      \
    } while (0)
#endif

#ifndef NX_STM32F1_SPI_STATUS
#define NX_STM32F1_SPI_STATUS(reg) ((reg)->SR)
#define NX_STM32F1_SPI_SEND(reg, value, bits)                                  \
    do {                                                                       \
        if ((bits) == 16U) {                                                   \
            LL_SPI_TransmitData16((reg), (value));                             \
        } else {                                                               \
            LL_SPI_TransmitData8((reg), (uint8_t)(value));                     \
        }                                                                      \
    } while (0)
#define NX_STM32F1_SPI_RECEIVE(reg, bits)                                      \
    ((bits) == 16U ? LL_SPI_ReceiveData16(reg) : LL_SPI_ReceiveData8(reg))
#endif

#define SPI_ERRORS         ((uint32_t)(SPI_SR_OVR | SPI_SR_MODF))
#define SPI_POLL_MAX_WORDS 256U

/** \brief Validate the exact reviewed SPI2 pins before any hardware effect. */
nx_result_t nx_stm32f1_spi_admit(const nx_stm32f1_spi_t* state,
                                 const nx_stm32f1_spi_config_t* config) {
    if (state == NULL || config == NULL || config->registers != SPI2 ||
        config->gpio != GPIOB || config->sck != 13U || config->miso != 14U ||
        config->mosi != 15U || config->cs != 12U ||
        (config->clock_hz != 8000000U && config->clock_hz != 36000000U)) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    return state->config == NULL ? NX_SUCCESS : NX_ERROR_STATE;
}

/** \brief Claim only this APB1 gate and its fixed pads, with CS preloaded high.
 */
nx_result_t nx_stm32f1_spi_prepare(nx_stm32f1_spi_t* state,
                                   const nx_stm32f1_spi_config_t* config) {
    nx_result_t result = nx_stm32f1_spi_admit(state, config);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (!nx_stm32f1_system_live()) {
        return NX_ERROR_STATE;
    }
    state->config = config;
    const uint32_t gate = RCC_APB1ENR_SPI2EN;
    state->gate_added = (RCC->APB1ENR & gate) == 0U;
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, gate, gate)) {
        return NX_ERROR_IO;
    }
    SPI_TypeDef* registers = config->registers;
    const uint32_t status = NX_STM32F1_SPI_STATUS(registers);
    if (registers->CR1 != 0U || registers->CR2 != 0U ||
        registers->I2SCFGR != 0U ||
        (RCC->APB1RSTR & RCC_APB1RSTR_SPI2RST) != 0U ||
        (status & (SPI_SR_BSY | SPI_SR_RXNE | SPI_ERRORS)) != 0U) {
        return NX_ERROR_BUSY;
    }
    state->peripheral_owned = true;

    const uint32_t pins = (1U << config->cs) | (1U << config->sck) |
                          (1U << config->miso) | (1U << config->mosi);
    result =
        nx_stm32f1_pads_capture(&state->pads, config->gpio, (uint16_t)pins);
    if (result != NX_SUCCESS) {
        return result;
    }
    const uint32_t alternate = GPIO_CRL_CNF0_1 | LL_GPIO_SPEED_FREQ_HIGH;
    const uint32_t fields =
        (LL_GPIO_SPEED_FREQ_LOW << (4U * (config->cs - 8U))) |
        (alternate << (4U * (config->sck - 8U))) |
        (LL_GPIO_MODE_FLOATING << (4U * (config->miso - 8U))) |
        (alternate << (4U * (config->mosi - 8U)));
    result = nx_stm32f1_pads_apply(&state->pads, 0U, fields,
                                   state->pads.latch | (1U << config->cs));
    state->live = result == NX_SUCCESS;
    return result;
}

/** \brief Override only the selected active-low CS and confirm its latch. */
static bool select_cs(nx_stm32f1_spi_t* state, bool active) {
    GPIO_TypeDef* gpio = state->config->gpio;
    const uint32_t mask = 1U << state->config->cs;
    if (active) {
        state->selected = true;
    }
    NX_STM32F1_SPI_CS(gpio, mask, active);
    nx_arch_dsb();
    if (((LL_GPIO_ReadOutputPort(gpio) & mask) == 0U) != active) {
        state->fault = true;
        return false;
    }
    state->selected = active;
    return true;
}

/** \brief Reset only SPI2 and prove its transmitter disabled and idle. */
static bool reset_spi(const nx_stm32f1_spi_config_t* config) {
    volatile uint32_t* resets = &RCC->APB1RSTR;
    const uint32_t bit = RCC_APB1RSTR_SPI2RST;
    if ((RCC->APB1ENR & RCC_APB1ENR_SPI2EN) == 0U ||
        !nx_stm32f1_write_masked(resets, bit, bit) ||
        !nx_stm32f1_write_masked(resets, bit, 0U)) {
        return false;
    }
    SPI_TypeDef* registers = config->registers;
    return registers->CR1 == 0U && registers->CR2 == 0U &&
           registers->I2SCFGR == 0U &&
           (NX_STM32F1_SPI_STATUS(registers) & (SPI_SR_BSY | SPI_ERRORS)) == 0U;
}

/** \brief Recover hardware only; failed transfer payloads are never replayed.
 */
static nx_result_t recover(void* context) {
    nx_stm32f1_spi_t* state = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->active) {
        return NX_ERROR_BUSY;
    }
    if (!state->live) {
        return NX_ERROR_STATE;
    }
    if (!reset_spi(state->config) || !select_cs(state, false)) {
        state->fault = true;
        return NX_ERROR_IO;
    }
    state->fault = false;
    return NX_SUCCESS;
}

/** \brief Withdraw the bus before returning its pads and originally added gate.
 */
nx_result_t nx_stm32f1_spi_close(nx_stm32f1_spi_t* state) {
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (state->config == NULL) {
        return NX_SUCCESS;
    }
    if (state->active) {
        return NX_ERROR_BUSY;
    }
    if (state->peripheral_owned) {
        if (!reset_spi(state->config) ||
            ((state->live || state->selected) && !select_cs(state, false))) {
            state->fault = true;
            return NX_ERROR_IO;
        }
        state->live = false;
        state->peripheral_owned = false;
    }
    const nx_result_t restored = nx_stm32f1_pads_restore(&state->pads);
    if (restored != NX_SUCCESS) {
        return restored;
    }
    if (state->gate_added &&
        !nx_stm32f1_write_masked(&RCC->APB1ENR, RCC_APB1ENR_SPI2EN, 0U)) {
        return NX_ERROR_IO;
    }
    *state = (nx_stm32f1_spi_t){0};
    return NX_SUCCESS;
}

/** \brief Wait for one peripheral predicate within the live Core deadline. */
static nx_result_t wait_for(SPI_TypeDef* registers, uint32_t mask,
                            uint32_t value, nx_time_us_t deadline) {
    for (;;) {
        const uint32_t status = NX_STM32F1_SPI_STATUS(registers);
        if ((status & SPI_ERRORS) != 0U) {
            return NX_ERROR_IO;
        }
        if (nx_deadline_expired(deadline, nx_time_now_us())) {
            return NX_ERROR_TIMEOUT;
        }
        if ((status & mask) == value) {
            return NX_SUCCESS;
        }
    }
}

/** \brief Preserve one selection until idle, with independent tail grace. */
static nx_result_t finish(nx_stm32f1_spi_t* state, uint32_t hz, uint8_t bits,
                          nx_time_us_t deadline, nx_result_t result) {
    SPI_TypeDef* registers = state->config->registers;
    const uint32_t idle_mask = SPI_SR_TXE | SPI_SR_BSY;
    if (result == NX_SUCCESS) {
        result = wait_for(registers, idle_mask, SPI_SR_TXE, deadline);
    }
    bool idle = result == NX_SUCCESS;
    if (!idle) {
        nx_time_us_t remaining;
        const uint32_t grace = (uint32_t)bits * 2000000U / hz + 100U;
        idle =
            nx_deadline_finite_after(nx_time_now_us(), grace, &remaining) ==
                NX_SUCCESS &&
            wait_for(registers, idle_mask, SPI_SR_TXE, remaining) == NX_SUCCESS;
    }
    if (!idle || !nx_stm32f1_write_masked(&registers->CR1, SPI_CR1_SPE, 0U)) {
        idle = reset_spi(state->config);
    } else if ((NX_STM32F1_SPI_STATUS(registers) & SPI_SR_RXNE) != 0U) {
        (void)NX_STM32F1_SPI_RECEIVE(registers, bits);
    }
    if (!idle || !select_cs(state, false)) {
        state->fault = true;
        return NX_ERROR_IO;
    }
    return result;
}

/** \brief Reject output or payload aliases of the actual hardware owner. */
static bool independent(const nx_stm32f1_spi_endpoint_t* endpoint,
                        const void* pointer, size_t bytes, size_t alignment) {
    const nx_stm32f1_spi_t* state = endpoint->port;
    return nx_io_dispatch_span(pointer, bytes, alignment) &&
           !nx_io_dispatch_overlap(pointer, bytes, endpoint,
                                   sizeof(*endpoint)) &&
           !nx_io_dispatch_overlap(pointer, bytes, state, sizeof(*state)) &&
           !nx_io_dispatch_overlap(pointer, bytes, state->config,
                                   sizeof(*state->config)) &&
           !nx_io_dispatch_overlap(pointer, bytes, state->config->registers,
                                   sizeof(*state->config->registers)) &&
           !nx_io_dispatch_overlap(pointer, bytes, endpoint->cs_gpio,
                                   sizeof(*endpoint->cs_gpio)) &&
           !nx_io_dispatch_overlap(pointer, bytes, RCC, sizeof(*RCC));
}

/** \brief Select a representable divisor without exceeding endpoint max_hz. */
static nx_result_t control_word(const nx_stm32f1_spi_endpoint_t* endpoint,
                                uint8_t bits, uint32_t* controls,
                                uint32_t* actual) {
    uint32_t div = 2U;
    uint32_t br = 0U;
    const uint32_t clock = endpoint->port->config->clock_hz;
    while (clock > endpoint->max_hz * div && div < 256U) {
        div <<= 1U;
        ++br;
    }
    if (clock > endpoint->max_hz * div) {
        return NX_ERROR_UNSUPPORTED;
    }
    *actual = clock / div;
    *controls = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                (br << SPI_CR1_BR_Pos) |
                ((endpoint->mode & 2U) != 0U ? SPI_CR1_CPOL : 0U) |
                ((endpoint->mode & 1U) != 0U ? SPI_CR1_CPHA : 0U) |
                (bits == 16U ? SPI_CR1_DFF : 0U) |
                (endpoint->lsb_first ? SPI_CR1_LSBFIRST : 0U);
    return NX_SUCCESS;
}

/** \brief Apply one cold shape, then poll paired write/read for each word. */
static nx_result_t exchange(const nx_stm32f1_spi_endpoint_t* endpoint,
                            const nx_spi_segment_t* phases, size_t count,
                            uint8_t bits, nx_time_us_t deadline,
                            size_t* transferred) {
    nx_stm32f1_spi_t* state = endpoint->port;
    uint32_t controls;
    uint32_t actual;
    nx_result_t result = control_word(endpoint, bits, &controls, &actual);
    if (result != NX_SUCCESS) {
        return result;
    }
    if (nx_deadline_expired(deadline, nx_time_now_us())) {
        return NX_ERROR_TIMEOUT;
    }
    state->active = true;
    *transferred = 0U;
    SPI_TypeDef* registers = state->config->registers;
    NX_STM32F1_WRITE(&registers->CR1, controls);
    nx_arch_dsb();
    if (registers->CR1 != controls ||
        !nx_stm32f1_write_masked(&registers->CR1, SPI_CR1_SPE, SPI_CR1_SPE) ||
        !select_cs(state, true)) {
        result = NX_ERROR_IO;
    }
    for (size_t phase = 0U; result == NX_SUCCESS && phase < count; ++phase) {
        const nx_spi_segment_t* step = &phases[phase];
        for (size_t index = 0U; index < step->words; ++index) {
            result = wait_for(registers, SPI_SR_TXE, SPI_SR_TXE, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            uint16_t word = step->dummy;
            if (step->tx != NULL) {
                word = bits == 16U ? ((const uint16_t*)step->tx)[index]
                                   : ((const uint8_t*)step->tx)[index];
            }
            NX_STM32F1_SPI_SEND(registers, word, bits);
            result = wait_for(registers, SPI_SR_RXNE, SPI_SR_RXNE, deadline);
            if (result != NX_SUCCESS) {
                break;
            }
            word = NX_STM32F1_SPI_RECEIVE(registers, bits);
            if (step->rx != NULL) {
                if (bits == 16U) {
                    ((uint16_t*)step->rx)[index] = word;
                } else {
                    ((uint8_t*)step->rx)[index] = (uint8_t)word;
                }
            }
            ++*transferred;
        }
    }
    result = finish(state, actual, bits, deadline, result);
    state->active = false;
    return result;
}

/** \brief Execute only a live statically bound endpoint in one Thread owner. */
static nx_result_t ready(const nx_stm32f1_spi_endpoint_t* endpoint,
                         nx_time_us_t deadline) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (endpoint == NULL || endpoint->port == NULL || endpoint->mode > 3U ||
        endpoint->max_hz == 0U || endpoint->max_hz > 18000000U) {
        return NX_ERROR_INVALID;
    }
    const nx_stm32f1_spi_t* state = endpoint->port;
    if (!state->live || state->fault) {
        return NX_ERROR_STATE;
    }
    if (state->active) {
        return NX_ERROR_BUSY;
    }
    if (endpoint->cs_gpio != state->config->gpio ||
        endpoint->cs_pin != state->config->cs) {
        return NX_ERROR_INVALID;
    }
    if (deadline == NX_DEADLINE_NEVER) {
        return NX_ERROR_UNSUPPORTED;
    }
    return (RCC->APB1ENR & RCC_APB1ENR_SPI2EN) != 0U ? NX_SUCCESS : NX_ERROR_IO;
}

/** \brief Byte-span endpoint permits exactly in-place exchange. */
static nx_result_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                            size_t length, nx_time_us_t deadline,
                            size_t* transferred) {
    const nx_stm32f1_spi_endpoint_t* endpoint = context;
    const nx_result_t accepted = ready(endpoint, deadline);
    if (accepted != NX_SUCCESS) {
        return accepted;
    }
    if (length == 0U || (tx == NULL && rx == NULL) ||
        !independent(endpoint, transferred, sizeof(*transferred),
                     _Alignof(size_t)) ||
        (tx != NULL && (!independent(endpoint, tx, length, 1U) ||
                        nx_io_dispatch_overlap(tx, length, transferred,
                                               sizeof(*transferred)))) ||
        (rx != NULL && (!independent(endpoint, rx, length, 1U) ||
                        nx_io_dispatch_overlap(rx, length, transferred,
                                               sizeof(*transferred)))) ||
        (tx != NULL && rx != NULL && tx != rx &&
         nx_io_dispatch_overlap(tx, length, rx, length))) {
        return NX_ERROR_INVALID;
    }
    if (length > SPI_POLL_MAX_WORDS) {
        return NX_ERROR_UNSUPPORTED;
    }
    const nx_spi_segment_t phase = {tx, rx, length, 0xffU};
    return exchange(endpoint, &phase, 1U, 8U, deadline, transferred);
}

/** \brief Enhanced interface reuses its public segment/span preflight. */
static nx_result_t transaction(void* context, const nx_spi_segment_t* phases,
                               size_t count, uint8_t bits,
                               nx_time_us_t deadline, size_t* transferred) {
    const nx_stm32f1_spi_endpoint_t* endpoint = context;
    const nx_result_t accepted = ready(endpoint, deadline);
    if (accepted != NX_SUCCESS) {
        return accepted;
    }
    if (!independent(endpoint, transferred, sizeof(*transferred),
                     _Alignof(size_t)) ||
        !independent(endpoint, phases, count * sizeof(*phases),
                     _Alignof(nx_spi_segment_t))) {
        return NX_ERROR_INVALID;
    }
    size_t words = 0U;
    const size_t stride = (size_t)(bits / 8U);
    for (size_t index = 0U; index < count; ++index) {
        const nx_spi_segment_t* step = &phases[index];
        if (step->words > SPI_POLL_MAX_WORDS - words) {
            return NX_ERROR_UNSUPPORTED;
        }
        words += step->words;
        const size_t bytes = step->words * stride;
        if ((step->tx != NULL &&
             !independent(endpoint, step->tx, bytes, stride)) ||
            (step->rx != NULL &&
             !independent(endpoint, step->rx, bytes, stride))) {
            return NX_ERROR_INVALID;
        }
    }
    return exchange(endpoint, phases, count, bits, deadline, transferred);
}

/** \brief Prove endpoint and controller are the same static execution owner. */
static bool on_port(const void* context, const nx_spi_port_t* port) {
    const nx_stm32f1_spi_endpoint_t* endpoint = context;
    return port->ops == &nx_stm32f1_spi_ops && port->context == endpoint->port;
}

const nx_spi_ops_t nx_stm32f1_spi_ops = {.recover = recover};

const nx_spi_endpoint_ops_t nx_stm32f1_spi_endpoint_ops = {.transfer = transfer,
                                                           .on_port = on_port};

const nx_spi_transaction_ops_t nx_stm32f1_spi_transaction_ops = {
    .transfer = transaction};
