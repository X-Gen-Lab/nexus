/* Only the register access is replaced. IRQ numbers come from the pinned SDK.
 */
#ifndef NEXUS_TEST_ISR_STM32F4_HAL_H
#define NEXUS_TEST_ISR_STM32F4_HAL_H
#include "stm32f407xx.h"

/* Preserve the SDK's real IRQ enum; replace only the hardware register port.
 * Three banks cover every STM32F407 external interrupt, including FPU_IRQn. */
typedef struct {
    uint32_t ISER[3];
    uint32_t ISPR[3];
    uint32_t IABR[3];
} nexus_test_nvic_t;
extern nexus_test_nvic_t nexus_test_nvic;
#undef NVIC
#define NVIC (&nexus_test_nvic)
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq);
#endif
