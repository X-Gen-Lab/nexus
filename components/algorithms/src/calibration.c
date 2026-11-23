/**
 * \file            calibration.c
 * \brief           Checked binary32 scalar calibration
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-22
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "nexus/algorithms/calibration.h"
#include <math.h>

nx_result_t nx_calibration_apply_f32(const nx_calibration_f32_t* calibration,
                                     float raw, float* out) {
    if (calibration == NULL || out == NULL || !isfinite(raw) ||
        !isfinite(calibration->offset) || !isfinite(calibration->scale)) {
        return NX_ERROR_INVALID;
    }
    const float shifted = raw - calibration->offset;
    const float value = shifted * calibration->scale;
    if (!isfinite(shifted) || !isfinite(value)) {
        return NX_ERROR_EXHAUSTED;
    }
    *out = value;
    return NX_SUCCESS;
}
