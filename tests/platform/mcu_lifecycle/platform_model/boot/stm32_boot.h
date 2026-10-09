#ifndef NX_LIFECYCLE_MODEL_BOOT_H
#define NX_LIFECYCLE_MODEL_BOOT_H
#include <stdint.h>
int stm32_platform_init(void);
int stm32_platform_deinit(void);
int stm32_platform_is_initialized(void);
uint32_t stm32_platform_get_sysclk(void);
#endif
