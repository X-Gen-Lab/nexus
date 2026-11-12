/**
 * \file            stm32f407_capture_frames.h
 * \brief           Complete CM1 snapshot sessions without finite-state growth
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F407_CAPTURE_FRAMES_H
#define NEXUS_STM32F407_CAPTURE_FRAMES_H
#include "stm32f407_capture.h"
/** \brief The same hardware owner holds one session and one producer cursor. */
typedef struct {
    nx_stm32_capture_state_t base;
    nx_capture_frames_request_t* session;
    size_t published;
} nx_stm32_capture_frames_state_t;
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Validate the entire selected owner before shared pin capture. */
nx_result_t
nx_stm32_capture_frames_admit(const nx_stm32_capture_frames_state_t* state,
                              const nx_stm32_capture_config_t* config);
/** \brief Prepare the original snapshot transport without starting a frame. */
nx_result_t
nx_stm32_capture_frames_prepare(nx_stm32_capture_frames_state_t* state,
                                const nx_stm32_capture_config_t* config);
/** \brief Join the entire frame session before the original cold close. */
nx_result_t
nx_stm32_capture_frames_close(nx_stm32_capture_frames_state_t* state);
/** \brief Explicit optional frame session methods, source policy is external.
 */
extern const nx_capture_ops_t nx_stm32_capture_frames_ops;
#ifdef __cplusplus
}
#endif
#endif
