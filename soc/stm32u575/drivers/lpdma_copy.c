/**
 * \file            lpdma_copy.c
 * \brief           Exact SRAM4 two-block receiver and compound final access
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-20
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/io/dma.h"
#include "stm32u575_lpdma_copy_hw.h"
#include "stm32u575_lpdma_copy_internal.h"
#include "stm32u5xx_ll_dma.h"
#include <string.h>
nx_result_t nx_stm32u575_retained_time_validate_alarm_a_copy(
    nx_stm32u575_retained_time_storage_t*, const void*, uint64_t);
_Static_assert(LPDMA1_Channel0_IRQn == 114 && SRAM4_BASE_NS == 0x28000000U &&
                   SRAM4_SIZE == 16384U &&
                   offsetof(DMA_Channel_TypeDef, CSR) == 0x10U &&
                   offsetof(DMA_Channel_TypeDef, CLLR) == 0x7cU,
               "unreviewed U575 LPDMA IP");
_Static_assert(LL_LPDMA1_TRIGGER_RTC_ALRA_TRG == 14U,
               "unreviewed official RTC Alarm A trigger route");
#define IRQ_WORD (114U / 32U)
#define IRQ_BIT  (1U << (114U % 32U))
#define IRQ_ALL  (15U << (114U % 32U))
#define EVENTS                                                                 \
    (DMA_CSR_TCF | DMA_CSR_HTF | DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF |  \
     DMA_CSR_SUSPF | DMA_CSR_TOF)
#define ERRORS (DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF | DMA_CSR_TOF)
#define INTERRUPTS                                                             \
    (DMA_CCR_TCIE | DMA_CCR_DTEIE | DMA_CCR_ULEIE | DMA_CCR_USEIE |            \
     DMA_CCR_TOIE)
#define SLEEP_BITS (RCC_AHB3SMENR_LPDMA1SMEN | RCC_AHB3SMENR_SRAM4SMEN)
#define AUTO_BITS  (RCC_SRDAMR_LPDMA1AMEN | RCC_SRDAMR_SRAM4AMEN)
#define UPDATE_BITS                                                            \
    (DMA_CLLR_UT1 | DMA_CLLR_UT2 | DMA_CLLR_UB1 | DMA_CLLR_USA |               \
     DMA_CLLR_UDA | DMA_CLLR_ULL)
#define DATA_BITS  (DMA_CTR1_SINC | DMA_CTR1_DINC)
#define FINAL_BITS (DMA_CTR2_SWREQ | DMA_CTR2_TCEM)
#define FIRST_BITS                                                             \
    (FINAL_BITS | DMA_CTR2_TRIGPOL_0 | (14U << DMA_CTR2_TRIGSEL_Pos))
typedef struct {
    const void* self;
    nx_stm32u575_retained_time_storage_t* clock;
    const void* wake_owner;
    nx_stm32u575_lpdma_copy_request_t* request;
    const void* source;
    void* destination;
    nx_stm32u575_lpdma_copy_workspace_t* workspace;
    uint64_t generation;
    size_t length, first, transferred;
    nx_time_us_t deadline;
    uint32_t sleep, autonomous, privilege;
    nx_result_t failure;
    bool opened, closing, configured, reset_done, drained, completed, failed,
        reset_uncertain;
} owner_t;
_Static_assert(sizeof(owner_t) == NX_U575_LPDMA_COPY_STORAGE_BYTES &&
                   _Alignof(owner_t) ==
                       _Alignof(nx_stm32u575_lpdma_copy_storage_t),
               "copy owner ABI");
/* One fixed physical-role reservation, never a pool or name registry. */
static nx_stm32u575_lpdma_copy_storage_t* claim;
static bool span(const void* p, size_t bytes, size_t alignment) {
    uintptr_t begin = (uintptr_t)p;
    return p != NULL && bytes != 0U && begin % alignment == 0U &&
           bytes <= UINTPTR_MAX - begin &&
           !(begin < UINT32_C(0x60000000) &&
             UINT32_C(0x40000000) < begin + bytes) &&
           !(begin < UINT32_C(0xe0100000) &&
             UINT32_C(0xe0000000) < begin + bytes);
}
static bool overlap(const void* a, size_t an, const void* b, size_t bn) {
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}
static bool context(bool masked) {
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    return nx_arch_is_privileged() && nx_arch_exception_number() == 0U &&
           masks.primask == (masked ? 1U : 0U) && masks.basepri == 0U &&
           masks.faultmask == 0U;
}
static bool profile(owner_t* owner) {
    bool reset = (NX_LPDMA_READ(RCC->AHB3RSTR) & RCC_AHB3RSTR_LPDMA1RST) != 0U;
    if (owner != NULL && reset)
        owner->reset_uncertain = true;
    /* External reset is outside the exclusive-owner contract. Reset-default
     * IDLE cannot join a bus transaction that was already in flight. */
    if (reset || (owner != NULL && owner->reset_uncertain))
        return false;
    uint32_t needed =
        RCC_AHB3ENR_PWREN | RCC_AHB3ENR_SRAM4EN | RCC_AHB3ENR_LPDMA1EN;
    if ((NX_LPDMA_READ(RCC->AHB3ENR) & needed) != needed)
        return false;
    uint32_t selection = NX_LPDMA_READ(RCC->ICSCR1);
    uint32_t range =
        (uint32_t)((selection & RCC_ICSCR1_MSIRGSEL) == 0U
                       ? (NX_LPDMA_READ(RCC->CSR) & RCC_CSR_MSISSRANGE) >>
                             RCC_CSR_MSISSRANGE_Pos
                       : (selection & RCC_ICSCR1_MSISRANGE) >>
                             RCC_ICSCR1_MSISRANGE_Pos);
    uint32_t kernel_range =
        (uint32_t)((selection & RCC_ICSCR1_MSIRGSEL) == 0U
                       ? (NX_LPDMA_READ(RCC->CSR) & RCC_CSR_MSIKSRANGE) >>
                             RCC_CSR_MSIKSRANGE_Pos
                       : (selection & RCC_ICSCR1_MSIKRANGE) >>
                             RCC_ICSCR1_MSIKRANGE_Pos);
    uint32_t allowed =
        RCC_CR_MSISON | RCC_CR_MSISRDY | RCC_CR_MSIKON | RCC_CR_MSIKRDY;
    uint32_t oscillator = NX_LPDMA_READ(RCC->CR);
    return range == 4U && kernel_range == 4U && (oscillator & ~allowed) == 0U &&
           (oscillator & (RCC_CR_MSISON | RCC_CR_MSISRDY)) ==
               (RCC_CR_MSISON | RCC_CR_MSISRDY) &&
           NX_LPDMA_READ(RCC->CFGR1) == 0U && NX_LPDMA_READ(RCC->CFGR2) == 0U &&
           NX_LPDMA_READ(RCC->CFGR3) == 0U &&
           (NX_LPDMA_READ(PWR->VOSR) & (PWR_VOSR_VOS | PWR_VOSR_VOSRDY |
                                        PWR_VOSR_BOOSTEN)) == PWR_VOSR_VOSRDY &&
           (NX_LPDMA_READ(PWR->CR1) & PWR_CR1_SRAM4PD) == 0U &&
           (NX_LPDMA_READ(PWR->CR2) & PWR_CR2_SRAM4PDS) == 0U &&
           (NX_LPDMA_READ(FLASH->OPTR) &
            (FLASH_OPTR_TZEN | FLASH_OPTR_SWAP_BANK)) == 0U &&
           (NX_LPDMA_READ(ICACHE->CR) & ICACHE_CR_EN) == 0U &&
           (NX_LPDMA_READ(DCACHE1->CR) & DCACHE_CR_EN) == 0U;
}
static bool checked(volatile uint32_t* p, uint32_t value) {
    NX_LPDMA_WRITE(*p, value);
    nx_arch_dsb();
    return NX_LPDMA_READ(*p) == value;
}
static bool update(volatile uint32_t* p, uint32_t bits, uint32_t value) {
    return checked(p, (NX_LPDMA_READ(*p) & ~bits) | (value & bits));
}
static bool wait_bits(volatile const uint32_t* p, uint32_t bits,
                      uint32_t expected, uint32_t limit) {
    while (limit-- != 0U)
        if ((NX_LPDMA_READ(*p) & bits) == expected)
            return true;
    return false;
}
static bool vector_quiet(uint32_t bits) {
    return (NX_LPDMA_READ(NVIC->ISER[IRQ_WORD]) & bits) == 0U &&
           (NX_LPDMA_READ(NVIC->ISPR[IRQ_WORD]) & bits) == 0U &&
           (NX_LPDMA_READ(NVIC->IABR[IRQ_WORD]) & bits) == 0U;
}
static bool registers_clear(void) {
    return NX_LPDMA_READ(LPDMA1_Channel0->CCR) == 0U &&
           NX_LPDMA_READ(LPDMA1_Channel0->CLBAR) == 0U &&
           NX_LPDMA_READ(LPDMA1_Channel0->CTR1) == 0U &&
           NX_LPDMA_READ(LPDMA1_Channel0->CTR2) == 0U &&
           NX_LPDMA_READ(LPDMA1_Channel0->CBR1) == 0U &&
           NX_LPDMA_READ(LPDMA1_Channel0->CSAR) == 0U &&
           NX_LPDMA_READ(LPDMA1_Channel0->CDAR) == 0U &&
           NX_LPDMA_READ(LPDMA1_Channel0->CLLR) == 0U &&
           NX_LPDMA_READ(LPDMA1_Channel0->CSR) == DMA_CSR_IDLEF;
}
static bool baseline(void) {
    DMA_Channel_TypeDef* channels[] = {LPDMA1_Channel0, LPDMA1_Channel1,
                                       LPDMA1_Channel2, LPDMA1_Channel3};
    if ((NX_LPDMA_READ(LPDMA1->SECCFGR) & DMA_SECCFGR_SEC0) != 0U ||
        (NX_LPDMA_READ(LPDMA1->RCFGLOCKR) & 1U) != 0U)
        return false;
    for (size_t n = 0U; n < 4U; ++n)
        if (NX_LPDMA_READ(channels[n]->CCR) != 0U ||
            NX_LPDMA_READ(channels[n]->CSR) != DMA_CSR_IDLEF)
            return false;
    return registers_clear() && vector_quiet(IRQ_ALL);
}
static void save(const nx_stm32u575_lpdma_copy_t* copy, const owner_t* owner) {
    memcpy(copy->context, owner, sizeof(*owner));
}
static nx_result_t load(const nx_stm32u575_lpdma_copy_t* copy, owner_t* owner) {
    if (!span(copy, sizeof(*copy), _Alignof(nx_stm32u575_lpdma_copy_t)) ||
        copy->ops != &nx_stm32u575_lpdma_copy_ops ||
        !span(copy->context, sizeof(nx_stm32u575_lpdma_copy_storage_t),
              _Alignof(nx_stm32u575_lpdma_copy_storage_t)))
        return NX_ERROR_INVALID;
    memcpy(owner, copy->context, sizeof(*owner));
    return owner->self != copy->context ||
                   (owner->opened && claim != copy->context)
               ? NX_ERROR_STATE
               : NX_SUCCESS;
}
static nx_result_t buffers(const void* source, void* destination, size_t length,
                           nx_stm32u575_lpdma_copy_workspace_t* workspace,
                           const void* control, size_t control_bytes) {
    static const nx_dma_memory_region_t region = {
        SRAM4_BASE_NS, SRAM4_SIZE, NX_DMA_MEMORY_READ | NX_DMA_MEMORY_WRITE};
    if (length < 2U || !span(control, control_bytes, 1U))
        return NX_ERROR_INVALID;
    nx_result_t result = nx_dma_buffer_validate(&region, 1U, source, length, 1U,
                                                NX_DMA_TO_DEVICE);
    if (result == NX_SUCCESS)
        result = nx_dma_buffer_validate(&region, 1U, destination, length, 1U,
                                        NX_DMA_FROM_DEVICE);
    if (result == NX_SUCCESS)
        result = nx_dma_buffer_validate(
            &region, 1U, workspace, sizeof(*workspace), 4U, NX_DMA_TO_DEVICE);
    if (result != NX_SUCCESS)
        return result;
    if (((uintptr_t)workspace & UINT32_C(0xffff)) >
            UINT32_C(0x10000) - sizeof(*workspace) ||
        overlap(source, length, destination, length) ||
        overlap(source, length, workspace, sizeof(*workspace)) ||
        overlap(destination, length, workspace, sizeof(*workspace)) ||
        overlap(control, control_bytes, source, length) ||
        overlap(control, control_bytes, destination, length) ||
        overlap(control, control_bytes, workspace, sizeof(*workspace)))
        return NX_ERROR_INVALID;
    return NX_SUCCESS;
}
static nx_result_t request_valid(const nx_stm32u575_lpdma_copy_t* copy,
                                 const owner_t* owner,
                                 nx_stm32u575_lpdma_copy_request_t* request) {
    if (!span(request, sizeof(*request),
              _Alignof(nx_stm32u575_lpdma_copy_request_t)))
        return NX_ERROR_INVALID;
    if (request->identity != request ||
        nx_request_state(&request->base) != NX_REQUEST_READY)
        return NX_ERROR_STATE;
    nx_result_t result =
        buffers(request->source, request->destination, request->length,
                request->workspace, request, sizeof(*request));
    if (result != NX_SUCCESS)
        return result;
    const void* controls[] = {copy, copy->context, owner->clock};
    size_t sizes[] = {sizeof(*copy), sizeof(nx_stm32u575_lpdma_copy_storage_t),
                      sizeof(*owner->clock)};
    for (size_t n = 0U; n < 3U; ++n) {
        result = buffers(request->source, request->destination, request->length,
                         request->workspace, controls[n], sizes[n]);
        if (result != NX_SUCCESS ||
            overlap(request, sizeof(*request), controls[n], sizes[n]))
            return NX_ERROR_INVALID;
    }
    return request->clock_identity != owner->clock ||
                   request->clock_generation != owner->generation
               ? NX_ERROR_STATE
               : NX_SUCCESS;
}
static nx_result_t clock_valid(owner_t* owner,
                               nx_stm32u575_retained_time_snapshot_t* out) {
    nx_result_t result = nx_stm32u575_retained_time_sample(owner->clock, out);
    if (result != NX_SUCCESS)
        return result;
    if (out->identity != owner->clock || out->generation != owner->generation)
        return NX_ERROR_STATE;
    return nx_stm32u575_retained_time_validate_alarm_a_copy(
        owner->clock, owner->wake_owner, owner->generation);
}
static void fail(owner_t* owner, nx_result_t cause) {
    if (cause != NX_SUCCESS && !owner->failed) {
        owner->failure = cause;
        owner->failed = true;
    }
    (void)nx_request_transition(&owner->request->base, NX_REQUEST_DRAINING);
}
static nx_result_t held(const nx_stm32u575_lpdma_copy_t* copy, owner_t* owner) {
    (void)nx_request_transition(&owner->request->base, NX_REQUEST_QUARANTINED);
    save(copy, owner);
    return NX_ERROR_BUSY;
}
nx_result_t nx_stm32u575_lpdma_copy_initialize(
    nx_stm32u575_lpdma_copy_storage_t* storage,
    const nx_stm32u575_lpdma_copy_config_t* config,
    nx_stm32u575_lpdma_copy_t* copy) {
    if (!context(false))
        return NX_ERROR_CONTEXT;
    if (!span(storage, sizeof(*storage),
              _Alignof(nx_stm32u575_lpdma_copy_storage_t)) ||
        !span(config, sizeof(*config),
              _Alignof(nx_stm32u575_lpdma_copy_config_t)) ||
        !span(copy, sizeof(*copy), _Alignof(nx_stm32u575_lpdma_copy_t)) ||
        overlap(storage, sizeof(*storage), config, sizeof(*config)) ||
        overlap(storage, sizeof(*storage), copy, sizeof(*copy)) ||
        overlap(config, sizeof(*config), copy, sizeof(*copy)))
        return NX_ERROR_INVALID;
    if (!span(config->clock, sizeof(*config->clock),
              _Alignof(nx_stm32u575_retained_time_storage_t)) ||
        config->wake_owner == NULL ||
        overlap(config->clock, sizeof(*config->clock), config,
                sizeof(*config)) ||
        overlap(config->clock, sizeof(*config->clock), storage,
                sizeof(*storage)) ||
        overlap(config->clock, sizeof(*config->clock), copy, sizeof(*copy)))
        return NX_ERROR_INVALID;
    if (claim != NULL)
        return claim == storage ? NX_ERROR_STATE : NX_ERROR_BUSY;
    owner_t previous;
    memcpy(&previous, storage, sizeof(previous));
    if (previous.opened || (previous.self != NULL && previous.self != storage))
        return NX_ERROR_STATE;
    if (previous.self == NULL) {
        for (size_t n = 0U; n < sizeof(*storage); ++n)
            if (storage->private_bytes[n] != 0U)
                return NX_ERROR_STATE;
    }
    if (!profile(NULL))
        return NX_ERROR_STATE;
    if (!baseline())
        return NX_ERROR_BUSY;
    nx_stm32u575_retained_time_snapshot_t sample;
    nx_result_t result =
        nx_stm32u575_retained_time_sample(config->clock, &sample);
    if (result != NX_SUCCESS)
        return result;
    if (sample.identity != config->clock || sample.generation == 0U)
        return NX_ERROR_STATE;
    result = nx_stm32u575_retained_time_validate_alarm_a_copy(
        config->clock, config->wake_owner, sample.generation);
    if (result != NX_SUCCESS)
        return result;
    owner_t owner = {0};
    owner.self = storage;
    owner.clock = config->clock;
    owner.wake_owner = config->wake_owner;
    owner.generation = sample.generation;
    owner.opened = true;
    memcpy(storage, &owner, sizeof(owner));
    claim = storage;
    copy->ops = &nx_stm32u575_lpdma_copy_ops;
    copy->context = storage;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_lpdma_copy_request_prepare(
    nx_stm32u575_lpdma_copy_request_t* request,
    const nx_stm32u575_retained_time_snapshot_t* domain, const void* source,
    void* destination, size_t length,
    nx_stm32u575_lpdma_copy_workspace_t* workspace, nx_time_us_t deadline) {
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U)
        return NX_ERROR_CONTEXT;
    if (!span(request, sizeof(*request),
              _Alignof(nx_stm32u575_lpdma_copy_request_t)) ||
        !span(domain, sizeof(*domain),
              _Alignof(nx_stm32u575_retained_time_snapshot_t)) ||
        overlap(request, sizeof(*request), domain, sizeof(*domain)))
        return NX_ERROR_INVALID;
    nx_result_t result = buffers(source, destination, length, workspace,
                                 request, sizeof(*request));
    if (result != NX_SUCCESS)
        return result;
    result = buffers(source, destination, length, workspace, domain,
                     sizeof(*domain));
    if (result != NX_SUCCESS)
        return result;
    if (!span(domain->identity, sizeof(nx_stm32u575_retained_time_storage_t),
              _Alignof(nx_stm32u575_retained_time_storage_t)) ||
        overlap(request, sizeof(*request), domain->identity,
                sizeof(nx_stm32u575_retained_time_storage_t)) ||
        domain->generation == 0U || domain->now == NX_DEADLINE_NEVER)
        return NX_ERROR_INVALID;
    result = nx_request_prepare(&request->base, deadline);
    if (result != NX_SUCCESS)
        return result;
    request->identity = request;
    request->source = source;
    request->destination = destination;
    request->length = length;
    request->workspace = workspace;
    request->clock_identity = domain->identity;
    request->clock_generation = domain->generation;
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_lpdma_copy_validate_ready(
    const nx_stm32u575_lpdma_copy_t* copy,
    nx_stm32u575_lpdma_copy_request_t* request,
    nx_stm32u575_retained_time_storage_t* clock, const void* wake_owner,
    uint64_t generation) {
    if (!context(true))
        return NX_ERROR_CONTEXT;
    owner_t owner;
    nx_result_t result = load(copy, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (!owner.opened || owner.closing || owner.request != NULL ||
        owner.clock != clock || owner.wake_owner != wake_owner ||
        owner.generation != generation)
        return NX_ERROR_STATE;
    result = request_valid(copy, &owner, request);
    if (result != NX_SUCCESS)
        return result;
    if (!profile(NULL))
        return NX_ERROR_STATE;
    if (!baseline())
        return NX_ERROR_BUSY;
    nx_stm32u575_retained_time_snapshot_t sample;
    result = clock_valid(&owner, &sample);
    if (result == NX_SUCCESS && request->base.deadline != NX_DEADLINE_NEVER &&
        sample.now >= request->base.deadline)
        result = NX_ERROR_TIMEOUT;
    return result;
}
nx_result_t
nx_stm32u575_lpdma_copy_arm(const nx_stm32u575_lpdma_copy_t* copy,
                            nx_stm32u575_lpdma_copy_request_t* request) {
    owner_t owner;
    nx_result_t result = load(copy, &owner);
    if (result != NX_SUCCESS)
        return result;
    result = nx_stm32u575_lpdma_copy_validate_ready(
        copy, request, owner.clock, owner.wake_owner, owner.generation);
    if (result != NX_SUCCESS)
        return result;
    result = nx_request_admit(&request->base, NX_REQUEST_ACTIVE);
    if (result != NX_SUCCESS)
        return result;
    owner.request = request;
    owner.source = request->source;
    owner.destination = request->destination;
    owner.workspace = request->workspace;
    owner.length = request->length;
    owner.first = owner.length / 2U;
    owner.deadline = request->base.deadline;
    owner.transferred = 0U;
    owner.failed = owner.completed = owner.reset_done = owner.drained =
        owner.reset_uncertain = false;
    owner.configured = false;
    owner.sleep = NX_LPDMA_READ(RCC->AHB3SMENR) & SLEEP_BITS;
    owner.autonomous = NX_LPDMA_READ(RCC->SRDAMR) & AUTO_BITS;
    owner.privilege = NX_LPDMA_READ(LPDMA1->PRIVCFGR) & DMA_PRIVCFGR_PRIV0;
    uint32_t* words = owner.workspace->private_words;
    words[0] = DATA_BITS;
    words[1] = FINAL_BITS;
    words[2] = (uint32_t)(owner.length - owner.first);
    words[3] = (uint32_t)((uintptr_t)owner.source + owner.first);
    words[4] = (uint32_t)((uintptr_t)owner.destination + owner.first);
    words[5] = 0U;
    nx_arch_dsb();
    bool programmed =
        update(&RCC->AHB3SMENR, SLEEP_BITS, SLEEP_BITS) &&
        update(&RCC->SRDAMR, AUTO_BITS, AUTO_BITS) &&
        update(&LPDMA1->PRIVCFGR, DMA_PRIVCFGR_PRIV0, DMA_PRIVCFGR_PRIV0) &&
        checked(&LPDMA1_Channel0->CLBAR, (uint32_t)((uintptr_t)owner.workspace &
                                                    UINT32_C(0xffff0000))) &&
        checked(&LPDMA1_Channel0->CTR1, DATA_BITS) &&
        checked(&LPDMA1_Channel0->CTR2, FIRST_BITS) &&
        checked(&LPDMA1_Channel0->CBR1, (uint32_t)owner.first) &&
        checked(&LPDMA1_Channel0->CSAR, (uint32_t)(uintptr_t)owner.source) &&
        checked(&LPDMA1_Channel0->CDAR,
                (uint32_t)(uintptr_t)owner.destination) &&
        checked(&LPDMA1_Channel0->CLLR,
                UPDATE_BITS |
                    ((uint32_t)(uintptr_t)owner.workspace & DMA_CLLR_LA)) &&
        checked(&LPDMA1_Channel0->CCR, INTERRUPTS);
    if (programmed) {
        NX_LPDMA_WRITE(NVIC->ISER[IRQ_WORD], IRQ_BIT);
        nx_arch_dsb();
        programmed = (NX_LPDMA_READ(NVIC->ISER[IRQ_WORD]) & IRQ_BIT) != 0U &&
                     checked(&LPDMA1_Channel0->CCR, INTERRUPTS | DMA_CCR_EN);
    }
    if (!programmed)
        fail(&owner, NX_ERROR_IO);
    else
        owner.configured = true;
    save(copy, &owner);
    return NX_SUCCESS; /* accepted failures retain the loan */
}
static nx_result_t observe(owner_t* owner) {
    if (!owner->failed && !owner->completed) {
        nx_stm32u575_retained_time_snapshot_t sample;
        nx_result_t result = clock_valid(owner, &sample);
        if (result == NX_ERROR_BUSY)
            return result;
        if (result != NX_SUCCESS) {
            fail(owner, result);
        } else if (owner->deadline != NX_DEADLINE_NEVER &&
                   sample.now >= owner->deadline)
            fail(owner, NX_ERROR_TIMEOUT);
    }
    uint32_t status = NX_LPDMA_READ(LPDMA1_Channel0->CSR);
    if (!owner->failed && (status & ERRORS) != 0U)
        fail(owner, NX_ERROR_IO);
    if (!owner->failed && (status & DMA_CSR_TCF) != 0U) {
        if ((status & DMA_CSR_IDLEF) != 0U &&
            (NX_LPDMA_READ(LPDMA1_Channel0->CCR) &
             (DMA_CCR_EN | DMA_CCR_LSM)) == 0U &&
            NX_LPDMA_READ(LPDMA1_Channel0->CLLR) == 0U &&
            NX_LPDMA_READ(LPDMA1_Channel0->CBR1) == 0U &&
            NX_LPDMA_READ(LPDMA1_Channel0->CTR1) == DATA_BITS &&
            NX_LPDMA_READ(LPDMA1_Channel0->CTR2) == FINAL_BITS &&
            NX_LPDMA_READ(LPDMA1_Channel0->CSAR) ==
                (uint32_t)((uintptr_t)owner->source + owner->length) &&
            NX_LPDMA_READ(LPDMA1_Channel0->CDAR) ==
                (uint32_t)((uintptr_t)owner->destination + owner->length))
            owner->completed = true;
        else
            fail(owner, NX_ERROR_IO);
    }
    return owner->failed || owner->completed ? NX_SUCCESS : NX_ERROR_BUSY;
}
static bool clear_registers(void) {
    return checked(&LPDMA1_Channel0->CCR, 0U) &&
           checked(&LPDMA1_Channel0->CLLR, 0U) &&
           checked(&LPDMA1_Channel0->CBR1, 0U) &&
           checked(&LPDMA1_Channel0->CSAR, 0U) &&
           checked(&LPDMA1_Channel0->CDAR, 0U) &&
           checked(&LPDMA1_Channel0->CTR1, 0U) &&
           checked(&LPDMA1_Channel0->CTR2, 0U) &&
           checked(&LPDMA1_Channel0->CLBAR, 0U);
}
nx_result_t nx_stm32u575_lpdma_copy_drain(const nx_stm32u575_lpdma_copy_t* copy,
                                          nx_result_t cause,
                                          uint32_t poll_limit) {
    if (!context(true))
        return NX_ERROR_CONTEXT;
    if (poll_limit == 0U)
        return NX_ERROR_INVALID;
    owner_t owner;
    nx_result_t result = load(copy, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (owner.request == NULL)
        return NX_SUCCESS;
    if (cause != NX_SUCCESS)
        fail(&owner, cause);
    if (!profile(&owner)) {
        fail(&owner, NX_ERROR_STATE);
        NX_LPDMA_WRITE(NVIC->ICER[IRQ_WORD], IRQ_BIT);
        nx_arch_dsb();
        return held(copy, &owner);
    }
    if (owner.drained) {
        save(copy, &owner);
        return NX_SUCCESS;
    }
    if (!owner.reset_done) {
        result = observe(&owner);
        if (result != NX_SUCCESS) {
            save(copy, &owner);
            return result;
        }
        (void)nx_request_transition(&owner.request->base, NX_REQUEST_DRAINING);
        uint32_t control = NX_LPDMA_READ(LPDMA1_Channel0->CCR);
        if ((control & DMA_CCR_EN) != 0U) {
            NX_LPDMA_WRITE(LPDMA1_Channel0->CCR, control | DMA_CCR_SUSP);
            nx_arch_dsb();
            if (!wait_bits(&LPDMA1_Channel0->CSR, DMA_CSR_SUSPF | DMA_CSR_IDLEF,
                           DMA_CSR_SUSPF | DMA_CSR_IDLEF, poll_limit))
                return held(copy, &owner);
        } else if ((NX_LPDMA_READ(LPDMA1_Channel0->CSR) & DMA_CSR_IDLEF) == 0U)
            return held(copy, &owner);
        uint32_t destination = NX_LPDMA_READ(LPDMA1_Channel0->CDAR);
        uintptr_t first = (uintptr_t)owner.destination;
        if ((uintptr_t)destination >= first &&
            (uintptr_t)destination - first <= owner.length)
            owner.transferred = (uintptr_t)destination - first;
        else {
            owner.transferred = 0U;
            fail(&owner, NX_ERROR_STATE);
        }
        NX_LPDMA_WRITE(LPDMA1_Channel0->CCR, DMA_CCR_RESET);
        nx_arch_dsb();
        /* RESET is write-only, not an acknowledgement bit. This full-word
         * write also writes every ordinary control/interrupt bit to zero.
         * Their readback distinguishes an ignored write even when a completed
         * channel already had EN/SUSP clear. Steady master state plus the
         * accepted MMIO write contract establishes the internal reset. */
        if (!wait_bits(&LPDMA1_Channel0->CCR, UINT32_MAX, 0U, poll_limit) ||
            (NX_LPDMA_READ(LPDMA1_Channel0->CSR) & DMA_CSR_IDLEF) == 0U)
            return held(copy, &owner);
        owner.reset_done = true;
    }
    if (!clear_registers())
        return held(copy, &owner);
    NX_LPDMA_WRITE(LPDMA1_Channel0->CFCR, EVENTS);
    NX_LPDMA_WRITE(NVIC->ICER[IRQ_WORD], IRQ_BIT);
    NX_LPDMA_WRITE(NVIC->ICPR[IRQ_WORD], IRQ_BIT);
    nx_arch_dsb();
    nx_arch_isb();
    if (!wait_bits(&LPDMA1_Channel0->CSR, EVENTS | DMA_CSR_IDLEF, DMA_CSR_IDLEF,
                   poll_limit) ||
        !vector_quiet(IRQ_BIT))
        return held(copy, &owner);
    if (!update(&LPDMA1->PRIVCFGR, DMA_PRIVCFGR_PRIV0, owner.privilege) ||
        !update(&RCC->AHB3SMENR, SLEEP_BITS, owner.sleep) ||
        !update(&RCC->SRDAMR, AUTO_BITS, owner.autonomous))
        return held(copy, &owner);
    owner.drained = true;
    save(copy, &owner);
    return NX_SUCCESS;
}
nx_result_t nx_stm32u575_lpdma_copy_validate_sleep(
    const nx_stm32u575_lpdma_copy_t* copy,
    nx_stm32u575_lpdma_copy_request_t* request,
    nx_stm32u575_retained_time_storage_t* clock, const void* wake_owner,
    uint64_t generation) {
    if (!context(true))
        return NX_ERROR_CONTEXT;
    owner_t owner;
    nx_result_t result = load(copy, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (!owner.opened || owner.request != request || owner.clock != clock ||
        owner.wake_owner != wake_owner || owner.generation != generation ||
        !owner.configured || owner.drained || owner.failed)
        return NX_ERROR_STATE;
    if (!profile(&owner)) {
        save(copy, &owner);
        return NX_ERROR_STATE;
    }
    uint32_t control = NX_LPDMA_READ(LPDMA1_Channel0->CCR);
    uint32_t transfer = NX_LPDMA_READ(LPDMA1_Channel0->CTR2);
    if ((control & ~(uint32_t)DMA_CCR_EN) != INTERRUPTS ||
        NX_LPDMA_READ(LPDMA1_Channel0->CTR1) != DATA_BITS ||
        (transfer != FIRST_BITS && transfer != FINAL_BITS) ||
        (NX_LPDMA_READ(RCC->AHB3SMENR) & SLEEP_BITS) != SLEEP_BITS ||
        (NX_LPDMA_READ(RCC->SRDAMR) & AUTO_BITS) != AUTO_BITS ||
        (NX_LPDMA_READ(LPDMA1->PRIVCFGR) & DMA_PRIVCFGR_PRIV0) == 0U)
        return NX_ERROR_STATE;
    return nx_stm32u575_retained_time_validate_alarm_a_copy(clock, wake_owner,
                                                            generation);
}
nx_result_t
nx_stm32u575_lpdma_copy_release(const nx_stm32u575_lpdma_copy_t* copy,
                                nx_stm32u575_lpdma_copy_request_t* request,
                                nx_result_t final_result) {
    if (!context(true))
        return NX_ERROR_CONTEXT;
    owner_t owner;
    nx_result_t result = load(copy, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (owner.request != request || request == NULL)
        return NX_ERROR_STATE;
    if (!profile(&owner) || !owner.drained) {
        save(copy, &owner);
        return NX_ERROR_BUSY;
    }
    if (!registers_clear() || !vector_quiet(IRQ_BIT) ||
        (NX_LPDMA_READ(RCC->AHB3SMENR) & SLEEP_BITS) != owner.sleep ||
        (NX_LPDMA_READ(RCC->SRDAMR) & AUTO_BITS) != owner.autonomous ||
        (NX_LPDMA_READ(LPDMA1->PRIVCFGR) & DMA_PRIVCFGR_PRIV0) !=
            owner.privilege ||
        (NX_LPDMA_READ(RCC->APB3ENR) & RCC_APB3ENR_RTCAPBEN) == 0U ||
        (NX_LPDMA_READ(RTC->CR) &
         (RTC_CR_ALRAE | RTC_CR_ALRAIE | RTC_CR_ALRAFCLR | RTC_CR_WUTE |
          RTC_CR_WUTIE)) != 0U ||
        NX_LPDMA_READ(RTC->SR) != 0U || NX_LPDMA_READ(RTC->ALRMAR) != 0U ||
        NX_LPDMA_READ(RTC->ALRMASSR) != 0U ||
        (NX_LPDMA_READ(NVIC->ISER[0]) & (1U << RTC_IRQn)) != 0U ||
        (NX_LPDMA_READ(NVIC->ISPR[0]) & (1U << RTC_IRQn)) != 0U ||
        (NX_LPDMA_READ(NVIC->IABR[0]) & (1U << RTC_IRQn)) != 0U ||
        (NX_LPDMA_READ(SCB->SCR) & SCB_SCR_SLEEPDEEP_Msk) != 0U)
        return NX_ERROR_BUSY;
    nx_result_t valid = nx_stm32u575_retained_time_validate_alarm_a_copy(
        owner.clock, owner.wake_owner, owner.generation);
    if (valid != NX_SUCCESS) {
        fail(&owner, NX_ERROR_STATE);
    }
    if (final_result != NX_SUCCESS)
        fail(&owner, final_result);
    if (!owner.failed && !owner.completed)
        return NX_ERROR_STATE;
    nx_result_t terminal = owner.failed ? owner.failure : NX_SUCCESS;
    size_t transferred = owner.transferred;
    owner.request = NULL;
    owner.source = NULL;
    owner.destination = NULL;
    owner.workspace = NULL;
    owner.configured = owner.reset_done = owner.drained = false;
    if (owner.closing) {
        owner.opened = false;
        claim = NULL;
    }
    save(copy, &owner);
    nx_request_settle(&request->base, terminal, transferred);
    return NX_SUCCESS;
}
static nx_result_t cancel(void* context_pointer,
                          nx_stm32u575_lpdma_copy_request_t* request) {
    nx_stm32u575_lpdma_copy_t copy = {&nx_stm32u575_lpdma_copy_ops,
                                      context_pointer};
    if (!context(true))
        return NX_ERROR_CONTEXT;
    owner_t owner;
    nx_result_t result = load(&copy, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (request == NULL || owner.request != request || owner.completed)
        return NX_ERROR_STATE;
    fail(&owner, NX_ERROR_CANCELLED);
    save(&copy, &owner);
    return NX_SUCCESS;
}
static nx_result_t service(void* context_pointer, uint32_t poll_limit) {
    if (!context(nx_arch_irq_masks().primask != 0U))
        return NX_ERROR_CONTEXT;
    nx_stm32u575_lpdma_copy_t copy = {&nx_stm32u575_lpdma_copy_ops,
                                      context_pointer};
    owner_t owner;
    nx_result_t result = load(&copy, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (poll_limit == 0U)
        return NX_ERROR_INVALID;
    if (owner.request == NULL)
        return NX_SUCCESS;
    result = nx_stm32u575_lpdma_copy_drain(&copy, NX_SUCCESS, poll_limit);
    return result == NX_SUCCESS ? NX_ERROR_BUSY : result;
}
static nx_result_t stop(void* context_pointer, uint32_t poll_limit) {
    if (!context(nx_arch_irq_masks().primask != 0U))
        return NX_ERROR_CONTEXT;
    nx_stm32u575_lpdma_copy_t copy = {&nx_stm32u575_lpdma_copy_ops,
                                      context_pointer};
    owner_t owner;
    nx_result_t result = load(&copy, &owner);
    if (result != NX_SUCCESS)
        return result;
    if (poll_limit == 0U)
        return NX_ERROR_INVALID;
    if (owner.request != NULL) {
        if (!context(true))
            return NX_ERROR_CONTEXT;
        owner.closing = true;
        if (!owner.completed)
            fail(&owner, NX_ERROR_CANCELLED);
        save(&copy, &owner);
        (void)service(context_pointer, poll_limit);
        return NX_ERROR_BUSY;
    }
    owner.opened = false;
    owner.closing = true;
    if (claim == context_pointer)
        claim = NULL;
    save(&copy, &owner);
    return NX_SUCCESS;
}
const nx_stm32u575_lpdma_copy_ops_t nx_stm32u575_lpdma_copy_ops = {
    cancel, service, stop};
nx_result_t
nx_stm32u575_lpdma_copy_cancel(const nx_stm32u575_lpdma_copy_t* copy,
                               nx_stm32u575_lpdma_copy_request_t* request) {
    owner_t owner;
    nx_result_t result = load(copy, &owner);
    return result == NX_SUCCESS ? cancel(copy->context, request) : result;
}
nx_result_t
nx_stm32u575_lpdma_copy_service(const nx_stm32u575_lpdma_copy_t* copy,
                                uint32_t poll_limit) {
    owner_t owner;
    nx_result_t result = load(copy, &owner);
    return result == NX_SUCCESS ? service(copy->context, poll_limit) : result;
}
nx_result_t nx_stm32u575_lpdma_copy_stop(const nx_stm32u575_lpdma_copy_t* copy,
                                         uint32_t poll_limit) {
    owner_t owner;
    nx_result_t result = load(copy, &owner);
    return result == NX_SUCCESS ? stop(copy->context, poll_limit) : result;
}
