/**
 * \file            spi_frames.c
 * \brief           IRQ SPI frames with explicit role, line and CRC ownership
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32f407_clock.h"
#include "stm32f407_spi_frames.h"
#include "stm32f4xx_ll_spi.h"

#define NX_SPI_FRAMES_SOURCES                                                  \
    (SPI_CR2_TXEIE | SPI_CR2_RXNEIE | SPI_CR2_ERRIE | SPI_CR2_TXDMAEN |        \
     SPI_CR2_RXDMAEN)
#define NX_SPI_FRAMES_ERRORS                                                   \
    (SPI_SR_MODF | SPI_SR_OVR | SPI_SR_FRE | SPI_SR_CRCERR)

#ifndef NX_SPI_FRAMES_WRITE
#define NX_SPI_FRAMES_WRITE(address, value) (*(address) = (uint32_t)(value))
#endif
#ifndef NX_SPI_FRAMES_TRANSMIT
#define NX_SPI_FRAMES_TRANSMIT(registers, value, bits)                         \
    do {                                                                       \
        if ((bits) == 16U)                                                     \
            LL_SPI_TransmitData16((registers), (uint16_t)(value));             \
        else                                                                   \
            LL_SPI_TransmitData8((registers), (uint8_t)(value));               \
    } while (0)
#endif
#ifndef NX_SPI_FRAMES_RECEIVE
#define NX_SPI_FRAMES_RECEIVE(registers, bits)                                 \
    ((bits) == 16U ? LL_SPI_ReceiveData16(registers)                           \
                   : LL_SPI_ReceiveData8(registers))
#endif
#ifndef NX_SPI_FRAMES_RESET_EVENT
#define NX_SPI_FRAMES_RESET_EVENT(registers) ((void)(registers))
#endif
#ifndef NX_SPI_FRAMES_IRQ_ENABLE
#define NX_SPI_FRAMES_IRQ_ENABLE(irq)  NVIC_EnableIRQ(irq)
#define NX_SPI_FRAMES_IRQ_DISABLE(irq) NVIC_DisableIRQ(irq)
#define NX_SPI_FRAMES_IRQ_CLEAR(irq)   NVIC_ClearPendingIRQ(irq)
#define NX_SPI_FRAMES_IRQ_PRIORITY_SET(irq, priority)                          \
    NVIC_SetPriority((irq), (priority))
#define NX_SPI_FRAMES_IRQ_ENABLED(irq)  NVIC_GetEnableIRQ(irq)
#define NX_SPI_FRAMES_IRQ_PENDING(irq)  NVIC_GetPendingIRQ(irq)
#define NX_SPI_FRAMES_IRQ_ACTIVE(irq)   NVIC_GetActive(irq)
#define NX_SPI_FRAMES_IRQ_PRIORITY(irq) NVIC_GetPriority(irq)
#endif

/** \brief No owner operation changes an incoming interrupt mask. */
static bool thread_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}

/** \brief Check extents before subtraction-based disjointness comparisons. */
static bool extent(const void* pointer, size_t bytes) {
    return pointer != NULL && bytes != 0U &&
           bytes - 1U <= UINTPTR_MAX - (uintptr_t)pointer;
}

/** \brief Compare live spans without forming an overflowing end pointer. */
static bool overlap(const void* first, size_t first_size, const void* second,
                    size_t second_size) {
    if (first == NULL || second == NULL || first_size == 0U ||
        second_size == 0U)
        return false;
    uintptr_t left = (uintptr_t)first;
    uintptr_t right = (uintptr_t)second;
    return left <= right ? right - left < first_size
                         : left - right < second_size;
}

/** \brief Owned control state can never be a caller descriptor or payload. */
static bool control_overlap(const nx_stm32_spi_frames_state_t* state,
                            const void* pointer, size_t bytes) {
    const nx_stm32_spi_frames_config_t* config = state->config;
    if (state->cold_config != NULL) {
        const nx_stm32_spi_frames_cold_config_t* cold = state->cold_config;
        if (overlap(cold, sizeof(*cold), pointer, bytes) ||
            overlap(state->cold_receipt, sizeof(*state->cold_receipt), pointer,
                    bytes) ||
            overlap(cold->pins, cold->pin_count * sizeof(*cold->pins), pointer,
                    bytes))
            return true;
        for (size_t i = 0U; i < cold->pin_count; ++i)
            if (overlap(cold->pins[i].snapshot, sizeof(uint16_t), pointer,
                        bytes))
                return true;
    }
    return overlap(state, sizeof(*state), pointer, bytes) ||
           overlap(config, sizeof(*config), pointer, bytes) ||
           overlap(config->registers, sizeof(*config->registers), pointer,
                   bytes) ||
           overlap(config->rcc, sizeof(*config->rcc), pointer, bytes) ||
           overlap(config->nss_gpio, sizeof(*config->nss_gpio), pointer,
                   bytes) ||
           overlap(config->sck_gpio, sizeof(*config->sck_gpio), pointer,
                   bytes) ||
           overlap(NVIC, sizeof(*NVIC), pointer, bytes) ||
           overlap(SCB, sizeof(*SCB), pointer, bytes) ||
           overlap(&g_nx_stm32_system, sizeof(g_nx_stm32_system), pointer,
                   bytes);
}

/** \brief A prepared whole-MHz clock is not inferred from a family name. */
static uint32_t peripheral_hz(IRQn_Type irq) {
    return irq == SPI1_IRQn                       ? NEXUS_APB2_HZ
           : irq == SPI2_IRQn || irq == SPI3_IRQn ? NEXUS_APB1_HZ
                                                  : 0U;
}

/** \brief A stopped, changed or failed Core clock domain cannot touch loans. */
static bool domain_live(const nx_stm32_spi_frames_config_t* config,
                        uint64_t epoch) {
    const nx_stm32_system_t* system = &g_nx_stm32_system;
    return system->started && system->rcc == config->rcc &&
           system->core_hz == NEXUS_CORE_HZ &&
           system->timer_error == NX_SUCCESS && system->time_epoch != 0U &&
           (epoch == 0U || epoch == system->time_epoch) &&
           nx_stm32_clock_profile_matches(config->rcc, NULL);
}

/** \brief Reset before changing slave receive to master, ES0182 2.12.2. */
static nx_result_t reset_controller(nx_stm32_spi_frames_state_t* state) {
    const nx_stm32_spi_frames_config_t* config = state->config;
    bool first = config->irq == SPI1_IRQn;
    volatile uint32_t* reset =
        first ? &config->rcc->APB2RSTR : &config->rcc->APB1RSTR;
    uint32_t mask = first                      ? RCC_APB2RSTR_SPI1RST
                    : config->irq == SPI2_IRQn ? RCC_APB1RSTR_SPI2RST
                                               : RCC_APB1RSTR_SPI3RST;
    NX_SPI_FRAMES_WRITE(reset, *reset | mask);
    nx_arch_dsb();
    if ((*reset & mask) == 0U)
        return NX_ERROR_IO;
    NX_SPI_FRAMES_RESET_EVENT(config->registers);
    NX_SPI_FRAMES_WRITE(reset, *reset & ~mask);
    nx_arch_dsb();
    if ((*reset & mask) != 0U || (config->registers->CR1 & SPI_CR1_SPE) != 0U ||
        (config->registers->CR2 & NX_SPI_FRAMES_SOURCES) != 0U)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

/** \brief Write only the inactive tuple; CRC starts per accepted request. */
static bool configure_controller(nx_stm32_spi_frames_state_t* state) {
    SPI_TypeDef* registers = state->config->registers;
    uint32_t polynomial = state->config->wire.crc_polynomial;
    if (polynomial == 0U)
        polynomial = 7U;
    NX_SPI_FRAMES_WRITE(&registers->CR1, state->control1);
    NX_SPI_FRAMES_WRITE(&registers->CR2, state->control2);
    NX_SPI_FRAMES_WRITE(&registers->CRCPR, polynomial);
    nx_arch_dsb();
    return registers->CR1 == state->control1 &&
           registers->CR2 == state->control2 && registers->CRCPR == polynomial;
}

/** \brief Pure bounded wire arithmetic shared with cold admission. */
static nx_result_t decode_config(const nx_stm32_spi_frames_config_t* config,
                                 uint16_t* divider_out, uint32_t* baud_out,
                                 uint32_t* tail_out) {
    if (config == NULL || config->registers == NULL || config->rcc == NULL ||
        config->nss_gpio == NULL || config->sck_gpio == NULL ||
        config->nss_mask == 0U || config->sck_mask == 0U ||
        config->nss_mask > 0x8000U || config->sck_mask > 0x8000U ||
        (config->nss_mask & (config->nss_mask - 1U)) != 0U ||
        (config->sck_mask & (config->sck_mask - 1U)) != 0U ||
        config->priority > 15U ||
        (unsigned)config->wire.role > NX_SPI_FRAMES_SLAVE ||
        (unsigned)config->wire.lines > NX_SPI_FRAMES_ONE_WIRE ||
        (unsigned)config->wire.format > NX_SPI_FRAMES_TI ||
        config->wire.mode > 3U ||
        config->clock_hz != peripheral_hz(config->irq) ||
        config->clock_hz == 0U || config->maximum_hz == 0U ||
        config->maximum_hz > config->clock_hz / 2U)
        return NX_ERROR_INVALID;
    const nx_spi_frames_config_t* wire = &config->wire;
    if ((wire->word_bits != 8U && wire->word_bits != 16U) ||
        wire->irq_latency_us == 0U || wire->irq_latency_us > 1000000U ||
        (wire->word_bits == 8U && wire->dummy > UINT8_MAX) ||
        (wire->crc_polynomial != 0U &&
         ((wire->crc_polynomial & 1U) == 0U ||
          (wire->word_bits == 8U && wire->crc_polynomial > UINT8_MAX))) ||
        (wire->format == NX_SPI_FRAMES_TI &&
         (wire->mode != 0U || wire->lsb_first)) ||
        (wire->lines == NX_SPI_FRAMES_ONE_WIRE && wire->crc_polynomial != 0U))
        return NX_ERROR_UNSUPPORTED;
    uint16_t divider = 2U;
    uint32_t baud = 0U;
    while ((uint64_t)config->maximum_hz * divider < config->clock_hz &&
           divider < 256U) {
        divider = (uint16_t)(divider * 2U);
        ++baud;
    }
    if (wire->role == NX_SPI_FRAMES_MASTER &&
        (uint64_t)config->maximum_hz * divider < config->clock_hz)
        return NX_ERROR_UNSUPPORTED;
    uint64_t frame_window =
        (uint64_t)(wire->word_bits - 2U) * divider * UINT64_C(1000000);
    uint32_t tail_delay =
        (uint32_t)(((uint64_t)divider * 1000000U + config->clock_hz - 1U) /
                   config->clock_hz);
    uint64_t latency = wire->irq_latency_us;
    if (wire->lines != NX_SPI_FRAMES_TWO_LINE)
        latency += tail_delay + 3U;
    bool insufficient = latency * config->clock_hz >= frame_window;
    if (wire->role == NX_SPI_FRAMES_SLAVE) {
        latency = wire->irq_latency_us;
        insufficient = latency * config->maximum_hz >=
                       (uint64_t)(wire->word_bits - 2U) * 1000000U;
    }
    if (insufficient)
        return NX_ERROR_UNSUPPORTED;
    *divider_out = divider;
    *baud_out = baud;
    *tail_out = tail_delay;
    return NX_SUCCESS;
}

/** \brief Reject foreign sources and storage before any cold effect. */
nx_result_t
nx_stm32_spi_frames_preflight(const nx_stm32_spi_frames_state_t* state,
                              const nx_stm32_spi_frames_config_t* config) {
    if (!thread_context())
        return NX_ERROR_CONTEXT;
    uint16_t divider;
    uint32_t baud, tail;
    nx_result_t result = decode_config(config, &divider, &baud, &tail);
    if (result != NX_SUCCESS)
        return result;
    if (state == NULL ||
        overlap(state, sizeof(*state), config, sizeof(*config)))
        return NX_ERROR_INVALID;
    if (state->generation == UINT64_MAX)
        return NX_ERROR_STATE;
    if (state->config != NULL || state->initialized || state->active != NULL ||
        (config->registers->CR1 & SPI_CR1_SPE) != 0U ||
        (config->registers->CR2 & NX_SPI_FRAMES_SOURCES) != 0U ||
        NX_SPI_FRAMES_IRQ_ENABLED(config->irq) != 0U ||
        NX_SPI_FRAMES_IRQ_PENDING(config->irq) != 0U ||
        NX_SPI_FRAMES_IRQ_ACTIVE(config->irq) != 0U)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}

/** \brief Validate every mode and rate before a configuration is retained. */
nx_result_t
nx_stm32_spi_frames_initialize(nx_stm32_spi_frames_state_t* state,
                               const nx_stm32_spi_frames_config_t* config) {
    if (!thread_context())
        return NX_ERROR_CONTEXT;
    nx_result_t checked = nx_stm32_spi_frames_preflight(state, config);
    if (checked != NX_SUCCESS)
        return checked;
    if (!domain_live(config, 0U))
        return NX_ERROR_STATE;
    uint16_t divider;
    uint32_t baud, tail_delay;
    (void)decode_config(config, &divider, &baud, &tail_delay);
    const nx_spi_frames_config_t* wire = &config->wire;
    ++state->generation;
    state->time_epoch = g_nx_stm32_system.time_epoch;
    state->config = config;
    state->initialized = true;
    state->closing = true;
    state->divider = divider;
    state->receive_tail_delay_us = tail_delay;
    state->control1 = (baud << 3U) |
                      (wire->word_bits == 16U ? SPI_CR1_DFF : 0U) |
                      (wire->lsb_first ? SPI_CR1_LSBFIRST : 0U) |
                      (wire->role == NX_SPI_FRAMES_MASTER ? SPI_CR1_MSTR : 0U) |
                      ((wire->mode & 1U) != 0U ? SPI_CR1_CPHA : 0U) |
                      ((wire->mode & 2U) != 0U ? SPI_CR1_CPOL : 0U);
    if (wire->format == NX_SPI_FRAMES_TI)
        state->control1 |= SPI_CR1_CPHA;
    state->control2 = (wire->role == NX_SPI_FRAMES_MASTER ? SPI_CR2_SSOE : 0U) |
                      (wire->format == NX_SPI_FRAMES_TI ? SPI_CR2_FRF : 0U);
    nx_result_t result = reset_controller(state);
    if (result != NX_SUCCESS || !configure_controller(state))
        return NX_ERROR_IO;
    NX_SPI_FRAMES_IRQ_PRIORITY_SET(config->irq, config->priority);
    NX_SPI_FRAMES_IRQ_CLEAR(config->irq);
    NX_SPI_FRAMES_IRQ_ENABLE(config->irq);
    nx_arch_dsb();
    if (NX_SPI_FRAMES_IRQ_PRIORITY(config->irq) != config->priority ||
        NX_SPI_FRAMES_IRQ_ENABLED(config->irq) == 0U ||
        NX_SPI_FRAMES_IRQ_PENDING(config->irq) != 0U ||
        NX_SPI_FRAMES_IRQ_ACTIVE(config->irq) != 0U)
        return NX_ERROR_IO;
    state->closing = false;
    return NX_SUCCESS;
}

/** \brief The current request selects direction only while SPE is clear. */
static bool receive_only(const nx_stm32_spi_frames_state_t* state) {
    return state->config->wire.lines == NX_SPI_FRAMES_RECEIVE_ONLY ||
           (state->config->wire.lines == NX_SPI_FRAMES_ONE_WIRE &&
            state->active->tx == NULL);
}

/** \brief Full two-line operation receives even when the caller discards. */
static bool receives(const nx_stm32_spi_frames_state_t* state) {
    return state->config->wire.lines != NX_SPI_FRAMES_ONE_WIRE ||
           state->active->rx != NULL;
}

/** \brief Latch the first terminal fact and stop further caller reads. */
static void begin_drain(nx_stm32_spi_frames_state_t* state,
                        nx_result_t terminal) {
    if (!state->terminal_latched) {
        state->terminal = terminal;
        state->terminal_latched = true;
        state->drain_deadline =
            !domain_live(state->config, state->time_epoch)
                ? 0U
                : nx_deadline_after(
                      nx_time_now_us(),
                      100U + (uint64_t)state->config->wire.word_bits *
                                 state->receive_tail_delay_us * 2U);
    }
    if (!domain_live(state->config, state->time_epoch))
        (void)nx_request_transition(&state->active->base,
                                    NX_REQUEST_QUARANTINED);
    else if (nx_request_state(&state->active->base) != NX_REQUEST_QUARANTINED)
        (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    SPI_TypeDef* registers = state->config->registers;
    NX_SPI_FRAMES_WRITE(&registers->CR2,
                        registers->CR2 & ~(uint32_t)NX_SPI_FRAMES_SOURCES);
}

/** \brief Withdraw master receive clocks inside the RM0090 last-frame window.
 * \note The declared latency bound is conditional physical integration input.
 *       A frozen/backwards timer fails boundedly; no loop invents completion.
 */
static bool receive_tail(nx_stm32_spi_frames_state_t* state) {
    if (!domain_live(state->config, state->time_epoch)) {
        begin_drain(state, NX_ERROR_IO);
        return false;
    }
    nx_time_us_t start = nx_time_now_us();
    nx_time_us_t target =
        nx_deadline_after(start, state->receive_tail_delay_us + 1U);
    nx_time_us_t previous = start;
    for (unsigned spin = 0U; spin < 10000U; ++spin) {
        if (!domain_live(state->config, state->time_epoch))
            break;
        nx_time_us_t now = nx_time_now_us();
        if (now < previous)
            break;
        previous = now;
        if (now < target)
            continue;
        if (now - start > state->receive_tail_delay_us + 2U)
            break;
        SPI_TypeDef* registers = state->config->registers;
        NX_SPI_FRAMES_WRITE(&registers->CR1,
                            registers->CR1 & ~(uint32_t)SPI_CR1_SPE);
        nx_arch_dsb();
        state->receive_tail = (registers->CR1 & SPI_CR1_SPE) == 0U;
        if (state->receive_tail)
            return true;
        break;
    }
    begin_drain(state, NX_ERROR_IO);
    (void)nx_request_transition(&state->active->base, NX_REQUEST_QUARANTINED);
    return false;
}

/** \brief Validate metadata and complete word spans before any write. */
static nx_result_t storage_valid(nx_stm32_spi_frames_state_t* state,
                                 nx_spi_frames_request_t* request,
                                 const void* tx, void* rx, size_t words) {
    if (state == NULL || state->config == NULL || request == NULL ||
        (uintptr_t)request % _Alignof(nx_spi_frames_request_t) != 0U ||
        !extent(request, sizeof(*request)))
        return NX_ERROR_INVALID;
    size_t unit = state->config->wire.word_bits / 8U;
    if (words == 0U || words > 65535U || (tx == NULL && rx == NULL))
        return NX_ERROR_INVALID;
    size_t bytes = words * unit;
    if ((tx != NULL && (!extent(tx, bytes) || (uintptr_t)tx % unit != 0U)) ||
        (rx != NULL && (!extent(rx, bytes) || (uintptr_t)rx % unit != 0U)) ||
        overlap(tx, bytes, rx, bytes) ||
        overlap(request, sizeof(*request), tx, bytes) ||
        overlap(request, sizeof(*request), rx, bytes) ||
        control_overlap(state, request, sizeof(*request)) ||
        control_overlap(state, tx, tx == NULL ? 0U : bytes) ||
        control_overlap(state, rx, rx == NULL ? 0U : bytes))
        return NX_ERROR_INVALID;
    const nx_spi_frames_config_t* wire = &state->config->wire;
    if ((wire->lines == NX_SPI_FRAMES_RECEIVE_ONLY &&
         (tx != NULL || rx == NULL)) ||
        (wire->lines == NX_SPI_FRAMES_ONE_WIRE && tx != NULL && rx != NULL) ||
        (wire->crc_polynomial != 0U &&
         wire->lines == NX_SPI_FRAMES_RECEIVE_ONLY && words < 2U))
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}

/** \brief Port-aware preparation binds a deadline to one owner generation. */
static nx_result_t prepare(void* context, nx_spi_frames_request_t* request,
                           const void* tx, void* rx, size_t words,
                           nx_time_us_t deadline) {
    if (!thread_context())
        return NX_ERROR_CONTEXT;
    nx_stm32_spi_frames_state_t* state = context;
    nx_result_t result = storage_valid(state, request, tx, rx, words);
    if (result != NX_SUCCESS)
        return result;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!state->initialized || state->closing || state->fault ||
        !domain_live(state->config, state->time_epoch)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    if (state->active != NULL) {
        const nx_spi_frames_request_t* active = state->active;
        size_t bytes = active->words * (state->config->wire.word_bits / 8U);
        if (request == active ||
            overlap(request, sizeof(*request), active, sizeof(*active)) ||
            overlap(request, sizeof(*request), active->tx,
                    active->tx == NULL ? 0U : bytes) ||
            overlap(request, sizeof(*request), active->rx,
                    active->rx == NULL ? 0U : bytes)) {
            nx_arch_irq_restore(saved);
            return NX_ERROR_STATE;
        }
    }
    result = nx_request_prepare(&request->base, deadline);
    if (result == NX_SUCCESS) {
        request->owner = state;
        request->generation = state->generation;
        request->time_epoch = state->time_epoch;
        request->tx = tx;
        request->rx = rx;
        request->words = words;
    }
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief Reject the complete descriptor before the unique admission. */
static nx_result_t validate_request(nx_stm32_spi_frames_state_t* state,
                                    nx_spi_frames_request_t* request) {
    if (request == NULL ||
        (uintptr_t)request % _Alignof(nx_spi_frames_request_t) != 0U ||
        !extent(request, sizeof(*request)))
        return NX_ERROR_INVALID;
    nx_result_t result =
        storage_valid(state, request, request->tx, request->rx, request->words);
    if (result != NX_SUCCESS)
        return result;
    if (!state->initialized || state->closing || state->fault ||
        !domain_live(state->config, state->time_epoch) ||
        request->owner != state || request->generation != state->generation ||
        request->time_epoch != state->time_epoch)
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    if (nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    if (nx_deadline_expired(request->base.deadline, nx_time_now_us()))
        return NX_ERROR_TIMEOUT;
    SPI_TypeDef* registers = state->config->registers;
    if ((registers->CR1 & SPI_CR1_SPE) != 0U ||
        (registers->CR2 & NX_SPI_FRAMES_SOURCES) != 0U)
        return NX_ERROR_BUSY;
    if (state->config->wire.role == NX_SPI_FRAMES_SLAVE) {
        bool nss =
            (state->config->nss_gpio->IDR & state->config->nss_mask) != 0U;
        if (nss != (state->config->wire.format == NX_SPI_FRAMES_MOTOROLA))
            return NX_ERROR_BUSY;
        bool clock =
            (state->config->sck_gpio->IDR & state->config->sck_mask) != 0U;
        if (clock != ((state->control1 & SPI_CR1_CPOL) != 0U))
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

/** \brief Inline data and CRCNEXT writes have no intervening firmware call. */
static inline bool transmit(nx_stm32_spi_frames_state_t* state) {
    if (!domain_live(state->config, state->time_epoch)) {
        begin_drain(state, NX_ERROR_IO);
        return false;
    }
    nx_spi_frames_request_t* request = state->active;
    uint8_t bits = state->config->wire.word_bits;
    uint16_t value = state->config->wire.dummy;
    if (request->tx != NULL)
        value = bits == 16U ? ((const uint16_t*)request->tx)[state->tx_position]
                            : ((const uint8_t*)request->tx)[state->tx_position];
    bool last = state->tx_position + 1U == request->words;
    ++state->tx_position;
    SPI_TypeDef* registers = state->config->registers;
    NX_SPI_FRAMES_TRANSMIT(registers, value, bits);
    if (last && state->config->wire.crc_polynomial != 0U)
        NX_SPI_FRAMES_WRITE(&registers->CR1, registers->CR1 | SPI_CR1_CRCNEXT);
    if (last &&
        !(state->config->wire.role == NX_SPI_FRAMES_SLAVE && !receives(state)))
        NX_SPI_FRAMES_WRITE(&registers->CR2,
                            registers->CR2 & ~(uint32_t)SPI_CR2_TXEIE);
    return true;
}

/** \brief Start hardware only after ownership transfers exactly once. */
static nx_result_t submit(void* context, nx_spi_frames_request_t* request) {
    if (!thread_context())
        return NX_ERROR_CONTEXT;
    nx_stm32_spi_frames_state_t* state = context;
    nx_result_t checked = validate_request(state, request);
    if (checked != NX_SUCCESS)
        return checked;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!domain_live(state->config, state->time_epoch)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_STATE;
    }
    checked = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (checked != NX_SUCCESS) {
        nx_arch_irq_restore(saved);
        return checked;
    }
    state->active = request;
    state->tx_position = state->rx_position = state->completed = 0U;
    state->terminal = NX_SUCCESS;
    state->terminal_latched = state->receive_tail = state->crc_received = false;
    state->slave_last_disabled = false;
    bool rx_only = receive_only(state);
    uint32_t control = state->control1;
    if (state->config->wire.lines == NX_SPI_FRAMES_ONE_WIRE)
        control |= SPI_CR1_BIDIMODE | (rx_only ? 0U : SPI_CR1_BIDIOE);
    else if (rx_only)
        control |= SPI_CR1_RXONLY;
    SPI_TypeDef* registers = state->config->registers;
    if (reset_controller(state) != NX_SUCCESS || !configure_controller(state)) {
        begin_drain(state, NX_ERROR_IO);
        nx_arch_irq_restore(saved);
        return NX_SUCCESS;
    }
    NX_SPI_FRAMES_WRITE(&registers->CR1, control);
    if (state->config->wire.crc_polynomial != 0U)
        control |= SPI_CR1_CRCEN;
    NX_SPI_FRAMES_WRITE(&registers->CR1, control);
    nx_arch_dsb();
    if (registers->CR1 != control) {
        begin_drain(state, NX_ERROR_IO);
        nx_arch_irq_restore(saved);
        return NX_SUCCESS;
    }
    NX_SPI_FRAMES_WRITE(&registers->CR1, control | SPI_CR1_SPE);
    nx_arch_dsb();
    if (registers->CR1 != (control | SPI_CR1_SPE)) {
        begin_drain(state, NX_ERROR_IO);
        nx_arch_irq_restore(saved);
        return NX_SUCCESS;
    }
    uint32_t sources = SPI_CR2_ERRIE | (receives(state) ? SPI_CR2_RXNEIE : 0U);
    if (!rx_only) {
        if ((registers->SR & SPI_SR_TXE) == 0U) {
            begin_drain(state, NX_ERROR_IO);
            nx_arch_irq_restore(saved);
            return NX_SUCCESS;
        }
        if (!transmit(state)) {
            nx_arch_irq_restore(saved);
            return NX_SUCCESS;
        }
        if (state->tx_position < request->words ||
            state->config->wire.role == NX_SPI_FRAMES_SLAVE)
            sources |= SPI_CR2_TXEIE;
    }
    NX_SPI_FRAMES_WRITE(&registers->CR2, state->control2 | sources);
    nx_arch_dsb();
    if (registers->CR2 != (state->control2 | sources))
        begin_drain(state, NX_ERROR_IO);
    else if (rx_only && state->config->wire.role == NX_SPI_FRAMES_MASTER &&
             request->words == 1U)
        (void)receive_tail(state);
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief Bound each IRQ to one received word and one transmitted word. */
void nx_stm32_spi_frames_irq(nx_stm32_spi_frames_state_t* state) {
    if (state == NULL || !state->initialized || state->active == NULL)
        return;
    if (!domain_live(state->config, state->time_epoch)) {
        begin_drain(state, NX_ERROR_IO);
        return;
    }
    SPI_TypeDef* registers = state->config->registers;
    nx_spi_frames_request_t* request = state->active;
    uint32_t status = registers->SR;
    if ((status & NX_SPI_FRAMES_ERRORS) != 0U) {
        begin_drain(state, NX_ERROR_IO);
        return;
    }
    if (nx_request_state(&request->base) != NX_REQUEST_ACTIVE)
        return;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if ((status & SPI_SR_RXNE) != 0U &&
        (registers->CR2 & SPI_CR2_RXNEIE) != 0U) {
        uint16_t value =
            NX_SPI_FRAMES_RECEIVE(registers, state->config->wire.word_bits);
        if (!domain_live(state->config, state->time_epoch)) {
            begin_drain(state, NX_ERROR_IO);
            nx_arch_irq_restore(saved);
            return;
        }
        if (state->rx_position < request->words) {
            if (request->rx != NULL) {
                if (state->config->wire.word_bits == 16U)
                    ((uint16_t*)request->rx)[state->rx_position] = value;
                else
                    ((uint8_t*)request->rx)[state->rx_position] =
                        (uint8_t)value;
            }
            ++state->rx_position;
            state->completed = state->rx_position;
            if (receive_only(state) &&
                state->config->wire.crc_polynomial != 0U &&
                state->rx_position + 1U == request->words)
                NX_SPI_FRAMES_WRITE(&registers->CR1,
                                    registers->CR1 | SPI_CR1_CRCNEXT);
        } else if (state->config->wire.crc_polynomial != 0U &&
                   !state->crc_received) {
            state->crc_received = true;
            ++state->rx_position;
        } else {
            begin_drain(state, NX_ERROR_OVERFLOW);
        }
        size_t total = request->words +
                       (state->config->wire.crc_polynomial != 0U ? 1U : 0U);
        if (state->rx_position == total) {
            NX_SPI_FRAMES_WRITE(&registers->CR2,
                                registers->CR2 & ~(uint32_t)SPI_CR2_RXNEIE);
        } else if (receive_only(state) &&
                   state->config->wire.role == NX_SPI_FRAMES_MASTER &&
                   state->rx_position + 1U == total) {
            (void)receive_tail(state);
        }
    }
    if (nx_request_state(&request->base) == NX_REQUEST_ACTIVE &&
        (status & SPI_SR_TXE) != 0U && (registers->CR2 & SPI_CR2_TXEIE) != 0U) {
        if (state->tx_position < request->words) {
            (void)transmit(state);
        } else {
            NX_SPI_FRAMES_WRITE(&registers->CR2,
                                registers->CR2 & ~(uint32_t)SPI_CR2_TXEIE);
            if (!receives(state) &&
                state->config->wire.role == NX_SPI_FRAMES_SLAVE) {
                /* ES0182 2.12.5: disable while the last frame shifts. */
                NX_SPI_FRAMES_WRITE(&registers->CR1,
                                    registers->CR1 & ~(uint32_t)SPI_CR1_SPE);
                nx_arch_dsb();
                state->slave_last_disabled =
                    (registers->CR1 & SPI_CR1_SPE) == 0U;
                if (!state->slave_last_disabled)
                    begin_drain(state, NX_ERROR_IO);
            }
        }
    }
    nx_arch_irq_restore(saved);
}

/** \brief Cancellation preserves the first observed terminal reason. */
static nx_result_t cancel(void* context, nx_spi_frames_request_t* request) {
    if (!thread_context())
        return NX_ERROR_CONTEXT;
    nx_stm32_spi_frames_state_t* state = context;
    if (state == NULL || request == NULL)
        return NX_ERROR_INVALID;
    if (state->active != request)
        return nx_request_state(&request->base) == NX_REQUEST_SETTLED
                   ? NX_SUCCESS
                   : NX_ERROR_STATE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!state->terminal_latched)
        begin_drain(state, domain_live(state->config, state->time_epoch)
                               ? NX_ERROR_CANCELLED
                               : NX_ERROR_IO);
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief End-of-wire evidence is specific to role and line direction. */
static bool wire_idle(nx_stm32_spi_frames_state_t* state, uint32_t status) {
    const nx_spi_frames_config_t* wire = &state->config->wire;
    if (wire->role == NX_SPI_FRAMES_MASTER) {
        if (receive_only(state))
            return (state->config->registers->CR1 & SPI_CR1_SPE) == 0U;
        return (status & (SPI_SR_TXE | SPI_SR_BSY)) == SPI_SR_TXE;
    }
    if (wire->format == NX_SPI_FRAMES_MOTOROLA) {
        if ((state->config->nss_gpio->IDR & state->config->nss_mask) == 0U)
            return false;
        /* NSS after the last RXNE proves the frame even when BSY is stuck. */
        return receives(state) ||
               (state->slave_last_disabled && (status & SPI_SR_BSY) == 0U);
    }
    return receives(state) ||
           (state->slave_last_disabled && (status & SPI_SR_BSY) == 0U);
}

/** \brief Mask and read back every source before final request access. */
static bool withdraw(nx_stm32_spi_frames_state_t* state) {
    SPI_TypeDef* registers = state->config->registers;
    NX_SPI_FRAMES_WRITE(&registers->CR2, state->control2);
    nx_arch_dsb();
    if ((registers->CR2 & NX_SPI_FRAMES_SOURCES) != 0U)
        return false;
    NX_SPI_FRAMES_WRITE(&registers->CR1,
                        registers->CR1 & ~(uint32_t)SPI_CR1_SPE);
    nx_arch_dsb();
    if ((registers->CR1 & SPI_CR1_SPE) != 0U)
        return false;
    NX_SPI_FRAMES_IRQ_CLEAR(state->config->irq);
    nx_arch_dsb();
    return NX_SPI_FRAMES_IRQ_PENDING(state->config->irq) == 0U &&
           NX_SPI_FRAMES_IRQ_ACTIVE(state->config->irq) == 0U;
}

/** \brief Only the Thread owner can settle a detached descriptor. */
static void service(void* context) {
    nx_stm32_spi_frames_state_t* state = context;
    if (!thread_context() || state == NULL || !state->initialized ||
        state->active == NULL)
        return;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_spi_frames_request_t* request = state->active;
    SPI_TypeDef* registers = state->config->registers;
    bool live = domain_live(state->config, state->time_epoch);
    if (!live)
        begin_drain(state, NX_ERROR_IO);
    uint32_t status = registers->SR;
    size_t expected =
        request->words + (state->config->wire.crc_polynomial != 0U ? 1U : 0U);
    if ((status & NX_SPI_FRAMES_ERRORS) != 0U ||
        (receives(state) && state->rx_position == expected &&
         (status & SPI_SR_RXNE) != 0U))
        begin_drain(state, NX_ERROR_IO);
    if (live && !state->terminal_latched &&
        nx_deadline_expired(request->base.deadline, nx_time_now_us()))
        begin_drain(state, NX_ERROR_TIMEOUT);
    bool complete =
        receives(state)
            ? state->rx_position ==
                  request->words +
                      (state->config->wire.crc_polynomial != 0U ? 1U : 0U)
            : state->tx_position == request->words;
    bool draining = state->terminal_latched;
    bool idle = wire_idle(state, status);
    bool reset = false;
    if (draining && (!idle || receive_only(state) || !live) &&
        (nx_request_state(&request->base) == NX_REQUEST_QUARANTINED ||
         nx_deadline_expired(state->drain_deadline, nx_time_now_us()))) {
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        reset = reset_controller(state) == NX_SUCCESS;
        state->fault = true;
        idle = reset;
    }
    if ((!complete && !draining) || !idle) {
        nx_arch_irq_restore(saved);
        return;
    }
    if (!withdraw(state)) {
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        state->fault = true;
        nx_arch_irq_restore(saved);
        return;
    }
    status = registers->SR;
    if ((!domain_live(state->config, state->time_epoch) ||
         (status & NX_SPI_FRAMES_ERRORS) != 0U ||
         (receives(state) && state->rx_position == expected &&
          (status & SPI_SR_RXNE) != 0U)) &&
        !state->terminal_latched) {
        state->terminal = NX_ERROR_IO;
        state->terminal_latched = true;
    }
    size_t completed = state->completed;
    if (!receives(state) && !draining)
        completed = request->words;
    nx_result_t result = state->terminal;
    state->active = NULL;
    nx_request_settle(&request->base, result, completed);
    nx_arch_irq_restore(saved);
}

/** \brief Stop admission without revoking any in-flight descriptor. */
static nx_result_t stop(void* context) {
    if (!thread_context())
        return NX_ERROR_CONTEXT;
    nx_stm32_spi_frames_state_t* state = context;
    if (state == NULL)
        return NX_ERROR_INVALID;
    if (!state->initialized)
        return NX_SUCCESS;
    state->closing = true;
    if (state->active != NULL) {
        if (!state->terminal_latched)
            (void)cancel(state, state->active);
        return NX_ERROR_BUSY;
    }
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    if (!withdraw(state)) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    NX_SPI_FRAMES_IRQ_DISABLE(state->config->irq);
    NX_SPI_FRAMES_IRQ_CLEAR(state->config->irq);
    nx_arch_dsb();
    if (NX_SPI_FRAMES_IRQ_ENABLED(state->config->irq) != 0U ||
        NX_SPI_FRAMES_IRQ_PENDING(state->config->irq) != 0U ||
        NX_SPI_FRAMES_IRQ_ACTIVE(state->config->irq) != 0U) {
        nx_arch_irq_restore(saved);
        return NX_ERROR_BUSY;
    }
    state->initialized = false;
    nx_arch_irq_restore(saved);
    return NX_SUCCESS;
}

/** \brief An idle recovery resets hardware but never replays a request. */
static nx_result_t recover(void* context) {
    if (!thread_context())
        return NX_ERROR_CONTEXT;
    nx_stm32_spi_frames_state_t* state = context;
    if (state == NULL || state->config == NULL || !state->initialized ||
        state->closing)
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    if (!domain_live(state->config, state->time_epoch))
        return NX_ERROR_STATE;
    nx_arch_irq_state_t saved = nx_arch_irq_save();
    nx_result_t result = reset_controller(state);
    if (result == NX_SUCCESS && !configure_controller(state))
        result = NX_ERROR_IO;
    state->fault = result != NX_SUCCESS;
    nx_arch_irq_restore(saved);
    return result;
}

/** \brief One immutable table shared by independently owned controllers. */
const nx_spi_frames_ops_t nx_stm32_spi_frames_ops = {
    .prepare = prepare,
    .submit = submit,
    .cancel = cancel,
    .service = service,
    .stop = stop,
    .recover = recover,
};
