/**
 * \file            u575_gpio_command_model.h
 * \brief           U575 word observations with the actual GPIO command port
 * \author          Nexus Team
 */
#ifndef NX_U575_GPIO_COMMAND_MODEL_H
#define NX_U575_GPIO_COMMAND_MODEL_H
#define NX_U575_GPIO_DEFAULT_PORT 1
#include "u575_model.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_u575_gpio_command_read(volatile uint32_t* reg);
#ifdef __cplusplus
}
#endif
/* The external latch observer never replaces the production command store. */
#undef NX_U575_READ
#define NX_U575_READ(reg) nx_u575_gpio_command_read(reg)
#endif
