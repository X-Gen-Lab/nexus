#include "gd32f470_platform.h"
#include "gd32f4xx.h"
#include "nexus_config.h"
#include <stdbool.h>

uint32_t SystemCoreClock = 16000000u;
static bool wait_bits(volatile uint32_t* reg, uint32_t mask, uint32_t expected) {
    for (uint32_t n = 0; n < 1000000u; ++n) {
        if ((*reg & mask) == expected) { return true; }
    }
    return false;
}
void SystemInit(void) {
    SCB->CPACR |= (3u << 20) | (3u << 22);
    __DSB();
    __ISB();
    RCU_CTL |= RCU_CTL_IRC16MEN;
    if (!wait_bits(&RCU_CTL, RCU_CTL_IRC16MSTB, RCU_CTL_IRC16MSTB)) {
        for (;;) { __NOP(); }
    }
    RCU_CFG0 &= ~RCU_CFG0_SCS;
    if (!wait_bits(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_IRC16M)) {
        for (;;) { __NOP(); }
    }
    RCU_CTL &= ~(RCU_CTL_PLLEN | RCU_CTL_HXTALEN | RCU_CTL_CKMEN);
    RCU_CFG0 &= ~(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    SCB->VTOR = 0x08000000u;
    SystemCoreClock = 16000000u;
}
void SystemCoreClockUpdate(void) { SystemCoreClock = rcu_clock_freq_get(CK_AHB); }
int nx_gd32f470_clock_validate(void) {
    /* Liangshan 25 MHz HXTAL, PLLM25/N400/P2: SYSCLK200/APB1 50/APB2 100 MHz.
     * No USB 48 MHz clock is advertised. Every stability wait is bounded. */
    if ((RCU_CFG0 & RCU_CFG0_SCSS) == RCU_SCSS_PLLP) {
        SystemCoreClockUpdate();
        rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL2);
        return SystemCoreClock == NX_CONFIG_GD32_SYSCLK_FREQ &&
            rcu_clock_freq_get(CK_APB1) == 50000000u &&
            rcu_clock_freq_get(CK_APB2) == 100000000u ? 0 : -1;
    }
    /* A failed previous attempt must not rewrite an enabled PLL. */
    RCU_CTL &= ~RCU_CTL_PLLEN;
    if (!wait_bits(&RCU_CTL, RCU_CTL_PLLSTB, 0u)) { return -1; }
    rcu_periph_clock_enable(RCU_PMU);
    PMU_CTL |= PMU_CTL_LDOVS;
    RCU_CTL |= RCU_CTL_HXTALEN;
    if (!wait_bits(&RCU_CTL, RCU_CTL_HXTALSTB, RCU_CTL_HXTALSTB)) { return -1; }
    /* Conservative Flash latency before raising frequency at nominal 3.3 V. */
    fmc_wscnt_set(WS_WSCNT_7);
    RCU_CFG0 = (RCU_CFG0 & ~(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC)) |
               RCU_AHB_CKSYS_DIV1 | RCU_APB1_CKAHB_DIV4 | RCU_APB2_CKAHB_DIV2;
    RCU_PLL = 25u | (400u << 6) | RCU_PLLSRC_HXTAL | (9u << 24);
    RCU_CTL |= RCU_CTL_PLLEN;
    if (!wait_bits(&RCU_CTL, RCU_CTL_PLLSTB, RCU_CTL_PLLSTB)) { return -1; }
    PMU_CTL |= PMU_CTL_HDEN;
    if (!wait_bits(&PMU_CS, PMU_CS_HDRF, PMU_CS_HDRF)) { return -1; }
    PMU_CTL |= PMU_CTL_HDS;
    if (!wait_bits(&PMU_CS, PMU_CS_HDSRF, PMU_CS_HDSRF)) { return -1; }
    RCU_CFG0 = (RCU_CFG0 & ~RCU_CFG0_SCS) | RCU_CKSYSSRC_PLLP;
    if (!wait_bits(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_PLLP)) { return -1; }
    SystemCoreClockUpdate();
    rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL2);
    return SystemCoreClock == NX_CONFIG_GD32_SYSCLK_FREQ &&
        rcu_clock_freq_get(CK_APB1) == 50000000u &&
        rcu_clock_freq_get(CK_APB2) == 100000000u ? 0 : -1;
}

int nx_gd32f470_clock_release(void) {
    RCU_CTL |= RCU_CTL_IRC16MEN;
    if (!wait_bits(&RCU_CTL, RCU_CTL_IRC16MSTB, RCU_CTL_IRC16MSTB)) {
        SystemCoreClockUpdate();
        return -1;
    }
    /* Preserve dividers until the CPU has really switched. Never disable a
     * PLL that might still be the system clock after a failed request. */
    RCU_CFG0 = (RCU_CFG0 & ~RCU_CFG0_SCS) | RCU_CKSYSSRC_IRC16M;
    if (!wait_bits(&RCU_CFG0, RCU_CFG0_SCSS, RCU_SCSS_IRC16M)) {
        SystemCoreClockUpdate();
        return -1;
    }
    RCU_CFG0 &= ~(RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC);
    SystemCoreClockUpdate();
    if ((RCU_CFG0 & (RCU_CFG0_AHBPSC | RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC)) !=
            0u ||
        SystemCoreClock != 16000000u) {
        return -1;
    }

    RCU_CTL &= ~(RCU_CTL_PLLEN | RCU_CTL_PLLI2SEN | RCU_CTL_PLLSAIEN);
    if (!wait_bits(&RCU_CTL,
                   RCU_CTL_PLLEN | RCU_CTL_PLLSTB | RCU_CTL_PLLI2SEN |
                       RCU_CTL_PLLI2SSTB | RCU_CTL_PLLSAIEN | RCU_CTL_PLLSAISTB,
                   0u)) {
        return -1;
    }
    RCU_CTL &= ~(RCU_CTL_CKMEN | RCU_CTL_HXTALEN);
    if (!wait_bits(&RCU_CTL, RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB, 0u)) {
        return -1;
    }
    /* Leave the conservative Flash latency and regulator/high-drive setting
     * intact. clock_validate() restores the published bus frequencies. */
    return 0;
}
