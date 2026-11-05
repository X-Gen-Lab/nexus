/**
 * \file            can_validation.h
 * \brief           Private classic CAN value validation without hardware
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_CAN_VALIDATION_H
#define NEXUS_PRIVATE_CAN_VALIDATION_H
#include "nexus/io/can.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Validate the complete wire value without retaining its address. */
bool nx_can_frame_valid(const nx_can_frame_t* frame);
#ifdef __cplusplus
}
#endif
#endif
