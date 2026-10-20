/**
 * \file            fdcan.c
 * \brief           Fixed G474 FDCAN1 ISO FD loopback and physical RAM leases
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "can_fd_impl.h"
#include "stm32g474_fdcan.h"
#include "stm32g474_system.h"
#include "stm32g4xx_hal_fdcan.h"
#include <stddef.h>

#ifndef NX_G474_FDCAN_READ
#define NX_G474_FDCAN_READ(reg) (*(reg))
#endif
#ifndef NX_G474_FDCAN_WRITE
#define NX_G474_FDCAN_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_G474_FDCAN_IRQ_AVAILABLE
#define NX_G474_FDCAN_IRQ_AVAILABLE(irq) NX_G474_IRQ_AVAILABLE(irq)
#endif
#ifndef NX_G474_FDCAN_IRQ_CONTROL
#define NX_G474_FDCAN_IRQ_CONTROL(irq, on, priority)                           \
    NX_G474_IRQ_CONTROL((irq), (on), (priority))
#endif
#ifndef NX_G474_FDCAN_IRQ_PRIORITY
#define NX_G474_FDCAN_IRQ_PRIORITY(irq) NX_G474_IRQ_PRIORITY(irq)
#endif
#ifndef NX_G474_FDCAN_IRQ_GROUP
#define NX_G474_FDCAN_IRQ_GROUP() NVIC_GetPriorityGrouping()
#endif

/* G4 reduced M_CAN has fixed sections. H7 configurable RAM is a different IP
 * integration and must never use these offsets. Every offset below is words. */
#define FDCAN_RAM_WORDS      212U
#define FDCAN_EXT_WORD       28U
#define FDCAN_RX0_WORD       44U
#define FDCAN_RX1_WORD       98U
#define FDCAN_TX_WORD        158U
#define FDCAN_ELEMENT_WORDS  18U
#define FDCAN_FIXED_PRIORITY 5U
#define FDCAN_ISO_CCCR                                                         \
    ((uint32_t)(FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE | FDCAN_CCCR_DAR))
#define FDCAN_MODE_MASK                                                        \
    ((uint32_t)(FDCAN_CCCR_TEST | FDCAN_CCCR_MON | FDCAN_CCCR_FDOE |           \
                FDCAN_CCCR_BRSE | FDCAN_CCCR_DAR | FDCAN_CCCR_ASM |            \
                FDCAN_CCCR_EFBI | FDCAN_CCCR_NISO | FDCAN_CCCR_PXHD |          \
                FDCAN_CCCR_TXP | FDCAN_CCCR_CSR))
#define FDCAN_INTERRUPTS                                                       \
    ((uint32_t)(FDCAN_IT_RX_FIFO0_NEW_MESSAGE |                                \
                FDCAN_IT_RX_FIFO0_MESSAGE_LOST |                               \
                FDCAN_IT_RX_FIFO1_NEW_MESSAGE |                                \
                FDCAN_IT_RX_FIFO1_MESSAGE_LOST | FDCAN_IT_TX_COMPLETE |        \
                FDCAN_IT_TX_ABORT_COMPLETE | FDCAN_IT_ERROR_PASSIVE |          \
                FDCAN_IT_ERROR_WARNING | FDCAN_IT_BUS_OFF |                    \
                FDCAN_IT_ARB_PROTOCOL_ERROR | FDCAN_IT_DATA_PROTOCOL_ERROR))

_Static_assert(offsetof(FDCAN_GlobalTypeDef, CCCR) == 0x18U,
               "G4 reduced M_CAN control layout");
_Static_assert(offsetof(FDCAN_GlobalTypeDef, RXGFC) == 0x80U,
               "G4 fixed RAM filter layout");
_Static_assert(offsetof(FDCAN_GlobalTypeDef, TXBC) == 0xC0U,
               "G4 fixed RAM transmit layout");
_Static_assert(FDCAN1_IT0_IRQn == 21 && FDCAN1_IT1_IRQn == 22,
               "G474 exact interrupt identities");
_Static_assert(FDCAN_FILTER_MASK == 2U && FDCAN_REJECT == 2U,
               "Pinned official mask filter definitions");

/** \brief Controls require the sole privileged, unmasked task executor. */
static bool task_context(void) {
    return nx_arch_is_privileged() && !nx_arch_in_isr() &&
           !nx_arch_irq_is_masked();
}
/** \brief Cold profile selection never adds a branch to the frame path. */
static uint32_t selected_control(const nx_stm32g474_fdcan_state_t* state) {
    return state->mode == NX_STM32G474_FDCAN_INTERNAL_LOOPBACK
               ? FDCAN_ISO_CCCR | (uint32_t)(FDCAN_CCCR_TEST | FDCAN_CCCR_MON)
               : FDCAN_ISO_CCCR;
}
/** \brief Reject wrapping storage intervals before any payload access. */
static bool overlap(const void* first, size_t first_size, const void* second,
                    size_t second_size) {
    uintptr_t a = (uintptr_t)first;
    uintptr_t b = (uintptr_t)second;
    if (first_size == 0U || second_size == 0U) {
        return false;
    }
    return first_size > UINTPTR_MAX - a || second_size > UINTPTR_MAX - b ||
           (a < b + second_size && b < a + first_size);
}
/** \brief Platform gate proof precedes every peripheral or RAM observation. */
static bool clock_ready(const nx_stm32g474_fdcan_domain_t* domain) {
    return domain != NULL && domain->rcc == RCC &&
           domain->configuration == FDCAN_CONFIG &&
           domain->ram == (volatile uint32_t*)(uintptr_t)SRAMCAN_BASE &&
           domain->kernel_hz == 24000000U && g_nx_stm32g474_system.started &&
           g_nx_stm32g474_system.rcc == RCC &&
           g_nx_stm32g474_system.core_hz == 170000000U &&
           (NX_G474_FDCAN_READ(&RCC->APB1ENR1) &
            (uint32_t)RCC_APB1ENR1_FDCANEN) != 0U &&
           (NX_G474_FDCAN_READ(&RCC->APB1RSTR1) &
            (uint32_t)RCC_APB1RSTR1_FDCANRST) == 0U &&
           (NX_G474_FDCAN_READ(&RCC->CR) &
            (uint32_t)(RCC_CR_HSEON | RCC_CR_HSERDY)) ==
               (uint32_t)(RCC_CR_HSEON | RCC_CR_HSERDY) &&
           (NX_G474_FDCAN_READ(&RCC->CCIPR) & (uint32_t)RCC_CCIPR_FDCANSEL) ==
               0U;
}
/** \brief Cold peers cannot be reset or silently adopted by this domain. */
static bool cold_controller(FDCAN_GlobalTypeDef* registers) {
    return (NX_G474_FDCAN_READ(&registers->CCCR) & (uint32_t)FDCAN_CCCR_INIT) !=
               0U &&
           (NX_G474_FDCAN_READ(&registers->TXBRP) |
            NX_G474_FDCAN_READ(&registers->TXBAR) |
            NX_G474_FDCAN_READ(&registers->TXBCR) |
            NX_G474_FDCAN_READ(&registers->IE) |
            NX_G474_FDCAN_READ(&registers->ILE)) == 0U;
}
/** \brief Read-only domain acquisition never changes the shared APB gate. */
nx_result_t
nx_stm32g474_fdcan_domain_initialize(nx_stm32g474_fdcan_domain_t* domain) {
    if (domain == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!clock_ready(domain)) {
        return NX_ERROR_STATE;
    }
    if (domain->retained || domain->acquired_mask != 0U ||
        !cold_controller(FDCAN1) || !cold_controller(FDCAN2) ||
        !cold_controller(FDCAN3)) {
        return NX_ERROR_BUSY;
    }
    if (NX_G474_FDCAN_READ(&domain->configuration->CKDIV) != 0U) {
        return NX_ERROR_STATE;
    }
    domain->retained = true;
    return NX_SUCCESS;
}
/** \brief Domain release cannot revoke an instance or close a peer clock. */
nx_result_t
nx_stm32g474_fdcan_domain_stop(nx_stm32g474_fdcan_domain_t* domain) {
    if (domain == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (domain->acquired_mask != 0U) {
        return NX_ERROR_BUSY;
    }
    domain->retained = false;
    return NX_SUCCESS;
}
/** \brief Reject storage that could corrupt provider controls or fixed RAM. */
static bool protected_storage(const nx_stm32g474_fdcan_state_t* state,
                              const void* storage, size_t bytes) {
    return overlap(storage, bytes, state, sizeof(*state)) ||
           overlap(storage, bytes, state->domain, sizeof(*state->domain)) ||
           overlap(storage, bytes, FDCAN1, sizeof(*FDCAN1)) ||
           overlap(storage, bytes, FDCAN2, sizeof(*FDCAN2)) ||
           overlap(storage, bytes, FDCAN3, sizeof(*FDCAN3)) ||
           overlap(storage, bytes, &g_nx_stm32g474_system,
                   sizeof(g_nx_stm32g474_system)) ||
           overlap(storage, bytes, &nx_stm32g474_fdcan_ops,
                   sizeof(nx_stm32g474_fdcan_ops)) ||
           overlap(storage, bytes, (const void*)(uintptr_t)SCS_BASE, 0x1000U) ||
           overlap(storage, bytes, state->domain->configuration,
                   sizeof(*state->domain->configuration)) ||
           overlap(storage, bytes, (const void*)state->domain->ram,
                   (size_t)3U * FDCAN_RAM_WORDS * sizeof(uint32_t)) ||
           overlap(storage, bytes, state->domain->rcc,
                   sizeof(*state->domain->rcc));
}
/** \brief Caller arrays are immutable filters and one bounded value ring. */
static bool arrays_valid(const nx_stm32g474_fdcan_state_t* state) {
    if (state->filter_count > 36U ||
        (state->filter_count != 0U && state->filters == NULL) ||
        state->events == NULL || state->capacity == 0U ||
        state->capacity > SIZE_MAX / sizeof(*state->events) ||
        state->poll_limit == 0U || state->drain_timeout_us == 0U) {
        return false;
    }
    size_t event_bytes = state->capacity * sizeof(*state->events);
    size_t filter_bytes = state->filter_count * sizeof(*state->filters);
    if (protected_storage(state, state->events, event_bytes) ||
        protected_storage(state, state->filters, filter_bytes) ||
        overlap(state->events, event_bytes, state->filters, filter_bytes)) {
        return false;
    }
    unsigned standard = 0U;
    unsigned extended = 0U;
    for (size_t i = 0U; i < state->filter_count; ++i) {
        const nx_can_fd_filter_t* filter = &state->filters[i];
        uint32_t limit =
            filter->format == NX_CAN_STANDARD ? 0x7FFU : 0x1FFFFFFFU;
        if ((filter->format != NX_CAN_STANDARD &&
             filter->format != NX_CAN_EXTENDED) ||
            filter->fifo > 1U || filter->id > limit || filter->mask > limit) {
            return false;
        }
        if (filter->format == NX_CAN_STANDARD) {
            ++standard;
        } else {
            ++extended;
        }
    }
    return standard <= 28U && extended <= 8U;
}
/** \brief Bounded acknowledgement reads do not require OS or tick progress. */
static bool wait_init(nx_stm32g474_fdcan_state_t* state, bool asserted) {
    for (uint32_t i = 0U; i < state->poll_limit; ++i) {
        if (((NX_G474_FDCAN_READ(&state->registers->CCCR) &
              (uint32_t)FDCAN_CCCR_INIT) != 0U) == asserted) {
            return true;
        }
    }
    return false;
}
/** \brief Preserve final protocol facts before configuration can clear them. */
static void errors(nx_stm32g474_fdcan_state_t* state);
/** \brief Enter configuration only after the task has drained every loan. */
static bool enter_init(nx_stm32g474_fdcan_state_t* state) {
    FDCAN_GlobalTypeDef* registers = state->registers;
    NX_G474_FDCAN_WRITE(&registers->CCCR, NX_G474_FDCAN_READ(&registers->CCCR) |
                                              (uint32_t)FDCAN_CCCR_INIT);
    if (!wait_init(state, true)) {
        return false;
    }
    errors(state);
    NX_G474_FDCAN_WRITE(&registers->CCCR, NX_G474_FDCAN_READ(&registers->CCCR) |
                                              (uint32_t)FDCAN_CCCR_CCE);
    return (NX_G474_FDCAN_READ(&registers->CCCR) & (uint32_t)FDCAN_CCCR_CCE) !=
           0U;
}
/** \brief Exact filter packing follows the pinned official mask definitions. */
static uint32_t program_filters(nx_stm32g474_fdcan_state_t* state) {
    uint32_t standard = 0U;
    uint32_t extended = 0U;
    for (size_t i = 0U; i < state->filter_count; ++i) {
        const nx_can_fd_filter_t* filter = &state->filters[i];
        uint32_t destination = (uint32_t)filter->fifo + 1U;
        if (filter->format == NX_CAN_STANDARD) {
            uint32_t word = (FDCAN_FILTER_MASK << 30U) | (destination << 27U) |
                            (filter->id << 16U) | filter->mask;
            NX_G474_FDCAN_WRITE(&state->domain->ram[standard], word);
            ++standard;
        } else {
            size_t offset = FDCAN_EXT_WORD + (size_t)extended * 2U;
            NX_G474_FDCAN_WRITE(&state->domain->ram[offset],
                                (destination << 29U) | filter->id);
            NX_G474_FDCAN_WRITE(&state->domain->ram[offset + 1U],
                                (FDCAN_FILTER_MASK << 30U) | filter->mask);
            ++extended;
        }
    }
    return (standard << FDCAN_RXGFC_LSS_Pos) |
           (extended << FDCAN_RXGFC_LSE_Pos) |
           (FDCAN_REJECT << FDCAN_RXGFC_ANFS_Pos) |
           (FDCAN_REJECT << FDCAN_RXGFC_ANFE_Pos) |
           (uint32_t)(FDCAN_RXGFC_RRFS | FDCAN_RXGFC_RRFE);
}
/** \brief Configure exact HSE24/500k/2M ISO FD normal or internal loopback. */
nx_result_t nx_stm32g474_fdcan_initialize(nx_stm32g474_fdcan_state_t* state) {
    if (state == NULL || state->registers != FDCAN1 || state->domain == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!clock_ready(state->domain) || !state->domain->retained) {
        return NX_ERROR_STATE;
    }
    if ((state->mode != NX_STM32G474_FDCAN_INTERNAL_LOOPBACK &&
         state->mode != NX_STM32G474_FDCAN_NORMAL) ||
        !arrays_valid(state) || state->irq_policy.priority_bits != 4U ||
        state->irq_policy.external_irq_count != 102U) {
        return NX_ERROR_INVALID;
    }
    if (state->retained || state->domain->acquired_mask != 0U ||
        !cold_controller(state->registers) ||
        !NX_G474_FDCAN_IRQ_AVAILABLE(FDCAN1_IT0_IRQn) ||
        !NX_G474_FDCAN_IRQ_AVAILABLE(FDCAN1_IT1_IRQn)) {
        return NX_ERROR_BUSY;
    }
    if (NX_G474_FDCAN_READ(&state->registers->ENDN) != 0x87654321U ||
        NX_G474_FDCAN_READ(&state->domain->configuration->CKDIV) != 0U) {
        return NX_ERROR_IO;
    }
    state->sources[0] = (nx_irq_source_t){FDCAN1_IT0_IRQn, FDCAN_FIXED_PRIORITY,
                                          (uint8_t)NX_G474_FDCAN_IRQ_GROUP()};
    state->sources[1] = (nx_irq_source_t){FDCAN1_IT1_IRQn, FDCAN_FIXED_PRIORITY,
                                          (uint8_t)NX_G474_FDCAN_IRQ_GROUP()};
    nx_result_t result =
        nx_irq_wake_validate(NULL, &state->irq_policy, &state->sources[0]);
    if (result != NX_SUCCESS) {
        return result;
    }
    state->retained = true;
    state->domain->acquired_mask = 1U;
    state->snapshot = (nx_can_fd_state_t){0};
    state->snapshot.controller.closing = true;
    if (!enter_init(state)) {
        return NX_ERROR_IO;
    }
    FDCAN_GlobalTypeDef* registers = state->registers;
    uint32_t control = selected_control(state);
    uint32_t test = state->mode == NX_STM32G474_FDCAN_INTERNAL_LOOPBACK
                        ? (uint32_t)FDCAN_TEST_LBCK
                        : 0U;
    NX_G474_FDCAN_WRITE(&registers->CCCR,
                        control | (uint32_t)(FDCAN_CCCR_INIT | FDCAN_CCCR_CCE));
    NX_G474_FDCAN_WRITE(&registers->TEST, test);
    NX_G474_FDCAN_WRITE(&registers->NBTP, 0x02020B02U);
    NX_G474_FDCAN_WRITE(&registers->DBTP, 0x00000811U);
    NX_G474_FDCAN_WRITE(&registers->TSCC, 0U);
    NX_G474_FDCAN_WRITE(&registers->TXBC, (uint32_t)FDCAN_TXBC_TFQM);
    NX_G474_FDCAN_WRITE(&registers->XIDAM, 0x1FFFFFFFU);
    for (size_t i = 0U; i < FDCAN_RAM_WORDS; ++i) {
        NX_G474_FDCAN_WRITE(&state->domain->ram[i], 0U);
    }
    uint32_t global_filters = program_filters(state);
    NX_G474_FDCAN_WRITE(&registers->RXGFC, global_filters);
    NX_G474_FDCAN_WRITE(&registers->TXBTIE, 7U);
    NX_G474_FDCAN_WRITE(&registers->TXBCIE, 7U);
    NX_G474_FDCAN_WRITE(&registers->ILS, 0U);
    NX_G474_FDCAN_WRITE(&registers->IR, UINT32_MAX);
    nx_arch_dsb();
    if (NX_G474_FDCAN_READ(&registers->NBTP) != 0x02020B02U ||
        NX_G474_FDCAN_READ(&registers->DBTP) != 0x00000811U ||
        NX_G474_FDCAN_READ(&registers->RXGFC) != global_filters ||
        NX_G474_FDCAN_READ(&registers->TXBC) != (uint32_t)FDCAN_TXBC_TFQM ||
        NX_G474_FDCAN_READ(&registers->TSCC) != 0U ||
        NX_G474_FDCAN_READ(&registers->XIDAM) != 0x1FFFFFFFU ||
        (NX_G474_FDCAN_READ(&registers->TEST) &
         (uint32_t)(FDCAN_TEST_LBCK | FDCAN_TEST_TX)) != test ||
        (NX_G474_FDCAN_READ(&registers->CCCR) & FDCAN_MODE_MASK) != control) {
        return NX_ERROR_IO;
    }
    NX_G474_FDCAN_WRITE(&registers->CCCR, control);
    if (!wait_init(state, false)) {
        return NX_ERROR_IO;
    }
    state->head = state->count = 0U;
    state->lost = 0U;
    state->loss_flags = 0U;
    state->wake = NULL;
    state->wake_publishers = 0U;
    for (unsigned i = 0U; i < 3U; ++i) {
        state->slots[i] = (nx_stm32g474_fdcan_tx_slot_t){0};
    }
    state->snapshot.controller.started = true;
    NX_G474_FDCAN_WRITE(&registers->IE, FDCAN_INTERRUPTS);
    NX_G474_FDCAN_WRITE(&registers->ILE, (uint32_t)FDCAN_ILE_EINT0);
    result =
        NX_G474_FDCAN_IRQ_CONTROL(FDCAN1_IT0_IRQn, true, FDCAN_FIXED_PRIORITY);
    nx_arch_dsb();
    if (result != NX_SUCCESS ||
        NX_G474_FDCAN_READ(&registers->IE) != FDCAN_INTERRUPTS ||
        NX_G474_FDCAN_READ(&registers->ILE) != (uint32_t)FDCAN_ILE_EINT0 ||
        NX_G474_FDCAN_READ(&registers->ILS) != 0U ||
        NX_G474_FDCAN_IRQ_PRIORITY(FDCAN1_IT0_IRQn) != FDCAN_FIXED_PRIORITY) {
        return NX_ERROR_IO;
    }
    state->snapshot.controller.closing = false;
    return NX_SUCCESS;
}

/** \brief Preserve ordered loss boundaries rather than replacing old frames. */
static void note_loss(nx_stm32g474_fdcan_state_t* state, uint32_t flags) {
    if (state->lost == 0U) {
        state->loss_time = nx_time_now_us();
    }
    if (state->lost == UINT64_MAX) {
        state->loss_flags |= NX_CAN_LOSS_SATURATED;
    } else {
        ++state->lost;
    }
    state->loss_flags |= flags;
}
/** \brief Append the loss after older records and before a new frame. */
static void push_loss(nx_stm32g474_fdcan_state_t* state) {
    if (state->lost != 0U && state->count < state->capacity) {
        size_t tail = (state->head + state->count) % state->capacity;
        state->events[tail] =
            (nx_can_fd_event_t){.acquired_us = state->loss_time,
                                .lost = state->lost,
                                .flags = state->loss_flags,
                                .kind = NX_CAN_EVENT_LOSS};
        ++state->count;
        state->lost = 0U;
        state->loss_flags = 0U;
    }
}
/** \brief Protocol errors remain authoritative when the receive ring is full.
 */
static void errors(nx_stm32g474_fdcan_state_t* state) {
    uint32_t protocol = NX_G474_FDCAN_READ(&state->registers->PSR);
    uint32_t counters = NX_G474_FDCAN_READ(&state->registers->ECR);
    state->snapshot.controller.bus_off |=
        (protocol & (uint32_t)FDCAN_PSR_BO) != 0U;
    state->snapshot.controller.transmit_errors = (uint8_t)counters;
    state->snapshot.controller.receive_errors =
        (uint8_t)((counters >> FDCAN_ECR_REC_Pos) & 127U);
    uint8_t nominal = (uint8_t)(protocol & 7U);
    uint8_t data = (uint8_t)((protocol >> FDCAN_PSR_DLEC_Pos) & 7U);
    /* PSR reads replace the last-error code with7 (no new event). */
    if (nominal != 7U) {
        state->snapshot.controller.last_error = nominal;
    }
    if (data != 7U) {
        state->snapshot.data_last_error = data;
    }
    state->snapshot.error_warning = (protocol & (uint32_t)FDCAN_PSR_EW) != 0U;
    state->snapshot.error_passive = (protocol & (uint32_t)FDCAN_PSR_EP) != 0U;
}
/** \brief Cancellation requests cannot revoke an outstanding physical slot. */
static void drain(nx_stm32g474_fdcan_state_t* state, unsigned index,
                  nx_result_t reason, nx_time_us_t now) {
    nx_stm32g474_fdcan_tx_slot_t* slot = &state->slots[index];
    if (slot->terminal == NX_SUCCESS) {
        slot->terminal = reason;
        slot->drain_deadline = nx_deadline_after(now, state->drain_timeout_us);
        (void)nx_request_transition(&slot->request->base, NX_REQUEST_DRAINING);
    }
    NX_G474_FDCAN_WRITE(&state->registers->TXBCR, 1U << index);
}
/** \brief Empty plus a real per-slot outcome precede final detachment. */
static void transmit(nx_stm32g474_fdcan_state_t* state, nx_time_us_t now) {
    FDCAN_GlobalTypeDef* registers = state->registers;
    for (unsigned i = 0U; i < 3U; ++i) {
        nx_stm32g474_fdcan_tx_slot_t* slot = &state->slots[i];
        if (slot->request == NULL) {
            continue;
        }
        uint32_t bit = 1U << i;
        /* BAR survives the hardware add scan. BRP=0 alone proves nothing.
         * Observe both source-withdrawal facts before fresh outcome reads. */
        uint32_t pending = NX_G474_FDCAN_READ(&registers->TXBAR) |
                           NX_G474_FDCAN_READ(&registers->TXBRP);
        nx_arch_dmb();
        uint32_t sent = NX_G474_FDCAN_READ(&registers->TXBTO);
        uint32_t cancelled = NX_G474_FDCAN_READ(&registers->TXBCF);
        if ((pending & bit) == 0U && ((sent | cancelled) & bit) != 0U) {
            nx_can_fd_tx_request_t* request = slot->request;
            nx_result_t result = (sent & bit) != 0U             ? NX_SUCCESS
                                 : slot->terminal != NX_SUCCESS ? slot->terminal
                                                                : NX_ERROR_IO;
            size_t bytes =
                result == NX_SUCCESS ? request->frame.payload_bytes : 0U;
            *slot = (nx_stm32g474_fdcan_tx_slot_t){0};
            --state->snapshot.controller.pending;
            nx_request_settle(&request->base, result, bytes);
            continue;
        }
        if (slot->terminal == NX_SUCCESS) {
            if (state->snapshot.controller.bus_off) {
                drain(state, i, NX_ERROR_IO, now);
            } else if (state->snapshot.controller.closing) {
                drain(state, i, NX_ERROR_CANCELLED, now);
            } else if (nx_deadline_expired(slot->request->base.deadline, now)) {
                drain(state, i, NX_ERROR_TIMEOUT, now);
            }
        } else if (nx_deadline_expired(slot->drain_deadline, now)) {
            (void)nx_request_transition(&slot->request->base,
                                        NX_REQUEST_QUARANTINED);
        }
    }
}
/** \brief DLC lookup is immutable wire encoding, not per-instance storage. */
static const uint8_t payload_bytes[16] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U};
/** \brief Consume at most the initial three elements of each blocking FIFO. */
static void receive(nx_stm32g474_fdcan_state_t* state, uint32_t interrupts) {
    FDCAN_GlobalTypeDef* registers = state->registers;
    for (unsigned fifo = 0U; fifo < 2U; ++fifo) {
        volatile uint32_t* status_register =
            fifo == 0U ? &registers->RXF0S : &registers->RXF1S;
        volatile uint32_t* ack =
            fifo == 0U ? &registers->RXF0A : &registers->RXF1A;
        uint32_t status = NX_G474_FDCAN_READ(status_register);
        unsigned budget = status & 15U;
        if (budget > 3U) {
            state->snapshot.controller.closing = true;
            return;
        }
        for (unsigned item = 0U; item < budget; ++item) {
            status = NX_G474_FDCAN_READ(status_register);
            unsigned index = (status >> 8U) & 3U;
            if (index >= 3U || (status & 15U) == 0U) {
                state->snapshot.controller.closing = true;
                return;
            }
            size_t offset = (fifo == 0U ? FDCAN_RX0_WORD : FDCAN_RX1_WORD) +
                            (size_t)index * FDCAN_ELEMENT_WORDS;
            uint32_t identifier =
                NX_G474_FDCAN_READ(&state->domain->ram[offset]);
            uint32_t property =
                NX_G474_FDCAN_READ(&state->domain->ram[offset + 1U]);
            nx_can_fd_event_t event = {
                .acquired_us = nx_time_now_us(),
                .flags = NX_CAN_FD_TIMESTAMP_UNAVAILABLE,
                .kind = NX_CAN_EVENT_FRAME,
                .hardware_timestamp = 0U,
                .fifo = (uint8_t)fifo,
                .filter = (uint8_t)((property >> 24U) & 127U),
                .raw_dlc = (uint8_t)((property >> 16U) & 15U),
                .error_passive = (identifier & (1U << 31U)) != 0U,
                .nonmatching = (property & (1U << 31U)) != 0U};
            event.frame.format = (identifier & FDCAN_EXTENDED_ID) != 0U
                                     ? NX_CAN_EXTENDED
                                     : NX_CAN_STANDARD;
            event.frame.id = event.frame.format == NX_CAN_STANDARD
                                 ? (identifier >> 18U) & 0x7FFU
                                 : identifier & 0x1FFFFFFFU;
            event.frame.kind = (identifier & FDCAN_REMOTE_FRAME) != 0U
                                   ? NX_CAN_REMOTE
                                   : NX_CAN_DATA;
            event.frame.wire = (property & FDCAN_FD_CAN) != 0U
                                   ? NX_CAN_WIRE_FD
                                   : NX_CAN_WIRE_CLASSIC;
            event.frame.brs = (property & FDCAN_BRS_ON) != 0U;
            event.frame.payload_bytes = event.frame.wire == NX_CAN_WIRE_FD
                                            ? payload_bytes[event.raw_dlc]
                                        : event.raw_dlc > 8U ? 8U
                                                             : event.raw_dlc;
            if (event.frame.wire == NX_CAN_WIRE_CLASSIC && event.raw_dlc > 8U) {
                event.flags |= NX_CAN_DLC_NORMALIZED;
            }
            if (event.frame.kind == NX_CAN_DATA) {
                for (unsigned byte = 0U; byte < event.frame.payload_bytes;
                     byte += 4U) {
                    uint32_t word = NX_G474_FDCAN_READ(
                        &state->domain->ram[offset + 2U + byte / 4U]);
                    for (unsigned part = 0U;
                         part < 4U && byte + part < event.frame.payload_bytes;
                         ++part) {
                        event.frame.data[byte + part] =
                            (uint8_t)(word >> (part * 8U));
                    }
                }
            }
            NX_G474_FDCAN_WRITE(ack, index);
            nx_arch_dsb();
            push_loss(state);
            if (state->count < state->capacity) {
                size_t tail = (state->head + state->count) % state->capacity;
                state->events[tail] = event;
                ++state->count;
            } else {
                note_loss(state, NX_CAN_LOSS_SOFTWARE);
            }
        }
        uint32_t lost =
            fifo == 0U ? (uint32_t)FDCAN_IR_RF0L : (uint32_t)FDCAN_IR_RF1L;
        if ((interrupts & lost) != 0U) {
            note_loss(state, NX_CAN_LOSS_HARDWARE_LOWER_BOUND);
        }
    }
}
/** \brief IRQ uses bounded physical facts and signals only after unlocking. */
void nx_stm32g474_fdcan_irq(nx_stm32g474_fdcan_state_t* state) {
    if (state == NULL || !nx_arch_is_privileged()) {
        return;
    }
    nx_time_us_t now = nx_time_now_us();
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (!state->retained || !state->snapshot.controller.started ||
        state->wake_publishers == UINT32_MAX || !clock_ready(state->domain)) {
        nx_arch_irq_restore(mask);
        return;
    }
    ++state->wake_publishers;
    const nx_irq_wake_t* wake = state->wake;
    uint32_t interrupts = NX_G474_FDCAN_READ(&state->registers->IR);
    errors(state);
    transmit(state, now);
    /* Acknowledge captured W1C facts before acquiring new FIFO records. */
    NX_G474_FDCAN_WRITE(&state->registers->IR, interrupts & FDCAN_INTERRUPTS);
    if (!state->snapshot.controller.closing) {
        receive(state, interrupts);
    }
    nx_arch_irq_restore(mask);
    (void)nx_irq_wake_signal(wake);
    mask = nx_arch_irq_save();
    --state->wake_publishers;
    nx_arch_irq_restore(mask);
}
/** \brief Reject control/output aliasing with provider-owned value storage. */
static bool external_storage(const nx_stm32g474_fdcan_state_t* state,
                             const void* storage, size_t bytes) {
    if (protected_storage(state, storage, bytes) ||
        overlap(storage, bytes, state->events,
                state->capacity * sizeof(*state->events)) ||
        overlap(storage, bytes, state->filters,
                state->filter_count * sizeof(*state->filters)) ||
        overlap(storage, bytes, state->wake,
                state->wake == NULL ? 0U : sizeof(*state->wake))) {
        return false;
    }
    /* IRQ completion may detach slots. Observe their exact loan spans in the
     * same exclusion domain; no payload copy or callback occurs here. */
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    bool valid = true;
    for (unsigned i = 0U; i < 3U; ++i) {
        const nx_can_fd_tx_request_t* request = state->slots[i].request;
        if (request != NULL &&
            overlap(storage, bytes, request, sizeof(*request))) {
            valid = false;
            break;
        }
    }
    nx_arch_irq_restore(mask);
    return valid;
}
/** \brief Physical queue put index, not software admission order, selects RAM.
 */
static nx_result_t submit(void* context, nx_can_fd_tx_request_t* request) {
    nx_stm32g474_fdcan_state_t* state = context;
    if (state == NULL || request == NULL || state->domain == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!external_storage(state, request, sizeof(*request)) ||
        !nx_can_fd_frame_valid(&request->frame)) {
        return NX_ERROR_INVALID;
    }
    if (request->frame.kind != NX_CAN_DATA) {
        return NX_ERROR_UNSUPPORTED;
    }
    nx_time_us_t now = nx_time_now_us();
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (!state->retained || !state->snapshot.controller.started ||
        state->snapshot.controller.closing ||
        state->snapshot.controller.bus_off || !clock_ready(state->domain)) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_STATE;
    }
    if (nx_deadline_expired(request->base.deadline, now)) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_TIMEOUT;
    }
    uint32_t queue = NX_G474_FDCAN_READ(&state->registers->TXFQS);
    unsigned index = (queue >> FDCAN_TXFQS_TFQPI_Pos) & 3U;
    if (index >= 3U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_IO;
    }
    uint32_t pending = NX_G474_FDCAN_READ(&state->registers->TXBRP) |
                       NX_G474_FDCAN_READ(&state->registers->TXBAR) |
                       NX_G474_FDCAN_READ(&state->registers->TXBCR);
    if ((queue & (uint32_t)FDCAN_TXFQS_TFQF) != 0U ||
        (pending & (1U << index)) != 0U ||
        state->slots[index].request != NULL) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    nx_result_t result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS) {
        nx_arch_irq_restore(mask);
        return result;
    }
    state->slots[index] = (nx_stm32g474_fdcan_tx_slot_t){
        .request = request, .drain_deadline = NX_DEADLINE_NEVER, .added = true};
    ++state->snapshot.controller.pending;
    const nx_can_fd_frame_t* frame = &request->frame;
    uint32_t identifier = frame->format == NX_CAN_STANDARD
                              ? frame->id << 18U
                              : frame->id | FDCAN_EXTENDED_ID;
    unsigned dlc = 0U;
    while (payload_bytes[dlc] != frame->payload_bytes) {
        ++dlc;
    }
    uint32_t property = (uint32_t)dlc << 16U;
    if (frame->wire == NX_CAN_WIRE_FD) {
        property |= FDCAN_FD_CAN;
    }
    if (frame->brs) {
        property |= FDCAN_BRS_ON;
    }
    size_t offset = FDCAN_TX_WORD + (size_t)index * FDCAN_ELEMENT_WORDS;
    NX_G474_FDCAN_WRITE(&state->domain->ram[offset], identifier);
    NX_G474_FDCAN_WRITE(&state->domain->ram[offset + 1U], property);
    for (unsigned byte = 0U; byte < frame->payload_bytes; byte += 4U) {
        uint32_t word = 0U;
        for (unsigned part = 0U;
             part < 4U && byte + part < frame->payload_bytes; ++part) {
            word |= (uint32_t)frame->data[byte + part] << (part * 8U);
        }
        NX_G474_FDCAN_WRITE(&state->domain->ram[offset + 2U + byte / 4U], word);
    }
    nx_arch_dsb();
    NX_G474_FDCAN_WRITE(&state->registers->TXBAR, 1U << index);
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Match retained identity before touching a cancellation descriptor. */
static nx_result_t cancel(void* context, nx_can_fd_tx_request_t* request) {
    nx_stm32g474_fdcan_state_t* state = context;
    if (state == NULL || request == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->retained || !clock_ready(state->domain)) {
        return NX_ERROR_STATE;
    }
    nx_time_us_t now = nx_time_now_us();
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    for (unsigned i = 0U; i < 3U; ++i) {
        if (state->slots[i].request == request) {
            errors(state);
            transmit(state, now);
            if (state->slots[i].request != NULL) {
                drain(state, i, NX_ERROR_CANCELLED, now);
                transmit(state, now);
            }
            nx_arch_irq_restore(mask);
            return NX_SUCCESS;
        }
    }
    nx_arch_irq_restore(mask);
    return NX_ERROR_STATE;
}
/** \brief Poll the same bounded physical mechanism as the reserved IRQ. */
static nx_result_t service(void* context) {
    nx_stm32g474_fdcan_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->retained || !state->snapshot.controller.started ||
        !clock_ready(state->domain)) {
        return NX_ERROR_STATE;
    }
    nx_stm32g474_fdcan_irq(state);
    return NX_SUCCESS;
}
/** \brief Unretained outputs cannot overlap any provider-owned storage. */
static nx_result_t read_events(void* context, nx_can_fd_event_t* events,
                               size_t capacity, size_t* count) {
    nx_stm32g474_fdcan_state_t* state = context;
    if (state == NULL || events == NULL || count == NULL || capacity == 0U ||
        state->domain == NULL || capacity > SIZE_MAX / sizeof(*events)) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!external_storage(state, events, capacity * sizeof(*events)) ||
        !external_storage(state, count, sizeof(*count)) ||
        overlap(events, capacity * sizeof(*events), count, sizeof(*count))) {
        return NX_ERROR_INVALID;
    }
    *count = 0U;
    if (!state->snapshot.controller.started) {
        return NX_ERROR_STATE;
    }
    while (*count < capacity) {
        nx_arch_irq_state_t mask = nx_arch_irq_save();
        push_loss(state);
        if (state->count == 0U) {
            nx_arch_irq_restore(mask);
            break;
        }
        nx_can_fd_event_t event = state->events[state->head];
        state->head = (state->head + 1U) % state->capacity;
        --state->count;
        nx_arch_irq_restore(mask);
        events[*count] = event;
        ++*count;
    }
    return *count == 0U ? NX_ERROR_EMPTY : NX_SUCCESS;
}
/** \brief Authoritative protocol state is independent of RX ring capacity. */
static nx_result_t controller_state(void* context, nx_can_fd_state_t* output) {
    nx_stm32g474_fdcan_state_t* state = context;
    if (state == NULL || output == NULL || state->domain == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!external_storage(state, output, sizeof(*output))) {
        return NX_ERROR_INVALID;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    *output = state->snapshot;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Close admission and withdraw sources before releasing any borrow. */
static nx_result_t stop(void* context) {
    nx_stm32g474_fdcan_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->retained) {
        return NX_SUCCESS;
    }
    if (!clock_ready(state->domain)) {
        return NX_ERROR_STATE;
    }
    nx_time_us_t now = nx_time_now_us();
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    state->snapshot.controller.closing = true;
    transmit(state, now);
    for (unsigned i = 0U; i < 3U; ++i) {
        if (state->slots[i].request != NULL) {
            drain(state, i, NX_ERROR_CANCELLED, now);
        }
    }
    transmit(state, now);
    if (state->snapshot.controller.pending != 0U ||
        state->wake_publishers != 0U ||
        (NX_G474_FDCAN_READ(&state->registers->TXBRP) |
         NX_G474_FDCAN_READ(&state->registers->TXBAR) |
         NX_G474_FDCAN_READ(&state->registers->TXBCR)) != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    NX_G474_FDCAN_WRITE(&state->registers->IE, 0U);
    NX_G474_FDCAN_WRITE(&state->registers->ILE, 0U);
    nx_result_t result =
        NX_G474_FDCAN_IRQ_CONTROL(FDCAN1_IT0_IRQn, false, FDCAN_FIXED_PRIORITY);
    nx_arch_dsb();
    bool detached = result == NX_SUCCESS &&
                    NX_G474_FDCAN_READ(&state->registers->IE) == 0U &&
                    NX_G474_FDCAN_READ(&state->registers->ILE) == 0U &&
                    NX_G474_FDCAN_IRQ_AVAILABLE(FDCAN1_IT0_IRQn);
    nx_arch_irq_restore(mask);
    if (!detached || !enter_init(state)) {
        return NX_ERROR_BUSY;
    }
    mask = nx_arch_irq_save();
    if (state->wake_publishers != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    state->snapshot.controller.started = false;
    state->wake = NULL;
    state->retained = false;
    state->domain->acquired_mask = 0U;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Explicit recovery retains identity and never replays a frame. */
static nx_result_t recover(void* context) {
    nx_stm32g474_fdcan_state_t* state = context;
    if (state == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!state->retained || !state->snapshot.controller.started ||
        !clock_ready(state->domain)) {
        return NX_ERROR_STATE;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (state->snapshot.controller.pending != 0U ||
        state->wake_publishers != 0U ||
        (NX_G474_FDCAN_READ(&state->registers->TXBRP) |
         NX_G474_FDCAN_READ(&state->registers->TXBAR) |
         NX_G474_FDCAN_READ(&state->registers->TXBCR)) != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    if (state->snapshot.controller.closing) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_STATE;
    }
    state->snapshot.controller.closing = true;
    nx_arch_irq_restore(mask);
    if (!enter_init(state)) {
        return NX_ERROR_IO;
    }
    NX_G474_FDCAN_WRITE(&state->registers->CCCR, selected_control(state));
    if (!wait_init(state, false)) {
        return NX_ERROR_IO;
    }
    mask = nx_arch_irq_save();
    state->snapshot.controller.bus_off = false;
    errors(state);
    state->snapshot.controller.closing = false;
    nx_arch_irq_restore(mask);
    return state->snapshot.controller.bus_off ? NX_ERROR_BUSY : NX_SUCCESS;
}
/** \brief Only enabled line0 imposes kernel policy on a borrowed wake sink. */
static nx_result_t attach_wake(void* context, const nx_irq_wake_t* wake,
                               uint8_t syscall_ceiling) {
    nx_stm32g474_fdcan_state_t* state = context;
    if (state == NULL || state->domain == NULL) {
        return NX_ERROR_INVALID;
    }
    if (!task_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (wake != NULL && !external_storage(state, wake, sizeof(*wake))) {
        return NX_ERROR_INVALID;
    }
    if (!state->retained || !state->snapshot.controller.started ||
        state->snapshot.controller.closing || !clock_ready(state->domain)) {
        return NX_ERROR_STATE;
    }
    nx_irq_policy_t policy = state->irq_policy;
    if (wake != NULL && wake->calls_kernel) {
        if (policy.kernel == NX_IRQ_KERNEL_PRIMASK && syscall_ceiling != 0U) {
            return NX_ERROR_INVALID;
        }
        if (policy.kernel == NX_IRQ_KERNEL_BASEPRI &&
            syscall_ceiling > policy.syscall_ceiling) {
            policy.syscall_ceiling = syscall_ceiling;
        }
    }
    nx_irq_source_t source = state->sources[0];
    source.priority = (uint8_t)NX_G474_FDCAN_IRQ_PRIORITY(FDCAN1_IT0_IRQn);
    source.priority_group = (uint8_t)NX_G474_FDCAN_IRQ_GROUP();
    nx_result_t result = nx_irq_wake_validate(wake, &policy, &source);
    if (result != NX_SUCCESS) {
        return result;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    if (state->wake_publishers != 0U) {
        nx_arch_irq_restore(mask);
        return NX_ERROR_BUSY;
    }
    state->wake = wake;
    nx_arch_irq_restore(mask);
    return NX_SUCCESS;
}
/** \brief Optional typed interface leaves classic CAN tables and RAM intact. */
const nx_can_fd_ops_t nx_stm32g474_fdcan_ops = {
    submit,           cancel, service, read_events,
    controller_state, stop,   recover, attach_wake};
