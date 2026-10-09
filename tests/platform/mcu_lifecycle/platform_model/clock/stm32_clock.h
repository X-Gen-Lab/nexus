#ifndef NX_LIFECYCLE_MODEL_CLOCK_H
#define NX_LIFECYCLE_MODEL_CLOCK_H
#include "hal/nx_status.h"
int SystemClock_Config(void);
nx_status_t nx_stm32f407_clock_release(void);
#endif
