#ifndef NEXUS_BOARD_HAL_FIXTURE_H
#define NEXUS_BOARD_HAL_FIXTURE_H
#include <stdint.h>
typedef struct { uint32_t latch, outputs, alternate; unsigned clocks; } GPIO_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
extern GPIO_TypeDef fixture_a, fixture_b, fixture_e, fixture_g;
#define GPIOA (&fixture_a)
#define GPIOB (&fixture_b)
#define GPIOE (&fixture_e)
#define GPIOG (&fixture_g)
#define GPIO_PIN_2 (1U << 2)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_9 (1U << 9)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_MODE_AF_PP 2U
#define GPIO_NOPULL 0U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_LOW 0U
#define GPIO_SPEED_FREQ_HIGH 3U
#define GPIO_AF7_USART1 7U
void fixture_clock(GPIO_TypeDef* port);
void fixture_uart_clock(unsigned enabled);
#define __HAL_RCC_GPIOA_CLK_ENABLE() fixture_clock(GPIOA)
#define __HAL_RCC_GPIOB_CLK_ENABLE() fixture_clock(GPIOB)
#define __HAL_RCC_GPIOE_CLK_ENABLE() fixture_clock(GPIOE)
#define __HAL_RCC_GPIOG_CLK_ENABLE() fixture_clock(GPIOG)
#define __HAL_RCC_USART1_CLK_ENABLE() fixture_uart_clock(1)
#define __HAL_RCC_USART1_CLK_DISABLE() fixture_uart_clock(0)
void HAL_GPIO_WritePin(GPIO_TypeDef*, uint16_t, GPIO_PinState);
void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*);
void HAL_GPIO_DeInit(GPIO_TypeDef*, uint32_t);
void HAL_MspInit(void);
#endif
