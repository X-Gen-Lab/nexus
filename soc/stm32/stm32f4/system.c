/**
 * \file            system.c
 * \brief           Selected STM32F4 clock startup, rollback and timer timebase
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/core/fault.h"
#include "nexus/core/time.h"
#include "nexus_config.h"
#include "stm32f407_clock.h"
#include "stm32f407_silicon.h"
#include "stm32f407_system.h"
#if !defined(NEXUS_STM32_MODEL)
/* The input catalog is independently checked against the selected SDK. */
_Static_assert(NEXUS_CPU_EXTERNAL_IRQ_COUNT == NX_STM32_EXTERNAL_IRQ_COUNT,
               "selected SDK IRQ extent differs from generated CPU facts");
#endif
#if defined(NEXUS_MEMORY_CCM) && NEXUS_MEMORY_CCM
#include "memory_init.h"
#endif
#if defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM
#include "stm32f407_sram_boot.h"
extern const nx_stm32_sram_boot_plan_t g_nx_stm32_sram_boot_plan;
#endif
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
#include "stm32f407_lowpower.h"
#endif

#ifndef NEXUS_STM32_MODEL
nx_stm32_system_t g_nx_stm32_system = {.rcc = RCC,
                                       .flash = FLASH,
                                       .power = PWR,
                                       .timer = NEXUS_STM32_TIMEBASE_TIMER};
#endif

extern nx_stm32_system_t g_nx_stm32_system;

#ifndef NX_STM32_BOOT_CPACR
#ifdef NEXUS_STM32_MODEL
#define NX_STM32_BOOT_CPACR  UINT32_C(0x00f00000)
#define NX_STM32_BOOT_MPU    0U
#define NX_STM32_BOOT_VTOR   UINT32_C(0x08000000)
#define NX_STM32_BOOT_VECTOR UINT32_C(0x08000000)
#else
extern const uint32_t g_pfnVectors[];
#define NX_STM32_BOOT_CPACR  SCB->CPACR
#define NX_STM32_BOOT_MPU    MPU->CTRL
#define NX_STM32_BOOT_VTOR   SCB->VTOR
#define NX_STM32_BOOT_VECTOR ((uintptr_t)g_pfnVectors)
#endif
#endif

/* CMSIS/HAL readers share the last confirmed fixed HCLK observation. Startup
 * initializes DATA after SystemInit, so this reset value needs no early RAM. */
uint32_t SystemCoreClock = 16000000U;
/* Official RCC frequency getters use these CMSIS prescaler exponents. */
const uint8_t AHBPrescTable[16] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
                                   1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U};
const uint8_t APBPrescTable[8] = {0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U};

#ifndef NX_STM32_TIMEBASE_ACK
#define NX_STM32_TIMEBASE_ACK(system, value) ((system)->timer->SR = (value))
#endif

/** \brief Read the unique fixed provider clock without an OS dependency. */
nx_time_us_t nx_time_now_us(void) {
    return nx_stm32_system_now(&g_nx_stm32_system);
}

/** \brief Bridge HAL milliseconds to the existing live monotonic epoch. */
uint32_t HAL_GetTick(void) {
    return (uint32_t)(nx_time_now_us() / 1000U);
}

/** \brief Forward the explicitly reserved timer vector to the static context.
 */
void NEXUS_STM32_TIMEBASE_HANDLER(void) {
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
    nx_arch_irq_state_t saved = nx_arch_irq_save();
#endif
    nx_stm32_system_timer_irq(&g_nx_stm32_system);
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
    if (g_nx_stm32_system.started) {
        nx_stm32_lowpower_alarm_irq();
    }
    nx_arch_irq_restore(saved);
#endif
    /* ES0182 2.1.3: the final overflow/alarm stores follow the inner ack. */
    nx_arch_dsb();
}

#ifndef NX_STM32_POLL
#define NX_STM32_POLL(system) ((void)(system))
#endif

#define NX_STM32_EFFECT_CLOCK       1U
#define NX_STM32_EFFECT_TIMER       2U
#define NX_STM32_EFFECT_TIMER_GATE  4U
#define NX_STM32_EFFECT_TIMER_QUIET 8U
#ifndef NX_STM32_WRITE
#define NX_STM32_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_STM32_RNG_CONTROL
#define NX_STM32_RNG_CONTROL RNG->CR
#endif
#ifndef NX_STM32_IRQ_AVAILABLE
#ifdef NEXUS_STM32_MODEL
#define NX_STM32_IRQ_AVAILABLE(irq) ((void)(irq), true)
#else
#define NX_STM32_IRQ_AVAILABLE(irq)                                            \
    (NVIC_GetEnableIRQ(irq) == 0U && NVIC_GetPendingIRQ(irq) == 0U &&          \
     NVIC_GetActive(irq) == 0U)
#endif
#endif
#ifndef NX_STM32_IRQ_CONTROL
/** \brief Observe reserved vector withdrawal before releasing its
 * responsibility.
 */
static int timer_irq_control(IRQn_Type irq, bool enabled) {
#ifndef NEXUS_STM32_MODEL
    if (enabled) {
        NVIC_SetPriority(irq, 0U);
        NVIC_ClearPendingIRQ(irq);
        NVIC_EnableIRQ(irq);
    } else {
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);
    }
    nx_arch_dsb();
    nx_arch_isb();
    if (NVIC_GetEnableIRQ(irq) != (enabled ? 1U : 0U) ||
        (!enabled &&
         (NVIC_GetPendingIRQ(irq) != 0U || NVIC_GetActive(irq) != 0U))) {
        return -1;
    }
#else
    (void)irq;
    (void)enabled;
#endif
    return 0;
}
#define NX_STM32_IRQ_CONTROL(irq, enabled) timer_irq_control(irq, enabled)
#endif

#ifndef NX_STM32_IRQ_ISOLATE
/** \brief Isolate this active reserved handler, retaining its ownership. */
static int timer_irq_isolate(IRQn_Type irq) {
#ifndef NEXUS_STM32_MODEL
    NVIC_DisableIRQ(irq);
    NVIC_ClearPendingIRQ(irq);
    nx_arch_dsb();
    nx_arch_isb();
    /* The caller is this vector: its own ACTIVE bit must remain set until
     * exception return. Task withdrawal still requires ACTIVE to clear. */
    if (NVIC_GetEnableIRQ(irq) != 0U || NVIC_GetPendingIRQ(irq) != 0U)
        return -1;
#else
    (void)irq;
#endif
    return 0;
}
#define NX_STM32_IRQ_ISOLATE(irq) timer_irq_isolate(irq)
#endif

/** \brief Retain masks and effects after loss of the monotonic time domain. */
static _Noreturn void timer_park(void) {
    (void)nx_arch_irq_save();
    nx_fault_observe(NX_FAULT_PLATFORM, NX_ERROR_IO, __FILE__, __LINE__);
    for (;;) {
    }
}

/** \brief A critical write is established only after a barrier and readback. */
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_STM32_WRITE(reg, value);
    nx_arch_dsb();
    return (*reg & mask) == (value & mask);
}

/** \brief Preserve reserved and unselected output bits during PLL setup. */
static bool write_pll_fields(nx_stm32_system_t* system, uint32_t value) {
    uint32_t mask = RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLSRC;
#if NEXUS_CLOCK_PLL_ENABLED
    mask |= RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP | RCC_PLLCFGR_PLLQ;
#endif
    uint32_t actual = system->rcc->PLLCFGR;
    uint32_t selected = (actual & ~mask) | (value & mask);
    return actual == selected ||
           write_checked(&system->rcc->PLLCFGR, selected, mask);
}

#if NEXUS_CLOCK_PLLSAI_ENABLED
/** \brief The common auxiliary input cannot outlive a prepared borrower. */
static bool aux_consumers_quiet(const RCC_TypeDef* rcc) {
    uint32_t gates = RCC_APB2ENR_SAI1EN;
#if defined(RCC_APB2ENR_SAI2EN)
    gates |= RCC_APB2ENR_SAI2EN;
#endif
#if defined(RCC_APB2ENR_LTDCEN)
    gates |= RCC_APB2ENR_LTDCEN;
#endif
    return nx_stm32_aux_clock48_unselected(rcc) &&
           (rcc->APB2ENR & gates) == 0U &&
           (rcc->CR & (RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY)) == 0U;
}

/** \brief Preserve every field outside this selected clock's receipt. */
static bool write_aux_fields(volatile uint32_t* reg, uint32_t value,
                             uint32_t mask) {
    uint32_t actual = *reg;
    uint32_t selected = (actual & ~mask) | (value & mask);
    return actual == selected || write_checked(reg, selected, mask);
}

#endif

/** \brief Exclude retained slave, trigger, channel and DMA-burst owners. */
static bool timer_mode_idle(const TIM_TypeDef* timer) {
    return timer->CR1 == 0U && timer->CR2 == 0U && timer->SMCR == 0U &&
           timer->DIER == 0U && timer->CCMR1 == 0U && timer->CCMR2 == 0U &&
           timer->CCER == 0U && timer->DCR == 0U;
}

#if defined(PWR_CR_ODEN) || defined(PWR_CR_ADCDC1)
/** \brief The selected scale-1/prefetch plan never adopts a foreign mode. */
static bool power_mode_idle(const PWR_TypeDef* power) {
    uint32_t mask = 0U;
#if defined(PWR_CR_ODEN)
    mask |= PWR_CR_ODEN | PWR_CR_ODSWEN;
#endif
#if defined(PWR_CR_ADCDC1)
    /* RM0383/0390/0386 permit ADCDC1 only with Flash prefetch disabled. */
    mask |= PWR_CR_ADCDC1;
#endif
#if defined(PWR_CR_FISSR)
    mask |= PWR_CR_FISSR | PWR_CR_FMSSR;
#endif
    return (power->CR & mask) == 0U;
}
#endif

/** \brief Check cold ownership before any global clock or Flash change. */
static nx_result_t preflight_hardware(const nx_stm32_system_t* system) {
    if (system == NULL || system->rcc == NULL || system->flash == NULL ||
        system->power == NULL || system->timer == NULL ||
        !nx_stm32_timebase_binding_valid()) {
        return NX_ERROR_INVALID;
    }
    if ((NX_STM32_BOOT_CPACR & UINT32_C(0x00f00000)) != UINT32_C(0x00f00000) ||
        (NX_STM32_BOOT_MPU & MPU_CTRL_ENABLE_Msk) != 0U ||
        NX_STM32_BOOT_VTOR != NX_STM32_BOOT_VECTOR ||
        (NX_STM32_BOOT_VECTOR & 511U) != 0U)
        return NX_ERROR_STATE;
#if defined(NEXUS_CLOCK_SDIO_HS375) && NEXUS_CLOCK_SDIO_HS375
    /* The closed 75 MHz PLLQ profile cannot retarget a foreign 48 MHz owner. */
    if ((system->rcc->AHB2ENR & (RCC_AHB2ENR_OTGFSEN | RCC_AHB2ENR_RNGEN)) !=
            0U ||
        (system->rcc->AHB1ENR &
         (RCC_AHB1ENR_OTGHSEN | RCC_AHB1ENR_OTGHSULPIEN)) != 0U)
        return NX_ERROR_BUSY;
#endif
#if NEXUS_CLOCK_PLLSAI_ENABLED
    if (!aux_consumers_quiet(system->rcc) ||
        !nx_stm32_aux_clock_outputs_valid(system->rcc))
        return NX_ERROR_BUSY;
#endif
    uint32_t clock = system->rcc->CR;
    uint32_t foreign_sources = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_HSEBYP |
                               RCC_CR_CSSON | RCC_CR_PLLON | RCC_CR_PLLRDY |
                               RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY;
#if defined(RCC_CR_PLLSAION)
    foreign_sources |= RCC_CR_PLLSAION | RCC_CR_PLLSAIRDY;
#endif
#if defined(RCC_DCKCFGR_TIMPRE)
    if ((system->rcc->DCKCFGR & RCC_DCKCFGR_TIMPRE) != 0U)
        return NX_ERROR_BUSY;
#endif
#if defined(PWR_CR_ODEN) || defined(PWR_CR_ADCDC1)
    if ((system->rcc->APB1ENR & RCC_APB1ENR_PWREN) != 0U &&
        !power_mode_idle(system->power))
        return NX_ERROR_BUSY;
#endif
    uint32_t bus_mask = (uint32_t)(RCC_CFGR_SW | RCC_CFGR_SWS | RCC_CFGR_HPRE |
                                   RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    if (system->started || system->time_epoch == UINT64_MAX ||
        system->remaining_effects != 0U ||
        (clock & (RCC_CR_HSION | RCC_CR_HSIRDY)) !=
            (RCC_CR_HSION | RCC_CR_HSIRDY) ||
        /* The nominal HSI fallback requires the reset trim, not foreign tuning.
         */
        (clock & RCC_CR_HSITRIM) != (16U << RCC_CR_HSITRIM_Pos) ||
        (clock & foreign_sources) != 0U ||
        (system->rcc->CIR & (UINT32_C(0x7f00) | RCC_CIR_CSSF)) != 0U ||
        (system->rcc->CFGR & bus_mask) != 0U ||
        (system->rcc->SSCGR & RCC_SSCGR_SSCGEN) != 0U ||
        (system->rcc->APB1ENR & NEXUS_STM32_TIMEBASE_GATE) != 0U ||
        (system->rcc->APB1RSTR & NEXUS_STM32_TIMEBASE_RESET) != 0U ||
        !timer_mode_idle(system->timer) ||
        (system->flash->SR & FLASH_SR_BSY) != 0U ||
        !NX_STM32_IRQ_AVAILABLE(NEXUS_STM32_TIMEBASE_IRQ)) {
        return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}

/** \brief Public cold admission also excludes every incoming IRQ mask. */
nx_result_t nx_stm32_system_preflight(const nx_stm32_system_t* system) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || nx_arch_irq_is_masked())
        return NX_ERROR_CONTEXT;
    return preflight_hardware(system);
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

/** \brief Wait for one hardware condition within an explicit boot poll bound.
 */
static bool wait_bits(nx_stm32_system_t* system, volatile uint32_t* reg,
                      uint32_t mask, uint32_t value, uint32_t limit) {
    while (limit-- != 0U) {
        NX_STM32_POLL(system);
        if ((*reg & mask) == value) {
            return true;
        }
    }
    return false;
}

#if NEXUS_CLOCK_PLLSAI_ENABLED
/** \brief Establish selected outputs within the caller's finite boot bound. */
static bool start_aux(nx_stm32_system_t* system, uint32_t limit) {
    RCC_TypeDef* rcc = system->rcc;
    if (!write_aux_fields(&rcc->PLLSAICFGR, NX_STM32_PLLSAI_FIELDS,
                          NX_STM32_PLLSAI_FIELD_MASK) ||
        !write_aux_fields(&rcc->DCKCFGR, NX_STM32_PLLSAI_MUX_BITS,
                          NX_STM32_PLLSAI_MUX_MASK) ||
        !write_checked(&rcc->CR, rcc->CR | RCC_CR_PLLSAION, RCC_CR_PLLSAION))
        return false;
    return wait_bits(system, &rcc->CR, RCC_CR_PLLSAIRDY, RCC_CR_PLLSAIRDY,
                     limit);
}

/** \brief Unknown RDY withdrawal retains configuration and common input. */
static bool stop_aux(nx_stm32_system_t* system, uint32_t limit) {
    RCC_TypeDef* rcc = system->rcc;
    if (!write_checked(&rcc->CR, rcc->CR & ~(uint32_t)RCC_CR_PLLSAION,
                       RCC_CR_PLLSAION) ||
        !wait_bits(system, &rcc->CR, RCC_CR_PLLSAIRDY, 0U, limit))
        return false;
    return write_aux_fields(&rcc->PLLSAICFGR, system->saved_aux_pll,
                            NX_STM32_PLLSAI_FIELD_MASK) &&
           write_aux_fields(&rcc->DCKCFGR, system->saved_aux_mux,
                            NX_STM32_PLLSAI_MUX_MASK);
}
#endif

/** \brief Restore HSI only after its ready and source status are observed. */
static int restore_hsi(nx_stm32_system_t* system, uint32_t limit) {
    NX_STM32_WRITE(&system->rcc->CR, system->rcc->CR | RCC_CR_HSION);
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY,
                   limit)) {
        return -1;
    }
    NX_STM32_WRITE(&system->rcc->CFGR,
                   system->rcc->CFGR & ~(uint32_t)RCC_CFGR_SW);
    if (!wait_bits(system, &system->rcc->CFGR, RCC_CFGR_SWS, 0U, limit)) {
        return -1;
    }
    /* Startup may fail before establishing the selected divider. Publish
     * the observed HSI/HPRE pair even if later withdrawal is refused. */
    uint32_t hpre = (uint32_t)((system->rcc->CFGR & RCC_CFGR_HPRE) >> 4U);
    system->core_hz = 16000000U >> AHBPrescTable[hpre];
    SystemCoreClock = system->core_hz;
#if NEXUS_CLOCK_PLLSAI_ENABLED
    if (!stop_aux(system, limit))
        return -1;
#endif
    NX_STM32_WRITE(&system->rcc->CR,
                   system->rcc->CR &
                       ~(uint32_t)(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON));
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_PLLRDY | RCC_CR_HSERDY, 0U,
                   limit)) {
        return -1;
    }
    if (!write_checked(
            &system->rcc->CFGR,
            system->rcc->CFGR &
                ~(uint32_t)(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2),
            RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) {
        return -1;
    }
    system->core_hz = 16000000U;
    SystemCoreClock = system->core_hz;
    if (!write_pll_fields(system, system->saved_pll))
        return -1;
    if (!write_checked(&system->flash->ACR, system->saved_flash, UINT32_MAX)) {
        return -1;
    }
    if (system->power_snapshotted &&
        !write_checked(&system->power->CR,
                       (system->power->CR & ~(uint32_t)PWR_CR_VOS) |
                           system->saved_power,
                       PWR_CR_VOS)) {
        return -1;
    }
    if (system->owned_power_clock &&
        !write_checked(&system->rcc->APB1ENR,
                       system->rcc->APB1ENR & ~(uint32_t)RCC_APB1ENR_PWREN,
                       RCC_APB1ENR_PWREN)) {
        return -1;
    }
    system->remaining_effects &= ~NX_STM32_EFFECT_CLOCK;
    return 0;
}

/** \brief Initialize CPU registers without touching not-yet-initialized RAM. */
bool nx_stm32_system_cpu_boot(SCB_Type* scb,
                              volatile const uint32_t* mpu_control,
                              uintptr_t vectors) {
    if (scb == NULL || mpu_control == NULL ||
        (*mpu_control & MPU_CTRL_ENABLE_Msk) != 0U || vectors < FLASH_BASE ||
        vectors > FLASH_END || (vectors & 511U) != 0U)
        return false;
    if (!write_checked(&scb->CPACR, scb->CPACR | UINT32_C(0x00f00000),
                       UINT32_C(0x00f00000)))
        return false;
    nx_arch_isb();
    if (!write_checked(&scb->VTOR, (uint32_t)vectors, UINT32_MAX))
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

/** \brief A failed HSE has already invalidated every timer-derived deadline.
 */
void nx_stm32_system_css_irq(nx_stm32_system_t* system) {
    (void)nx_arch_irq_save();
    if (system != NULL) {
        system->timer_error = NX_ERROR_IO;
        system->started = false;
        if (system->rcc != NULL) {
            uint32_t enabled = system->rcc->CIR & UINT32_C(0x7f00);
            NX_STM32_WRITE(&system->rcc->CIR, enabled | RCC_CIR_CSSC);
            if ((system->rcc->CFGR & RCC_CFGR_SWS) == 0U) {
                uint32_t hpre =
                    (uint32_t)((system->rcc->CFGR & RCC_CFGR_HPRE) >> 4U);
                system->core_hz = 16000000U >> AHBPrescTable[hpre];
                SystemCoreClock = system->core_hz;
            }
        }
        if (system->timer != NULL) {
            NX_STM32_WRITE(&system->timer->DIER, 0U);
            NX_STM32_WRITE(&system->timer->CR1, 0U);
        }
        nx_arch_dsb();
    }
    timer_park();
}

#if NEXUS_CLOCK_CSS
void NMI_Handler(void);
/** \brief Selected CSS owns NMI; unrelated unexpected NMI also fails stop. */
void NMI_Handler(void) {
    nx_stm32_system_css_irq(&g_nx_stm32_system);
}
#endif

/** \brief Establish the one supported fixed clock profile and selected timer
 * source. */
static nx_stm32_start_result_t start_owned(nx_stm32_system_t* system,
                                           uint32_t poll_limit) {
    nx_stm32_start_result_t result = {0, 0, 0U};
    if (poll_limit == 0U || preflight_hardware(system) != NX_SUCCESS) {
        result.primary = -2;
        return result;
    }
    system->timer_error = NX_SUCCESS;
    system->core_hz = 16000000U;
    SystemCoreClock = system->core_hz;
    if (!system->reset_captured) {
        system->reset_cause = system->rcc->CSR;
        system->reset_captured = true;
    }
    system->saved_pll = system->rcc->PLLCFGR;
#if NEXUS_CLOCK_PLLSAI_ENABLED
    system->saved_aux_pll = system->rcc->PLLSAICFGR;
    system->saved_aux_mux = system->rcc->DCKCFGR & NX_STM32_PLLSAI_MUX_MASK;
#endif
    system->saved_flash = system->flash->ACR;
    system->owned_power_clock =
        (system->rcc->APB1ENR & RCC_APB1ENR_PWREN) == 0U;
    system->power_snapshotted = false;
    system->remaining_effects = NX_STM32_EFFECT_CLOCK;
    if (!write_checked(&system->rcc->APB1ENR,
                       system->rcc->APB1ENR | RCC_APB1ENR_PWREN,
                       RCC_APB1ENR_PWREN)) {
        result.primary = -6;
        goto rollback;
    }
#if defined(PWR_CR_ODEN) || defined(PWR_CR_ADCDC1)
    /* A previously gated PWR block cannot prove absence of a foreign mode.
     * No selected clock plan adopts or clears foreign overdrive/ADC DC modes.
     */
    if (!power_mode_idle(system->power)) {
        result.primary = -6;
        goto rollback;
    }
#endif
    system->saved_power = system->power->CR & PWR_CR_VOS;
    system->power_snapshotted = true;
    if (!write_checked(&system->power->CR, system->power->CR | PWR_CR_VOS,
                       PWR_CR_VOS) ||
        !write_checked(&system->flash->ACR,
                       NEXUS_FLASH_WAIT_STATES | FLASH_ACR_ICEN |
                           FLASH_ACR_DCEN | FLASH_ACR_PRFTEN,
                       FLASH_ACR_LATENCY | FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                           FLASH_ACR_PRFTEN)) {
        result.primary = -6;
        goto rollback;
    }
#if NEXUS_CLOCK_SOURCE_HSE
    NX_STM32_WRITE(&system->rcc->CR, system->rcc->CR | RCC_CR_HSEON);
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_HSERDY, RCC_CR_HSERDY,
                   poll_limit)) {
        result.primary = -3;
        goto rollback;
    }
#endif
    if (!write_pll_fields(system, NEXUS_CLOCK_PLLCFGR)) {
        result.primary = -6;
        goto rollback;
    }
#if NEXUS_CLOCK_PLLSAI_ENABLED
    if (!start_aux(system, poll_limit)) {
        result.primary = -7;
        goto rollback;
    }
#endif
    if (!write_checked(&system->rcc->CFGR, NEXUS_CLOCK_CFGR,
                       RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) {
        result.primary = -6;
        goto rollback;
    }
#if NEXUS_CLOCK_PLL_ENABLED
    NX_STM32_WRITE(&system->rcc->CR, system->rcc->CR | RCC_CR_PLLON);
    if (!wait_bits(system, &system->rcc->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY,
                   poll_limit)) {
        result.primary = -4;
        goto rollback;
    }
#if defined(PWR_CR_VOS_1)
    /* RM0383/0390/0386: the two-bit voltage scale becomes active only after
     * PLLON. PLLRDY alone does not authorize the faster system-clock switch. */
    if (!wait_bits(system, &system->power->CSR, PWR_CSR_VOSRDY, PWR_CSR_VOSRDY,
                   poll_limit)) {
        result.primary = -8;
        goto rollback;
    }
#endif
#endif
    NX_STM32_WRITE(&system->rcc->CFGR,
                   (system->rcc->CFGR & ~(uint32_t)RCC_CFGR_SW) |
                       NEXUS_CLOCK_SW);
    if (!wait_bits(system, &system->rcc->CFGR, RCC_CFGR_SWS,
                   NEXUS_CLOCK_SW << 2U, poll_limit)) {
        result.primary = -5;
        goto rollback;
    }
    /* Consumers start only after the timebase, but failed timer preparation
     * can leave the PLL live. Metadata follows hardware, not API success. */
    system->core_hz = NEXUS_CORE_HZ;
    SystemCoreClock = system->core_hz;
    if (NEXUS_CLOCK_CSS &&
        !write_checked(&system->rcc->CR, system->rcc->CR | RCC_CR_CSSON,
                       RCC_CR_CSSON)) {
        result.primary = -6;
        goto rollback;
    }
    system->remaining_effects |= NX_STM32_EFFECT_TIMER_GATE;
    if (!write_checked(&system->rcc->APB1ENR,
                       system->rcc->APB1ENR | NEXUS_STM32_TIMEBASE_GATE,
                       NEXUS_STM32_TIMEBASE_GATE)) {
        result.primary = -6;
        goto rollback;
    }
    /* Gated MMIO may have read as zero during preflight. A foreign timer
     * discovered now lends no register or IRQ ownership to cleanup. */
    if (!timer_mode_idle(system->timer) ||
        (system->rcc->APB1RSTR & NEXUS_STM32_TIMEBASE_RESET) != 0U) {
        result.primary = -6;
        goto rollback;
    }
    system->remaining_effects |= NX_STM32_EFFECT_TIMER;
    if (!write_checked(&system->rcc->APB1RSTR,
                       system->rcc->APB1RSTR | NEXUS_STM32_TIMEBASE_RESET,
                       NEXUS_STM32_TIMEBASE_RESET) ||
        !write_checked(&system->rcc->APB1RSTR,
                       system->rcc->APB1RSTR &
                           ~(uint32_t)NEXUS_STM32_TIMEBASE_RESET,
                       NEXUS_STM32_TIMEBASE_RESET) ||
        !timer_mode_idle(system->timer)) {
        result.primary = -6;
        goto rollback;
    }
    bool valid =
        write_checked(&system->timer->CR1, 0U, UINT32_MAX) &&
        write_checked(&system->timer->PSC, NEXUS_APB1_TIMER_HZ / 1000000U - 1U,
                      UINT32_MAX) &&
        write_checked(&system->timer->ARR, UINT32_MAX, UINT32_MAX) &&
        write_checked(&system->timer->SR, 0U, UINT32_MAX);
    if (!valid) {
        result.primary = -6;
        goto rollback;
    }
    /* MMS=reset exports UG as TRGO. While CEN is zero, MMS=enable keeps
     * TRGO quiet through shadow transfer. Restore reset before enabling. */
    system->remaining_effects |= NX_STM32_EFFECT_TIMER_QUIET;
    if (!write_checked(&system->timer->CR2, TIM_CR2_MMS_0, TIM_CR2_MMS)) {
        result.primary = -6;
        goto rollback;
    }
    /* Observe a fresh update event before accepting the prescaler shadow.
     * An old UIF or a refused UG cannot establish this new time epoch. */
    NX_STM32_WRITE(&system->timer->EGR, TIM_EGR_UG);
    nx_arch_dsb();
    if (!wait_bits(system, &system->timer->SR, TIM_SR_UIF, TIM_SR_UIF,
                   poll_limit) ||
        !write_checked(&system->timer->SR, 0U, UINT32_MAX) ||
        !write_checked(&system->timer->CNT, 0U, UINT32_MAX) ||
        !write_checked(&system->timer->CR2, 0U, TIM_CR2_MMS)) {
        result.primary = -6;
        goto rollback;
    }
    system->remaining_effects &= ~NX_STM32_EFFECT_TIMER_QUIET;
    system->overflow_us = 0U;
    valid = write_checked(&system->timer->DIER, TIM_DIER_UIE, UINT32_MAX) &&
            write_checked(&system->timer->CR1, TIM_CR1_CEN, UINT32_MAX) &&
            valid;
    if (!valid || NX_STM32_IRQ_CONTROL(NEXUS_STM32_TIMEBASE_IRQ, true) != 0) {
        result.primary = -6;
        goto rollback;
    }
    ++system->time_epoch;
    system->started = true;
    return result;
rollback:
    result.cleanup = nx_stm32_system_stop(system, poll_limit);
    result.remaining_effects = system->remaining_effects;
    return result;
}

/** \brief Ordinary startup never changes an incoming caller mask. */
nx_stm32_start_result_t nx_stm32_system_start(nx_stm32_system_t* system,
                                              uint32_t poll_limit) {
    if (nx_stm32_system_preflight(system) != NX_SUCCESS)
        return (nx_stm32_start_result_t){-2, 0, 0U};
    return start_owned(system, poll_limit);
}

/** \brief The deep-power owner restores while wake IRQs remain excluded. */
nx_stm32_start_result_t nx_stm32_system_restore(nx_stm32_system_t* system,
                                                uint32_t poll_limit) {
    nx_arch_irq_masks_t masks = nx_arch_irq_masks();
    if (!nx_arch_is_privileged() || nx_arch_in_isr() || masks.primask != 1U ||
        masks.basepri != 0U || masks.faultmask != 0U)
        return (nx_stm32_start_result_t){-2, 0, 0U};
    return start_owned(system, poll_limit);
}

/** \brief Stop timer access before attempting clock cleanup. */
int nx_stm32_system_stop(nx_stm32_system_t* system, uint32_t poll_limit) {
    if (system == NULL || system->rcc == NULL || system->timer == NULL ||
        system->flash == NULL || system->power == NULL || poll_limit == 0U) {
        return -2;
    }
#if NEXUS_CLOCK_PLLSAI_ENABLED
    if ((system->remaining_effects & NX_STM32_EFFECT_CLOCK) != 0U &&
        !aux_consumers_quiet(system->rcc))
        return -1;
#endif
        /* An explicitly enabled RNG still leases PLL48 after synchronous calls
         * have returned. Its owner withdraws RNGEN before this clock teardown.
         */
#if defined(RCC_AHB2ENR_RNGEN)
    if ((system->rcc->AHB2ENR & RCC_AHB2ENR_RNGEN) != 0U &&
        (NX_STM32_RNG_CONTROL & (RNG_CR_RNGEN | RNG_CR_IE)) != 0U)
        return -1;
#endif
#if defined(NEXUS_TIMEBASE_LOWPOWER) && NEXUS_TIMEBASE_LOWPOWER
    if (nx_stm32_lowpower_busy()) {
        return -1;
    }
#endif
    if ((system->remaining_effects & NX_STM32_EFFECT_TIMER) != 0U) {
        system->started = false;
        /* Initial withdrawal is best effort: an asserted source can repend
         * while it is still enabled. Its final proof follows source clear. */
        (void)NX_STM32_IRQ_CONTROL(NEXUS_STM32_TIMEBASE_IRQ, false);
        if (!write_checked(&system->rcc->APB1RSTR,
                           system->rcc->APB1RSTR &
                               ~(uint32_t)NEXUS_STM32_TIMEBASE_RESET,
                           NEXUS_STM32_TIMEBASE_RESET) ||
            !write_checked(&system->timer->DIER, 0U, UINT32_MAX) ||
            !write_checked(&system->timer->CR1, 0U, UINT32_MAX) ||
            !write_checked(&system->timer->SR, 0U, UINT32_MAX)) {
            return -1;
        }
        if ((system->remaining_effects & NX_STM32_EFFECT_TIMER_QUIET) != 0U) {
            if (!write_checked(&system->timer->CR2, 0U, TIM_CR2_MMS))
                return -1;
            system->remaining_effects &= ~NX_STM32_EFFECT_TIMER_QUIET;
        }
        /* The source can repend after the first vector withdrawal. Only a
         * stopped, cleared peripheral permits final vector/owner release. */
        if (NX_STM32_IRQ_CONTROL(NEXUS_STM32_TIMEBASE_IRQ, false) != 0)
            return -1;
        system->remaining_effects &= ~NX_STM32_EFFECT_TIMER;
    }
    if ((system->remaining_effects & NX_STM32_EFFECT_TIMER_GATE) != 0U) {
        if (!write_checked(&system->rcc->APB1ENR,
                           system->rcc->APB1ENR &
                               ~(uint32_t)NEXUS_STM32_TIMEBASE_GATE,
                           NEXUS_STM32_TIMEBASE_GATE)) {
            return -1;
        }
        system->remaining_effects &= ~NX_STM32_EFFECT_TIMER_GATE;
    }
    if ((system->remaining_effects & NX_STM32_EFFECT_CLOCK) != 0U) {
        return restore_hsi(system, poll_limit);
    }
    return 0;
}

/** \brief Count a hardware overflow once, clearing only the update source. */
void nx_stm32_system_timer_irq(nx_stm32_system_t* system) {
    if (system != NULL &&
        (system->started ||
         (system->remaining_effects & NX_STM32_EFFECT_TIMER) != 0U) &&
        (system->timer->SR & TIM_SR_UIF) != 0U) {
        if (system->timer_error != NX_SUCCESS)
            return;
        NX_STM32_TIMEBASE_ACK(system, ~(uint32_t)TIM_SR_UIF);
        nx_arch_dsb();
        if ((system->timer->SR & TIM_SR_UIF) != 0U) {
            /* An unconfirmed wrap invalidates all old deadlines. Retain the
             * timer and clock through terminal observation; never count twice.
             */
            system->started = false;
            system->timer_error = NX_ERROR_IO;
            (void)write_checked(&system->timer->DIER, 0U, UINT32_MAX);
            (void)write_checked(&system->timer->CR1, 0U, TIM_CR1_CEN);
            (void)NX_STM32_IRQ_ISOLATE(NEXUS_STM32_TIMEBASE_IRQ);
            /* Isolation cannot recover deadlines already held by consumers.
             * Always observe the lost domain and retain terminal ownership. */
            timer_park();
        }
        /* Retained IRQs still acknowledge their level source during failed
         * withdrawal; that does not make a withdrawn epoch valid again. */
        if (system->started) {
            system->overflow_us += (UINT64_C(1) << 32U);
        }
    }
}

/** \brief Fold pending overflow into one interrupt-protected timer snapshot. */
uint64_t nx_stm32_system_now(nx_stm32_system_t* system) {
    if (system == NULL || !system->started) {
        return 0U;
    }
    nx_arch_irq_state_t mask = nx_arch_irq_save();
    /* A legal timer IRQ may have ended this epoch before exclusion. */
    if (!system->started) {
        nx_arch_irq_restore(mask);
        return 0U;
    }
    uint64_t high = system->overflow_us;
    uint32_t count = system->timer->CNT;
    if ((system->timer->SR & TIM_SR_UIF) != 0U) {
        high += (UINT64_C(1) << 32U);
        count = system->timer->CNT;
    }
    nx_arch_irq_restore(mask);
    return high + count;
}

/** \brief Validate both sides of the snapshot; valid zero remains observable.
 */
uint64_t nx_stm32_system_clock_read(void* context) {
    if (context != &g_nx_stm32_system || !g_nx_stm32_system.started ||
        g_nx_stm32_system.timer_error != NX_SUCCESS)
        return UINT64_MAX;
    uint64_t now = nx_stm32_system_now(&g_nx_stm32_system);
    return g_nx_stm32_system.started &&
                   g_nx_stm32_system.timer_error == NX_SUCCESS
               ? now
               : UINT64_MAX;
}

/** \brief Validate observed density without pretending DEV_ID encodes package.
 */
nx_result_t nx_stm32_validate_identity(uint32_t expected_flash_bytes) {
    if (expected_flash_bytes != NX_STM32_FLASH_MIN_BYTES &&
        expected_flash_bytes != NX_STM32_FLASH_MAX_BYTES) {
        return NX_ERROR_INVALID;
    }
#ifndef NX_STM32_FLASH_KIB
#define NX_STM32_FLASH_KIB() (*(volatile const uint16_t*)FLASHSIZE_BASE)
#define NX_STM32_DEVICE_ID() (DBGMCU->IDCODE)
#endif
    uint32_t observed_bytes = (uint32_t)NX_STM32_FLASH_KIB() * 1024U;
    if (observed_bytes != expected_flash_bytes) {
        return NX_ERROR_IO;
    }
    return nx_stm32f407_silicon_admit(NX_STM32_DEVICE_ID());
}
