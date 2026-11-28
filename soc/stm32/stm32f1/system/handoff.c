/**
 * \file            handoff.c
 * \brief           Exact F103RB cold admission for a masked offset image.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/arch/arch.h"
#include "nexus/soc/stm32f103rb_handoff.h"
#include "nexus_bindings.h"
#include "nexus_config.h"
#include "stm32f1xx.h"
#include "system/platform.h"

#if !defined(STM32F103xB)
#error "This handoff contract uses STM32F103xB"
#endif

#ifdef NEXUS_F103_HANDOFF_MODEL
#include "handoff_model.h"
#define READ(reg)            f103_read((uintptr_t) & (reg))
#define FLASH_KIB()          f103_flash_kib()
#define CONTROL()            f103_control()
#define VECTOR_WORD(address) f103_vector_word(address)
#else
#if !defined(__ARM_ARCH_7M__) || !defined(NEXUS_CLOCK_SOURCE_HSI) ||           \
    NEXUS_CORE_HZ != 8000000U
#error "The handoff requires the reviewed Cortex-M3 HSI8 profile"
#endif
#define READ(reg)            ((uint32_t)(reg))
#define FLASH_KIB()          (*(volatile const uint16_t*)(uintptr_t)FLASHSIZE_BASE)
#define VECTOR_WORD(address) (*(volatile const uint32_t*)(uintptr_t)(address))
static uint32_t CONTROL(void) {
    uint32_t control;
    __asm__ volatile("mrs %0, control" : "=r"(control)::"memory");
    return control;
}
_Noreturn void nx_stm32f103rb_handoff_tail(uint32_t vector, uint32_t stack,
                                           uint32_t pc);
#endif

_Static_assert(USBWakeUp_IRQn == 42U && __MPU_PRESENT == 0U,
               "Exact Cortex-M3 Startup and vector geometry");
enum {
    VECTOR = 0x08004200U,
    VECTOR_BYTES = (16U + 43U + 8U) * sizeof(uint32_t),
    IMAGE_END = 0x08013c00U,
    STACK_MIN = 0x20000800U,
    STACK_END = 0x20005000U,
};
_Static_assert(VECTOR % 512U == 0U, "RM PM0056 reviewed VTOR alignment");

nx_result_t nx_stm32f103rb_handoff_prepare(nx_stm32f103rb_handoff_t* owner,
                                           nx_clock_barrier_t barrier) {
    if (owner == NULL || barrier.domain != &owner->domain)
        return NX_ERROR_INVALID;
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U ||
        nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    owner->stopped_epoch = 0U;
    nx_result_t result = nx_clock_domain_quiescent(barrier);
    if (result != NX_SUCCESS)
        return result;
    result = nx_platform_stop();
    if (result == NX_SUCCESS)
        owner->stopped_epoch = barrier.epoch;
    return result;
}

/** \brief Exclude exception publishers, alternate stack mode and debug. */
static nx_result_t cpu_ready(void) {
    if ((READ(SCB->CPUID) & UINT32_C(0xff00fff0)) != UINT32_C(0x4100c230) ||
        READ(SCB->VTOR) != FLASH_BASE || READ(SCB->SCR) != 0U ||
        (READ(SCB->CCR) & ~(uint32_t)SCB_CCR_STKALIGN_Msk) != 0U ||
        (READ(DCB->DHCSR) & UINT32_C(0xf)) != 0U || READ(DCB->DEMCR) != 0U)
        return NX_ERROR_UNSUPPORTED;
    const uint32_t pending = SCB_ICSR_VECTACTIVE_Msk |
                             SCB_ICSR_VECTPENDING_Msk |
                             SCB_ICSR_ISRPENDING_Msk | SCB_ICSR_ISRPREEMPT_Msk |
                             SCB_ICSR_PENDSTSET_Msk | SCB_ICSR_PENDSVSET_Msk |
                             SCB_ICSR_NMIPENDSET_Msk;
    if ((READ(SCB->ICSR) & pending) != 0U || READ(SCB->SHCSR) != 0U ||
        READ(SCB->CFSR) != 0U || READ(SCB->HFSR) != 0U ||
        (READ(SysTick->CTRL) &
         (SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk)) != 0U ||
        READ(SysTick->LOAD) != 0U || READ(SysTick->VAL) != 0U)
        return NX_ERROR_BUSY;
    for (size_t bank = 0U; bank < 2U; ++bank) {
        if (READ(NVIC->ISER[bank]) != 0U || READ(NVIC->ISPR[bank]) != 0U ||
            READ(NVIC->IABR[bank]) != 0U)
            return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

/** \brief Reject any live bus, backup-domain, Flash or watchdog ownership. */
static bool clocks_idle(void) {
    const uint32_t sources = RCC_CR_HSION | RCC_CR_HSIRDY | RCC_CR_HSITRIM |
                             RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_HSEBYP |
                             RCC_CR_CSSON | RCC_CR_PLLON | RCC_CR_PLLRDY;
    const uint32_t hsi8 =
        RCC_CR_HSION | RCC_CR_HSIRDY | (16U << RCC_CR_HSITRIM_Pos);
    const uint32_t publishers = RCC_CIR_CSSF | RCC_CIR_LSIRDYIE |
                                RCC_CIR_LSERDYIE | RCC_CIR_HSIRDYIE |
                                RCC_CIR_HSERDYIE | RCC_CIR_PLLRDYIE;
    const uint32_t backup = RCC_BDCR_BDRST | RCC_BDCR_RTCEN | RCC_BDCR_RTCSEL |
                            RCC_BDCR_LSEBYP | RCC_BDCR_LSEON | RCC_BDCR_LSERDY;
    return (READ(RCC->CR) & sources) == hsi8 && READ(RCC->CFGR) == 0U &&
           (READ(RCC->CIR) & publishers) == 0U &&
           READ(RCC->AHBENR) == (RCC_AHBENR_SRAMEN | RCC_AHBENR_FLITFEN) &&
           READ(RCC->APB1ENR) == 0U && READ(RCC->APB2ENR) == 0U &&
           READ(RCC->APB1RSTR) == 0U && READ(RCC->APB2RSTR) == 0U &&
           (READ(RCC->BDCR) & backup) == 0U &&
           (READ(RCC->CSR) & (RCC_CSR_LSION | RCC_CSR_LSIRDY)) == 0U &&
           (READ(FLASH->OBR) & (FLASH_OBR_IWDG_SW | FLASH_OBR_OPTERR)) ==
               FLASH_OBR_IWDG_SW &&
           (READ(FLASH->ACR) & (FLASH_ACR_LATENCY | FLASH_ACR_HLFCYA)) == 0U &&
           (READ(FLASH->SR) & FLASH_SR_BSY) == 0U &&
           READ(FLASH->CR) == FLASH_CR_LOCK;
}

nx_result_t
nx_stm32f103rb_handoff_enter(const nx_stm32f103rb_handoff_t* owner,
                             nx_clock_barrier_t barrier,
                             nx_stm32f103rb_executable_t executable) {
    if (owner == NULL || barrier.domain != &owner->domain ||
        executable.begin < VECTOR + VECTOR_BYTES ||
        executable.end > IMAGE_END || executable.begin >= executable.end ||
        ((executable.begin | executable.end) & 1U) != 0U)
        return NX_ERROR_INVALID;
    if (!nx_arch_is_privileged() || nx_arch_exception_number() != 0U)
        return NX_ERROR_CONTEXT;
    const nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (masks.primask != 1U || masks.basepri != 0U || masks.faultmask != 0U ||
        CONTROL() != 0U)
        return NX_ERROR_CONTEXT;
    nx_result_t result = nx_clock_domain_quiescent(barrier);
    if (result != NX_SUCCESS)
        return result;
    if (owner->stopped_epoch != barrier.epoch || nx_stm32f1_system_live())
        return NX_ERROR_STATE;
    result = cpu_ready();
    if (result != NX_SUCCESS)
        return result;
    /* ES096 forbids relying on vendor DBGMCU reads for identification. */
    if (FLASH_KIB() != 128U)
        return NX_ERROR_UNSUPPORTED;
    if (!clocks_idle())
        return NX_ERROR_BUSY;
    const uint32_t stack = VECTOR_WORD(VECTOR);
    const uint32_t pc = VECTOR_WORD(VECTOR + 4U);
    const uint32_t target = pc & ~UINT32_C(1);
    if (stack < STACK_MIN || stack > STACK_END || (stack & 7U) != 0U ||
        (pc & 1U) == 0U || target < executable.begin ||
        target > executable.end - 2U)
        return NX_ERROR_INVALID;
#ifdef NEXUS_F103_HANDOFF_MODEL
    return f103_tail(VECTOR, stack, pc);
#else
    nx_stm32f103rb_handoff_tail(VECTOR, stack, pc);
#endif
}
