/**
 * \file            stm32f4xx_hal_conf.h
 * \brief           Isolated DMA2D official transport compilation profile
 * \author          Nexus Team
 */
#ifndef NX_DMA2D_HAL_CONF_H
#define NX_DMA2D_HAL_CONF_H
#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_DMA2D_MODULE_ENABLED
#if !defined(USE_HAL_DMA2D_REGISTER_CALLBACKS)
#define USE_HAL_DMA2D_REGISTER_CALLBACKS 0U
#elif USE_HAL_DMA2D_REGISTER_CALLBACKS != 0U
#error "Nexus graphics requires USE_HAL_DMA2D_REGISTER_CALLBACKS=0"
#endif
#define assert_param(value) ((void)0U)
#include "stm32f4xx_hal_dma2d.h"
#include "stm32f4xx_hal_rcc.h"
#endif /* NX_DMA2D_HAL_CONF_H */
