/**
 * \file            gd32_cold_pins_model.h
 * \brief           Host external MMIO write observation and failure injection
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32_COLD_PINS_MODEL_H
#define NEXUS_GD32_COLD_PINS_MODEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Observe a production write or model a refused peripheral effect. */
void nx_gd32_cold_model_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
#endif
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_gd32_cold_model_write(&(reg), (uint32_t)(value))
#endif /* NEXUS_GD32_COLD_PINS_MODEL_H */
