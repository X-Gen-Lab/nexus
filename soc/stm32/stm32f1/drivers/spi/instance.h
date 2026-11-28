/**
 * \file            instance.h
 * \brief           Private fixed-route SPI2 short-poll owners and endpoint.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_SPI_INSTANCE_H
#define NEXUS_STM32F1_SPI_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/spi.h"
#include "nexus/io/spi_transaction.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Generated SPI2 route and one explicitly wired active-low select. */
typedef struct {
    SPI_TypeDef* registers;
    GPIO_TypeDef* gpio;
    uint32_t clock_hz;
    uint8_t sck;
    uint8_t miso;
    uint8_t mosi;
    uint8_t cs;
} nx_stm32f1_spi_config_t;

/** \brief One serialized bus owner; faults retain only hardware receipts. */
typedef struct {
    const nx_stm32f1_spi_config_t* config;
    nx_stm32f1_pads_t pads;
    bool gate_added;
    bool peripheral_owned;
    bool live;
    bool active;
    bool selected;
    bool fault;
} nx_stm32f1_spi_t;

/** \brief Immutable endpoint profile, separate from the bus execution state. */
typedef struct {
    nx_stm32f1_spi_t* port;
    GPIO_TypeDef* cs_gpio;
    uint32_t max_hz;
    uint8_t cs_pin;
    uint8_t mode;
    bool lsb_first;
} nx_stm32f1_spi_endpoint_t;

nx_result_t nx_stm32f1_spi_admit(const nx_stm32f1_spi_t* state,
                                 const nx_stm32f1_spi_config_t* config);
nx_result_t nx_stm32f1_spi_prepare(nx_stm32f1_spi_t* state,
                                   const nx_stm32f1_spi_config_t* config);
nx_result_t nx_stm32f1_spi_close(nx_stm32f1_spi_t* state);
extern const nx_spi_ops_t nx_stm32f1_spi_ops;
extern const nx_spi_endpoint_ops_t nx_stm32f1_spi_endpoint_ops;
extern const nx_spi_transaction_ops_t nx_stm32f1_spi_transaction_ops;

#ifdef __cplusplus
}
#endif
#endif
