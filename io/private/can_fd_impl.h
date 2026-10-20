/**
 * \file            can_fd_impl.h
 * \brief           Shared private CAN FD value validation without hardware
 * \author          Nexus Team
 */
#ifndef NEXUS_PRIVATE_CAN_FD_IMPL_H
#define NEXUS_PRIVATE_CAN_FD_IMPL_H
#include "nexus/io/can_fd.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Validate the exact wire value without admission or retaining it. */
bool nx_can_fd_frame_valid(const nx_can_fd_frame_t* frame);
#ifdef __cplusplus
}
#endif
#endif
