/**
 * \file            f407_timer_waveform_model.h
 * \brief           External register and CPU leaves for the F407 timer owner
 * \author          Nexus Team
 */
#ifndef NX_F407_TIMER_WAVEFORM_MODEL_H
#define NX_F407_TIMER_WAVEFORM_MODEL_H
#include "f407_capture_audio_model.h"
#define NX_TIMER_WRITE(reg, value) NX_AV_WRITE(&(reg), (value))
#define NX_TIMER_READ(reg)         NX_AV_READ(&(reg))
#endif /* NX_F407_TIMER_WAVEFORM_MODEL_H */
