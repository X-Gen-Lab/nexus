/**
 * \file            fault.c
 * \brief           Explicit F103RB fault sampling
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/soc/stm32f103rb_fault.h"
#include "stm32f1xx.h"

#if !defined(STM32F103xB)
#error "Fault snapshot requires the reviewed STM32F103RB register map"
#endif

#ifndef NX_F1_FAULT_SCB
#define NX_F1_FAULT_SCB        SCB
#define NX_F1_FAULT_SRAM_START SRAM_BASE
#define NX_F1_FAULT_SRAM_END   (SRAM_BASE + UINT32_C(0x5000))
#endif

nx_result_t nx_stm32f103rb_fault_capture(volatile nx_fault_record_t* record,
                                         uint32_t sequence, uint32_t reason,
                                         uint32_t exception_return) {
    if (!nx_arch_is_privileged()) {
        return NX_ERROR_CONTEXT;
    }
    const uintptr_t begin = (uintptr_t)record;
    if (begin < NX_F1_FAULT_SRAM_START ||
        begin > NX_F1_FAULT_SRAM_END - sizeof(*record) ||
        begin % _Alignof(nx_fault_record_t) != 0U) {
        return NX_ERROR_INVALID;
    }
    const SCB_Type* const scb = NX_F1_FAULT_SCB;
    /* PM0056 requires saving the addresses before reading the validity bits. */
    const uint32_t mmfar = scb->MMFAR;
    const uint32_t bfar = scb->BFAR;
    const uint32_t cfsr = scb->CFSR;
    const uint32_t icsr = scb->ICSR;
    const nx_fault_capture_t facts = {
        .sequence = sequence,
        .domain = NX_FAULT_PLATFORM,
        .reason = reason,
        .flags = NX_FAULT_STATUS_VALID,
        .exception = icsr & SCB_ICSR_VECTACTIVE_Msk,
        .exception_return = exception_return,
        .status =
            {
                [0] = cfsr,
                [1] = scb->HFSR,
                [4] = (cfsr & SCB_CFSR_MMARVALID_Msk) != 0U ? mmfar : 0U,
                [5] = (cfsr & SCB_CFSR_BFARVALID_Msk) != 0U ? bfar : 0U,
                [6] = scb->SHCSR,
                [7] = icsr,
            },
    };
    return nx_fault_record_capture(record, &facts);
}
