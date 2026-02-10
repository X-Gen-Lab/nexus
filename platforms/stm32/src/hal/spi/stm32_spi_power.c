/**
 * \file            stm32_spi_power.c
 * \brief           STM32 SPI power interface implementation
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#include "stm32_spi.h"
#include "stm32_spi_types.h"

/*---------------------------------------------------------------------------*/
/* Helper Functions                                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Get implementation from power interface
 */
static inline stm32_spi_impl_t* spi_power_get_impl(nx_power_t* self) {
    return self ? NX_CONTAINER_OF(self, stm32_spi_impl_t, power) : NULL;
}

/*---------------------------------------------------------------------------*/
/* Power Interface Implementation                                            */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Suspend SPI peripheral
 */
static nx_status_t spi_power_suspend(nx_power_t* self) {
    stm32_spi_impl_t* impl = spi_power_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INITIALIZED;
    }

    if (impl->state->suspended) {
        return NX_OK;
    }

    /* Disable SPI peripheral */
    __HAL_SPI_DISABLE(&impl->hspi);

    impl->state->suspended = true;
    return NX_OK;
}

/**
 * \brief           Resume SPI peripheral
 */
static nx_status_t spi_power_resume(nx_power_t* self) {
    stm32_spi_impl_t* impl = spi_power_get_impl(self);
    if (!impl || !impl->state) {
        return NX_ERR_INVALID_PARAM;
    }

    if (!impl->state->initialized) {
        return NX_ERR_NOT_INITIALIZED;
    }

    if (!impl->state->suspended) {
        return NX_OK;
    }

    /* Enable SPI peripheral */
    __HAL_SPI_ENABLE(&impl->hspi);

    impl->state->suspended = false;
    return NX_OK;
}

/*---------------------------------------------------------------------------*/
/* Interface Initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize power interface
 */
void spi_init_power(nx_power_t* power) {
    NX_INIT_POWER(power, spi_power_suspend, spi_power_resume);
}
