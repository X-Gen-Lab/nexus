/**
 * \file            stm32f407_spi_cold.h
 * \brief           Static SPI pin and endpoint assembly with checked rollback
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_SPI_COLD_H
#define NX_STM32F407_SPI_COLD_H

#include "stm32f407_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One exact pad and its existing Platform-owned snapshot slot. */
typedef struct {
    GPIO_TypeDef* registers;
    uint16_t* snapshot;
    uint8_t port_index;
    uint8_t pin;
    uint8_t alternate;
} nx_stm32_spi_pin_config_t;

/** \brief One immutable CS association, separate from controller identity. */
typedef struct {
    nx_stm32_spi_endpoint_state_t* endpoint;
    nx_stm32_gpio_state_t* chip_select;
    uint32_t frequency_hz;
    uint8_t mode;
} nx_stm32_spi_endpoint_config_t;

/** \brief Exact static resources; three signal pads precede one pad per CS.
 * \note Platform captures and owns the same snapshot slots until its final
 *       resource cleanup. This description adds no runtime dispatch or worker.
 */
typedef struct {
    SPI_TypeDef* registers;
    uint32_t clock_hz;
    const nx_stm32_spi_pin_config_t* pins;
    size_t pin_count;
    const nx_stm32_spi_endpoint_config_t* endpoints;
    size_t endpoint_count;
} nx_stm32_spi_config_t;

/** \brief Retained cold effects; shared pin snapshots stay Platform-owned. */
typedef struct {
    uint32_t gpio_mask;
    uint32_t gpio_clocks;
    uint32_t peripheral_clock;
    uint32_t cr1;
    uint32_t cr2;
    size_t captured;
    bool registers_captured;
    bool rollback_pending;
} nx_stm32_spi_cold_receipt_t;

/** \brief Validate static pad/endpoint shape without hardware effects. */
bool nx_stm32_spi_config_valid(const nx_stm32_spi_config_t* config);

/** \brief Assemble a cold polling controller and its exact static endpoints.
 * \param[in,out] port: Serialized unmasked privileged task, initially cold.
 * \param[in] config: Static selected resources with disjoint snapshot slots.
 * \param[in,out] receipt: Static cold effects retained through failed cleanup.
 * \return Success or error; failed pad withdrawal retains the Platform lease.
 */
nx_result_t nx_stm32_spi_prepare(nx_stm32_spi_state_t* port,
                                 const nx_stm32_spi_config_t* config,
                                 nx_stm32_spi_cold_receipt_t* receipt);
/** \brief Withdraw idle SPI and CS before Platform restores original pads.
 * \param[in,out] port: Owner with all calls and loans already joined.
 * \param[in] config: The unchanged resources used by prepare.
 * \param[in,out] receipt: The same retained cold effects supplied to prepare.
 * \return Success or error retaining the owner and its resource lease.
 */
nx_result_t nx_stm32_spi_close(nx_stm32_spi_state_t* port,
                               const nx_stm32_spi_config_t* config,
                               nx_stm32_spi_cold_receipt_t* receipt);
/** \brief Shared cold acquisition used directly by the selected DMA wrapper.
 * \param[in,out] port: Initially cold serialized owner.
 * \param[in] config: Exact static resources owned by the Platform pin lease.
 * \param[in,out] receipt: Static cold receipt retained through failed cleanup.
 * \return Success or error after attempted restoration of acquired effects.
 */
nx_result_t nx_stm32_spi_cold_acquire(nx_stm32_spi_state_t* port,
                                      const nx_stm32_spi_config_t* config,
                                      nx_stm32_spi_cold_receipt_t* receipt);
/** \brief Undo a failed cold acquisition without releasing unproved pads.
 * \param[in,out] port: Cold owner whose DMA/IRQ acquisition did not succeed.
 * \param[in] config: The resources supplied to cold acquire.
 * \param[in,out] receipt: Actual effects, cleared only after observed recovery.
 * \return Success only after all acquired effects are observed restored.
 * \note Failed restoration preserves this receipt and the Platform lease.
 */
nx_result_t nx_stm32_spi_cold_rollback(nx_stm32_spi_state_t* port,
                                       const nx_stm32_spi_config_t* config,
                                       nx_stm32_spi_cold_receipt_t* receipt);

#ifdef __cplusplus
}
#endif
#endif
