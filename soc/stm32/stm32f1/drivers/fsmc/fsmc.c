/**
 * \file            fsmc.c
 * \brief           STM32F1 asynchronous bank setup and exclusive CPU lease.
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-11-28
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "instance.h"
#include "private/hardware.h"
#include "stm32f1xx_ll_fsmc.h"
#include "system/platform.h"

#if defined(NX_STM32F1_FSMC_LCD)
#define NX_F1_FSMC_BANK        FSMC_NORSRAM_BANK4
#define NX_F1_FSMC_ADDRESS     NX_STM32F1_FSMC_LCD_COMMAND
#define NX_F1_FSMC_BYTES       4U
#define NX_F1_FSMC_MIN_ADDRESS 2U
#define NX_F1_FSMC_MIN_DATA    8U
#else
#define NX_F1_FSMC_BANK        FSMC_NORSRAM_BANK3
#define NX_F1_FSMC_ADDRESS     UINT32_C(0x68000000)
#define NX_F1_FSMC_BYTES       UINT32_C(0x100000)
#define NX_F1_FSMC_MIN_ADDRESS 1U
#define NX_F1_FSMC_MIN_DATA    3U
#endif
#define NX_F1_FSMC_CONTROL                                                     \
    (UINT32_C(0x80) | FSMC_NORSRAM_MEM_BUS_WIDTH_16 |                          \
     FSMC_WRITE_OPERATION_ENABLE)

#ifndef NX_STM32F1_FSMC_IRQ_IDLE
#define NX_STM32F1_FSMC_IRQ_IDLE()                                             \
    (NVIC_GetEnableIRQ(FSMC_IRQn) == 0U && NVIC_GetActive(FSMC_IRQn) == 0U &&  \
     NVIC_GetPendingIRQ(FSMC_IRQn) == 0U)
#endif

/** \brief Mode 1 uses one timing for both reads and writes under HSI8. */
static bool timing_supported(const nx_stm32f1_fsmc_config_t* config) {
    nx_stm32f1_fsmc_timing_t read = config->read_timing;
    nx_stm32f1_fsmc_timing_t write = config->write_timing;
    return read.address_setup >= NX_F1_FSMC_MIN_ADDRESS &&
           read.address_setup <= 15U && read.address_hold >= 1U &&
           read.address_hold <= 15U && read.data_setup >= NX_F1_FSMC_MIN_DATA &&
           read.bus_turn >= 1U && read.bus_turn <= 15U &&
           read.address_setup == write.address_setup &&
           read.address_hold == write.address_hold &&
           read.data_setup == write.data_setup &&
           read.bus_turn == write.bus_turn;
}

static bool hsi8(void) {
    const uint32_t hsi = RCC_CR_HSION | RCC_CR_HSIRDY;
    return (RCC->CR & hsi) == hsi &&
           (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE)) == 0U;
}

/** \brief Configure one group and hold sibling chip selects high. */
static nx_result_t prepare_pads(nx_stm32f1_pads_t* pads,
                                const nx_stm32f1_fsmc_config_t* config,
                                GPIO_TypeDef* registers) {
    uint32_t fields[2] = {0U, 0U};
    uint16_t mask = 0U, inactive = 0U;
    for (size_t index = 0U; index < config->pin_count; ++index) {
        const nx_stm32f1_fsmc_pin_t* pin = &config->pins[index];
        if (pin->registers == registers) {
            uint16_t bit = (uint16_t)(1U << pin->pin);
            mask |= bit;
            if (pin->deselect) {
                inactive |= bit;
            }
            const uint32_t mode = pin->deselect ? 0x3U : 0xBU;
            fields[pin->pin / 8U] |= mode << (4U * (pin->pin % 8U));
        }
    }
    if (mask == 0U) {
        return NX_SUCCESS;
    }
    nx_result_t result = nx_stm32f1_pads_capture(pads, registers, mask);
    return result == NX_SUCCESS
               ? nx_stm32f1_pads_apply(pads, fields[0], fields[1], inactive)
               : result;
}

static bool pins_supported(const nx_stm32f1_fsmc_config_t* config) {
    if (config->pin_count == 0U || config->pin_count > 64U ||
        config->pins == NULL) {
        return false;
    }
    for (size_t index = 0U; index < config->pin_count; ++index) {
        const nx_stm32f1_fsmc_pin_t* pin = &config->pins[index];
        if (pin->pin > 15U ||
            (pin->registers != GPIOD && pin->registers != GPIOE &&
             pin->registers != GPIOF && pin->registers != GPIOG)) {
            return false;
        }
    }
    return true;
}

nx_result_t nx_stm32f1_fsmc_prepare(nx_stm32f1_fsmc_t* bus,
                                    const nx_stm32f1_fsmc_config_t* config) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (bus == NULL || config == NULL ||
        config->address != NX_F1_FSMC_ADDRESS ||
        config->bytes != NX_F1_FSMC_BYTES || !timing_supported(config) ||
        !pins_supported(config)) {
        return NX_ERROR_INVALID;
    }
    if (bus->config != NULL || (RCC->AHBENR & RCC_AHBENR_FSMCEN) != 0U ||
        !NX_STM32F1_FSMC_IRQ_IDLE()) {
        return NX_ERROR_BUSY;
    }
    if (!nx_stm32f1_system_live() || !hsi8()) {
        return NX_ERROR_STATE;
    }
    bus->config = config;
    bus->clock_owned = true;
    if (!nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_FSMCEN,
                                 RCC_AHBENR_FSMCEN)) {
        return NX_ERROR_IO;
    }
    volatile uint32_t* control = &FSMC_Bank1->BTCR[NX_F1_FSMC_BANK];
    volatile uint32_t* timing = control + 1U;
    bus->saved_control = *control;
    if ((bus->saved_control & FSMC_BCRx_MBKEN) != 0U) {
        return NX_ERROR_BUSY;
    }
    bus->saved_timing = *timing;
    bus->bank_touched = true;
    const FSMC_NORSRAM_InitTypeDef mode = {
        .NSBank = NX_F1_FSMC_BANK,
        .MemoryType = FSMC_MEMORY_TYPE_SRAM,
        .MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16,
        .WriteOperation = FSMC_WRITE_OPERATION_ENABLE,
    };
    const FSMC_NORSRAM_TimingTypeDef cycles = {
        .AddressSetupTime = config->read_timing.address_setup,
        .AddressHoldTime = config->read_timing.address_hold,
        .DataSetupTime = config->read_timing.data_setup,
        .BusTurnAroundDuration = config->read_timing.bus_turn,
        .CLKDivision = 1U,
        .DataLatency = 2U,
        .AccessMode = FSMC_ACCESS_MODE_A,
    };
    uint32_t encoded = (cycles.AddressSetupTime << FSMC_BTRx_ADDSET_Pos) |
                       (cycles.AddressHoldTime << FSMC_BTRx_ADDHLD_Pos) |
                       (cycles.DataSetupTime << FSMC_BTRx_DATAST_Pos) |
                       (cycles.BusTurnAroundDuration << FSMC_BTRx_BUSTURN_Pos);
    if (FSMC_NORSRAM_Init(FSMC_Bank1, &mode) != HAL_OK ||
        FSMC_NORSRAM_Timing_Init(FSMC_Bank1, &cycles, NX_F1_FSMC_BANK) !=
            HAL_OK) {
        return NX_ERROR_IO;
    }
    nx_arch_dsb();
    /* BCR bit 7 is reserved at its reset value on this part. */
    if (*control != NX_F1_FSMC_CONTROL || *timing != encoded) {
        return NX_ERROR_IO;
    }
    GPIO_TypeDef* const ports[] = {GPIOD, GPIOE, GPIOF, GPIOG};
    for (size_t index = 0U; index < sizeof(ports) / sizeof(ports[0]); ++index) {
        nx_result_t result =
            prepare_pads(&bus->pads[index], config, ports[index]);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    if (!nx_stm32f1_write_masked(control, FSMC_BCRx_MBKEN, FSMC_BCRx_MBKEN)) {
        return NX_ERROR_IO;
    }
    bus->prepared = true;
    return NX_SUCCESS;
}

nx_result_t nx_stm32f1_fsmc_stop(nx_stm32f1_fsmc_t* bus) {
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (bus == NULL) {
        return NX_ERROR_INVALID;
    }
    if (bus->loan != NULL) {
        return NX_ERROR_BUSY;
    }
    bus->prepared = false;
    if (bus->bank_touched) {
        volatile uint32_t* control = &FSMC_Bank1->BTCR[NX_F1_FSMC_BANK];
        volatile uint32_t* timing = control + 1U;
        nx_arch_dsb();
        if (!nx_stm32f1_write_masked(control, FSMC_BCRx_MBKEN, 0U) ||
            !nx_stm32f1_write_masked(timing, UINT32_MAX, bus->saved_timing) ||
            !nx_stm32f1_write_masked(control, UINT32_MAX, bus->saved_control)) {
            return NX_ERROR_IO;
        }
        bus->bank_touched = false;
    }
    for (size_t index = sizeof(bus->pads) / sizeof(bus->pads[0]); index != 0U;
         --index) {
        nx_result_t result = nx_stm32f1_pads_restore(&bus->pads[index - 1U]);
        if (result != NX_SUCCESS) {
            return result;
        }
    }
    if (bus->clock_owned &&
        !nx_stm32f1_write_masked(&RCC->AHBENR, RCC_AHBENR_FSMCEN, 0U)) {
        return NX_ERROR_IO;
    }
    bus->clock_owned = false;
    bus->config = NULL;
    return NX_SUCCESS;
}

static nx_external_bus_state_t bus_state(const void* context) {
    const nx_stm32f1_fsmc_t* bus = context;
    if (bus->prepared) {
        return NX_EXTERNAL_BUS_PREPARED;
    }
    return bus->config != NULL ? NX_EXTERNAL_BUS_CLEANUP_PENDING
                               : NX_EXTERNAL_BUS_STOPPED;
}

static nx_result_t bus_stop(void* context) {
    return nx_stm32f1_fsmc_stop(context);
}

/** \brief Receipts remain in internal RAM, disjoint from mutable ownership. */
static bool output_storage(const nx_stm32f1_fsmc_t* bus, const void* output,
                           size_t bytes) {
    uintptr_t address = (uintptr_t)output;
    uintptr_t state = (uintptr_t)bus;
    bool internal =
        address >= SRAM_BASE && address <= SRAM_BASE + 98304U - bytes;
#if UINTPTR_MAX > UINT32_MAX
    internal = internal || address > UINT32_MAX;
#endif
    return internal &&
           !(address < state + sizeof(*bus) && state < address + bytes);
}

static nx_result_t acquire(void* context, nx_memory_window_lease_t* loan) {
    nx_stm32f1_fsmc_t* bus = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!output_storage(bus, loan, sizeof(*loan))) {
        return NX_ERROR_INVALID;
    }
    if (!bus->prepared || bus->epoch == UINT64_MAX) {
        return NX_ERROR_STATE;
    }
    if (bus->loan != NULL) {
        return NX_ERROR_BUSY;
    }
    if (loan->owner != NULL || loan->epoch != 0U || loan->address != 0U ||
        loan->bytes != 0U) {
        return NX_ERROR_INVALID;
    }
    *loan = (nx_memory_window_lease_t){bus, bus->config->address,
                                       bus->config->bytes, ++bus->epoch};
    bus->loan = loan;
    return NX_SUCCESS;
}

static nx_result_t release(void* context, nx_memory_window_lease_t* loan,
                           uint64_t epoch) {
    nx_stm32f1_fsmc_t* bus = context;
    if (!nx_stm32f1_cold_context()) {
        return NX_ERROR_CONTEXT;
    }
    if (!bus->prepared) {
        return NX_ERROR_STATE;
    }
    if (bus->loan != loan || !output_storage(bus, loan, sizeof(*loan))) {
        return NX_ERROR_INVALID;
    }
    if (bus->epoch != epoch || loan->epoch != epoch || loan->owner != bus ||
        loan->address != bus->config->address ||
        loan->bytes != bus->config->bytes) {
        return NX_ERROR_STATE;
    }
    nx_arch_dsb();
    *loan = (nx_memory_window_lease_t){0};
    bus->loan = NULL;
    return NX_SUCCESS;
}

static nx_result_t geometry(const void* context,
                            nx_memory_window_geometry_t* result) {
    const nx_stm32f1_fsmc_t* bus = context;
    if (!output_storage(bus, result, sizeof(*result)) ||
        (bus->loan != NULL &&
         (uintptr_t)result < (uintptr_t)bus->loan + sizeof(*bus->loan) &&
         (uintptr_t)bus->loan < (uintptr_t)result + sizeof(*result))) {
        return NX_ERROR_INVALID;
    }
    if (!bus->prepared) {
        return NX_ERROR_STATE;
    }
    *result = (nx_memory_window_geometry_t){bus->config->address,
                                            bus->config->bytes, false};
    return NX_SUCCESS;
}

const nx_external_bus_ops_t nx_stm32f1_fsmc_ops = {bus_state, bus_stop};
const nx_memory_window_ops_t nx_stm32f1_fsmc_window_ops = {acquire, release,
                                                           geometry};
