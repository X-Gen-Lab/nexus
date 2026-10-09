#include "check.h"
#include "model.h"
#include "clock/stm32_clock.h"
#include "interrupt/stm32_interrupt.h"
#include "hal/resource/nx_isr_manager.h"
#include <string.h>

static int clock_release(void) {
    model_stm32_reset();
    model_stm_snapshot_t before, after;
    model_stm32_snapshot(&before);
    CHECK(nx_stm32f407_clock_release() == NX_OK);
    CHECK(SystemCoreClock == 16000000u && !model_stm32_unsafe_stop);
    CHECK((model_stm32_clock_regs.CFGR & (RCC_CFGR_SWS | RCC_CFGR_SW |
        RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) == 0u);
    CHECK((model_stm32_clock_regs.CR & (RCC_CR_PLLON | RCC_CR_PLLRDY |
        RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY | RCC_CR_HSEON | RCC_CR_HSERDY |
        RCC_CR_CSSON)) == 0u);
    CHECK((model_stm32_clock_regs.CR & (RCC_CR_HSION | RCC_CR_HSIRDY)) ==
        (RCC_CR_HSION | RCC_CR_HSIRDY));
    model_stm32_snapshot(&after);
    CHECK(before.flash_acr == after.flash_acr && before.regulator == after.regulator);
    CHECK(nx_stm32f407_clock_release() == NX_OK);
    return 0;
}
static int clock_faults(void) {
    const uint32_t faults[] = {MODEL_STM_HSI_START, MODEL_STM_SWITCH,
        MODEL_STM_PLL_STOP, MODEL_STM_HSE_STOP, MODEL_STM_CLOCK_REPORT};
    for (unsigned n = 0; n < sizeof(faults) / sizeof(faults[0]); ++n) {
        model_stm32_reset();
        model_stm32_faults = faults[n];
        CHECK(nx_stm32f407_clock_release() ==
            (faults[n] == MODEL_STM_CLOCK_REPORT ? NX_ERR_HARDWARE : NX_ERR_TIMEOUT));
        CHECK(!model_stm32_unsafe_stop);
        if (faults[n] == MODEL_STM_HSI_START || faults[n] == MODEL_STM_SWITCH) {
            CHECK((model_stm32_clock_regs.CR & (RCC_CR_PLLON | RCC_CR_HSEON)) ==
                (RCC_CR_PLLON | RCC_CR_HSEON));
            CHECK(SystemCoreClock == 168000000u);
        } else {
            CHECK((model_stm32_clock_regs.CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSI);
        }
        if (faults[n] == MODEL_STM_PLL_STOP) {
            CHECK((model_stm32_clock_regs.CR & RCC_CR_HSEON) != 0u);
        }
        model_stm32_faults = 0u;
        CHECK(nx_stm32f407_clock_release() == NX_OK);
        CHECK(SystemCoreClock == 16000000u && !model_stm32_unsafe_stop);
    }
    return 0;
}
static void noop_callback(void* data) { (void)data; }
static int resources_idle(void) {
    model_stm32_reset();
    CHECK(nx_stm32f407_resources_idle() == NX_OK);
    for (unsigned kind = 0; kind < 3u; ++kind) {
        for (unsigned irq = 0; irq <= (unsigned)FPU_IRQn; ++irq) {
            model_stm32_reset();
            volatile uint32_t* array = kind == 0u ? model_stm32_nvic.ISER :
                (kind == 1u ? model_stm32_nvic.ISPR : model_stm32_nvic.IABR);
            array[irq / 32u] = 1u << (irq % 32u);
            model_stm_snapshot_t before, after;
            model_stm32_snapshot(&before);
            CHECK(nx_stm32f407_resources_idle() == NX_ERR_BUSY);
            model_stm32_snapshot(&after);
            CHECK(memcmp(&before,&after,sizeof(before)) == 0);
        }
    }
    for (unsigned stream = 0; stream < 16u; ++stream) {
        model_stm32_reset();
        model_stm32_dma[stream].CR = DMA_SxCR_EN;
        model_stm_snapshot_t before, after;
        model_stm32_snapshot(&before);
        CHECK(nx_stm32f407_resources_idle() == NX_ERR_BUSY);
        model_stm32_snapshot(&after);
        CHECK(memcmp(&before,&after,sizeof(before)) == 0);
    }
    model_stm32_reset();
    nx_isr_manager_t* manager = nx_isr_manager_get();
    CHECK(manager->connect(manager, (uint32_t)FPU_IRQn, noop_callback, NULL, 5u) == NX_OK);
    /* Even manually masked/cleared hardware does not release callback ownership. */
    memset(&model_stm32_nvic,0,sizeof(model_stm32_nvic));
    CHECK(!stm32_isr_manager_is_idle());
    CHECK(nx_stm32f407_resources_idle() == NX_ERR_BUSY);
    model_stm32_nvic.ISPR[(uint32_t)FPU_IRQn / 32u] |= 1u << ((uint32_t)FPU_IRQn % 32u);
    CHECK(manager->disconnect(manager, (uint32_t)FPU_IRQn) == NX_OK);
    CHECK(stm32_isr_manager_is_idle() && nx_stm32f407_resources_idle() == NX_OK);
    return 0;
}
static unsigned dispatch_calls;
static bool dispatch_bad;
static nx_status_t callback_disconnect;
static void pinned_callback(void* data) {
    nx_isr_manager_t* manager = data;
    ++dispatch_calls;
    if (model_stm32_mask != 0u || nx_stm32f407_resources_idle() != NX_ERR_BUSY ||
        stm32_isr_manager_is_idle()) { dispatch_bad = true; }
    callback_disconnect = manager->disconnect(manager, (uint32_t)FPU_IRQn);
    if (callback_disconnect != NX_ERR_BUSY) { dispatch_bad = true; }
    /* Model nested dispatch without ever relinquishing the outer context. */
    if (dispatch_calls == 1u) { stm32_isr_dispatch(FPU_IRQn); }
}
static int registration_lifecycle(void) {
    model_stm32_reset();
    nx_isr_manager_t* manager = nx_isr_manager_get();
    model_stm32_mask = 1u;
    CHECK(manager->connect(manager, (uint32_t)FPU_IRQn, noop_callback, NULL, 5u) == NX_ERR_CONTEXT);
    CHECK(model_stm32_mask == 1u && stm32_isr_manager_is_idle());
    model_stm32_mask = 0u;model_stm32_isr = 1u;
    CHECK(manager->connect(manager, (uint32_t)FPU_IRQn, noop_callback, NULL, 5u) == NX_ERR_CONTEXT);
    model_stm32_isr = 0u;model_stm32_shutdown = true;
    model_stm_snapshot_t before, after;
    model_stm32_snapshot(&before);
    CHECK(manager->connect(manager, (uint32_t)FPU_IRQn, noop_callback, NULL, 5u) == NX_ERR_BUSY);
    model_stm32_snapshot(&after);
    CHECK(memcmp(&before,&after,sizeof(before)) == 0 && stm32_isr_manager_is_idle());
    model_stm32_shutdown = false;
    CHECK(manager->connect(manager, (uint32_t)FPU_IRQn, pinned_callback, manager, 5u) == NX_OK);
    dispatch_calls = 0u;dispatch_bad = false;model_stm32_isr = 1u;
    stm32_isr_dispatch(FPU_IRQn);
    CHECK(dispatch_calls == 2u && !dispatch_bad && callback_disconnect == NX_ERR_BUSY);
    CHECK(model_stm32_mask == 0u && !stm32_isr_manager_is_idle());
    model_stm32_isr = 0u;
    unsigned bank = (unsigned)FPU_IRQn / 32u;
    uint32_t bit = 1u << ((unsigned)FPU_IRQn % 32u);
    model_stm32_nvic.IABR[bank] = bit;
    model_stm32_snapshot(&before);
    CHECK(manager->disconnect(manager, (uint32_t)FPU_IRQn) == NX_ERR_BUSY);
    model_stm32_snapshot(&after);
    CHECK(memcmp(&before,&after,sizeof(before)) == 0);
    model_stm32_nvic.IABR[bank] = 0u;
    CHECK(manager->disconnect(manager, (uint32_t)FPU_IRQn) == NX_OK);
    CHECK(stm32_isr_manager_is_idle() && nx_stm32f407_resources_idle() == NX_OK);
    stm32_isr_dispatch(FPU_IRQn);
    CHECK(dispatch_calls == 2u);
    return 0;
}
int main(int argc, char** argv) {
    const char* name = argc == 2 ? argv[1] : "all";
    if (argc > 2) { return 2; }
    if (strcmp(name,"clock") == 0) { return clock_release(); }
    if (strcmp(name,"clock_faults") == 0) { return clock_faults(); }
    if (strcmp(name,"resources") == 0) { return resources_idle(); }
    if (strcmp(name,"registration") == 0) { return registration_lifecycle(); }
    if (strcmp(name,"all") != 0) { return 2; }
    CHECK(clock_release() == 0);
    CHECK(clock_faults() == 0);
    CHECK(resources_idle() == 0);
    CHECK(registration_lifecycle() == 0);
    puts("STM32 production register teardown: clock, faults, IRQ/registration/DMA ownership passed");
    return 0;
}
