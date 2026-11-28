/**
 * \file            public_cpp.cpp
 * \brief           Compile the selected F1 public interfaces in C++17
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus_bindings.h"
#include "nexus_factory_strong.h"
#include <type_traits>

#if !defined(__ARM_ARCH_7M__) || defined(__ARM_PCS_VFP) || defined(__ARM_FP)
#error "The F1 consumer requires Cortex-M3 soft-float"
#endif
#if __has_include("stm32f1xx.h") || __has_include("stm32f1xx_hal.h")
#error "The F1 consumer must not inherit private vendor headers"
#endif

static_assert(
    std::is_same_v<decltype(nx_factory_gpio_signal_ref(NX_GPIO_REF_STATUS)),
                   const nx_gpio_signal_t*>);
static_assert(std::is_same_v<decltype(nx_factory_uart_ref(NX_UART_REF_CONSOLE)),
                             const nx_uart_port_t*>);
static_assert(!std::is_convertible_v<nx_gpio_ref_t, nx_uart_ref_t>);
