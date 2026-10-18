/**
 * \file            flash_ram_owner.c
 * \brief           H743 selected RAM pod cold copy and physical-owner binding
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "stm32h743_clock.h"
#include "stm32h743_flash_execution.h"
#include "stm32h743_flash_ram.h"

#define NX_H743_AXI_FIRST   UINT32_C(0x24000000)
#define NX_H743_AXI_END     UINT32_C(0x24080000)
#define NX_H743_FLASH_BYTES UINT32_C(0x200000)
#define NX_H743_BUSY        ((uint32_t)(FLASH_SR_BSY | FLASH_SR_QW | FLASH_SR_WBNE))
#ifndef NX_H743_FLASH_RAM_CONTROL
#define NX_H743_FLASH_RAM_CONTROL() __get_CONTROL()
#endif
#ifndef NX_H743_FLASH_RAM_IPSR
#define NX_H743_FLASH_RAM_IPSR() __get_IPSR()
#endif
#ifndef NX_H743_FLASH_RAM_PRIMASK
#define NX_H743_FLASH_RAM_PRIMASK() __get_PRIMASK()
#endif
#ifndef NX_H743_FLASH_RAM_BASEPRI
#define NX_H743_FLASH_RAM_BASEPRI() __get_BASEPRI()
#endif
#ifndef NX_H743_FLASH_RAM_FAULTMASK
#define NX_H743_FLASH_RAM_FAULTMASK() __get_FAULTMASK()
#endif
#ifndef NX_H743_FLASH_RAM_FPCCR
#define NX_H743_FLASH_RAM_FPCCR() NX_H743_READ(&FPU->FPCCR)
#endif
#ifndef NX_H743_FLASH_RAM_MPU_CONTROL
#define NX_H743_FLASH_RAM_MPU_CONTROL() NX_H743_READ(&MPU->CTRL)
#endif
#ifndef NX_H743_SILICON_ID
#define NX_H743_SILICON_ID() NX_H743_READ(&DBGMCU->IDCODE)
#endif
#ifndef NX_H743_FLASH_KIB
#define NX_H743_FLASH_KIB() (*(const volatile uint16_t*)FLASHSIZE_BASE)
#endif
#ifndef NX_H743_FLASH_RAM_ADDRESS
#define NX_H743_FLASH_RAM_ADDRESS(value) ((uintptr_t)(value))
#endif
#ifndef NX_H743_FLASH_RAM_SYMBOL
extern uint8_t __nx_flash_ram_start[];
extern uint8_t __nx_flash_ram_end[];
extern uint8_t __nx_flash_ram_load_start[];
extern uint8_t __nx_flash_ram_load_end[];
extern const uint32_t g_pfnVectors[];
void Default_Handler(void);
#define NX_H743_FLASH_RAM_SYMBOL(name) ((uintptr_t)(name))
#endif
#ifndef NX_H743_FLASH_RAM_READ8
#define NX_H743_FLASH_RAM_READ8(address) (*(const volatile uint8_t*)(address))
#endif
#ifndef NX_H743_FLASH_RAM_WRITE8
#define NX_H743_FLASH_RAM_WRITE8(address, value)                               \
    (*(volatile uint8_t*)(address) = (value))
#endif
#ifndef NX_H743_FLASH_RAM_READ32
#define NX_H743_FLASH_RAM_READ32(address) (*(const volatile uint32_t*)(address))
#endif
#ifndef NX_H743_FLASH_RAM_BARRIER
#define NX_H743_FLASH_RAM_BARRIER()                                            \
    do {                                                                       \
        __DSB();                                                               \
        __ISB();                                                               \
    } while (0)
#endif
#ifndef NX_H743_FLASH_IMAGE
extern const nx_stm32h743_flash_image_t g_nx_stm32h743_flash_image;
#define NX_H743_FLASH_IMAGE() (&g_nx_stm32h743_flash_image)
#endif

/** \brief No overflowing interval is proof of disjoint ownership. */
static bool overlap(uintptr_t first, size_t bytes, uintptr_t second,
                    size_t other) {
    return bytes > UINTPTR_MAX - first || other > UINTPTR_MAX - second ||
           (first < second + other && second < first + bytes);
}

static bool alias(const void* first, size_t bytes, const void* second,
                  size_t other) {
    return overlap((uintptr_t)first, bytes, (uintptr_t)second, other) ||
           overlap(NX_H743_FLASH_RAM_ADDRESS(first), bytes,
                   NX_H743_FLASH_RAM_ADDRESS(second), other);
}

static bool axi(const void* value, size_t bytes) {
    const uintptr_t address = NX_H743_FLASH_RAM_ADDRESS(value);
    return value != NULL && bytes <= UINTPTR_MAX - (uintptr_t)value &&
           address >= NX_H743_AXI_FIRST && address <= NX_H743_AXI_END &&
           bytes <= NX_H743_AXI_END - address;
}

static bool system_alias(const void* value, size_t bytes) {
    const nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    return alias(value, bytes, s, sizeof(*s)) ||
           (s->rcc != NULL && alias(value, bytes, s->rcc, sizeof(*s->rcc))) ||
           (s->flash != NULL &&
            alias(value, bytes, s->flash, sizeof(*s->flash))) ||
           (s->power != NULL &&
            alias(value, bytes, s->power, sizeof(*s->power))) ||
           (s->timer != NULL &&
            alias(value, bytes, s->timer, sizeof(*s->timer))) ||
           (s->core != NULL && alias(value, bytes, s->core, sizeof(*s->core)));
}

static bool code_alias(const void* value, size_t bytes) {
    const uintptr_t first = NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_start);
    const uintptr_t end = NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_end);
    return end <= first ||
           overlap(NX_H743_FLASH_RAM_ADDRESS(value), bytes, first, end - first);
}

/** \brief Validate both complete control objects before the first access. */
static bool objects(const nx_stm32h743_flash_state_t* port,
                    const nx_stm32h743_flash_ram_workspace_t* execution) {
    return axi(port, sizeof(*port)) && axi(execution, sizeof(*execution)) &&
           NX_H743_FLASH_RAM_ADDRESS(port) %
                   _Alignof(nx_stm32h743_flash_state_t) ==
               0U &&
           NX_H743_FLASH_RAM_ADDRESS(execution) % 1024U == 0U &&
           !alias(port, sizeof(*port), execution, sizeof(*execution)) &&
           !system_alias(port, sizeof(*port)) &&
           !system_alias(execution, sizeof(*execution)) &&
           !code_alias(port, sizeof(*port)) &&
           !code_alias(execution, sizeof(*execution));
}

static nx_result_t cpu_admission(void) {
    if ((NX_H743_FLASH_RAM_CONTROL() & 1U) != 0U ||
        NX_H743_FLASH_RAM_IPSR() != 0U || NX_H743_FLASH_RAM_PRIMASK() != 0U ||
        NX_H743_FLASH_RAM_BASEPRI() != 0U ||
        NX_H743_FLASH_RAM_FAULTMASK() != 0U)
        return NX_ERROR_CONTEXT;
    const nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    if ((NX_H743_FLASH_RAM_FPCCR() & FPU_FPCCR_LSPACT_Msk) != 0U ||
        (NX_H743_FLASH_RAM_MPU_CONTROL() & ~UINT32_C(4)) != 0U ||
        (s->core != NULL && (NX_H743_READ(&s->core->CCR) &
                             (SCB_CCR_IC_Msk | SCB_CCR_DC_Msk)) != 0U))
        return NX_ERROR_UNSUPPORTED;
    return NX_SUCCESS;
}

static bool idle(const nx_stm32h743_flash_state_t* port) {
    return !port->active && ((NX_H743_READ(&port->registers->SR1) |
                              NX_H743_READ(&port->registers->SR2)) &
                             NX_H743_BUSY) == 0U;
}

nx_result_t nx_stm32h743_flash_ram_initialize(
    nx_stm32h743_flash_state_t* port,
    nx_stm32h743_flash_ram_workspace_t* execution) {
    if (!objects(port, execution))
        return NX_ERROR_INVALID;
    nx_result_t result = cpu_admission();
    if (result != NX_SUCCESS)
        return result;
    nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    if (!s->started || s->core == NULL || s->timer == NULL ||
        s->flash == NULL || port->registers != s->flash ||
        port->memory == NULL ||
        (uintptr_t)port->memory > UINTPTR_MAX - NX_H743_FLASH_BYTES ||
        (uintptr_t)port->memory % _Alignof(uint32_t) != 0U ||
        port->poll_limit == 0U ||
        alias(port, sizeof(*port), (const void*)port->memory,
              NX_H743_FLASH_BYTES) ||
        alias(execution, sizeof(*execution), (const void*)port->memory,
              NX_H743_FLASH_BYTES))
        return NX_ERROR_INVALID;
    if (port->execution != NULL || execution->prepared || execution->active ||
        execution->port != NULL || !idle(port))
        return NX_ERROR_BUSY;
    if (!nx_stm32h743_is_revision_v(NX_H743_SILICON_ID()) ||
        NX_H743_FLASH_KIB() != 2048U ||
        (NX_H743_READ(&port->registers->OPTSR_CUR) &
         FLASH_OPTSR_SWAP_BANK_OPT) != 0U)
        return NX_ERROR_UNSUPPORTED;
    const uintptr_t first = NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_start);
    const uintptr_t end = NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_end);
    const uintptr_t load = NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_load_start);
    const uintptr_t load_end =
        NX_H743_FLASH_RAM_SYMBOL(__nx_flash_ram_load_end);
    const uintptr_t vectors = NX_H743_FLASH_RAM_SYMBOL(g_pfnVectors);
    /* The pinned assembly's NOTYPE symbol can be even while .thumb_set
     * vector aliases are odd. Cortex-M vector entries require Thumb bit0. */
    const uintptr_t fallback =
        NX_H743_FLASH_RAM_SYMBOL(Default_Handler) | (uintptr_t)1U;
    const nx_stm32h743_flash_image_t* image = NX_H743_FLASH_IMAGE();
    if (first < NX_H743_AXI_FIRST || end <= first || end > NX_H743_AXI_END ||
        load_end <= load || load_end - load != end - first || image == NULL ||
        image->load_start < UINT32_C(0x08000000) ||
        image->load_end > UINT32_C(0x08200000) ||
        image->load_start >= image->load_end || load < image->load_start ||
        load_end > image->load_end || vectors < image->live_start ||
        vectors > image->live_end ||
        sizeof(execution->vectors) > image->live_end - vectors ||
        NX_H743_READ(&s->core->VTOR) != vectors ||
        NX_H743_FLASH_RAM_READ32(vectors + 8U) != fallback ||
        NX_H743_FLASH_RAM_READ32(vectors + 12U) != fallback)
        return NX_ERROR_STATE;
    for (uintptr_t offset = 0U; offset < end - first; ++offset)
        NX_H743_FLASH_RAM_WRITE8(first + offset,
                                 NX_H743_FLASH_RAM_READ8(load + offset));
    NX_H743_FLASH_RAM_BARRIER();
    for (uintptr_t offset = 0U; offset < end - first; ++offset)
        if (NX_H743_FLASH_RAM_READ8(first + offset) !=
            NX_H743_FLASH_RAM_READ8(load + offset))
            return NX_ERROR_IO;
    for (size_t slot = 0U; slot < NX_H743_FLASH_RAM_VECTOR_WORDS; ++slot)
        execution->vectors[slot] =
            NX_H743_FLASH_RAM_READ32(vectors + slot * 4U);
    execution->vectors[0] =
        (uint32_t)(NX_H743_FLASH_RAM_ADDRESS(execution->stack) +
                   sizeof(execution->stack));
    execution->vectors[2] =
        (uint32_t)NX_H743_FLASH_RAM_SYMBOL(nx_stm32h743_flash_ram_nmi);
    execution->vectors[3] =
        (uint32_t)NX_H743_FLASH_RAM_SYMBOL(nx_stm32h743_flash_ram_hardfault);
    execution->port = port;
    execution->system = s;
    execution->saved_vtor = (uint32_t)vectors;
    execution->fault = NX_H743_FLASH_RAM_FAULT_NONE;
    execution->fault_status = execution->fault_exception = 0U;
    execution->destination = NULL;
    port->execution = execution;
    execution->prepared = true;
    return NX_SUCCESS;
}

nx_result_t
nx_stm32h743_flash_ram_admission(const nx_stm32h743_flash_state_t* port) {
    nx_result_t result = cpu_admission();
    if (result != NX_SUCCESS)
        return result;
    if (port->execution == NULL || port->execution->port != port ||
        port->execution->system != &g_nx_stm32h743_system ||
        !port->execution->prepared)
        return NX_ERROR_STATE;
    if (port->execution->active ||
        port->execution->guard.remaining_effects != 0U || !idle(port))
        return NX_ERROR_BUSY;
    const nx_stm32h743_system_t* s = &g_nx_stm32h743_system;
    return s->core == NULL ||
                   NX_H743_READ(&s->core->VTOR) != port->execution->saved_vtor
               ? NX_ERROR_STATE
               : NX_SUCCESS;
}

bool nx_stm32h743_flash_ram_external(const nx_stm32h743_flash_state_t* port,
                                     const void* value, size_t bytes) {
    return !code_alias(value, bytes) &&
           (port->execution == NULL ||
            !alias(value, bytes, port->execution, sizeof(*port->execution)));
}

nx_result_t
nx_stm32h743_flash_ram_stop(nx_stm32h743_flash_ram_workspace_t* execution) {
    if (execution == NULL)
        return NX_ERROR_INVALID;
    if (!execution->prepared && !execution->active && execution->port == NULL)
        return NX_SUCCESS;
    if (execution->port == NULL || execution->port->execution != execution ||
        execution->system != &g_nx_stm32h743_system)
        return NX_ERROR_STATE;
    nx_result_t result = cpu_admission();
    if (result != NX_SUCCESS)
        return result;
    if (execution->active || !idle(execution->port) ||
        execution->guard.remaining_effects != 0U)
        return NX_ERROR_BUSY;
    if (execution->system->core == NULL ||
        NX_H743_READ(&execution->system->core->VTOR) != execution->saved_vtor)
        return NX_ERROR_STATE;
    execution->port->execution = NULL;
    execution->prepared = false;
    execution->port = NULL;
    execution->system = NULL;
    execution->destination = NULL;
    execution->deadline = 0U;
    return NX_SUCCESS;
}
