/**
 * \file            stm32f407_fsmc_profile.h
 * \brief           Minimal immutable F407 asynchronous memory bank facts
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_FSMC_PROFILE_H
#define NX_STM32F407_FSMC_PROFILE_H
#include <stdint.h>

/** \brief Exact Qiming 16-bit NE3 SRAM or NE4 command/data personality. */
typedef enum {
    NX_STM32_FSMC_SRAM_NE3 = 3,
    NX_STM32_FSMC_LCD_NE4 = 4
} nx_stm32_fsmc_kind_t;

/**
 * \brief           Explicit asynchronous access-mode-A cycle fields
 * \note            Values use the official FSMC LL timing convention:
 *                  address_setup 0..15, address_hold 1..15, data_setup 1..255,
 *                  bus_turn 0..15. They are declared board/device timing
 *                  assumptions, never measured nanoseconds or auto-tuning.
 */
typedef struct {
    uint8_t address_setup;
    uint8_t address_hold;
    uint8_t data_setup;
    uint8_t bus_turn;
} nx_stm32_fsmc_timing_t;

/** \brief Immutable exact bank with independent read and write timing. */
typedef struct nx_stm32_fsmc_bank_config {
    nx_stm32_fsmc_kind_t kind;
    nx_stm32_fsmc_timing_t read;
    nx_stm32_fsmc_timing_t write;
} nx_stm32_fsmc_bank_config_t;

#endif
