#ifndef NX_LIFECYCLE_MODEL_PERFORMANCE_H
#define NX_LIFECYCLE_MODEL_PERFORMANCE_H
#include <stdint.h>
int stm32_perf_init(void);
void stm32_perf_deinit(void);
void stm32_boot_time_mark(uint8_t);
#endif
