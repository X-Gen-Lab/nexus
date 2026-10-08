#ifndef TEST_STM32F4_HAL_H
#define TEST_STM32F4_HAL_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define __IO volatile
#define HAL_UNLOCKED 0
#define HAL_DMA_STATE_READY 0
#define HAL_DMA_STATE_BUSY 1
#define DMA_SxCR_EN 1U
#define DMA_IT_TC 2U
#define DMA_IT_HT 4U
#define DMA_IT_TE 8U
#define DMA_IT_DME 16U
#define DMA_IT_FE 32U
#define SPI_CR2_TXDMAEN 1U
#define SPI_CR2_RXDMAEN 2U
#define SPI_CR2_TXEIE 4U
#define SPI_CR2_RXNEIE 8U
#define SPI_CR2_ERRIE 16U
#define SPI_MODE_MASTER 1
#define SPI_DIRECTION_2LINES 0
#define SPI_DATASIZE_8BIT 0
#define SPI_POLARITY_LOW 0
#define SPI_POLARITY_HIGH 1
#define SPI_PHASE_1EDGE 0
#define SPI_PHASE_2EDGE 1
#define SPI_NSS_SOFT 1
#define SPI_FIRSTBIT_MSB 0
#define SPI_FIRSTBIT_LSB 1
#define SPI_TIMODE_DISABLE 0
#define SPI_CRCCALCULATION_DISABLE 0
#define SPI_BAUDRATEPRESCALER_2 2
#define SPI_BAUDRATEPRESCALER_4 4
#define SPI_BAUDRATEPRESCALER_8 8
#define SPI_BAUDRATEPRESCALER_16 16
#define SPI_BAUDRATEPRESCALER_32 32
#define SPI_BAUDRATEPRESCALER_64 64
#define SPI_BAUDRATEPRESCALER_128 128
#define SPI_BAUDRATEPRESCALER_256 256
#define CLEAR_BIT(reg, bits) ((reg) &= ~(bits))
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { uint32_t CR2; bool enabled; } SPI_TypeDef;
typedef struct { uint32_t CR, FCR, flags; } DMA_Stream_TypeDef;
typedef struct __DMA_HandleTypeDef {
    DMA_Stream_TypeDef* Instance;
    void* Parent;
    uint32_t State, Lock;
    void (*XferCpltCallback)(struct __DMA_HandleTypeDef*);
    void (*XferHalfCpltCallback)(struct __DMA_HandleTypeDef*);
    void (*XferErrorCallback)(struct __DMA_HandleTypeDef*);
    void (*XferAbortCallback)(struct __DMA_HandleTypeDef*);
} DMA_HandleTypeDef;
typedef struct {
    uint32_t Mode, Direction, DataSize, CLKPolarity, CLKPhase, NSS;
    uint32_t BaudRatePrescaler, FirstBit, TIMode, CRCCalculation, CRCPolynomial;
} SPI_InitTypeDef;
typedef struct {
    SPI_TypeDef* Instance;
    SPI_InitTypeDef Init;
    DMA_HandleTypeDef *hdmatx, *hdmarx;
    uint8_t *pTxBuffPtr, *pRxBuffPtr;
    uint16_t TxXferCount, RxXferCount;
} SPI_HandleTypeDef;
extern SPI_TypeDef fake_spi;
#define SPI1 (&fake_spi)
#define __HAL_SPI_DISABLE(h) ((h)->Instance->enabled = false)
#define __HAL_SPI_ENABLE(h) ((h)->Instance->enabled = true)
#define __HAL_DMA_DISABLE(h) ((h)->Instance->CR &= ~DMA_SxCR_EN)
#define __HAL_DMA_DISABLE_IT(h, mask) ((h)->Instance->CR &= ~(mask))
#define __HAL_DMA_CLEAR_FLAG(h, flags_) ((h)->Instance->flags &= ~(flags_))
#define __HAL_DMA_GET_TC_FLAG_INDEX(h) DMA_IT_TC
#define __HAL_DMA_GET_HT_FLAG_INDEX(h) DMA_IT_HT
#define __HAL_DMA_GET_TE_FLAG_INDEX(h) DMA_IT_TE
#define __HAL_DMA_GET_DME_FLAG_INDEX(h) DMA_IT_DME
#define __HAL_DMA_GET_FE_FLAG_INDEX(h) DMA_IT_FE
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t value);
uint32_t __get_IPSR(void);
void __DMB(void);
void __DSB(void);
void __NOP(void);
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef*);
HAL_StatusTypeDef HAL_SPI_DeInit(SPI_HandleTypeDef*);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*, uint8_t*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef*, uint8_t*, uint16_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef*, uint8_t*, uint8_t*, uint16_t);
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef*);
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef*);
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef*);
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef*);
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef*);
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef*);
/* GPIO model for production GPIO lifecycle contracts. */
typedef struct { uint16_t value; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
extern GPIO_TypeDef fake_gpio_a, fake_gpio_d;
#define GPIOA (&fake_gpio_a)
#define GPIOD (&fake_gpio_d)
#define GPIO_PIN_12 (1U << 12)
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_OUTPUT_OD 2U
#define GPIO_NOPULL 0U
#define GPIO_SPEED_FREQ_LOW 0U
void fake_gpio_clock(GPIO_TypeDef*);
#define __HAL_RCC_GPIOA_CLK_ENABLE() fake_gpio_clock(GPIOA)
#define __HAL_RCC_GPIOD_CLK_ENABLE() fake_gpio_clock(GPIOD)
void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*);
void HAL_GPIO_DeInit(GPIO_TypeDef*, uint32_t);
void HAL_GPIO_WritePin(GPIO_TypeDef*, uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*, uint16_t);
void HAL_GPIO_TogglePin(GPIO_TypeDef*, uint16_t);
#endif
