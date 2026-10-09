#include "model.h"
#include "interrupt/stm32_interrupt.h"
#include "arch/nx_arch.h"
#include "hal/provider/nx_device_provider.h"
#include <string.h>
RCC_TypeDef model_stm32_clock_regs;
NVIC_Type model_stm32_nvic;
DMA_Stream_TypeDef model_stm32_dma[16];
uint32_t model_stm32_faults;
bool model_stm32_unsafe_stop;
uint32_t SystemCoreClock;
uint32_t model_stm32_mask, model_stm32_isr;
bool model_stm32_shutdown;
static uint32_t flash_acr, regulator;

static void progress(void) {
    uint32_t cr = model_stm32_clock_regs.CR;
    uint32_t cfg = model_stm32_clock_regs.CFGR;
    if ((cr & RCC_CR_HSION) != 0u &&
        (model_stm32_faults & MODEL_STM_HSI_START) == 0u) {
        cr |= RCC_CR_HSIRDY;
    } else {
        cr &= ~RCC_CR_HSIRDY;
    }
    if ((model_stm32_faults & MODEL_STM_SWITCH) == 0u &&
        (cfg & RCC_CFGR_SW) == 0u && (cr & RCC_CR_HSIRDY) != 0u) {
        cfg &= ~RCC_CFGR_SWS;
    }
    if ((cfg & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL &&
        (cr & RCC_CR_PLLON) == 0u) {
        model_stm32_unsafe_stop = true;
    }
    if ((cr & RCC_CR_PLLON) == 0u &&
        (model_stm32_faults & MODEL_STM_PLL_STOP) == 0u) {
        cr &= ~RCC_CR_PLLRDY;
    }
    if ((cr & RCC_CR_PLLI2SON) == 0u &&
        (model_stm32_faults & MODEL_STM_PLL_STOP) == 0u) {
        cr &= ~RCC_CR_PLLI2SRDY;
    }
    if ((cr & RCC_CR_HSEON) == 0u &&
        (model_stm32_faults & MODEL_STM_HSE_STOP) == 0u) {
        cr &= ~RCC_CR_HSERDY;
    }
    model_stm32_clock_regs.CR = cr;
    model_stm32_clock_regs.CFGR = cfg;
}
RCC_TypeDef* model_stm32_rcc(void) {
    progress();
    return &model_stm32_clock_regs;
}
void SystemCoreClockUpdate(void) {
    progress();
    uint32_t source = (model_stm32_clock_regs.CFGR & RCC_CFGR_SWS) ==
                       RCC_CFGR_SWS_PLL ? 168000000u : 16000000u;
    uint32_t div = (model_stm32_clock_regs.CFGR & RCC_CFGR_HPRE) >> 4;
    static const uint8_t shift[16] = {0,0,0,0,0,0,0,0,1,2,3,4,6,7,8,9};
    SystemCoreClock = source >> shift[div];
    if ((model_stm32_faults & MODEL_STM_CLOCK_REPORT) != 0u) {
        ++SystemCoreClock;
    }
}
void model_stm32_reset(void) {
    memset(&model_stm32_clock_regs, 0, sizeof(model_stm32_clock_regs));
    memset(&model_stm32_nvic, 0, sizeof(model_stm32_nvic));
    memset(model_stm32_dma, 0, sizeof(model_stm32_dma));
    model_stm32_faults = 0u;
    model_stm32_unsafe_stop = false;
    model_stm32_mask = 0u;
    model_stm32_isr = 0u;
    model_stm32_shutdown = false;
    model_stm32_clock_regs.CR = RCC_CR_HSEON | RCC_CR_HSERDY | RCC_CR_CSSON |
        RCC_CR_PLLON | RCC_CR_PLLRDY | RCC_CR_PLLI2SON | RCC_CR_PLLI2SRDY;
    model_stm32_clock_regs.CFGR = 2u | RCC_CFGR_SWS_PLL | (5u << 10) | (4u << 13);
    flash_acr = 5u;
    regulator = 3u;
    SystemCoreClock = 168000000u;
}
void model_stm32_snapshot(model_stm_snapshot_t* out) {
    memset(out, 0, sizeof(*out));
    memcpy(&out->rcc, &model_stm32_clock_regs, sizeof(out->rcc));
    memcpy(&out->nvic, &model_stm32_nvic, sizeof(out->nvic));
    memcpy(out->dma, model_stm32_dma, sizeof(out->dma));
    out->flash_acr = flash_acr;
    out->regulator = regulator;
}
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) {
    model_stm32_nvic.ISPR[(uint32_t)irq / 32u] &= ~(1u << ((uint32_t)irq % 32u));
}
void stm32_irq_prepare(IRQn_Type irq, uint8_t priority) {
    (void)priority;
    HAL_NVIC_ClearPendingIRQ(irq);
}
void stm32_irq_enable(IRQn_Type irq) {
    model_stm32_nvic.ISER[(uint32_t)irq / 32u] |= 1u << ((uint32_t)irq % 32u);
}
void stm32_irq_disable(IRQn_Type irq) {
    model_stm32_nvic.ISER[(uint32_t)irq / 32u] &= ~(1u << ((uint32_t)irq % 32u));
}
nx_arch_irq_state_t nx_arch_irq_save(void) {
    nx_arch_irq_state_t previous = {model_stm32_mask};
    model_stm32_mask = 1u;
    return previous;
}
void nx_arch_irq_restore(nx_arch_irq_state_t previous) {
    model_stm32_mask = previous.value;
}
bool nx_arch_irq_is_masked(void) { return model_stm32_mask != 0u; }
bool nx_arch_in_isr(void) { return model_stm32_isr != 0u; }
void nx_arch_dmb(void) { }
bool nx_device_shutdown_is_active(void) { return model_stm32_shutdown; }
