/**
 * \file            stm32f407_fsmc_profile.h
 * \brief           Explicit immutable F407 static-memory bank facts
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_FSMC_PROFILE_H
#define NX_STM32F407_FSMC_PROFILE_H
#include <stdbool.h>
#include <stdint.h>

/** \brief Exact legacy personalities and explicitly selected software
 * references. */
typedef enum {
    NX_STM32_FSMC_SRAM_NE3 = 3,
    NX_STM32_FSMC_LCD_NE4 = 4,
    NX_STM32_FSMC_PSRAM = 5,
    NX_STM32_FSMC_NOR = 6,
    NX_STM32_FSMC_NAND = 7
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

/** \brief Explicit NAND common/attribute HCLK encoded timing fields.
 * \note Each field is 0..254 under the official LL convention.
 */
typedef struct {
    uint8_t setup;
    uint8_t wait;
    uint8_t hold;
    uint8_t hiz;
} nx_stm32_fsmc_nand_timing_t;

/** \brief Software-reference bank electrical and addressing assumptions.
 * \note Present only for explicitly authored generic endpoints. This is not
 *       a fitted-part or PCB assertion. Bank is NE1..4 or NAND2..3; width is
 *       8/16. NORSRAM bytes is the actual positive mapped size, address_lines
 *       counts device word-address pads (1..26). Multiplexing requires NL.
 *       Byte lanes require NBL0 and, at width16, NBL1. Multiplexing excludes
 *       extended_mode; disabled EXTMOD requires one read/write timing. CRAM
 *       burst_page_bytes is 0 or 128/256/512/1024. Burst clock must remain
 *       <=60 MHz. CLK division 2..16; latency 2..17; mode 0..3 means A..D.
 *       NAND uses tclr/tar 0..15, ECC chunk 256..8192 power-of-two bytes.
 *       WAIT polarity and sampling are explicit; no timing is auto-derived.
 */
typedef struct {
    uint32_t bytes;
    uint16_t ecc_bytes;
    uint16_t burst_page_bytes;
    uint8_t bank;
    uint8_t width;
    uint8_t address_lines;
    uint8_t clock_division;
    uint8_t data_latency;
    uint8_t access_mode;
    uint8_t tclr;
    uint8_t tar;
    bool multiplexed;
    bool extended_mode;
    bool byte_lanes;
    bool burst_read;
    bool burst_write;
    bool wait_signal;
    bool asynchronous_wait;
    bool wait_high;
    bool wait_before;
    bool wrap;
    nx_stm32_fsmc_nand_timing_t common;
    nx_stm32_fsmc_nand_timing_t attribute;
} nx_stm32_fsmc_parameters_t;

#endif
