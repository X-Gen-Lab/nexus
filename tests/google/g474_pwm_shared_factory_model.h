/**
 * \file            g474_pwm_shared_factory_model.h
 * \brief           External MMIO for one normally generated shared TIM3 owner
 * \author          Nexus Team
 */
#ifndef NX_G474_PWM_SHARED_FACTORY_MODEL_H
#define NX_G474_PWM_SHARED_FACTORY_MODEL_H

#include "g474_pwm_shared_model.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_g474_shared_factory_read(volatile uint32_t* reg);
void nx_g474_shared_factory_write(volatile uint32_t* reg, uint32_t value);
#ifdef __cplusplus
}
/** \brief Scoped bus failures; no Platform/provider algorithm is replaced. */
struct nx_g474_shared_factory_faults_t {
    uint32_t refused_mode_fields;
    uint32_t refused_latch_set;
    uint32_t snapshot_mode;
    bool refuse_snapshot_mode;
    unsigned refused_mode_writes;
    unsigned refused_latch_writes;
    unsigned refused_snapshot_writes;
};
extern nx_g474_shared_factory_faults_t g_shared_factory_faults;
#endif

#undef FLASH
#undef PWR
#undef TIM2
#define FLASH (&g_g474_model.flash)
#define PWR   (&g_g474_model.power)
#define TIM2  (&g_g474_model.timer)
#undef NX_G474_READ
#undef NX_G474_WRITE
#define NX_G474_READ(reg) nx_g474_shared_factory_read(reg)
#define NX_G474_WRITE(reg, value)                                              \
    nx_g474_shared_factory_write((reg), (uint32_t)(value))
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_g474_shared_factory_write(&(reg), (uint32_t)(value))
#endif
