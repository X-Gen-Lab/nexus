/**
 * \file            supervision.c
 * \brief           Explicit F407 first-fault, reset and DWT mechanisms
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-10
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "nexus/soc/stm32f407_supervision.h"
#include "stm32f407_clock.h"
#include "stm32f407_system.h"

extern nx_stm32_system_t g_nx_stm32_system;

#ifndef NX_SUPERVISION_SCB
#define NX_SUPERVISION_SCB   SCB
#define NX_SUPERVISION_DWT   DWT
#define NX_SUPERVISION_DEBUG DCB
#define NX_SUPERVISION_RCC   RCC
#endif
#ifndef NX_STM32_WRITE
#define NX_STM32_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#define RECORD_WRITING 1U
#define RECORD_READY   UINT32_C(0x4e584631)
#define RESET_FLAGS                                                            \
    (RCC_CSR_BORRSTF | RCC_CSR_PINRSTF | RCC_CSR_PORRSTF | RCC_CSR_SFTRSTF |   \
     RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF)

_Static_assert(__atomic_always_lock_free(sizeof(uint32_t), 0),
               "First-fault publication cannot call a lock or worker");
_Static_assert(offsetof(nx_stm32_fault_record_t, checksum) ==
                   sizeof(nx_stm32_fault_record_t) - sizeof(uint32_t),
               "Fault checksum follows the complete copied payload");

/** \brief Validate complete aligned call-owned RAM without reading MMIO. */
static bool storage(const void* pointer, size_t bytes, size_t alignment) {
    uintptr_t begin = (uintptr_t)pointer;
    if (pointer == NULL || begin % alignment != 0U ||
        bytes > UINTPTR_MAX - begin)
        return false;
    uintptr_t end = begin + bytes;
    return !(begin < UINT32_C(0x60000000) && end > UINT32_C(0x40000000)) &&
           !(begin < UINT32_C(0xf0000000) && end > UINT32_C(0xe0000000));
}

/** \brief Validate disjoint complete objects before any output mutation. */
static bool overlaps(const void* a, size_t a_bytes, const void* b,
                     size_t b_bytes) {
    return (uintptr_t)a < (uintptr_t)b + b_bytes &&
           (uintptr_t)b < (uintptr_t)a + a_bytes;
}

/** \brief Hash copied payload bytes only, excluding the publication word. */
static uint32_t checksum(const nx_stm32_fault_record_t* record) {
    const unsigned char* bytes = (const unsigned char*)record;
    uint32_t hash = UINT32_C(2166136261);
    for (size_t offset = sizeof(record->state);
         offset < offsetof(nx_stm32_fault_record_t, checksum); ++offset)
        hash = (hash ^ bytes[offset]) * UINT32_C(16777619);
    return hash;
}

nx_result_t
nx_stm32f407_fault_capture(nx_stm32_fault_record_t* record, uint32_t reason,
                           uint32_t exception_return,
                           const nx_stm32_exception_frame_t* frame) {
    if (!nx_arch_is_privileged())
        return NX_ERROR_CONTEXT;
    if (!storage(record, sizeof(*record), _Alignof(nx_stm32_fault_record_t)) ||
        (frame != NULL &&
         (!storage(frame, sizeof(*frame),
                   _Alignof(nx_stm32_exception_frame_t)) ||
          overlaps(record, sizeof(*record), frame, sizeof(*frame)))))
        return NX_ERROR_INVALID;
    uint32_t empty = 0U;
    if (!__atomic_compare_exchange_n(&record->state, &empty, RECORD_WRITING,
                                     false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return NX_ERROR_BUSY;
    record->format = 1U;
    record->reason = reason;
    record->exception_return = exception_return;
    record->cfsr = NX_SUPERVISION_SCB->CFSR;
    record->hfsr = NX_SUPERVISION_SCB->HFSR;
    record->dfsr = NX_SUPERVISION_SCB->DFSR;
    record->afsr = NX_SUPERVISION_SCB->AFSR;
    record->mmfar = NX_SUPERVISION_SCB->MMFAR;
    record->bfar = NX_SUPERVISION_SCB->BFAR;
    record->shcsr = NX_SUPERVISION_SCB->SHCSR;
    record->icsr = NX_SUPERVISION_SCB->ICSR;
    record->reset_flags = NX_SUPERVISION_RCC->CSR;
    record->clock_flags = NX_SUPERVISION_RCC->CIR;
    record->frame_present = frame != NULL ? 1U : 0U;
    record->frame = frame != NULL ? *frame : (nx_stm32_exception_frame_t){0};
    record->checksum = checksum(record);
    __atomic_store_n(&record->state, RECORD_READY, __ATOMIC_RELEASE);
    return NX_SUCCESS;
}

nx_result_t nx_stm32f407_fault_read(const nx_stm32_fault_record_t* record,
                                    nx_stm32_fault_record_t* output) {
    if (!storage(record, sizeof(*record), _Alignof(nx_stm32_fault_record_t)) ||
        !storage(output, sizeof(*output), _Alignof(nx_stm32_fault_record_t)) ||
        overlaps(record, sizeof(*record), output, sizeof(*output)))
        return NX_ERROR_INVALID;
    if (__atomic_load_n(&record->state, __ATOMIC_ACQUIRE) != RECORD_READY)
        return NX_ERROR_STATE;
    if (record->format != 1U || checksum(record) != record->checksum)
        return NX_ERROR_IO;
    *output = *record;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f407_reset_flags_clear(void) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (!g_nx_stm32_system.reset_captured)
        return NX_ERROR_STATE;
    RCC_TypeDef* rcc = NX_SUPERVISION_RCC;
    NX_STM32_WRITE(&rcc->CSR, rcc->CSR | RCC_CSR_RMVF);
    nx_arch_dsb();
    return (rcc->CSR & RESET_FLAGS) == 0U ? NX_SUCCESS : NX_ERROR_IO;
}

_Noreturn void nx_stm32f407_reset_request(void) {
    if (nx_arch_is_privileged()) {
        (void)nx_arch_irq_save();
        nx_arch_dsb();
        uint32_t group = NX_SUPERVISION_SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk;
        NX_STM32_WRITE(&NX_SUPERVISION_SCB->AIRCR,
                       (UINT32_C(0x5fa) << SCB_AIRCR_VECTKEY_Pos) | group |
                           SCB_AIRCR_SYSRESETREQ_Msk);
        nx_arch_dsb();
    } else {
        nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_CONTEXT, __FILE__,
                         __LINE__);
    }
    for (;;) {
    }
}

nx_result_t nx_stm32f407_cycle_trace(bool enable, bool reset_counter,
                                     nx_stm32_cycle_trace_t* output) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    if (!storage(output, sizeof(*output), _Alignof(nx_stm32_cycle_trace_t)) ||
        overlaps(output, sizeof(*output), &g_nx_stm32_system,
                 sizeof(g_nx_stm32_system)) ||
        (reset_counter && !enable))
        return NX_ERROR_INVALID;
    if (!g_nx_stm32_system.started)
        return NX_ERROR_STATE;
    if ((NX_SUPERVISION_DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) != 0U)
        return NX_ERROR_UNSUPPORTED;
    if (enable) {
        NX_STM32_WRITE(&NX_SUPERVISION_DEBUG->DEMCR,
                       NX_SUPERVISION_DEBUG->DEMCR | DCB_DEMCR_TRCENA_Msk);
        nx_arch_dsb();
        if ((NX_SUPERVISION_DEBUG->DEMCR & DCB_DEMCR_TRCENA_Msk) == 0U)
            return NX_ERROR_IO;
        if (reset_counter) {
            NX_STM32_WRITE(&NX_SUPERVISION_DWT->CTRL,
                           NX_SUPERVISION_DWT->CTRL &
                               ~(uint32_t)DWT_CTRL_CYCCNTENA_Msk);
            nx_arch_dsb();
            if ((NX_SUPERVISION_DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U)
                return NX_ERROR_IO;
            NX_STM32_WRITE(&NX_SUPERVISION_DWT->CYCCNT, 0U);
            nx_arch_dsb();
            if (NX_SUPERVISION_DWT->CYCCNT != 0U)
                return NX_ERROR_IO;
        }
    }
    uint32_t control =
        NX_SUPERVISION_DWT->CTRL & ~(uint32_t)DWT_CTRL_CYCCNTENA_Msk;
    if (enable)
        control |= DWT_CTRL_CYCCNTENA_Msk;
    NX_STM32_WRITE(&NX_SUPERVISION_DWT->CTRL, control);
    nx_arch_dsb();
    if ((NX_SUPERVISION_DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) !=
        (enable ? DWT_CTRL_CYCCNTENA_Msk : 0U))
        return NX_ERROR_IO;
    *output = (nx_stm32_cycle_trace_t){.cycles = NX_SUPERVISION_DWT->CYCCNT,
                                       .core_hz = g_nx_stm32_system.core_hz,
                                       .enabled = enable};
    return NX_SUCCESS;
}
