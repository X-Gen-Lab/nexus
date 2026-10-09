/* Register model for production F407 idle teardown; no physical timing model.
 */
#ifndef NEXUS_STM32_REGISTER_MODEL_HAL_H
#define NEXUS_STM32_REGISTER_MODEL_HAL_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { FPU_IRQn = 81 } IRQn_Type;
typedef struct {
    volatile uint32_t CR, CFGR;
} RCC_TypeDef;
typedef struct {
    volatile uint32_t ISER[3], ISPR[3], IABR[3];
} NVIC_Type;
typedef struct {
    volatile uint32_t CR;
} DMA_Stream_TypeDef;
RCC_TypeDef* model_stm32_rcc(void);
extern NVIC_Type model_stm32_nvic;
extern DMA_Stream_TypeDef model_stm32_dma[16];
#define RCC          (model_stm32_rcc())
#define NVIC         (&model_stm32_nvic)
#define DMA1_Stream0 (&model_stm32_dma[0])
#define DMA1_Stream1 (&model_stm32_dma[1])
#define DMA1_Stream2 (&model_stm32_dma[2])
#define DMA1_Stream3 (&model_stm32_dma[3])
#define DMA1_Stream4 (&model_stm32_dma[4])
#define DMA1_Stream5 (&model_stm32_dma[5])
#define DMA1_Stream6 (&model_stm32_dma[6])
#define DMA1_Stream7 (&model_stm32_dma[7])
#define DMA2_Stream0 (&model_stm32_dma[8])
#define DMA2_Stream1 (&model_stm32_dma[9])
#define DMA2_Stream2 (&model_stm32_dma[10])
#define DMA2_Stream3 (&model_stm32_dma[11])
#define DMA2_Stream4 (&model_stm32_dma[12])
#define DMA2_Stream5 (&model_stm32_dma[13])
#define DMA2_Stream6 (&model_stm32_dma[14])
#define DMA2_Stream7 (&model_stm32_dma[15])
/* Bit values follow the locked ST F407 device header. */
#define DMA_SxCR_EN      (1u << 0)
#define RCC_CR_HSION     (1u << 0)
#define RCC_CR_HSIRDY    (1u << 1)
#define RCC_CR_HSEON     (1u << 16)
#define RCC_CR_HSERDY    (1u << 17)
#define RCC_CR_CSSON     (1u << 19)
#define RCC_CR_PLLON     (1u << 24)
#define RCC_CR_PLLRDY    (1u << 25)
#define RCC_CR_PLLI2SON  (1u << 26)
#define RCC_CR_PLLI2SRDY (1u << 27)
#define RCC_CFGR_SW      0x3u
#define RCC_CFGR_SWS     0xcu
#define RCC_CFGR_SWS_HSI 0u
#define RCC_CFGR_SWS_PLL 0x8u
#define RCC_CFGR_HPRE    0xf0u
#define RCC_CFGR_PPRE1   0x1c00u
#define RCC_CFGR_PPRE2   0xe000u
extern uint32_t SystemCoreClock;
void SystemCoreClockUpdate(void);
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq);
#endif
