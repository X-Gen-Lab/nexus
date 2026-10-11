/**
 * \file            timebase_lowpower_model.h
 * \brief           External register hooks preserve real SysTick/W0C semantics
 * \author          Nexus Team
 */
#ifndef NX_TIMEBASE_LOWPOWER_MODEL_H
#define NX_TIMEBASE_LOWPOWER_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_timebase_model_read(volatile uint32_t* reg);
void nx_timebase_model_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_TIMEBASE_READ(reg)         nx_timebase_model_read(reg)
#define NX_TIMEBASE_WRITE(reg, value) nx_timebase_model_write((reg), (value))
#endif /* NX_TIMEBASE_LOWPOWER_MODEL_H */
