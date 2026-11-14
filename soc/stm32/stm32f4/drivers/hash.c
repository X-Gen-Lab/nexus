/**
 * \file            hash.c
 * \brief           Byte-safe bounded HASH/HMAC protocol over official HAL init
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "crypto_validation.h"
#include "nexus/arch/arch.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
#include "stm32f4_hash.h"
#include <string.h>

extern nx_stm32_system_t g_nx_stm32_system;
enum { PLAIN, KEY_FIRST, MESSAGE, KEY_LAST };

/** \brief The serialized owner does not run in masked task or unprivileged
 * mode. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Check the whole static owner before dereferencing it. */
static bool valid_state(const nx_stm32f4_hash_state_t* state) {
    return nx_crypto_span_valid(state, sizeof(*state),
                                _Alignof(nx_stm32f4_hash_state_t), false);
}
/** \brief Public spans exclude complete owners and fixed hardware resources. */
static bool isolated(const nx_stm32f4_hash_state_t* state, const void* pointer,
                     size_t bytes) {
#if NEXUS_STM32_HASH_DMA
    if (state->config != NULL && state->config->dma != NULL) {
        const nx_stm32f4_hash_dma_config_t* dma = state->config->dma;
        if (nx_crypto_overlap(pointer, bytes, (const void*)(uintptr_t)DMA2_BASE,
                              0xD0U) ||
            nx_crypto_overlap(pointer, bytes, NVIC, sizeof(*NVIC)) ||
            nx_crypto_overlap(pointer, bytes, dma, sizeof(*dma)) ||
            nx_crypto_overlap(pointer, bytes, dma->state,
                              sizeof(*dma->state)) ||
            nx_crypto_overlap(pointer, bytes, dma->regions,
                              dma->region_count * sizeof(*dma->regions)))
            return false;
    }
#endif
    return !nx_crypto_overlap(pointer, bytes, state, sizeof(*state)) &&
           (state->config == NULL ||
            !nx_crypto_overlap(pointer, bytes, state->config,
                               sizeof(*state->config))) &&
           (state->wake == NULL ||
            !nx_crypto_overlap(pointer, bytes, state->wake,
                               sizeof(*state->wake))) &&
           !nx_crypto_overlap(pointer, bytes, HASH, sizeof(*HASH)) &&
           !nx_crypto_overlap(pointer, bytes, RCC, sizeof(*RCC)) &&
           !nx_crypto_overlap(pointer, bytes, &g_nx_stm32_system,
                              sizeof(g_nx_stm32_system));
}
/** \brief A selected live clock and time domain are required for every phase.
 */
static bool clock_live(void) {
    const nx_stm32_system_t* system = &g_nx_stm32_system;
    return system->started && system->time_epoch != 0U && system->rcc == RCC &&
           system->flash == FLASH && system->timer_error == NX_SUCCESS &&
           system->core_hz == NEXUS_CORE_HZ &&
           nx_stm32_clock_profile_matches(RCC, FLASH);
}
/** \brief Observe shared vector urgency without mutating its lease. */
static nx_irq_source_t irq_source(void) {
    nx_irq_source_t source = {
        .irq_number = HASH_RNG_IRQn,
        .priority = (uint8_t)(NVIC->IPR[(unsigned)HASH_RNG_IRQn] >>
                              (8U - __NVIC_PRIO_BITS)),
        .priority_group = (uint8_t)((SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) >>
                                    SCB_AIRCR_PRIGROUP_Pos)};
    return source;
}
/** \brief Never adopt an existing engine gate or erase another owner's context.
 */
nx_result_t nx_stm32f4_hash_admit(const nx_stm32f4_hash_state_t* state,
                                  const nx_stm32f4_hash_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state) ||
        !nx_crypto_span_valid(config, sizeof(*config),
                              _Alignof(nx_stm32f4_hash_config_t), false) ||
        nx_crypto_overlap(state, sizeof(*state), config, sizeof(*config)) ||
        nx_crypto_overlap(config, sizeof(*config), HASH, sizeof(*HASH)) ||
        nx_crypto_overlap(config, sizeof(*config), RCC, sizeof(*RCC)))
        return NX_ERROR_INVALID;
    nx_irq_source_t source = irq_source();
    nx_result_t result =
        nx_irq_wake_validate(NULL, &config->irq_policy, &source);
    if (result != NX_SUCCESS)
        return result;
    if (state->config != NULL || state->active != NULL || state->running ||
        state->gate_owned || state->reset_owned ||
        (RCC->AHB2ENR & RCC_AHB2ENR_HASHEN) != 0U ||
        (RCC->AHB2RSTR & RCC_AHB2RSTR_HASHRST) != 0U)
        return NX_ERROR_BUSY;
#if NEXUS_STM32_HASH_DMA
    return nx_stm32f4_hash_dma_admit(state, config);
#else
    if (config->dma != NULL)
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
#endif
}
/** \brief Retain the exact responsibility before the first gate write. */
nx_result_t nx_stm32f4_hash_start(nx_stm32f4_hash_state_t* state,
                                  const nx_stm32f4_hash_config_t* config) {
    nx_result_t result = nx_stm32f4_hash_admit(state, config);
    if (result != NX_SUCCESS)
        return result;
    if (!clock_live())
        return NX_ERROR_STATE;
    state->config = config;
    state->epoch = g_nx_stm32_system.time_epoch;
    state->gate_owned = true;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    SET_BIT(RCC->AHB2ENR, RCC_AHB2ENR_HASHEN);
    nx_arch_dsb();
    nx_arch_irq_restore(mask);
    if ((RCC->AHB2ENR & RCC_AHB2ENR_HASHEN) == 0U)
        return NX_ERROR_IO;
    if ((HASH->CR & (HASH_CR_DMAE | HASH_CR_NBW | HASH_CR_DINNE)) != 0U ||
        HASH->IMR != 0U || (HASH->SR & (HASH_SR_BUSY | HASH_SR_DMAS)) != 0U)
        return NX_ERROR_IO;
#if NEXUS_STM32_HASH_DMA
    result = nx_stm32f4_hash_dma_prepare(state);
    if (result != NX_SUCCESS)
        return result;
#endif
    state->running = true;
    state->faulted = false;
    return NX_SUCCESS;
}
/** \brief Latch once and silence only HASH, preserving the shared RNG
 * publisher. */
static void terminal(nx_stm32f4_hash_state_t* state, nx_result_t result) {
    if (!state->terminal_pending) {
        state->terminal = result;
        state->terminal_pending = true;
        (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    }
    WRITE_REG(HASH->IMR, 0U);
    nx_arch_dsb();
    if (HASH->IMR != 0U) {
        /* Dedicated HASH reset can quiet this source without disabling RNG.
         * Keep reset ownership and every borrower for task recovery. */
        state->faulted = true;
        state->reset_owned = true;
        SET_BIT(RCC->AHB2RSTR, RCC_AHB2RSTR_HASHRST);
        nx_arch_dsb();
    }
}
/** \brief Exclusive reset withdraws the internal FIFO, key and context state.
 */
static bool reset_engine(nx_stm32f4_hash_state_t* state) {
    if ((RCC->AHB2ENR & RCC_AHB2ENR_HASHEN) == 0U)
        return false;
#if NEXUS_STM32_HASH_DMA
    if (nx_stm32f4_hash_dma_withdraw(state) != NX_SUCCESS)
        return false;
#endif
    WRITE_REG(HASH->IMR, 0U);
    CLEAR_BIT(HASH->CR, HASH_CR_DMAE);
    state->reset_owned = true;
    SET_BIT(RCC->AHB2RSTR, RCC_AHB2RSTR_HASHRST);
    nx_arch_dsb();
    if ((RCC->AHB2RSTR & RCC_AHB2RSTR_HASHRST) == 0U)
        return false;
    CLEAR_BIT(RCC->AHB2RSTR, RCC_AHB2RSTR_HASHRST);
    nx_arch_dsb();
    if ((RCC->AHB2RSTR & RCC_AHB2RSTR_HASHRST) != 0U || HASH->CR != 0U ||
        HASH->IMR != 0U || (HASH->SR & (HASH_SR_BUSY | HASH_SR_DMAS)) != 0U)
        return false;
    state->reset_owned = false;
    return true;
}
/** \brief Query outputs cannot overwrite an active request or borrowed span. */
static bool result_isolated(const nx_stm32f4_hash_state_t* state,
                            const void* output, size_t bytes) {
    if (!isolated(state, output, bytes))
        return false;
    if (state->active == NULL)
        return true;
    const nx_hash_operation_t* operation = &state->active->operation;
    return !nx_crypto_overlap(output, bytes, state->active,
                              sizeof(*state->active)) &&
           !nx_crypto_overlap(output, bytes, operation->input,
                              operation->bytes) &&
           !nx_crypto_overlap(output, bytes, operation->key,
                              operation->key_bytes) &&
           !nx_crypto_overlap(output, bytes, operation->digest,
                              operation->digest_capacity);
}
/** \brief Report actual byte handling, not the SDK's padded word assumptions.
 */
static nx_result_t query(void* context, nx_hash_algorithm_t algorithm,
                         nx_hash_constraints_t* output) {
    nx_stm32f4_hash_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state) ||
        !nx_crypto_span_valid(output, sizeof(*output),
                              _Alignof(nx_hash_constraints_t), false) ||
        !result_isolated(state, output, sizeof(*output)) ||
        (unsigned)algorithm > (unsigned)NX_HASH_HMAC_MD5)
        return NX_ERROR_INVALID;
    nx_hash_constraints_t value = {
        .input_alignment = 1U,
        .key_alignment = 1U,
        .digest_alignment = 1U,
        .maximum_bytes = UINT32_MAX,
        .maximum_key_bytes = UINT32_MAX,
        .digest_bytes =
            algorithm == NX_HASH_SHA1 || algorithm == NX_HASH_HMAC_SHA1 ? 20U
                                                                        : 16U,
        .empty_message = true,
        .empty_key = true,
        .input_length_multiple = 1U,
        .key_length_multiple = 1U};
#if NEXUS_STM32_HASH_DMA
    if (state->config != NULL && state->config->dma != NULL) {
        bool keyed =
            algorithm == NX_HASH_HMAC_SHA1 || algorithm == NX_HASH_HMAC_MD5;
        value.input_alignment = value.key_alignment = 4U;
        value.maximum_bytes = 262140U;
        value.maximum_key_bytes = keyed ? 262140U : 0U;
        value.input_length_multiple = value.key_length_multiple = 4U;
        value.empty_message = false;
        value.empty_key = !keyed;
    }
#endif
    *output = value;
    return NX_SUCCESS;
}
/** \brief Select the immutable byte span for the current hardware phase. */
static const uint8_t* phase_data(const nx_stm32f4_hash_state_t* state,
                                 size_t* bytes) {
    const nx_hash_operation_t* operation = &state->active->operation;
    bool key = state->phase == KEY_FIRST || state->phase == KEY_LAST;
    *bytes = key ? operation->key_bytes : operation->bytes;
    return key ? operation->key : operation->input;
}
/** \brief NBLW describes the whole phase, before any final word is entered. */
static void begin_phase(nx_stm32f4_hash_state_t* state) {
    size_t bytes;
    (void)phase_data(state, &bytes);
    state->offset = 0U;
    state->first_block = true;
    state->waiting = false;
    WRITE_REG(HASH->STR, (uint32_t)(bytes % 4U) * 8U);
    WRITE_REG(HASH->IMR, HASH_IMR_DINIE | HASH_IMR_DCIE);
}
/** \brief Initialize through HAL, then use the reviewed byte-safe FIFO
 * protocol. */
static nx_result_t submit(void* context, nx_hash_request_t* request) {
    nx_stm32f4_hash_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state) ||
        !nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_hash_request_t), false) ||
        !isolated(state, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (!state->running || state->config == NULL || state->faulted)
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    const nx_hash_operation_t* operation = &request->operation;
    if (nx_hash_operation_check(request, operation, request->base.deadline) !=
            NX_SUCCESS ||
        !isolated(state, operation->input, operation->bytes) ||
        !isolated(state, operation->key, operation->key_bytes) ||
        !isolated(state, operation->digest, operation->digest_capacity) ||
        operation->bytes > UINT32_MAX || operation->key_bytes > UINT32_MAX)
        return NX_ERROR_INVALID;
#if NEXUS_STM32_HASH_DMA
    if (state->config->dma != NULL) {
        bool keyed = operation->algorithm == NX_HASH_HMAC_SHA1 ||
                     operation->algorithm == NX_HASH_HMAC_MD5;
        if (operation->bytes == 0U || operation->bytes > 262140U ||
            operation->bytes % 4U != 0U ||
            (uintptr_t)operation->input % 4U != 0U ||
            (keyed &&
             (operation->key_bytes == 0U || operation->key_bytes > 262140U ||
              operation->key_bytes % 4U != 0U ||
              (uintptr_t)operation->key % 4U != 0U)))
            return NX_ERROR_INVALID;
        const nx_stm32f4_hash_dma_config_t* dma = state->config->dma;
        nx_result_t checked = nx_dma_buffer_validate(
            dma->regions, dma->region_count, operation->input, operation->bytes,
            4U, NX_DMA_TO_DEVICE);
        if (checked == NX_SUCCESS && keyed)
            checked = nx_dma_buffer_validate(
                dma->regions, dma->region_count, operation->key,
                operation->key_bytes, 4U, NX_DMA_TO_DEVICE);
        if (checked != NX_SUCCESS)
            return checked;
    }
#endif
    nx_time_us_t now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_IO;
    if (now >= request->base.deadline)
        return NX_ERROR_TIMEOUT;
    if (!clock_live() || state->epoch != g_nx_stm32_system.time_epoch ||
        (RCC->AHB2ENR & RCC_AHB2ENR_HASHEN) == 0U)
        return NX_ERROR_STATE;
    if (HASH->IMR != 0U || (HASH->SR & HASH_SR_BUSY) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    state->active = request;
    state->terminal_pending = false;
#if NEXUS_STM32_HASH_DMA
    if (state->config->dma != NULL) {
        nx_result_t started = nx_stm32f4_hash_dma_submit(state);
        if (started != NX_SUCCESS)
            terminal(state, started);
        return NX_SUCCESS;
    }
#endif
    bool keyed = operation->algorithm == NX_HASH_HMAC_SHA1 ||
                 operation->algorithm == NX_HASH_HMAC_MD5;
    bool md5 = operation->algorithm == NX_HASH_MD5 ||
               operation->algorithm == NX_HASH_HMAC_MD5;
    state->phase = keyed ? KEY_FIRST : PLAIN;
    /* HAL init has no asynchronous ownership and receives no secret pointer.
     * Its large generic handle is a cold local, not per-instance mutable RAM.
     */
    HASH_HandleTypeDef handle = {0};
    handle.Init.DataType = HASH_DATATYPE_8B;
    if (HAL_HASH_Init(&handle) != HAL_OK) {
        terminal(state, NX_ERROR_IO);
        return NX_SUCCESS;
    }
    uint32_t control =
        HASH_DATATYPE_8B | (md5 ? HASH_CR_ALGO : 0U) |
        (keyed ? HASH_CR_MODE : 0U) |
        (keyed && operation->key_bytes > 64U ? HASH_CR_LKEY : 0U);
    WRITE_REG(HASH->CR, control | HASH_CR_INIT);
    nx_arch_dsb();
    if ((HASH->CR & (HASH_CR_ALGO | HASH_CR_MODE | HASH_CR_LKEY |
                     HASH_CR_DATATYPE)) != control) {
        terminal(state, NX_ERROR_IO);
        return NX_SUCCESS;
    }
    begin_phase(state);
    return NX_SUCCESS;
}
/** \brief At most seventeen words per observation; every tail load is in range.
 * \note RM0090 25.3.3: the seventeenth first word starts a partial digest.
 *       Even if it was the final padded word, wait for DINIS before DCAL.
 *       Later rounds have at most sixteen words. No IRQ BUSY loop is used.
 */
static void feed(nx_stm32f4_hash_state_t* state) {
    size_t total;
    const uint8_t* bytes = phase_data(state, &total);
    size_t remaining = total - state->offset;
    bool final = remaining <= 64U;
    size_t capacity = final ? remaining : state->first_block ? 68U : 64U;
    if (capacity > remaining)
        capacity = remaining;
    size_t end = state->offset + capacity;
    while (state->offset < end) {
        volatile uint32_t word = 0U;
        unsigned shift = 0U;
        while (state->offset < end && shift < 32U) {
            word |= (uint32_t)bytes[state->offset++] << shift;
            shift += 8U;
        }
        WRITE_REG(HASH->DIN, word);
        word = 0U;
    }
    state->first_block = false;
    if (final) {
        CLEAR_BIT(HASH->IMR, HASH_IMR_DINIE);
        SET_BIT(HASH->STR, HASH_STR_DCAL);
        nx_arch_dsb();
        state->waiting = true;
    }
}
/** \brief The public byte-aligned digest uses one shared bounded output copy.
 */
static void copy_digest(nx_stm32f4_hash_state_t* state) {
    const nx_hash_operation_t* operation = &state->active->operation;
    size_t count = operation->algorithm == NX_HASH_SHA1 ||
                           operation->algorithm == NX_HASH_HMAC_SHA1
                       ? 20U
                       : 16U;
    uint8_t* output = operation->digest;
    for (size_t i = 0U; i < count / 4U; ++i) {
        uint32_t word = HASH->HR[i];
        output[4U * i] = (uint8_t)(word >> 24U);
        output[4U * i + 1U] = (uint8_t)(word >> 16U);
        output[4U * i + 2U] = (uint8_t)(word >> 8U);
        output[4U * i + 3U] = (uint8_t)word;
    }
}
/** \brief Explicit service advances HMAC phases; IRQs never spin on BUSY. */
static void observe(nx_stm32f4_hash_state_t* state, bool advance_phase) {
    if (state->terminal_pending)
        return;
    nx_time_us_t now = nx_time_now_us();
    if (!clock_live() || state->epoch != g_nx_stm32_system.time_epoch ||
        now == NX_DEADLINE_NEVER || (RCC->AHB2ENR & RCC_AHB2ENR_HASHEN) == 0U) {
        terminal(state, NX_ERROR_IO);
        return;
    }
    if (now >= state->active->base.deadline) {
        terminal(state, NX_ERROR_TIMEOUT);
        return;
    }
    uint32_t status = HASH->SR;
    if ((status & HASH_SR_BUSY) != 0U)
        return;
    if (state->waiting && (state->phase == PLAIN || state->phase == KEY_LAST)) {
        if ((status & HASH_SR_DCIS) == 0U)
            return;
        copy_digest(state);
        terminal(state, NX_SUCCESS);
        return;
    }
    if (state->waiting) {
        if (!advance_phase)
            return;
        ++state->phase;
        begin_phase(state);
    }
    if ((HASH->SR & (HASH_SR_DINIS | HASH_SR_BUSY)) == HASH_SR_DINIS)
        feed(state);
}
/** \brief Shared dispatch never disables RNG or clears the whole vector. */
void nx_stm32f4_hash_irq(nx_stm32f4_hash_state_t* state) {
    if (!valid_state(state) || !state->running || state->active == NULL ||
        state->terminal_pending)
        return;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if ((HASH->SR & HASH->IMR & (HASH_SR_DINIS | HASH_SR_DCIS)) == 0U) {
        nx_arch_irq_restore(mask);
        return;
    }
#if NEXUS_STM32_HASH_DMA
    if (state->config->dma != NULL) {
        /* DMA callbacks remain task-owned. Keep the digest fact latched while
         * withdrawing its level source, preserving the shared RNG vector. */
        CLEAR_BIT(HASH->IMR, HASH_IMR_DCIE);
        nx_arch_dsb();
        if ((READ_REG(HASH->IMR) & HASH_IMR_DCIE) != 0U)
            terminal(state, NX_ERROR_IO);
        const nx_irq_wake_t* wake = state->wake;
        nx_arch_irq_restore(mask);
        (void)nx_irq_wake_signal(wake);
        return;
    }
#endif
    observe(state, false);
    const nx_irq_wake_t* wake = state->wake;
    nx_arch_irq_restore(mask);
    (void)nx_irq_wake_signal(wake);
}
/** \brief The first terminal intent wins without revoking any borrow. */
static nx_result_t cancel(void* context, nx_hash_request_t* request) {
    nx_stm32f4_hash_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state) ||
        !nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_hash_request_t), false) ||
        !isolated(state, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (state->active != request) {
        nx_result_t result =
            state->active == NULL &&
                    nx_request_state(&request->base) == NX_REQUEST_SETTLED
                ? NX_SUCCESS
                : NX_ERROR_STATE;
        nx_arch_irq_restore(mask);
        return result;
    }
    terminal(state, NX_ERROR_CANCELLED);
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Make bounded progress and publish only after complete hardware reset.
 */
static nx_result_t service(void* context) {
    nx_stm32f4_hash_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_hash_request_t* request = state->active;
    if (request == NULL) {
        nx_arch_irq_restore(mask);
        return NX_SUCCESS;
    }
#if NEXUS_STM32_HASH_DMA
    if (state->config->dma != NULL && !state->terminal_pending) {
        const nx_stm32f4_hash_config_t* config = state->config;
        uint64_t epoch = state->epoch;
        nx_time_us_t deadline = request->base.deadline;
        nx_arch_irq_restore(mask);
        /* The serialized task alone advances DMA/HMAC callbacks. The shared
         * peripheral IRQ only withdraws its level hint and cannot settle. */
        nx_result_t progress = NX_ERROR_BUSY;
        nx_time_us_t now = nx_time_now_us();
        if (!clock_live() || epoch != g_nx_stm32_system.time_epoch ||
            now == NX_DEADLINE_NEVER)
            progress = NX_ERROR_IO;
        else if (now >= deadline)
            progress = NX_ERROR_TIMEOUT;
        else if (!state->terminal_pending)
            progress = nx_stm32f4_hash_dma_observe(state);
        mask = nx_arch_irq_save();
        if (state->active != request || state->config != config ||
            state->epoch != epoch || !state->running) {
            nx_arch_irq_restore(mask);
            return NX_ERROR_STATE;
        }
        nx_request_state_t retained = nx_request_state(&request->base);
        if (retained != NX_REQUEST_ACTIVE && retained != NX_REQUEST_DRAINING &&
            retained != NX_REQUEST_QUARANTINED) {
            nx_arch_irq_restore(mask);
            return NX_ERROR_STATE;
        }
        if (!state->terminal_pending) {
            now = nx_time_now_us();
            if (!clock_live() || epoch != g_nx_stm32_system.time_epoch ||
                now == NX_DEADLINE_NEVER)
                terminal(state, NX_ERROR_IO);
            else if (now >= deadline)
                terminal(state, NX_ERROR_TIMEOUT);
            else if (progress != NX_ERROR_BUSY) {
                if (progress == NX_SUCCESS)
                    copy_digest(state);
                terminal(state, progress);
            }
        }
    } else if (state->config->dma == NULL) {
        observe(state, true);
    }
#else
    observe(state, true);
#endif
    if (!state->terminal_pending) {
        nx_arch_irq_restore(mask);
        return NX_SUCCESS;
    }
    if (!reset_engine(state)) {
        state->faulted = true;
        (void)nx_request_transition(&request->base, NX_REQUEST_QUARANTINED);
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    nx_result_t result = state->terminal;
    size_t count = result != NX_SUCCESS ? 0U
                   : request->operation.algorithm == NX_HASH_SHA1 ||
                           request->operation.algorithm == NX_HASH_HMAC_SHA1
                       ? 20U
                       : 16U;
    state->active = NULL;
    state->offset = 0U;
    state->phase = PLAIN;
    state->waiting = false;
    state->terminal_pending = false;
    nx_request_settle(&request->base, result, count);
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Successful stop withdraws HASH without disturbing the RNG sibling. */
static nx_result_t stop(void* context) {
    nx_stm32f4_hash_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state))
        return NX_ERROR_INVALID;
    if (state->active != NULL) {
        nx_result_t result = cancel(state, state->active);
        if (result == NX_SUCCESS)
            result = service(state);
        if (result != NX_SUCCESS)
            return result;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if ((state->running || state->reset_owned) && !reset_engine(state)) {
        state->faulted = true;
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    state->running = false;
#if NEXUS_STM32_HASH_DMA
    nx_result_t dma_stop = nx_stm32f4_hash_dma_stop(state);
    if (dma_stop != NX_SUCCESS) {
        nx_arch_irq_restore(mask);
        return dma_stop;
    }
#endif
    if (state->gate_owned) {
        CLEAR_BIT(RCC->AHB2ENR, RCC_AHB2ENR_HASHEN);
        nx_arch_dsb();
        if ((RCC->AHB2ENR & RCC_AHB2ENR_HASHEN) != 0U) {
            nx_arch_irq_restore(mask);
            return NX_ERROR_IO;
        }
    }
    memset(state, 0, sizeof(*state));
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Check the actual shared priority against the immutable kernel bound.
 */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32f4_hash_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state) ||
        (wake != NULL &&
         (!nx_crypto_span_valid(wake, sizeof(*wake), _Alignof(nx_irq_wake_t),
                                false) ||
          !isolated(state, wake, sizeof(*wake)))))
        return NX_ERROR_INVALID;
    if (!state->running || state->config == NULL)
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    if (syscall_ceiling != state->config->irq_policy.syscall_ceiling)
        return NX_ERROR_INVALID;
    nx_irq_source_t source = irq_source();
    nx_result_t result =
        nx_irq_wake_validate(wake, &state->config->irq_policy, &source);
    if (result == NX_SUCCESS)
        state->wake = wake;
    return result;
}
const nx_hash_ops_t nx_stm32f4_hash_ops = {query,   submit, cancel,
                                           service, stop,   attach_wake};
