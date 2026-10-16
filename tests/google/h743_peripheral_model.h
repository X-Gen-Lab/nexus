/**
 * \file            h743_peripheral_model.h
 * \brief           H743 exact external peripheral MMIO addresses
 * \author          Nexus Team
 */
#ifndef NX_H743_PERIPHERAL_MODEL_H
#define NX_H743_PERIPHERAL_MODEL_H
#include "h743_model.h"
#undef EXTI
#undef SYSCFG
#undef GPIOA
#define EXTI   (&g_h743_model.exti)
#define SYSCFG (&g_h743_model.mux)
#define GPIOA  (&g_h743_model.gpio_a)
#endif
