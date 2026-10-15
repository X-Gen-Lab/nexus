/**
 * \file            guard.c
 * \brief           Admit privileged bootstrap exactly once before task restore
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-11
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "guard.h"
#include <stdbool.h>

/** \brief Exact protected spans cannot overflow or authorize user storage. */
static bool protected_span(uintptr_t begin, uintptr_t end, uintptr_t address,
                           size_t bytes) {
    return begin < end && bytes != 0 && address >= begin && address < end &&
           bytes <= end - address;
}

/** \brief Verify the linked bootstrap instruction remains protected code. */
static bool startup_origin(const nx_freertos_mpu_start_facts_t* facts) {
    return facts->startup_pc >= 2 && (facts->startup_pc & 1U) == 0 &&
           protected_span(facts->flash_begin, facts->flash_end,
                          facts->startup_pc - 2, 2);
}

BaseType_t nx_freertos_mpu_start_arm(nx_freertos_mpu_start_t* start) {
    nx_freertos_mpu_start_facts_t facts = nx_freertos_mpu_start_facts();
    if (start == NULL || facts.exception != 0 || (facts.control & 7U) != 0 ||
        !startup_origin(&facts) ||
        !protected_span(facts.ram_begin, facts.ram_end, (uintptr_t)start,
                        sizeof(*start)) ||
        start->phase != 0) {
        return pdFALSE;
    }
    start->phase = 1;
    return pdTRUE;
}

BaseType_t nx_freertos_mpu_start_consume(nx_freertos_mpu_start_t* start,
                                         const uint32_t* frame,
                                         uint32_t exception_return) {
    nx_freertos_mpu_start_facts_t facts = nx_freertos_mpu_start_facts();
#if NEXUS_ARCH_MPU_VERSION == 7
    bool valid_return = exception_return == 0xfffffff9U;
#elif NEXUS_ARCH_MPU_VERSION == 8
    /* Bind to the actual pinned tuple: secure-only initial EXC_RETURN is
     * FD, the maintained nonsecure NTZ tuple uses BC. Startup uses MSP. */
#if configRUN_FREERTOS_SECURE_ONLY == 1
    bool valid_return = exception_return == 0xfffffff9U;
#else
    bool valid_return = exception_return == 0xffffffb8U;
#endif
#else
#error "Scheduler-start guard requires explicit MPU architecture facts"
#endif
    if (start == NULL || frame == NULL || facts.exception != 11 ||
        (facts.control & 7U) != 0 || !valid_return || !startup_origin(&facts) ||
        !protected_span(facts.ram_begin, facts.ram_end, (uintptr_t)start,
                        sizeof(*start)) ||
        start->phase != 1 || ((uintptr_t)frame & 7U) != 0 ||
        !protected_span(facts.ram_begin, facts.ram_end, (uintptr_t)frame,
                        8 * sizeof(uint32_t)) ||
        frame[6] != facts.startup_pc || (frame[7] & (1U << 24)) == 0 ||
        (frame[7] & 0x1ffU) != 0) {
        return pdFALSE;
    }
    /* Consume before the original naked restore can change MSP, MPU or
     * CONTROL. A repeated privileged or user START SVC cannot reuse it. */
    start->phase = 2;
    return pdTRUE;
}
