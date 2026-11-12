/**
 * \file            stm32f407_packet_cold.h
 * \brief           Shared packet pad admission and retained restoration
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_PACKET_COLD_H
#define NX_STM32F407_PACKET_COLD_H
#include "stm32f407_platform_cold.h"

#ifndef NX_PLATFORM_REGISTER_WRITE
#define NX_PLATFORM_REGISTER_WRITE(reg, value) ((reg) = (uint32_t)(value))
#endif

/** \brief Check static packet pads before any state or MMIO write. */
static inline bool
nx_stm32_packet_pins_valid(const nx_stm32_platform_pin_config_t* pins,
                           size_t count) {
    if (pins == NULL || count == 0U || count > 17U)
        return false;
    for (size_t i = 0U; i < count; ++i) {
        if (pins[i].registers == NULL || pins[i].snapshot == NULL ||
            pins[i].port_index > 8U || pins[i].pin > 15U)
            return false;
        for (size_t j = 0U; j < i; ++j) {
            if (pins[i].snapshot == pins[j].snapshot ||
                (pins[i].registers == pins[j].registers &&
                 pins[i].pin == pins[j].pin))
                return false;
        }
    }
    return true;
}

/** \brief Withdraw and read back one leased packet pad before restoration.
 * \note The caller keeps its lease bit until the exact original fields read
 *       back. Failed withdrawal does not restore a still-driven latch.
 */
static inline nx_result_t
nx_stm32_packet_pin_close(const nx_stm32_platform_pin_config_t* pin) {
    uint32_t mask = 3U << (pin->pin * 2U);
    uint32_t mode = pin->registers->MODER & mask;
    if (mode != 0U && mode != mask) {
        NX_PLATFORM_REGISTER_WRITE(pin->registers->MODER,
                                   pin->registers->MODER | mask);
        nx_arch_dsb();
        if ((pin->registers->MODER & mask) != mask)
            return NX_ERROR_IO;
    }
    nx_stm32_pin_restore(pin->registers, pin->pin, *pin->snapshot);
    return nx_stm32_pin_capture(pin->registers, pin->pin) == *pin->snapshot
               ? NX_SUCCESS
               : NX_ERROR_IO;
}

#endif
