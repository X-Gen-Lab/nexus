/**
 * \file            instance.h
 * \brief           Private STM32F4 physical Flash owner and methods
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_FLASH_INSTANCE_H
#define NX_STM32F4_FLASH_INSTANCE_H

#include "nexus/io/flash.h"
#include "nexus/io/flash_maintenance.h"
#include "private/model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FLASH_TypeDef* registers;
    const nx_flash_geometry_t* geometry;
    volatile uint8_t* memory;
    uint32_t supply_mv;
    bool active;
    bool power_on_reset_required;
    bool options_owned;
#if defined(STM32F479xx)
    uint8_t cache_flags;
    bool cache_owned;
#endif
} nx_stm32_flash_state_t;

/** \brief Explicitly assembled optional face shares the physical pulse owner.
 */
extern const nx_flash_maintenance_ops_t nx_stm32_flash_maintenance_ops;
extern const nx_flash_ops_t nx_stm32_flash_ops;
const nx_flash_geometry_t* nx_stm32_flash_geometry(const void* context);
nx_result_t nx_stm32_flash_read(const void* context, uint32_t offset,
                                void* data, size_t length);
nx_result_t nx_stm32_flash_program(void* context, uint32_t offset,
                                   const void* data, size_t length,
                                   nx_time_us_t deadline);
nx_result_t nx_stm32_flash_erase(void* context, uint32_t offset, size_t length,
                                 nx_time_us_t deadline);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_FLASH_INSTANCE_H */
