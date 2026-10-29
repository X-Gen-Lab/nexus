/**
 * \file            stm32h743_platform_cold.h
 * \brief           Static selected GPIO and RCC lease ownership
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32H743_PLATFORM_COLD_H
#define NEXUS_STM32H743_PLATFORM_COLD_H
#include "stm32h743_system.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief One selected bank snapshot, retained until observed restoration. */
typedef struct {
    uint32_t mode;
    uint32_t type;
    uint32_t speed;
    uint32_t pull;
    uint32_t af0;
    uint32_t af1;
    uint32_t odr;
    bool captured;
} nx_stm32h743_gpio_lease_t;
/** \brief Exact selected bank and mask; storage is shared across its users. */
typedef struct {
    GPIO_TypeDef* registers;
    nx_stm32h743_gpio_lease_t* receipt;
    uint16_t pins;
    uint8_t bank;
} nx_stm32h743_gpio_lease_config_t;
/** \brief Selected bits and exact storage; C owns the named gate algorithm. */
typedef struct {
    uint32_t mask;
    uint32_t* added;
} nx_stm32h743_gate_lease_config_t;
/** \brief Static assembly data, without operation codes or a register script.
 */
typedef struct {
    const nx_stm32h743_gpio_lease_config_t* ports;
    size_t port_count;
    uint32_t* gpio_added;
    uint32_t* uart_added;
    uint32_t* dma_added;
    nx_stm32h743_gate_lease_config_t apb2;
    nx_stm32h743_gate_lease_config_t apb1;
    nx_stm32h743_gate_lease_config_t ahb1;
    nx_stm32h743_gate_lease_config_t apb4;
    uint8_t dma_lanes;
    uint8_t dma_strict;
} nx_stm32h743_platform_config_t;
/** \brief Read selected powered pads without acquiring any hardware effects.
 * \note Task-only; exact reviewed static facts, no foreign writers. Disabled
 *       banks are admitted again only after their clock has been acquired.
 */
nx_result_t
nx_stm32h743_platform_preflight(const nx_stm32h743_platform_config_t* config);
/** \brief Check explicit DMA1 lane/vector ownership without mutation.
 * \note Task-only. strict selects full SPI/duplex residue checks; when a gate
 *       is off, stream MMIO is never treated as evidence of ownership.
 */
nx_result_t nx_stm32h743_dma_preflight(uint8_t lanes, uint8_t strict);
/** \brief Acquire gates, admit all banks, then capture exactly one snapshot.
 * \note Task-only, serialized through successful release. Partial acquisition
 *       retains the same receipts for caller cleanup and a subsequent retry.
 */
nx_result_t
nx_stm32h743_platform_acquire(const nx_stm32h743_platform_config_t* config);
/** \brief Restore pads before withdrawing only clock bits acquired here.
 * \note All controller, DMA and IRQ users have joined first. Failure retains
 *       each unproved effect and forbids overwriting its receipt on restart.
 */
nx_result_t
nx_stm32h743_platform_release(const nx_stm32h743_platform_config_t* config);
#ifdef __cplusplus
}
#endif
#endif
