#include "check.h"
#include "model.h"
#include "gd32f470_platform.h"
#include <string.h>
extern void TIMER1_IRQHandler(void);

static int clock_cycle(void) {
    model_gd_reset();
    SystemInit();
    CHECK(SystemCoreClock == 16000000u && model_gd_scb.VTOR == 0x08000000u);
    for (unsigned cycle = 0; cycle < 4u; ++cycle) {
        CHECK(nx_gd32f470_clock_validate() == 0);
        CHECK(SystemCoreClock == 200000000u &&
              rcu_clock_freq_get(CK_APB1) == 50000000u &&
              rcu_clock_freq_get(CK_APB2) == 100000000u);
        model_gd_snapshot_t before, after;
        model_gd_snapshot(&before);
        CHECK(nx_gd32f470_clock_release() == 0);
        CHECK(SystemCoreClock == 16000000u && !model_gd_unsafe_stop);
        CHECK((RCU_CFG0 & (RCU_CFG0_SCSS | RCU_CFG0_SCS | RCU_CFG0_AHBPSC |
                           RCU_CFG0_APB1PSC | RCU_CFG0_APB2PSC)) == 0u);
        CHECK((RCU_CTL &
               (RCU_CTL_PLLEN | RCU_CTL_PLLSTB | RCU_CTL_PLLI2SEN |
                RCU_CTL_PLLI2SSTB | RCU_CTL_PLLSAIEN | RCU_CTL_PLLSAISTB |
                RCU_CTL_HXTALEN | RCU_CTL_HXTALSTB | RCU_CTL_CKMEN)) == 0u);
        model_gd_snapshot(&after);
        CHECK(before.flash_latency == after.flash_latency &&
              before.registers[MODEL_GD_PMU_CTL] ==
                  after.registers[MODEL_GD_PMU_CTL]);
    }
    return 0;
}
static int clock_faults(void) {
    const uint32_t release_faults[] = {MODEL_GD_IRC_START, MODEL_GD_SWITCH,
                                       MODEL_GD_PLL_STOP, MODEL_GD_HXTAL_STOP};
    for (unsigned i = 0; i < sizeof(release_faults) / sizeof(release_faults[0]);
         ++i) {
        model_gd_reset();
        CHECK(nx_gd32f470_clock_validate() == 0);
        model_gd_faults = release_faults[i];
        CHECK(nx_gd32f470_clock_release() == -1 && !model_gd_unsafe_stop);
        if (release_faults[i] == MODEL_GD_IRC_START ||
            release_faults[i] == MODEL_GD_SWITCH) {
            CHECK((model_gd_registers[MODEL_GD_CTL] &
                   (RCU_CTL_PLLEN | RCU_CTL_HXTALEN)) ==
                  (RCU_CTL_PLLEN | RCU_CTL_HXTALEN));
            CHECK(SystemCoreClock == 200000000u);
        }
        if (release_faults[i] == MODEL_GD_PLL_STOP) {
            CHECK((model_gd_registers[MODEL_GD_CTL] & RCU_CTL_HXTALEN) != 0u);
        }
        model_gd_faults = 0u;
        CHECK(nx_gd32f470_clock_release() == 0 &&
              nx_gd32f470_clock_validate() == 0);
    }
    const uint32_t init_faults[] = {MODEL_GD_HXTAL_START, MODEL_GD_PLL_START,
                                    MODEL_GD_HIGH_DRIVE, MODEL_GD_SWITCH};
    for (unsigned i = 0; i < sizeof(init_faults) / sizeof(init_faults[0]);
         ++i) {
        model_gd_reset();
        model_gd_faults = init_faults[i];
        CHECK(nx_gd32f470_clock_validate() == -1 && !model_gd_unsafe_stop);
        model_gd_faults = 0u;
        CHECK(nx_gd32f470_clock_release() == 0);
        CHECK(nx_gd32f470_clock_validate() == 0 &&
              SystemCoreClock == 200000000u);
    }
    return 0;
}
static int resources_idle(void) {
    model_gd_reset();
    CHECK(nx_gd32f470_resources_idle() == NX_OK);
    for (unsigned kind = 0; kind < 3u; ++kind) {
        for (unsigned irq = 0; irq <= (unsigned)IPA_IRQn; ++irq) {
            model_gd_reset();
            uint32_t* array = kind == 0u ? model_gd_nvic.ISER
                                         : (kind == 1u ? model_gd_nvic.ISPR
                                                       : model_gd_nvic.IABR);
            array[irq / 32u] = 1u << (irq % 32u);
            model_gd_snapshot_t before, after;
            model_gd_snapshot(&before);
            CHECK(nx_gd32f470_resources_idle() == NX_ERR_BUSY);
            model_gd_snapshot(&after);
            CHECK(memcmp(&before, &after, sizeof(before)) == 0);
        }
    }
    for (unsigned dma = 0; dma < 2u; ++dma) {
        for (unsigned channel = 0; channel < 8u; ++channel) {
            model_gd_reset();
            model_gd_dma_ctl[dma][channel] = DMA_CHXCTL_CHEN;
            model_gd_snapshot_t before, after;
            model_gd_snapshot(&before);
            CHECK(nx_gd32f470_resources_idle() == NX_ERR_BUSY);
            model_gd_snapshot(&after);
            CHECK(memcmp(&before, &after, sizeof(before)) == 0);
        }
    }
    model_gd_reset();
    uint32_t timer_bit = 1u << TIMER1_IRQn;
    model_gd_nvic.ISER[0] = timer_bit;
    model_gd_nvic.ISPR[0] = timer_bit;
    model_gd_snapshot_t before, after;
    model_gd_snapshot(&before);
    /* No ownership: even explicit timebase cleanup must not stop this user's
     * direct-SDK timer/IRQ. Startup admission rejects it. */
    CHECK(nx_gd32f470_resources_idle() == NX_ERR_BUSY);
    CHECK(nx_gd32f470_timebase_deinit() == 0);
    model_gd_snapshot(&after);
    CHECK(memcmp(&before, &after, sizeof(before)) == 0);
    model_gd_reset();
    CHECK(nx_gd32f470_timebase_init() == 0);
    model_gd_nvic.ISPR[0] = timer_bit;
    CHECK(nx_gd32f470_resources_idle() == NX_OK);
    model_gd_nvic.IABR[0] = timer_bit;
    CHECK(nx_gd32f470_resources_idle() == NX_ERR_BUSY);
    model_gd_nvic.IABR[0] = 0u;
    CHECK(nx_gd32f470_timebase_deinit() == 0);
    model_gd_nvic.ISER[0] = timer_bit;
    CHECK(nx_gd32f470_resources_idle() == NX_ERR_BUSY);
    model_gd_reset();
    return 0;
}
static int timebase_cycle(void) {
    model_gd_reset();
    CHECK(nx_gd32f470_clock_validate() == 0);
    for (unsigned i = 0; i < 4u; ++i) {
        CHECK(nx_gd32f470_timebase_init() == 0 &&
              nx_gd32f470_resources_idle() == NX_OK);
        model_gd_snapshot_t before, after;
        model_gd_snapshot(&before);
        CHECK(nx_gd32f470_timebase_init() == -1);
        model_gd_snapshot(&after);
        CHECK(memcmp(&before, &after, sizeof(before)) == 0);
        model_gd_timer_count = 7u;
        model_gd_timer_pending = true;
        model_gd_mask = 1u;
        CHECK(nx_gd32f470_timestamp_us() == UINT64_C(4294967303) &&
              model_gd_mask == 1u);
        TIMER1_IRQHandler();
        CHECK(nx_gd32f470_timestamp_us() == UINT64_C(4294967303) &&
              !model_gd_timer_pending);
        CHECK(nx_gd32f470_timebase_deinit() == 0 && model_gd_mask == 1u);
        CHECK((model_gd_timer_ctl0 & TIMER_CTL0_CEN) == 0u &&
              model_gd_timer_dmainten == 0u);
        CHECK((RCU_APB1EN & RCU_APB1EN_TIMER1EN) == 0u &&
              (RCU_APB1SPEN & RCU_APB1SPEN_TIMER1SPEN) == 0u);
        CHECK(nx_gd32f470_resources_idle() == NX_OK);
        CHECK(nx_gd32f470_timebase_init() == 0 &&
              nx_gd32f470_timestamp_us() == 0u);
        CHECK(nx_gd32f470_timebase_deinit() == 0);
    }
    return 0;
}
static int timebase_faults(void) {
    const uint32_t faults[] = {MODEL_GD_TIMER_STOP, MODEL_GD_IRQ_DISABLE,
                               MODEL_GD_PENDING_CLEAR, MODEL_GD_GATE_DISABLE};
    for (unsigned i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
        model_gd_reset();
        CHECK(nx_gd32f470_timebase_init() == 0);
        model_gd_nvic.ISPR[(unsigned)TIMER1_IRQn / 32u] |=
            1u << ((unsigned)TIMER1_IRQn % 32u);
        model_gd_faults = faults[i];
        CHECK(nx_gd32f470_timebase_deinit() == -1);
        /* Partial cleanup retains the original lease; a new initialization
         * must not erase the retryable timer/IRQ state. */
        model_gd_snapshot_t failed_before, failed_after;
        model_gd_snapshot(&failed_before);
        CHECK(nx_gd32f470_timebase_init() == -1);
        model_gd_snapshot(&failed_after);
        CHECK(memcmp(&failed_before, &failed_after, sizeof(failed_before)) ==
              0);
        CHECK((model_gd_registers[MODEL_GD_APB1EN] & RCU_APB1EN_TIMER1EN) !=
              0u);
        if (faults[i] == MODEL_GD_TIMER_STOP) {
            CHECK((model_gd_timer_ctl0 & TIMER_CTL0_CEN) != 0u);
        }
        if (faults[i] == MODEL_GD_IRQ_DISABLE) {
            CHECK((model_gd_nvic.ISER[0] & (1u << TIMER1_IRQn)) != 0u);
        }
        if (faults[i] == MODEL_GD_PENDING_CLEAR) {
            CHECK((model_gd_nvic.ISPR[0] & (1u << TIMER1_IRQn)) != 0u);
        }
        model_gd_faults = 0u;
        CHECK(nx_gd32f470_timebase_deinit() == 0 &&
              nx_gd32f470_timebase_init() == 0);
        CHECK(nx_gd32f470_timebase_deinit() == 0);
    }
    model_gd_reset();
    CHECK(nx_gd32f470_timebase_init() == 0);
    model_gd_nvic.IABR[0] |= 1u << TIMER1_IRQn;
    model_gd_snapshot_t before, after;
    model_gd_snapshot(&before);
    CHECK(nx_gd32f470_timebase_deinit() == -1);
    model_gd_snapshot(&after);
    CHECK(memcmp(&before, &after, sizeof(before)) == 0);
    model_gd_nvic.IABR[0] = 0u;
    CHECK(nx_gd32f470_timebase_deinit() == 0);
    model_gd_faults = MODEL_GD_TIMER_INIT;
    CHECK(nx_gd32f470_timebase_init() == -1);
    CHECK(nx_gd32f470_timebase_init() == -1);
    CHECK(nx_gd32f470_resources_idle() == NX_OK);
    model_gd_faults = 0u;
    CHECK(nx_gd32f470_timebase_deinit() == 0 &&
          nx_gd32f470_timebase_init() == 0);
    CHECK(nx_gd32f470_timebase_deinit() == 0);
    return 0;
}
int main(int argc, char** argv) {
    const char* name = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        return 2;
    }
    if (strcmp(name, "clock") == 0) {
        return clock_cycle();
    }
    if (strcmp(name, "clock_faults") == 0) {
        return clock_faults();
    }
    if (strcmp(name, "resources") == 0) {
        return resources_idle();
    }
    if (strcmp(name, "timebase") == 0) {
        return timebase_cycle();
    }
    if (strcmp(name, "timebase_faults") == 0) {
        return timebase_faults();
    }
    if (strcmp(name, "all") != 0) {
        return 2;
    }
    CHECK(clock_cycle() == 0);
    CHECK(clock_faults() == 0);
    CHECK(resources_idle() == 0);
    CHECK(timebase_cycle() == 0);
    CHECK(timebase_faults() == 0);
    puts("GD32 production register lifecycle: clock restart, faults, timer cleanup, IRQ/DMA ownership passed");
    return 0;
}
