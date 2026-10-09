/* Register/ownership observations only; no electrical or DMA timing model. */
#ifndef NEXUS_DISCOVERY_SPI_BOARD_MODEL_H
#define NEXUS_DISCOVERY_SPI_BOARD_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef int IRQn_Type;
enum { DMA2_Stream0_IRQn = 56, DMA2_Stream3_IRQn = 59, SPI1_IRQn = 35 };
typedef struct {
    uint32_t value;
} SPI_TypeDef;
typedef struct {
    bool live;
} DMA_Stream_TypeDef;
typedef struct {
    uint32_t value;
} GPIO_TypeDef;
typedef struct {
    uint32_t Channel, Direction, PeriphInc, MemInc, PeriphDataAlignment;
    uint32_t MemDataAlignment, Mode, Priority, FIFOMode;
} DMA_InitTypeDef;
typedef struct {
    DMA_Stream_TypeDef* Instance;
    DMA_InitTypeDef Init;
    void* Parent;
} DMA_HandleTypeDef;
typedef struct {
    SPI_TypeDef* Instance;
    DMA_HandleTypeDef *hdmatx, *hdmarx;
} SPI_HandleTypeDef;
typedef struct {
    uint32_t Pin, Mode, Pull, Speed, Alternate;
} GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
extern SPI_TypeDef model_spi;
extern DMA_Stream_TypeDef model_dma_tx, model_dma_rx;
extern GPIO_TypeDef model_gpio_a, model_gpio_b;
#define SPI1                 (&model_spi)
#define DMA2_Stream3         (&model_dma_tx)
#define DMA2_Stream0         (&model_dma_rx)
#define GPIOA                (&model_gpio_a)
#define GPIOB                (&model_gpio_b)
#define GPIO_PIN_0           (1u << 0)
#define GPIO_PIN_1           (1u << 1)
#define GPIO_PIN_5           (1u << 5)
#define GPIO_PIN_6           (1u << 6)
#define GPIO_PIN_7           (1u << 7)
#define GPIO_MODE_OUTPUT_PP  1u
#define GPIO_MODE_AF_PP      2u
#define GPIO_NOPULL          0u
#define GPIO_SPEED_FREQ_HIGH 3u
#define GPIO_AF5_SPI1        5u
#define DMA_CHANNEL_3        3u
#define DMA_MEMORY_TO_PERIPH 1u
#define DMA_PERIPH_TO_MEMORY 2u
#define DMA_PINC_DISABLE     0u
#define DMA_MINC_ENABLE      1u
#define DMA_PDATAALIGN_BYTE  1u
#define DMA_MDATAALIGN_BYTE  1u
#define DMA_NORMAL           0u
#define DMA_PRIORITY_HIGH    2u
#define DMA_FIFOMODE_DISABLE 0u
void model_clock_enable(unsigned clock);
void model_clock_disable(unsigned clock);
#define __HAL_RCC_GPIOA_CLK_ENABLE() model_clock_enable(0u)
#define __HAL_RCC_GPIOB_CLK_ENABLE() model_clock_enable(1u)
#define __HAL_RCC_SPI1_CLK_ENABLE()  model_clock_enable(2u)
#define __HAL_RCC_DMA2_CLK_ENABLE()  model_clock_enable(3u)
#define __HAL_RCC_SPI1_CLK_DISABLE() model_clock_disable(2u)
#define __HAL_LINKDMA(handle, field, dma) do {                                                                       \
        (handle)->field = &(dma); (dma).Parent = (handle);                                               \
    } while (0)
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef*);
HAL_StatusTypeDef HAL_DMA_DeInit(DMA_HandleTypeDef*);
void HAL_DMA_IRQHandler(DMA_HandleTypeDef*);
void HAL_SPI_IRQHandler(SPI_HandleTypeDef*);
void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*);
void HAL_GPIO_DeInit(GPIO_TypeDef*, uint32_t);
void HAL_GPIO_WritePin(GPIO_TypeDef*, uint16_t, GPIO_PinState);
void HAL_NVIC_ClearPendingIRQ(IRQn_Type);
uint32_t HAL_RCC_GetPCLK2Freq(void);
#endif
