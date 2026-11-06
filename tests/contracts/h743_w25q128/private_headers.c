/**
 * \file            private_headers.c
 * \brief           Negative header probe in the public product compile scope
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-06
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "composition.h"
#if __has_include("stm32h743_w25q128.h") ||                                    \
                  __has_include("stm32h7xx_hal.h") ||                          \
                                __has_include("stm32h7xx_hal_conf.h") ||       \
                                              __has_include("stm32h743xx.h")
#error Private construction or supplier headers leaked into public product code
#endif

/** \brief The preprocessor, actual argv and dependency graph prove isolation.
 */
bool nx_h743_w25q128_public_headers_hidden(void) {
    return true;
}
