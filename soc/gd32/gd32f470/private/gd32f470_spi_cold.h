/**
 * \file            gd32f470_spi_cold.h
 * \brief           Static GD470 SPI resources and retained cold pin leases
 * \author          Nexus Team
 */
#ifndef NEXUS_GD32F470_SPI_COLD_H
#define NEXUS_GD32F470_SPI_COLD_H

#include "gd32f470_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exact pad and the same snapshot slot owned by Platform cleanup. */
typedef struct {
    uint32_t registers;
    uint16_t* snapshot;
    uint8_t port_index;
    uint8_t pin;
    uint8_t alternate;
} nx_gd32_spi_pin_config_t;

/** \brief One immutable endpoint bound to the corresponding CS pad. */
typedef struct {
    nx_gd32_spi_endpoint_state_t* endpoint;
    uint32_t frequency_hz;
    uint8_t mode;
} nx_gd32_spi_endpoint_config_t;

/** \brief Three signal pads followed by one pad per endpoint. */
typedef struct {
    const nx_gd32_spi_controller_t* controller;
    const nx_gd32_spi_pin_config_t* pins;
    size_t pin_count;
    const nx_gd32_spi_endpoint_config_t* endpoints;
    size_t endpoint_count;
} nx_gd32_spi_config_t;

/** \brief Actual acquired effects retained until checked close succeeds. */
typedef struct {
    uint16_t pin_leases;
    bool constructed;
    bool clock_owned;
} nx_gd32_spi_cold_t;

/** \brief Validate static endpoint and pin identities without hardware effects.
 */
bool nx_gd32_spi_config_valid(const nx_gd32_spi_config_t* config);
/** \brief Acquire exact signal and CS pads for a cold selected controller.
 * \param[in] config: Static resources with distinct Platform snapshot slots.
 * \param[in,out] cold: Initially zero acquisition receipt, retained on failure.
 * \return Success or error; after any failure close must consume actual leases.
 * \note Privileged serialized task with interrupts unmasked; no allocation.
 */
nx_result_t nx_gd32_spi_pins_prepare(const nx_gd32_spi_config_t* config,
                                     nx_gd32_spi_cold_t* cold);
/** \brief Prove CS inactive, restore leased pads, then release the owned gate.
 * \param[in] config: The same static resources supplied to prepare.
 * \param[in,out] cold: Actual receipt; failed restoration remains owned.
 * \return Success or error retaining responsibility for explicit retry.
 */
nx_result_t nx_gd32_spi_pins_close(const nx_gd32_spi_config_t* config,
                                   nx_gd32_spi_cold_t* cold);
/** \brief Prepare polling SPI and all selected endpoints without discovery.
 * \param[in,out] port: Initially cold static provider in serialized task
 * context. \param[in] config: Exact immutable controller, pad and endpoint
 * connections. \param[in,out] cold: Initially zero receipt retained until
 * successful close. \return Success or error; retained failure requires close
 * even before start.
 */
nx_result_t nx_gd32_spi_prepare(nx_gd32_spi_state_t* port,
                                const nx_gd32_spi_config_t* config,
                                nx_gd32_spi_cold_t* cold);
/** \brief Stop idle polling SPI and release actual cold acquisition effects.
 * \param[in,out] port: Same owner; all I/O calls must already be joined.
 * \param[in] config: Unchanged static description used during preparation.
 * \param[in,out] cold: Acquired receipt, retained across failed cleanup
 * retries. \return Success or BUSY/IO retaining actual unfinished
 * responsibilities.
 */
nx_result_t nx_gd32_spi_close(nx_gd32_spi_state_t* port,
                              const nx_gd32_spi_config_t* config,
                              nx_gd32_spi_cold_t* cold);

#ifdef __cplusplus
}
#endif
#endif
