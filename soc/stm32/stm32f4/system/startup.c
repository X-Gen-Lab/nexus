/**
 * \file            startup.c
 * \brief           Pre-C CPU and selected memory admission without global RAM
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-14
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "system/stm32f407_system.h"
#if defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM
#include "system/memory_init.h"
#endif
#if defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM
#include "system/stm32f407_sram_boot.h"
extern const nx_stm32_sram_boot_plan_t g_nx_stm32_sram_boot_plan;
#endif

#ifndef NEXUS_STM32_MODEL
extern const uint32_t g_pfnVectors[];
#endif
#ifndef NX_STM32_WRITE
#define NX_STM32_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif

/** \brief Prove a pre-C write using only reset-stack locals and MMIO. */
static bool boot_write_checked(volatile uint32_t* reg, uint32_t value,
                               uint32_t mask) {
    NX_STM32_WRITE(reg, value);
    nx_arch_dsb();
    return (*reg & mask) == (value & mask);
}

#if !defined(NEXUS_STM32_MODEL) ||                                             \
    (defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM) ||                         \
    (defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM)
#ifndef NX_STM32_BOOT_FAILURE
/** \brief Fail before constructors without observing initialized global RAM. */
static _Noreturn void boot_failure(uint32_t reason) {
    volatile uint32_t retained_reason = reason;
    __disable_irq();
    for (;;) {
        (void)retained_reason;
        __NOP();
    }
}
#define NX_STM32_BOOT_FAILURE(reason) boot_failure(reason)
#endif
#endif

/** \brief Initialize CPU registers without touching not-yet-initialized RAM. */
bool nx_stm32_system_cpu_boot(SCB_Type* scb,
                              volatile const uint32_t* mpu_control,
                              uintptr_t vectors) {
    if (scb == NULL || mpu_control == NULL ||
        (*mpu_control & MPU_CTRL_ENABLE_Msk) != 0U || vectors < FLASH_BASE ||
        vectors > FLASH_END || (vectors & 511U) != 0U)
        return false;
    if (!boot_write_checked(&scb->CPACR, scb->CPACR | UINT32_C(0x00f00000),
                            UINT32_C(0x00f00000)))
        return false;
    nx_arch_isb();
    if (!boot_write_checked(&scb->VTOR, (uint32_t)vectors, UINT32_MAX))
        return false;
    nx_arch_isb();
    return true;
}

/** \brief Initialize CPU registers without erasing inherited clock evidence. */
void SystemInit(void) {
#if defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM
    /* The complete Flash plan and incoming reset profile must be proved
     * before RCC writes can erase evidence of a warm/offset handoff. */
    if (!nx_stm32_sram_bootstrap(&g_nx_stm32_sram_boot_plan)) {
        NX_STM32_BOOT_FAILURE(3U);
    }
#endif
#ifndef NEXUS_STM32_MODEL
    if (!nx_stm32_system_cpu_boot(SCB, &MPU->CTRL, (uintptr_t)g_pfnVectors))
        NX_STM32_BOOT_FAILURE(4U);
    NVIC_SetPriorityGrouping(3U);
#endif
#if defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM
    /* Official ST startup has not copied DATA or cleared BSS yet. The plan
     * comes only from linker symbols/Flash and uses the internal SRAM stack. */
    RCC->AHB1ENR |= RCC_AHB1ENR_CCMDATARAMEN;
    nx_arch_dsb();
    if ((RCC->AHB1ENR & RCC_AHB1ENR_CCMDATARAMEN) == 0U) {
        NX_STM32_BOOT_FAILURE(1U);
    }
    if (!nx_stm32_memory_boot()) {
        NX_STM32_BOOT_FAILURE(2U);
    }
    nx_arch_dsb();
#endif
}
