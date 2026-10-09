#ifndef NEXUS_STM32_REGISTER_MODEL_H
#define NEXUS_STM32_REGISTER_MODEL_H
#include "stm32f4xx_hal.h"
enum {
    MODEL_STM_HSI_START = 1u << 0,
    MODEL_STM_SWITCH = 1u << 1,
    MODEL_STM_PLL_STOP = 1u << 2,
    MODEL_STM_HSE_STOP = 1u << 3,
    MODEL_STM_CLOCK_REPORT = 1u << 4,
};
typedef struct {
    RCC_TypeDef rcc;
    NVIC_Type nvic;
    DMA_Stream_TypeDef dma[16];
    uint32_t flash_acr, regulator;
} model_stm_snapshot_t;
extern RCC_TypeDef model_stm32_clock_regs;
extern uint32_t model_stm32_faults;
extern bool model_stm32_unsafe_stop;
extern uint32_t model_stm32_mask, model_stm32_isr;
extern bool model_stm32_shutdown;
void model_stm32_reset(void);
void model_stm32_snapshot(model_stm_snapshot_t* out);
#endif
