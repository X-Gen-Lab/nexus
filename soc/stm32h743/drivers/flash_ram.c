/**
 * \file            flash_ram.c
 * \brief           H743 RAM-only physical Flash trigger and drain
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_flash_ram.h"

#define NX_H743_RAM __attribute__((section(".nx_flash_ram.text"), noinline))
#define NX_H743_RAM_BUSY                                                       \
    ((uint32_t)(FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE))
#define NX_H743_RAM_ERRORS                                                     \
    ((uint32_t)(FLASH_SR_WRPERR | FLASH_SR_PGSERR | FLASH_SR_STRBERR |         \
                FLASH_SR_INCERR | FLASH_SR_OPERR | FLASH_SR_RDPERR |           \
                FLASH_SR_RDSERR | FLASH_SR_SNECCERR | FLASH_SR_DBECCERR))
#ifndef NX_H743_FLASH_RAM_PRIMASK
#define NX_H743_FLASH_RAM_PRIMASK() __get_PRIMASK()
#endif
#ifndef NX_H743_FLASH_RAM_SP
#define NX_H743_FLASH_RAM_SP() __get_MSP()
#endif
#ifndef NX_H743_FLASH_RAM_CONTROL
#define NX_H743_FLASH_RAM_CONTROL() __get_CONTROL()
#endif
#ifndef NX_H743_FLASH_RAM_IPSR
#define NX_H743_FLASH_RAM_IPSR() __get_IPSR()
#endif
#ifndef NX_H743_FLASH_RAM_BASEPRI
#define NX_H743_FLASH_RAM_BASEPRI() __get_BASEPRI()
#endif
#ifndef NX_H743_FLASH_RAM_FAULTMASK
#define NX_H743_FLASH_RAM_FAULTMASK() __get_FAULTMASK()
#endif
#ifndef NX_H743_FLASH_RAM_VTOR
#define NX_H743_FLASH_RAM_VTOR(execution) ((uintptr_t)(execution)->vectors)
#endif
#ifndef NX_H743_FLASH_RAM_POLL
#define NX_H743_FLASH_RAM_POLL(execution) ((void)(execution))
#endif
#ifndef NX_H743_FLASH_RAM_BARRIER
#define NX_H743_FLASH_RAM_BARRIER()                                            \
    do {                                                                       \
        __DSB();                                                               \
        __ISB();                                                               \
    } while (0)
#endif
#ifndef NX_H743_FLASH_RAM_MODEL_TERMINAL
#define NX_H743_FLASH_RAM_MODEL_TERMINAL(execution) ((void)(execution))
#endif

NX_H743_RAM _Noreturn void
nx_stm32h743_flash_ram_terminal(nx_stm32h743_flash_ram_workspace_t* execution,
                                nx_stm32h743_flash_ram_fault_t reason) {
    execution->fault = reason;
    execution->fault_status = NX_H743_READ(&execution->port->registers->SR1) |
                              NX_H743_READ(&execution->port->registers->SR2);
    execution->active = true;
    execution->port->active = true;
    NX_H743_FLASH_RAM_BARRIER();
    NX_H743_FLASH_RAM_MODEL_TERMINAL(execution);
    for (;;) {
    }
}

/** \brief A single acknowledged UIF supplies at most one overflow epoch. */
static NX_H743_RAM void
observe_time(nx_stm32h743_flash_ram_workspace_t* execution) {
    nx_stm32h743_system_t* system = execution->system;
    if ((NX_H743_READ(&system->timer->SR) & TIM_SR_UIF) == 0U)
        return;
    /* TIM2 SR is W0C. Ones leave every unowned flag unchanged. Counting
     * follows readback, so a refused acknowledgement never counts twice. */
    NX_H743_WRITE(&system->timer->SR, ~(uint32_t)TIM_SR_UIF);
    NX_H743_FLASH_RAM_BARRIER();
    if ((NX_H743_READ(&system->timer->SR) & TIM_SR_UIF) != 0U) {
        system->started = false;
        system->timer_error = NX_ERROR_IO;
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_TIME);
    }
    if (system->overflow_us > UINT64_MAX - (UINT64_C(1) << 32U)) {
        system->started = false;
        system->timer_error = NX_ERROR_EXHAUSTED;
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_TIME);
    }
    system->overflow_us += UINT64_C(1) << 32U;
}

NX_H743_RAM nx_time_us_t
nx_stm32h743_flash_ram_time(nx_stm32h743_flash_ram_workspace_t* execution) {
    uint32_t count;
    do {
        observe_time(execution);
        count = NX_H743_READ(&execution->system->timer->CNT);
        /* A wrap between acknowledgement and CNT must not mix an old epoch
         * with a new low word. Consume that wrap before retrying. */
    } while ((NX_H743_READ(&execution->system->timer->SR) & TIM_SR_UIF) != 0U);
    return execution->system->overflow_us + count;
}

/** \brief Both banks must be quiet before the RAM frame may return. */
static NX_H743_RAM bool
busy(const nx_stm32h743_flash_ram_workspace_t* execution) {
    return ((NX_H743_READ(&execution->port->registers->SR1) |
             NX_H743_READ(&execution->port->registers->SR2)) &
            NX_H743_RAM_BUSY) != 0U;
}

NX_H743_RAM bool
nx_stm32h743_flash_ram_lock(nx_stm32h743_flash_ram_workspace_t* execution) {
    if (busy(execution))
        return false;
    volatile uint32_t* control = execution->bank == 0U
                                     ? &execution->port->registers->CR1
                                     : &execution->port->registers->CR2;
    const uint32_t restored = execution->saved_control | FLASH_CR_LOCK;
    NX_H743_WRITE(control, restored);
    NX_H743_FLASH_RAM_BARRIER();
    return NX_H743_READ(control) == restored;
}

NX_H743_RAM nx_result_t nx_stm32h743_flash_ram_context(
    const nx_stm32h743_flash_ram_workspace_t* execution) {
    if (execution == NULL || !execution->prepared || !execution->active ||
        execution->port == NULL || !execution->port->active ||
        execution->system == NULL || !execution->system->started ||
        execution->system->timer == NULL || execution->system->core == NULL ||
        execution->port->registers == NULL || execution->bank > 1U ||
        execution->port->poll_limit == 0U ||
        (execution->program && execution->destination == NULL))
        return NX_ERROR_STATE;
    if (NX_H743_FLASH_RAM_PRIMASK() != 1U ||
        (NX_H743_FLASH_RAM_CONTROL() & 3U) != 0U ||
        NX_H743_FLASH_RAM_IPSR() != 0U || NX_H743_FLASH_RAM_BASEPRI() != 0U ||
        NX_H743_FLASH_RAM_FAULTMASK() != 0U)
        return NX_ERROR_CONTEXT;
    const uintptr_t first = (uintptr_t)execution->stack;
    const uintptr_t stack = NX_H743_FLASH_RAM_SP();
    /* Two extended exception frames plus alignment consume at most224B.
     * The actual bridge/ELF additionally bounds every reachable C frame. */
    if (stack < first || stack - first < 224U ||
        stack - first >= sizeof(execution->stack) || stack % 8U != 0U ||
        NX_H743_READ(&execution->system->core->VTOR) !=
            NX_H743_FLASH_RAM_VTOR(execution))
        return NX_ERROR_STATE;
    return NX_SUCCESS;
}

/** \brief Prepare controls without consuming a physical programming pulse. */
static NX_H743_RAM nx_result_t
arm(nx_stm32h743_flash_ram_workspace_t* execution) {
    observe_time(execution);
    FLASH_TypeDef* flash = execution->port->registers;
    volatile uint32_t* control =
        execution->bank == 0U ? &flash->CR1 : &flash->CR2;
    volatile uint32_t* status =
        execution->bank == 0U ? &flash->SR1 : &flash->SR2;
    volatile uint32_t* clear =
        execution->bank == 0U ? &flash->CCR1 : &flash->CCR2;
    if (busy(execution))
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_DRAIN);
    if ((NX_H743_READ(control) & FLASH_CR_LOCK) != 0U) {
        volatile uint32_t* key =
            execution->bank == 0U ? &flash->KEYR1 : &flash->KEYR2;
        NX_H743_WRITE(key, UINT32_C(0x45670123));
        NX_H743_WRITE(key, UINT32_C(0xcdef89ab));
        NX_H743_FLASH_RAM_BARRIER();
        if ((NX_H743_READ(control) & FLASH_CR_LOCK) != 0U)
            return NX_ERROR_IO;
    }
    /* Completion belongs to this pulse. A stale EOP cannot establish that
     * a new START was accepted, and a refused W1C forbids a new trigger. */
    NX_H743_WRITE(clear, NX_H743_RAM_ERRORS | FLASH_SR_EOP);
    NX_H743_FLASH_RAM_BARRIER();
    if ((NX_H743_READ(status) & (NX_H743_RAM_ERRORS | FLASH_SR_EOP)) != 0U)
        return NX_ERROR_IO;
    NX_H743_WRITE(control, execution->configuration);
    NX_H743_FLASH_RAM_BARRIER();
    if (NX_H743_READ(control) != execution->configuration)
        return NX_ERROR_IO;
    return NX_SUCCESS;
}

/** \brief Both RAM drain policies use the same eight stores or erase START. */
static NX_H743_RAM void issue(nx_stm32h743_flash_ram_workspace_t* execution) {
    if (execution->program) {
        for (unsigned word = 0U; word < 8U; ++word)
            NX_H743_FLASH_WRITE32(execution->destination + word,
                                  execution->words[word]);
    } else {
        volatile uint32_t* control = execution->bank == 0U
                                         ? &execution->port->registers->CR1
                                         : &execution->port->registers->CR2;
        /* START may self-clear. Only drained status and fresh EOP establish
         * completion, rather than an assumption that START remains set. */
        NX_H743_WRITE(control, execution->configuration | FLASH_CR_START);
    }
    NX_H743_FLASH_RAM_BARRIER();
}

static NX_H743_RAM nx_result_t
completion(const nx_stm32h743_flash_ram_workspace_t* execution) {
    volatile uint32_t* status = execution->bank == 0U
                                    ? &execution->port->registers->SR1
                                    : &execution->port->registers->SR2;
    const uint32_t observed = NX_H743_READ(status);
    return (observed & NX_H743_RAM_ERRORS) == 0U &&
                   (observed & FLASH_SR_EOP) != 0U
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

NX_H743_RAM nx_result_t
nx_stm32h743_flash_ram_pulse(nx_stm32h743_flash_ram_workspace_t* execution) {
    nx_result_t result = nx_stm32h743_flash_ram_context(execution);
    if (result != NX_SUCCESS)
        return result;
    result = arm(execution);
    if (result == NX_SUCCESS) {
        issue(execution);
        uint32_t remaining = execution->port->poll_limit;
        do {
            NX_H743_FLASH_RAM_POLL(execution);
            observe_time(execution);
            if (!busy(execution))
                break;
        } while (--remaining != 0U);
        if (busy(execution))
            nx_stm32h743_flash_ram_terminal(execution,
                                            NX_H743_FLASH_RAM_FAULT_DRAIN);
        result = completion(execution);
    }
    if (!nx_stm32h743_flash_ram_lock(execution))
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_LOCK);
    /* The bridge retains masks, vectors, stack and active flags until its
     * entire transaction, including protection restoration, has completed. */
    return result;
}

NX_H743_RAM nx_stm32h743_flash_ram_pulse_outcome_t
nx_stm32h743_flash_ram_pulse_until_idle(
    nx_stm32h743_flash_ram_workspace_t* execution, nx_time_us_t deadline,
    bool* timed_out) {
    if (busy(execution))
        nx_stm32h743_flash_ram_terminal(execution,
                                        NX_H743_FLASH_RAM_FAULT_DRAIN);
    *timed_out = nx_stm32h743_flash_ram_time(execution) >= deadline;
    nx_result_t result = NX_SUCCESS;
    if (!*timed_out) {
        result = arm(execution);
        if (result == NX_SUCCESS) {
            /* Unlock, acknowledgements and configuration consume real time.
             * Observe expiry again at the last software boundary before an
             * irreversible store/START. An armed but unissued mode is locked
             * below without manufacturing an EOP or consuming ECC cells. */
            *timed_out = nx_stm32h743_flash_ram_time(execution) >= deadline;
        }
        if (result == NX_SUCCESS && !*timed_out) {
            issue(execution);
            /* A deadline is an outcome, never physical cancellation. This
             * loop deliberately has no software poll-budget escape. */
            do {
                NX_H743_FLASH_RAM_POLL(execution);
                const nx_time_us_t now = nx_stm32h743_flash_ram_time(execution);
                if (now >= deadline)
                    *timed_out = true;
            } while (busy(execution));
            result = completion(execution);
        }
    }
    if (!nx_stm32h743_flash_ram_lock(execution))
        return NX_H743_FLASH_RAM_PULSE_LOCK_UNPROVEN;
    return result == NX_SUCCESS ? NX_H743_FLASH_RAM_PULSE_SUCCESS
                                : NX_H743_FLASH_RAM_PULSE_IO;
}
