/**
 * \file            instance.h
 * \brief           Private selected F103 CRC gate ownership.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_CRC_INSTANCE_H
#define NEXUS_STM32F1_CRC_INSTANCE_H

#include "nexus/io/crc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One generated controller's initialized state and gate receipt. */
typedef struct {
    bool live;
    bool gate_added;
} nx_stm32f1_crc_t;

nx_result_t nx_stm32f1_crc_prepare(nx_stm32f1_crc_t* state);
nx_result_t nx_stm32f1_crc_close(nx_stm32f1_crc_t* state);
extern const nx_crc_ops_t nx_stm32f1_crc_ops;

#ifdef __cplusplus
}
#endif
#endif
