#include "../stm32f4xx_hal.h"
#define FLASH_FLAG_BSY (1U<<16)
#define FLASH_ACR_ICEN (1U<<9)
#define __HAL_FLASH_INSTRUCTION_CACHE_DISABLE() (fake_flash.ACR &= ~FLASH_ACR_ICEN)
#define __HAL_FLASH_INSTRUCTION_CACHE_RESET() fake_flash_cache_reset()
#define __HAL_FLASH_INSTRUCTION_CACHE_ENABLE() (fake_flash.ACR |= FLASH_ACR_ICEN)
int fake_flash_busy(void);
#define __HAL_FLASH_GET_FLAG(flag) ((void)(flag),fake_flash_busy())
