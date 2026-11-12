/**
 * \file            cipher.c
 * \brief           Official HAL CRYP transport with explicit terminal cleanup
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "crypto_validation.h"
#include "nexus/arch/arch.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"
#include "stm32f4_cipher.h"
#include <string.h>

extern nx_stm32_system_t g_nx_stm32_system;

/** \brief The serialized owner has no NMI, masked-task or user-mode entry. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Check the complete owner before consulting any of its fields. */
static bool valid_state(const nx_stm32f4_cipher_state_t* state) {
    return nx_crypto_span_valid(state, sizeof(*state),
                                _Alignof(nx_stm32f4_cipher_state_t), false);
}
/** \brief No client span can overwrite private owners or fixed MMIO. */
static bool isolated(const nx_stm32f4_cipher_state_t* state,
                     const void* pointer, size_t bytes) {
    return !nx_crypto_overlap(pointer, bytes, state, sizeof(*state)) &&
           (state->config == NULL ||
            !nx_crypto_overlap(pointer, bytes, state->config,
                               sizeof(*state->config))) &&
           (state->wake == NULL ||
            !nx_crypto_overlap(pointer, bytes, state->wake,
                               sizeof(*state->wake))) &&
           !nx_crypto_overlap(pointer, bytes, CRYP, sizeof(*CRYP)) &&
           !nx_crypto_overlap(pointer, bytes, RCC, sizeof(*RCC)) &&
           !nx_crypto_overlap(pointer, bytes, &g_nx_stm32_system,
                              sizeof(g_nx_stm32_system));
}
/** \brief A live hardware clock and time epoch are independent of HAL state. */
static bool clock_live(void) {
    const nx_stm32_system_t* system = &g_nx_stm32_system;
    return system->started && system->time_epoch != 0U && system->rcc == RCC &&
           system->flash == FLASH && system->timer_error == NX_SUCCESS &&
           system->core_hz == NEXUS_CORE_HZ &&
           nx_stm32_clock_profile_matches(RCC, FLASH);
}
/** \brief Read actual vector facts without changing the Platform IRQ lease. */
static nx_irq_source_t irq_source(void) {
    nx_irq_source_t source = {
        .irq_number = CRYP_IRQn,
        .priority = (uint8_t)(NVIC->IPR[(unsigned)CRYP_IRQn] >>
                              (8U - __NVIC_PRIO_BITS)),
        .priority_group = (uint8_t)((SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) >>
                                    SCB_AIRCR_PRIGROUP_Pos)};
    return source;
}
/** \brief Validate cold storage and reject every foreign gate/reset lease. */
nx_result_t nx_stm32f4_cipher_admit(const nx_stm32f4_cipher_state_t* state,
                                    const nx_stm32f4_cipher_config_t* config) {
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state) ||
        !nx_crypto_span_valid(config, sizeof(*config),
                              _Alignof(nx_stm32f4_cipher_config_t), false) ||
        nx_crypto_overlap(state, sizeof(*state), config, sizeof(*config)) ||
        nx_crypto_overlap(config, sizeof(*config), CRYP, sizeof(*CRYP)) ||
        nx_crypto_overlap(config, sizeof(*config), RCC, sizeof(*RCC)))
        return NX_ERROR_INVALID;
    nx_irq_source_t source = irq_source();
    nx_result_t result =
        nx_irq_wake_validate(NULL, &config->irq_policy, &source);
    if (result != NX_SUCCESS)
        return result;
    if (state->config != NULL || state->active != NULL || state->running ||
        state->gate_owned || state->reset_owned ||
        (RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN) != 0U ||
        (RCC->AHB2RSTR & RCC_AHB2RSTR_CRYPRST) != 0U)
        return NX_ERROR_BUSY;
    return NX_SUCCESS;
}
/** \brief Record gate responsibility before the first shared RCC write. */
nx_result_t nx_stm32f4_cipher_start(nx_stm32f4_cipher_state_t* state,
                                    const nx_stm32f4_cipher_config_t* config) {
    nx_result_t result = nx_stm32f4_cipher_admit(state, config);
    if (result != NX_SUCCESS)
        return result;
    if (!clock_live())
        return NX_ERROR_STATE;
    state->config = config;
    state->epoch = g_nx_stm32_system.time_epoch;
    state->gate_owned = true;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    SET_BIT(RCC->AHB2ENR, RCC_AHB2ENR_CRYPEN);
    nx_arch_dsb();
    nx_arch_irq_restore(mask);
    if ((RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN) == 0U)
        return NX_ERROR_IO;
    if ((CRYP->CR & CRYP_CR_CRYPEN) != 0U || CRYP->IMSCR != 0U ||
        CRYP->DMACR != 0U)
        return NX_ERROR_IO;
    state->running = true;
    state->faulted = false;
    return NX_SUCCESS;
}
/** \brief Securely discard only private copies, never caller-owned key bytes.
 */
static void clear_bytes(void* pointer, size_t bytes) {
    volatile uint8_t* destination = pointer;
    while (bytes-- != 0U)
        *destination++ = 0U;
}
/** \brief Latch the first terminal cause and close the peripheral IRQ source.
 */
static void terminal(nx_stm32f4_cipher_state_t* state, nx_result_t result) {
    if (!state->terminal_pending) {
        state->terminal = result;
        state->terminal_pending = true;
        (void)nx_request_transition(&state->active->base, NX_REQUEST_DRAINING);
    }
    WRITE_REG(CRYP->IMSCR, 0U);
    nx_arch_dsb();
    if (CRYP->IMSCR != 0U) {
        /* This vector is dedicated to the selected CRYP owner. A refused
         * source write must not prevent task recovery through an IRQ storm.
         * Platform owns re-enabling the lease after a complete stop/restart. */
        unsigned number = (unsigned)CRYP_IRQn;
        WRITE_REG(NVIC->ICER[number / 32U], UINT32_C(1) << (number % 32U));
        nx_arch_dsb();
        state->faulted = true;
    }
}
/** \brief Reset is the exclusive engine's documented FIFO/context withdrawal.
 * \note This provider never enables DMA. Reset assertion/release and the
 *       reset-visible control, FIFO and IV state must all be observed before
 *       the final request access. A refused reset leaves the borrow intact.
 */
static bool reset_engine(nx_stm32f4_cipher_state_t* state) {
    if ((RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN) == 0U)
        return false;
    WRITE_REG(CRYP->IMSCR, 0U);
    WRITE_REG(CRYP->DMACR, 0U);
    CLEAR_BIT(CRYP->CR, CRYP_CR_CRYPEN);
    nx_arch_dsb();
    state->reset_owned = true;
    SET_BIT(RCC->AHB2RSTR, RCC_AHB2RSTR_CRYPRST);
    nx_arch_dsb();
    if ((RCC->AHB2RSTR & RCC_AHB2RSTR_CRYPRST) == 0U)
        return false;
    CLEAR_BIT(RCC->AHB2RSTR, RCC_AHB2RSTR_CRYPRST);
    nx_arch_dsb();
    if ((RCC->AHB2RSTR & RCC_AHB2RSTR_CRYPRST) != 0U || CRYP->CR != 0U ||
        CRYP->IMSCR != 0U || CRYP->DMACR != 0U ||
        (CRYP->SR & (CRYP_SR_BUSY | CRYP_SR_OFNE)) != 0U ||
        (CRYP->IV0LR | CRYP->IV0RR | CRYP->IV1LR | CRYP->IV1RR) != 0U)
        return false;
    /* RM0090 key registers are write-only and always read as zero. Their
     * reads cannot prove erasure; exclusive peripheral reset is the proof. */
    state->reset_owned = false;
    return true;
}
/** \brief No result output may overwrite any live borrowed descriptor/span. */
static bool result_isolated(const nx_stm32f4_cipher_state_t* state,
                            const void* output, size_t bytes) {
    if (!isolated(state, output, bytes))
        return false;
    if (state->active == NULL)
        return true;
    const nx_cipher_operation_t* operation = &state->active->operation;
    return !nx_crypto_overlap(output, bytes, state->active,
                              sizeof(*state->active)) &&
           !nx_crypto_overlap(output, bytes, operation->input,
                              operation->bytes) &&
           !nx_crypto_overlap(output, bytes, operation->output,
                              operation->bytes) &&
           !nx_crypto_overlap(output, bytes, operation->key,
                              operation->key_bytes) &&
           !nx_crypto_overlap(output, bytes, operation->iv,
                              operation->iv_bytes);
}
/** \brief One explicit transport supports whole blocks and standard key bytes.
 */
static nx_result_t query(void* context, nx_cipher_algorithm_t algorithm,
                         nx_cipher_constraints_t* output) {
    nx_stm32f4_cipher_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state) ||
        !nx_crypto_span_valid(output, sizeof(*output),
                              _Alignof(nx_cipher_constraints_t), false) ||
        !result_isolated(state, output, sizeof(*output)) ||
        (unsigned)algorithm > (unsigned)NX_CIPHER_TDES_CBC)
        return NX_ERROR_INVALID;
    const bool aes = algorithm <= NX_CIPHER_AES_CTR;
    const bool des =
        algorithm == NX_CIPHER_DES_ECB || algorithm == NX_CIPHER_DES_CBC;
    nx_cipher_constraints_t constraints = {
        .alignment = 4U,
        .length_multiple = aes ? 16U : 8U,
        .maximum_bytes = aes ? 65520U : 65528U,
        .key_sizes =
            aes   ? NX_CIPHER_KEY_16 | NX_CIPHER_KEY_24 | NX_CIPHER_KEY_32
            : des ? NX_CIPHER_KEY_8
                  : NX_CIPHER_KEY_16 | NX_CIPHER_KEY_24,
        .iv_bytes =
            (algorithm == NX_CIPHER_AES_ECB || algorithm == NX_CIPHER_DES_ECB ||
             algorithm == NX_CIPHER_TDES_ECB)
                ? 0U
            : aes ? 16U
                  : 8U,
        .exact_in_place = true};
    *output = constraints;
    return NX_SUCCESS;
}
/** \brief Pack standard big-endian key/IV words without unaligned RAM loads. */
static void pack_words(uint32_t* words, const uint8_t* bytes, size_t count) {
    for (size_t i = 0U; i < count / 4U; ++i) {
        words[i] = ((uint32_t)bytes[4U * i] << 24U) |
                   ((uint32_t)bytes[4U * i + 1U] << 16U) |
                   ((uint32_t)bytes[4U * i + 2U] << 8U) |
                   (uint32_t)bytes[4U * i + 3U];
    }
}
/** \brief Validate before admission, then let the official HAL own FIFO I/O. */
static nx_result_t submit(void* context, nx_cipher_request_t* request) {
    nx_stm32f4_cipher_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state) ||
        !nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_cipher_request_t), false) ||
        !isolated(state, request, sizeof(*request)))
        return NX_ERROR_INVALID;
    if (!state->running || state->config == NULL || state->faulted)
        return NX_ERROR_STATE;
    if (state->active != NULL)
        return NX_ERROR_BUSY;
    const nx_cipher_operation_t* operation = &request->operation;
    if (nx_cipher_operation_check(request, operation, request->base.deadline) !=
            NX_SUCCESS ||
        !isolated(state, operation->input, operation->bytes) ||
        !isolated(state, operation->output, operation->bytes) ||
        !isolated(state, operation->key, operation->key_bytes) ||
        !isolated(state, operation->iv, operation->iv_bytes))
        return NX_ERROR_INVALID;
    nx_cipher_constraints_t constraints;
    nx_result_t result = query(state, operation->algorithm, &constraints);
    if (result != NX_SUCCESS)
        return result;
    if ((uintptr_t)operation->input % constraints.alignment != 0U ||
        (uintptr_t)operation->output % constraints.alignment != 0U ||
        operation->bytes % constraints.length_multiple != 0U ||
        operation->bytes > constraints.maximum_bytes)
        return NX_ERROR_INVALID;
    nx_time_us_t now = nx_time_now_us();
    if (now == NX_DEADLINE_NEVER)
        return NX_ERROR_IO;
    if (now >= request->base.deadline)
        return NX_ERROR_TIMEOUT;
    if (!clock_live() || state->epoch != g_nx_stm32_system.time_epoch ||
        (RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN) == 0U)
        return NX_ERROR_STATE;
    if ((CRYP->CR & CRYP_CR_CRYPEN) != 0U || CRYP->IMSCR != 0U ||
        CRYP->DMACR != 0U)
        return NX_ERROR_BUSY;
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    state->active = request;
    state->terminal_pending = false;
    uint32_t key[8] = {0U};
    uint32_t iv[4] = {0U};
    pack_words(key, operation->key, operation->key_bytes);
    pack_words(iv, operation->iv, operation->iv_bytes);
    if ((operation->algorithm == NX_CIPHER_TDES_ECB ||
         operation->algorithm == NX_CIPHER_TDES_CBC) &&
        operation->key_bytes == 16U) {
        key[4] = key[0];
        key[5] = key[1];
    }
    static const uint32_t algorithms[] = {
        CRYP_AES_ECB, CRYP_AES_CBC,  CRYP_AES_CTR, CRYP_DES_ECB,
        CRYP_DES_CBC, CRYP_TDES_ECB, CRYP_TDES_CBC};
    CRYP_HandleTypeDef* handle = &state->handle;
    handle->Instance = CRYP;
    handle->Init.DataType = CRYP_DATATYPE_8B;
    handle->Init.KeySize = operation->key_bytes == 32U   ? CRYP_KEYSIZE_256B
                           : operation->key_bytes == 24U ? CRYP_KEYSIZE_192B
                                                         : CRYP_KEYSIZE_128B;
    handle->Init.Algorithm = algorithms[operation->algorithm];
    handle->Init.pKey = key;
    handle->Init.pInitVect = iv;
    handle->Init.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
    handle->Init.KeyIVConfigSkip = CRYP_KEYIVCONFIG_ALWAYS;
    HAL_StatusTypeDef status = HAL_CRYP_Init(handle);
    if (status == HAL_OK) {
        uint32_t* input = (uint32_t*)(uintptr_t)operation->input;
        status =
            operation->direction == NX_CIPHER_ENCRYPT
                ? HAL_CRYP_Encrypt_IT(handle, input, (uint16_t)operation->bytes,
                                      operation->output)
                : HAL_CRYP_Decrypt_IT(handle, input, (uint16_t)operation->bytes,
                                      operation->output);
    }
    /* Basic-mode IT entry consumes all key/IV words synchronously. Neither
     * the IRQ nor completion consults these pointers after this call. */
    handle->Init.pKey = NULL;
    handle->Init.pInitVect = NULL;
    clear_bytes(key, sizeof(key));
    clear_bytes(iv, sizeof(iv));
    if (status != HAL_OK) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        terminal(state, NX_ERROR_IO);
        nx_arch_irq_restore(mask);
    }
    return NX_SUCCESS;
}
/** \brief Observe completion/deadline once; never wait in an interrupt. */
void nx_stm32f4_cipher_irq(nx_stm32f4_cipher_state_t* state) {
    if (!valid_state(state) || !state->running || state->active == NULL ||
        state->terminal_pending)
        return;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_time_us_t now = nx_time_now_us();
    if (!clock_live() || state->epoch != g_nx_stm32_system.time_epoch ||
        now == NX_DEADLINE_NEVER) {
        terminal(state, NX_ERROR_IO);
    } else if (now >= state->active->base.deadline) {
        terminal(state, NX_ERROR_TIMEOUT);
    } else {
        HAL_CRYP_IRQHandler(&state->handle);
        if (state->handle.ErrorCode != HAL_CRYP_ERROR_NONE)
            terminal(state, NX_ERROR_IO);
        else if (state->handle.CrypOutCount == state->handle.Size / 4U)
            terminal(state, NX_SUCCESS);
    }
    const nx_irq_wake_t* wake = state->terminal_pending ? state->wake : NULL;
    nx_arch_irq_restore(mask);
    (void)nx_irq_wake_signal(wake);
}
/** \brief Cancellation latches intent while the request/key borrow stays live.
 */
static nx_result_t cancel(void* context, nx_cipher_request_t* request) {
    nx_stm32f4_cipher_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state) ||
        !nx_crypto_span_valid(request, sizeof(*request),
                              _Alignof(nx_cipher_request_t), false) ||
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
/** \brief Publish only after verified reset and removal of all HAL references.
 */
static nx_result_t service(void* context) {
    nx_stm32f4_cipher_state_t* state = context;
    if (!task_context())
        return NX_ERROR_CONTEXT;
    if (!valid_state(state))
        return NX_ERROR_INVALID;
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    nx_cipher_request_t* request = state->active;
    if (request == NULL) {
        nx_arch_irq_restore(mask);
        return NX_SUCCESS;
    }
    if (!state->terminal_pending) {
        nx_time_us_t now = nx_time_now_us();
        if (!clock_live() || state->epoch != g_nx_stm32_system.time_epoch ||
            now == NX_DEADLINE_NEVER)
            terminal(state, NX_ERROR_IO);
        else if (now >= request->base.deadline)
            terminal(state, NX_ERROR_TIMEOUT);
    }
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
    size_t transferred = (size_t)state->handle.CrypOutCount * 4U;
    nx_result_t result = state->terminal;
    clear_bytes(&state->handle, sizeof(state->handle));
    state->active = NULL;
    state->terminal_pending = false;
    nx_request_settle(&request->base, result, transferred);
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Stop is a join; no key/context or vector borrow is implicitly freed.
 */
static nx_result_t stop(void* context) {
    nx_stm32f4_cipher_state_t* state = context;
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
    if (state->running || state->reset_owned) {
        if (!reset_engine(state)) {
            state->faulted = true;
            nx_arch_irq_restore(mask);
            return NX_ERROR_IO;
        }
    }
    state->running = false;
    if (state->gate_owned) {
        CLEAR_BIT(RCC->AHB2ENR, RCC_AHB2ENR_CRYPEN);
        nx_arch_dsb();
        if ((RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN) != 0U) {
            nx_arch_irq_restore(mask);
            return NX_ERROR_IO;
        }
    }
    clear_bytes(state, sizeof(*state));
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Attach only an idle, independent sink allowed by actual NVIC facts.
 */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32f4_cipher_state_t* state = context;
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
const nx_cipher_ops_t nx_stm32f4_cipher_ops = {query,   submit, cancel,
                                               service, stop,   attach_wake};
