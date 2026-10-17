/**
 * \file            h743_flash_image_model.cpp
 * \brief           Physical image facts independent of host Flash backing
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-17
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "h743_flash_image_model.h"
extern "C" const nx_stm32h743_flash_image_t* nx_h743_flash_model_image(void) {
    /* The same logical execution/load interval is used across Flash model
     * suites. Actual ARM firmware obtains its exact bounds from the linker. */
    static const nx_stm32h743_flash_image_t image{0x08000000U, 0x08005c20U,
                                                  0x08000000U, 0x08005c6cU};
    return &image;
}
