/* Read-only F407 teardown gate. No reset of unowned IRQs or DMA. */
#include "interrupt/stm32_interrupt.h"

nx_status_t nx_stm32f407_resources_idle(void) {
    if (!stm32_isr_manager_is_idle()) {
        return NX_ERR_BUSY;
    }

    for (uint32_t irq = 0; irq <= (uint32_t)FPU_IRQn; ++irq) {
        uint32_t bank = irq / 32U;
        uint32_t bit = UINT32_C(1) << (irq % 32U);
        if (((NVIC->ISER[bank] | NVIC->ISPR[bank] | NVIC->IABR[bank]) & bit) !=
            0U) {
            return NX_ERR_BUSY;
        }
    }

    DMA_Stream_TypeDef* const streams[] = {
        DMA1_Stream0, DMA1_Stream1, DMA1_Stream2, DMA1_Stream3,
        DMA1_Stream4, DMA1_Stream5, DMA1_Stream6, DMA1_Stream7,
        DMA2_Stream0, DMA2_Stream1, DMA2_Stream2, DMA2_Stream3,
        DMA2_Stream4, DMA2_Stream5, DMA2_Stream6, DMA2_Stream7,
    };
    for (uint32_t i = 0; i < sizeof(streams) / sizeof(streams[0]); ++i) {
        if ((streams[i]->CR & DMA_SxCR_EN) != 0U) {
            return NX_ERR_BUSY;
        }
    }
    return NX_OK;
}
