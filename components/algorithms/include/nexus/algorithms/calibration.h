/**
 * \file            calibration.h
 * \brief           Explicit scalar calibration without device or storage policy
 * \author          Nexus Team
 */
#ifndef NEXUS_ALGORITHMS_CALIBRATION_H
#define NEXUS_ALGORITHMS_CALIBRATION_H
#include "nexus/core/status.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Product-selected raw offset and output units per raw unit. */
typedef struct {
    float offset;
    float scale;
} nx_calibration_f32_t;
/**
 * \brief           Evaluate (raw - offset) * scale in binary32 arithmetic.
 * \param[in]       calibration: Stable finite offset and scale; zero allowed.
 * \param[in]       raw: Finite raw measurement.
 * \param[out]      out: Separate aligned output, unchanged on failure.
 * \return          SUCCESS, INVALID or EXHAUSTED for intermediate overflow.
 * \note            Pure Task/IRQ calculation without clock, device, heap or
 *                  retained references. The caller owns validity, identity,
 *                  version, units and activation. Both coefficients come from
 *                  one serialized snapshot.
 */
nx_result_t nx_calibration_apply_f32(const nx_calibration_f32_t* calibration,
                                     float raw, float* out);
#ifdef __cplusplus
}
#endif
#endif
