/**
 * \file            system.c
 * \brief           GD32F470 fixed clock profile and reset CPU initialization
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-09
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "private/system.h"
#include "gd32f4xx.h"
#include "nexus_config.h"

#include <stdbool.h>

#if (defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM == 1) ||            \
    (defined(NEXUS_MEMORY_TCM) && NEXUS_MEMORY_TCM == 1)
#include "private/memory_init.h"
extern uint32_t __nx_flash_start;
extern uint32_t __nx_flash_end;
#if defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM == 1
extern uint32_t __nx_addsram_region_start;
extern uint32_t __nx_addsram_region_end;
extern uint32_t __nx_addsram_data_load;
extern uint32_t __nx_addsram_data_start;
extern uint32_t __nx_addsram_data_end;
extern uint32_t __nx_addsram_bss_start;
extern uint32_t __nx_addsram_bss_end;
extern uint32_t __nx_addsram_noinit_start;
extern uint32_t __nx_addsram_noinit_end;
#endif
#if defined(NEXUS_MEMORY_TCM) && NEXUS_MEMORY_TCM == 1
extern uint32_t __nx_tcm_region_start;
extern uint32_t __nx_tcm_region_end;
extern uint32_t __nx_tcm_data_load;
extern uint32_t __nx_tcm_data_start;
extern uint32_t __nx_tcm_data_end;
extern uint32_t __nx_tcm_bss_start;
extern uint32_t __nx_tcm_bss_end;
extern uint32_t __nx_tcm_noinit_start;
extern uint32_t __nx_tcm_noinit_end;
#endif

/** \brief Reset stack and Flash control must not depend on extra memory. */
static bool initialize_extra_memory(void) {
    const nx_gd32_memory_region_t regions[] = {
#if defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM == 1
        {(uintptr_t)&__nx_addsram_region_start,
         (uintptr_t)&__nx_addsram_region_end,
         (uintptr_t)&__nx_addsram_data_load,
         (uintptr_t)&__nx_addsram_data_start, (uintptr_t)&__nx_addsram_data_end,
         (uintptr_t)&__nx_addsram_bss_start, (uintptr_t)&__nx_addsram_bss_end,
         (uintptr_t)&__nx_addsram_noinit_start,
         (uintptr_t)&__nx_addsram_noinit_end},
#endif
#if defined(NEXUS_MEMORY_TCM) && NEXUS_MEMORY_TCM == 1
        {(uintptr_t)&__nx_tcm_region_start, (uintptr_t)&__nx_tcm_region_end,
         (uintptr_t)&__nx_tcm_data_load, (uintptr_t)&__nx_tcm_data_start,
         (uintptr_t)&__nx_tcm_data_end, (uintptr_t)&__nx_tcm_bss_start,
         (uintptr_t)&__nx_tcm_bss_end, (uintptr_t)&__nx_tcm_noinit_start,
         (uintptr_t)&__nx_tcm_noinit_end},
#endif
    };
    return nx_gd32_memory_initialize(
        regions, sizeof(regions) / sizeof(regions[0]),
        (uintptr_t)&__nx_flash_start, (uintptr_t)&__nx_flash_end);
}
#endif

uint32_t SystemCoreClock = 16000000u;
uint32_t g_gd32_reset_flags;
volatile uint32_t g_gd32_boot_error;
#ifndef NX_GD32_CLOCK_WRITE
#define NX_GD32_CLOCK_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#define NX_GD32_POWER_FIELDS                                                   \
    ((uint32_t)(PMU_CTL_LDOVS | PMU_CTL_HDEN | PMU_CTL_HDS))
static struct {
    uint32_t pll;
    uint32_t flash;
    uint32_t power;
    bool captured;
    bool power_snapshotted;
    bool owned_power_clock;
} s_clock;

/** \brief Readback, not a successful store instruction, establishes a field. */
static bool write_checked(volatile uint32_t* reg, uint32_t value,
                          uint32_t mask) {
    NX_GD32_CLOCK_WRITE(reg, value);
    nx_arch_dsb();
    return (*reg & mask) == (value & mask);
}
/** \brief           Wait with a boot-safe poll bound independent of timer IRQs.
 */
static bool wait_bits(volatile uint32_t* reg, uint32_t mask,
                      uint32_t expected) {
    for (uint32_t n = 0; n < 1000000u; ++n) {
#ifdef NX_GD32_CLOCK_MODEL
        nx_gd32_model_clock_poll();
#endif
        if ((*reg & mask) == expected) {
            return true;
        }
    }
    return false;
}
/** \brief           Establish reset CPU state before constructors and main. */
void SystemInit(void) {
    g_gd32_reset_flags = RCU_RSTSCK;
    RCU_RSTSCK |= RCU_RSTSCK_RSTFC;
    SCB->CPACR |= (3u << 20) | (3u << 22);
    __DSB();
    __ISB();
    RCU_CTL |= RCU_CTL_IRC16MEN;
    if (!wait_bits(&RCU_CTL, RCU_CTL_IRC16MSTB, RCU_CTL_IRC16MSTB)) {
        g_gd32_boot_error = 1u;
        for (;;) {
            __NOP();
        }
    }
    RCU_CFG0 &= ~(uint32_t)RCU_CFG0_SCS;
    if (!wait_bits(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_IRC16M)) {
        g_gd32_boot_error = 2u;
        for (;;) {
            __NOP();
        }
    }
    RCU_CTL &= ~(RCU_CTL_PLLEN | RCU_CTL_HXTALEN | RCU_CTL_CKMEN);
    RCU_CFG0 &=
        ~(uint32_t)(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    extern const uint32_t __gVectors[];
    SCB->VTOR = (uint32_t)(uintptr_t)__gVectors;
    SystemCoreClock = 16000000u;
#if defined(NEXUS_MEMORY_TCM) && NEXUS_MEMORY_TCM == 1
    /* TCM is only on DBUS. Keep unrelated AHB1 enables and establish access
     * before any initialized TCM object or constructor can be observed. */
    RCU_AHB1EN |= RCU_AHB1EN_TCMSRAMEN;
    __DSB();
    if ((RCU_AHB1EN & RCU_AHB1EN_TCMSRAMEN) == 0u) {
        g_gd32_boot_error = 3u;
        for (;;) {
            __NOP();
        }
    }
#endif
#if (defined(NEXUS_MEMORY_ADDSRAM) && NEXUS_MEMORY_ADDSRAM == 1) ||            \
    (defined(NEXUS_MEMORY_TCM) && NEXUS_MEMORY_TCM == 1)
    if (!initialize_extra_memory()) {
        g_gd32_boot_error = 4u;
        for (;;) {
            __NOP();
        }
    }
    __DSB();
#endif
}
/** \brief           Publish the current AHB frequency after clock changes. */
void SystemCoreClockUpdate(void) {
    SystemCoreClock = rcu_clock_freq_get(CK_AHB);
}
/** \brief Reject unknown running owners before changing global clock fields. */
nx_result_t nx_gd32_clock_preflight(void) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return NX_ERROR_CONTEXT;
    }
    const uint32_t sources = RCU_CTL_HXTALEN | RCU_CTL_HXTALBPS |
                             RCU_CTL_CKMEN | RCU_CTL_PLLEN | RCU_CTL_PLLSTB |
                             RCU_CTL_PLLI2SEN | RCU_CTL_PLLI2SSTB |
                             RCU_CTL_PLLSAIEN | RCU_CTL_PLLSAISTB;
    const uint32_t bus =
        (uint32_t)(RCU_CFG0_SCS | RCU_CFG0_SCSS | RCU_CFG0_AHBPSC |
                   RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    if (s_clock.captured ||
        (RCU_CTL & (RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB)) !=
            (RCU_CTL_IRC16MEN | RCU_CTL_IRC16MSTB) ||
        (RCU_CTL & sources) != 0U || (RCU_CFG0 & bus) != RCU_SCSS_IRC16M ||
        (RCU_PLLSSCTL & RCU_PLLSSCTL_SSCGON) != 0U ||
        (RCU_CFG1 & RCU_CFG1_TIMERSEL) != 0U ||
        (FMC_STAT & FMC_STAT_BUSY) != 0U) {
        return NX_ERROR_BUSY;
    }
    return NX_SUCCESS;
}
/** \brief           Configure the reviewed fixed nominal 3.3 V clock profile.
 */
int nx_gd32_clock_start(void) {
    /* Liangshan 25 MHz HXTAL, PLLM25/N400/P2: SYSCLK200/APB1 50/APB2 100 MHz.
     * USB uses a separately owned PLLSAI profile. Every wait is bounded. */
    if (nx_gd32_clock_preflight() != NX_SUCCESS) {
        return -1;
    }
    s_clock.pll = RCU_PLL;
    s_clock.flash = FMC_WS & FMC_WC_WSCNT;
    s_clock.owned_power_clock = (RCU_APB1EN & RCU_APB1EN_PMUEN) == 0U;
    s_clock.power_snapshotted = false;
    s_clock.captured = true;
    if (!write_checked(&RCU_APB1EN, RCU_APB1EN | RCU_APB1EN_PMUEN,
                       RCU_APB1EN_PMUEN)) {
        return -1;
    }
    /* Readback and barrier precede the first powered-interface snapshot. */
    s_clock.power = PMU_CTL & NX_GD32_POWER_FIELDS;
    s_clock.power_snapshotted = true;
    if (!write_checked(&PMU_CTL, PMU_CTL | PMU_CTL_LDOVS, PMU_CTL_LDOVS)) {
        return -1;
    }
    NX_GD32_CLOCK_WRITE(&RCU_CTL, RCU_CTL | RCU_CTL_HXTALEN);
    if (!wait_bits(&RCU_CTL, RCU_CTL_HXTALSTB, RCU_CTL_HXTALSTB)) {
        return -1;
    }
    /* Conservative Flash latency before raising frequency at nominal 3.3 V. */
    fmc_wscnt_set(WS_WSCNT_7);
    nx_arch_dsb();
    if ((FMC_WS & FMC_WC_WSCNT) != WS_WSCNT_7 ||
        !write_checked(
            &RCU_CFG0,
            (RCU_CFG0 & ~(uint32_t)(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC |
                                    RCU_CFG0_APB2PSC)) |
                RCU_AHB_CKSYS_DIV1 | RCU_APB1_CKAHB_DIV4 | RCU_APB2_CKAHB_DIV2,
            RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC) ||
        !write_checked(&RCU_PLL,
                       25u | (400u << 6) | RCU_PLLSRC_HXTAL | (9u << 24),
                       UINT32_MAX)) {
        return -1;
    }
    NX_GD32_CLOCK_WRITE(&RCU_CTL, RCU_CTL | RCU_CTL_PLLEN);
    if (!wait_bits(&RCU_CTL, RCU_CTL_PLLSTB, RCU_CTL_PLLSTB)) {
        return -1;
    }
    if (!write_checked(&PMU_CTL, PMU_CTL | PMU_CTL_HDEN, PMU_CTL_HDEN)) {
        return -1;
    }
    if (!wait_bits(&PMU_CS, PMU_CS_HDRF, PMU_CS_HDRF)) {
        return -1;
    }
    if (!write_checked(&PMU_CTL, PMU_CTL | PMU_CTL_HDS, PMU_CTL_HDS)) {
        return -1;
    }
    if (!wait_bits(&PMU_CS, PMU_CS_HDSRF, PMU_CS_HDSRF)) {
        return -1;
    }
    NX_GD32_CLOCK_WRITE(&RCU_CFG0, (RCU_CFG0 & ~(uint32_t)RCU_CFG0_SCS) |
                                       RCU_CKSYSSRC_PLLP);
    if (!wait_bits(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_PLLP)) {
        return -1;
    }
    SystemCoreClockUpdate();
    rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL2);
    return SystemCoreClock == 200000000u &&
                   rcu_clock_freq_get(CK_APB1) == 50000000u &&
                   rcu_clock_freq_get(CK_APB2) == 100000000u &&
                   (RCU_CFG1 & RCU_CFG1_TIMERSEL) == 0u
               ? 0
               : -1;
}

/** \brief           Return to IRC16M without disabling a live system PLL. */
int nx_gd32_clock_stop(void) {
    if (!nx_arch_is_privileged() || nx_arch_in_isr() ||
        nx_arch_irq_is_masked()) {
        return -1;
    }
    if (!s_clock.captured) {
        return 0;
    }
    if ((RCU_CTL & (RCU_CTL_PLLI2SEN | RCU_CTL_PLLI2SSTB | RCU_CTL_PLLSAIEN |
                    RCU_CTL_PLLSAISTB)) != 0U) {
        return -1;
    }
    NX_GD32_CLOCK_WRITE(&RCU_CTL, RCU_CTL | RCU_CTL_IRC16MEN);
    if (!wait_bits(&RCU_CTL, RCU_CTL_IRC16MSTB, RCU_CTL_IRC16MSTB)) {
        SystemCoreClockUpdate();
        return -1;
    }
    /* Preserve dividers until the CPU has really switched. Never disable a
     * PLL that might still be the system clock after a failed request. */
    NX_GD32_CLOCK_WRITE(&RCU_CFG0, (RCU_CFG0 & ~(uint32_t)RCU_CFG0_SCS) |
                                       RCU_CKSYSSRC_IRC16M);
    if (!wait_bits(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_IRC16M)) {
        SystemCoreClockUpdate();
        return -1;
    }
    if (!write_checked(&RCU_CFG0,
                       RCU_CFG0 &
                           ~(uint32_t)(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC |
                                       RCU_CFG0_APB2PSC),
                       RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC)) {
        return -1;
    }
    SystemCoreClockUpdate();
    if ((RCU_CFG0 & (RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC)) !=
            0u ||
        SystemCoreClock != 16000000u) {
        return -1;
    }

    /* Auxiliary PLL consumers must already have released their own domains.
     * This owner never disables another subsystem's still-enabled source. */
    NX_GD32_CLOCK_WRITE(&RCU_CTL, RCU_CTL & ~(uint32_t)RCU_CTL_PLLEN);
    if (!wait_bits(&RCU_CTL, RCU_CTL_PLLEN | RCU_CTL_PLLSTB, 0u)) {
        return -1;
    }
    NX_GD32_CLOCK_WRITE(&RCU_CTL,
                        RCU_CTL & ~(uint32_t)(RCU_CTL_CKMEN | RCU_CTL_HXTALEN));
    if (!wait_bits(&RCU_CTL, RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB, 0u)) {
        return -1;
    }
    if (!write_checked(&RCU_PLL, s_clock.pll, UINT32_MAX) ||
        !write_checked(&FMC_WS,
                       (FMC_WS & ~(uint32_t)FMC_WC_WSCNT) | s_clock.flash,
                       FMC_WC_WSCNT)) {
        return -1;
    }
    if (s_clock.power_snapshotted) {
        /* On the observed IRC16M source, switch drive before disabling its
         * regulator, then restore voltage. Control readback alone is not the
         * hardware's high-drive transition acknowledgement. */
        uint32_t drive_switch = s_clock.power & PMU_CTL_HDS;
        if (!write_checked(&PMU_CTL,
                           (PMU_CTL & ~(uint32_t)PMU_CTL_HDS) | drive_switch,
                           PMU_CTL_HDS) ||
            !wait_bits(&PMU_CS, PMU_CS_HDSRF,
                       drive_switch != 0U ? PMU_CS_HDSRF : 0U)) {
            return -1;
        }
        uint32_t drive = s_clock.power & PMU_CTL_HDEN;
        if (!write_checked(&PMU_CTL,
                           (PMU_CTL & ~(uint32_t)PMU_CTL_HDEN) | drive,
                           PMU_CTL_HDEN) ||
            !wait_bits(&PMU_CS, PMU_CS_HDRF, drive != 0U ? PMU_CS_HDRF : 0U) ||
            !write_checked(&PMU_CTL,
                           (PMU_CTL & ~(uint32_t)PMU_CTL_LDOVS) |
                               (s_clock.power & PMU_CTL_LDOVS),
                           PMU_CTL_LDOVS)) {
            return -1;
        }
    }
    if (s_clock.owned_power_clock &&
        !write_checked(&RCU_APB1EN, RCU_APB1EN & ~(uint32_t)RCU_APB1EN_PMUEN,
                       RCU_APB1EN_PMUEN)) {
        return -1;
    }
    s_clock.captured = false;
    return 0;
}
