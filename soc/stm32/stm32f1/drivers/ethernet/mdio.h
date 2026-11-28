/**
 * \file            mdio.h
 * \brief           Private F107 station-management owner and pad receipts.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_ETHERNET_MDIO_H
#define NEXUS_STM32F1_ETHERNET_MDIO_H

#include "drivers/gpio/instance.h"
#include "nexus/io/mdio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Exclusive MAC clock and two reversible station-management pads. */
typedef struct {
    nx_stm32f1_pads_t mdc;
    nx_stm32f1_pads_t mdio;
    uint8_t effects;
} nx_stm32f1_mdio_t;

extern const nx_mdio_ops_t nx_stm32f1_mdio_ops;

/** \brief Acquire the unused management circuit on the reviewed F107 clock. */
nx_result_t nx_stm32f1_mdio_prepare(nx_stm32f1_mdio_t* state);

/** \brief Refuse new requests and relinquish the pads only after BUSY clears.
 */
nx_result_t nx_stm32f1_mdio_close(nx_stm32f1_mdio_t* state);

#ifdef __cplusplus
}
#endif
#endif
