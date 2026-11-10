/**
 * \file            stm32f4_crc.h
 * \brief           Statically selected fixed-polynomial F4 CRC ownership
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F4_CRC_H
#define NEXUS_STM32F4_CRC_H
#include "nexus/io/crc.h"
#include "stm32f407_system.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Only the selected controller has this zero-initialized receipt. */
typedef struct {
    bool initialized;
    bool clock_added;
} nx_stm32_crc_state_t;
/** \brief Acquire a CRC gate under exclusive configuration ownership. */
nx_result_t nx_stm32_crc_prepare(nx_stm32_crc_state_t* state);
/** \brief Release only this owner's gate; refused writes retain the receipt. */
nx_result_t nx_stm32_crc_close(nx_stm32_crc_state_t* state);
extern const nx_crc_ops_t nx_stm32_crc_ops;
#ifdef __cplusplus
}
#endif
#endif
