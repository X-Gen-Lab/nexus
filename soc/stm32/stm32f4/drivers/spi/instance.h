/**
 * \file            instance.h
 * \brief           Private STM32F4 SPI controller and endpoint state
 * \author          Nexus Team
 */
#ifndef NX_STM32F4_SPI_INSTANCE_H
#define NX_STM32F4_SPI_INSTANCE_H

#include "drivers/gpio/instance.h"
#include "nexus/io/spi.h"
#include "nexus/io/spi_transaction.h"
#include "private/model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nx_stm32_spi_endpoint_state nx_stm32_spi_endpoint_state_t;
typedef struct {
    SPI_TypeDef* registers;
    RCC_TypeDef* rcc;
    uint32_t clock_hz;
    bool active;
    bool fault;
    const nx_stm32_spi_endpoint_state_t* held_endpoint;
} nx_stm32_spi_state_t;
struct nx_stm32_spi_endpoint_state {
    nx_stm32_spi_state_t* port;
    nx_stm32_gpio_state_t* cs;
    uint32_t cs_mask;
    uint32_t frequency_hz;
    uint8_t mode;
    bool lsb_first;
};

/** \brief Check the selected CS output latch, retaining failed withdrawal.
 * \note Endpoint identity is static; this never retains a payload pointer.
 */
nx_result_t nx_stm32_spi_select(const nx_stm32_spi_endpoint_state_t* endpoint,
                                bool selected);

extern const nx_spi_ops_t nx_stm32_spi_ops;
nx_result_t nx_stm32_spi_recover(void* context);
extern const nx_spi_endpoint_ops_t nx_stm32_spi_endpoint_ops;
extern const nx_spi_transaction_ops_t nx_stm32_spi_transaction_ops;
nx_result_t nx_stm32_spi_endpoint_transfer(void* context, const uint8_t* tx,
                                           uint8_t* rx, size_t length,
                                           nx_time_us_t deadline,
                                           size_t* transferred);

#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F4_SPI_INSTANCE_H */
