#include "../stm32f4xx_hal.h"
#undef GPIO_MODE_OUTPUT_OD
#define GPIO_MODE_OUTPUT_OD 0x11U
#define GPIO_MODE_AF_PP 0x02U
#define GPIO_MODE_AF_OD 0x12U
#define GPIO_PIN_13 (1U << 13)
#define GPIO_PIN_14 (1U << 14)
