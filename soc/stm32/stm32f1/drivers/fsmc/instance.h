/**
 * \file            instance.h
 * \brief           Private STM32F1 asynchronous bank owner.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_FSMC_INSTANCE_H
#define NEXUS_STM32F1_FSMC_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/external_bus.h"
#include "nexus/io/memory_window.h"
#include "nexus/io/parallel16.h"

/* On a 16-bit bus, CN16 RS on FSMC_A0 is CPU address bit 1. */
#define NX_STM32F1_FSMC_LCD_COMMAND UINT32_C(0x6c000000)
#define NX_STM32F1_FSMC_LCD_DATA    UINT32_C(0x6c000002)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    GPIO_TypeDef* registers;
    uint8_t pin;
    bool deselect;
} nx_stm32f1_fsmc_pin_t;

typedef struct {
    uint8_t address_setup;
    uint8_t address_hold;
    uint8_t data_setup;
    uint8_t bus_turn;
} nx_stm32f1_fsmc_timing_t;

typedef struct {
    const nx_stm32f1_fsmc_pin_t* pins;
    size_t pin_count;
    uintptr_t address;
    size_t bytes;
    nx_stm32f1_fsmc_timing_t read_timing;
    nx_stm32f1_fsmc_timing_t write_timing;
} nx_stm32f1_fsmc_config_t;

typedef struct {
    const nx_stm32f1_fsmc_config_t* config;
    nx_memory_window_lease_t* loan;
    uint64_t epoch;
    nx_stm32f1_pads_t pads[4];
    uint32_t saved_control;
    uint32_t saved_timing;
    bool clock_owned;
    bool bank_touched;
    bool prepared;
} nx_stm32f1_fsmc_t;

/** \brief Acquire the selected NE bank under fixed HSI8 ownership. */
nx_result_t nx_stm32f1_fsmc_prepare(nx_stm32f1_fsmc_t* bus,
                                    const nx_stm32f1_fsmc_config_t* config);

/** \brief Withdraw only acquired effects after the exclusive loan returns. */
nx_result_t nx_stm32f1_fsmc_stop(nx_stm32f1_fsmc_t* bus);

extern const nx_external_bus_ops_t nx_stm32f1_fsmc_ops;
extern const nx_memory_window_ops_t nx_stm32f1_fsmc_window_ops;
extern const nx_parallel16_ops_t nx_stm32f1_fsmc_parallel16_ops;

#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F1_FSMC_INSTANCE_H */
