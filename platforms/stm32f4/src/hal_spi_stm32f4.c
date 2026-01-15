/**
 * \file            hal_spi_stm32f4.c
 * \brief           STM32F4 SPI HAL Implementation (ST HAL Wrapper)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation wraps ST HAL SPI functions to provide the Nexus HAL
 * interface. It uses HAL_SPI_Init(), HAL_SPI_Transmit(), HAL_SPI_Receive(),
 * HAL_SPI_TransmitReceive(), HAL_SPI_Transmit_IT(), HAL_SPI_Receive_IT(),
 * and HAL_SPI_IRQHandler() from the ST HAL library.
 */

#include "hal/hal_spi.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           Maximum number of SPI instances
 */
#define SPI_MAX_INSTANCES 3

/**
 * \brief           Default timeout for operations (ms)
 */
#define SPI_DEFAULT_TIMEOUT 1000

/**
 * \brief           SPI instance data structure - wraps ST HAL Handle
 */
typedef struct {
    SPI_HandleTypeDef hspi;         /**< ST HAL SPI Handle */
    hal_spi_config_t config;        /**< Nexus configuration */
    hal_spi_callback_t tx_callback; /**< TX complete callback */
    hal_spi_callback_t rx_callback; /**< RX complete callback */
    void* tx_context;               /**< TX callback context */
    void* rx_context;               /**< RX callback context */
    GPIO_TypeDef* cs_port;          /**< CS GPIO port */
    uint16_t cs_pin;                /**< CS GPIO pin mask */
    bool cs_configured;             /**< CS pin configured flag */
    bool initialized;               /**< Initialization flag */
} spi_data_t;

/**
 * \brief           SPI instance data array
 */
static spi_data_t spi_data[SPI_MAX_INSTANCES];

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Get SPI data by instance
 * \param[in]       instance: SPI instance
 * \return          Pointer to SPI data or NULL
 */
static spi_data_t* spi_get_data(hal_spi_instance_t instance) {
    if (instance >= SPI_MAX_INSTANCES) {
        return NULL;
    }
    return &spi_data[instance];
}

/**
 * \brief           Get SPI peripheral pointer by instance
 * \param[in]       instance: SPI instance
 * \return          SPI peripheral pointer
 */
static SPI_TypeDef* spi_get_instance(hal_spi_instance_t instance) {
    switch (instance) {
        case HAL_SPI_0:
            return SPI1;
        case HAL_SPI_1:
            return SPI2;
        case HAL_SPI_2:
            return SPI3;
        default:
            return NULL;
    }
}

/**
 * \brief           Map Nexus SPI mode to ST HAL CPOL/CPHA
 * \param[in]       mode: Nexus SPI mode
 * \param[out]      cpol: Pointer to store CPOL value
 * \param[out]      cpha: Pointer to store CPHA value
 */
static void map_spi_mode(hal_spi_mode_t mode, uint32_t* cpol, uint32_t* cpha) {
    switch (mode) {
        case HAL_SPI_MODE_0:
            *cpol = SPI_POLARITY_LOW;
            *cpha = SPI_PHASE_1EDGE;
            break;
        case HAL_SPI_MODE_1:
            *cpol = SPI_POLARITY_LOW;
            *cpha = SPI_PHASE_2EDGE;
            break;
        case HAL_SPI_MODE_2:
            *cpol = SPI_POLARITY_HIGH;
            *cpha = SPI_PHASE_1EDGE;
            break;
        case HAL_SPI_MODE_3:
            *cpol = SPI_POLARITY_HIGH;
            *cpha = SPI_PHASE_2EDGE;
            break;
        default:
            *cpol = SPI_POLARITY_LOW;
            *cpha = SPI_PHASE_1EDGE;
            break;
    }
}

/**
 * \brief           Map Nexus bit order to ST HAL first bit
 */
static uint32_t map_bit_order(hal_spi_bit_order_t bit_order) {
    switch (bit_order) {
        case HAL_SPI_LSB_FIRST:
            return SPI_FIRSTBIT_LSB;
        case HAL_SPI_MSB_FIRST:
        default:
            return SPI_FIRSTBIT_MSB;
    }
}

/**
 * \brief           Map Nexus data width to ST HAL data size
 */
static uint32_t map_data_width(hal_spi_data_width_t data_width) {
    switch (data_width) {
        case HAL_SPI_DATA_16BIT:
            return SPI_DATASIZE_16BIT;
        case HAL_SPI_DATA_8BIT:
        default:
            return SPI_DATASIZE_8BIT;
    }
}

/**
 * \brief           Map Nexus role to ST HAL mode
 */
static uint32_t map_role(hal_spi_role_t role) {
    switch (role) {
        case HAL_SPI_ROLE_SLAVE:
            return SPI_MODE_SLAVE;
        case HAL_SPI_ROLE_MASTER:
        default:
            return SPI_MODE_MASTER;
    }
}

/**
 * \brief           Calculate prescaler for desired clock frequency
 * \param[in]       instance: SPI instance
 * \param[in]       clock_hz: Desired clock frequency in Hz
 * \return          ST HAL prescaler value
 */
static uint32_t calculate_prescaler(hal_spi_instance_t instance,
                                    uint32_t clock_hz) {
    uint32_t pclk;

    /* SPI1 is on APB2 (84MHz), SPI2/SPI3 are on APB1 (42MHz) */
    if (instance == HAL_SPI_0) {
        pclk = 84000000; /* APB2 clock */
    } else {
        pclk = 42000000; /* APB1 clock */
    }

    /* Find the smallest prescaler that gives a clock <= desired */
    if (clock_hz >= pclk / 2) {
        return SPI_BAUDRATEPRESCALER_2;
    } else if (clock_hz >= pclk / 4) {
        return SPI_BAUDRATEPRESCALER_4;
    } else if (clock_hz >= pclk / 8) {
        return SPI_BAUDRATEPRESCALER_8;
    } else if (clock_hz >= pclk / 16) {
        return SPI_BAUDRATEPRESCALER_16;
    } else if (clock_hz >= pclk / 32) {
        return SPI_BAUDRATEPRESCALER_32;
    } else if (clock_hz >= pclk / 64) {
        return SPI_BAUDRATEPRESCALER_64;
    } else if (clock_hz >= pclk / 128) {
        return SPI_BAUDRATEPRESCALER_128;
    } else {
        return SPI_BAUDRATEPRESCALER_256;
    }
}

/**
 * \brief           Map ST HAL status to Nexus HAL status
 */
static hal_status_t map_hal_status(HAL_StatusTypeDef status) {
    switch (status) {
        case HAL_OK:
            return HAL_OK;
        case HAL_BUSY:
            return HAL_ERROR_BUSY;
        case HAL_TIMEOUT:
            return HAL_ERROR_TIMEOUT;
        case HAL_ERROR:
        default:
            return HAL_ERROR;
    }
}

/**
 * \brief           Get NVIC IRQ number for SPI instance
 */
static IRQn_Type spi_get_irqn(hal_spi_instance_t instance) {
    switch (instance) {
        case HAL_SPI_0:
            return SPI1_IRQn;
        case HAL_SPI_1:
            return SPI2_IRQn;
        case HAL_SPI_2:
            return SPI3_IRQn;
        default:
            return SPI1_IRQn;
    }
}

/*===========================================================================*/
/* ST HAL MSP Functions (Clock and GPIO Configuration)                        */
/*===========================================================================*/

/**
 * \brief           SPI MSP Initialization
 * \note            This function is called by HAL_SPI_Init() to configure
 *                  clocks and GPIO pins for the SPI peripheral.
 * \param[in]       hspi: SPI handle pointer
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi) {
    GPIO_InitTypeDef gpio_init = {0};

    if (hspi->Instance == SPI1) {
        /* Enable SPI1 clock (APB2) */
        __HAL_RCC_SPI1_CLK_ENABLE();
        /* Enable GPIOA clock for SCK (PA5), MISO (PA6), MOSI (PA7) */
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* Configure SPI1 SCK (PA5) */
        gpio_init.Pin = GPIO_PIN_5;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        /* Configure SPI1 MISO (PA6) */
        gpio_init.Pin = GPIO_PIN_6;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        /* Configure SPI1 MOSI (PA7) */
        gpio_init.Pin = GPIO_PIN_7;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio_init);

    } else if (hspi->Instance == SPI2) {
        /* Enable SPI2 clock (APB1) */
        __HAL_RCC_SPI2_CLK_ENABLE();
        /* Enable GPIOB clock for SCK (PB13), MISO (PB14), MOSI (PB15) */
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* Configure SPI2 SCK (PB13) */
        gpio_init.Pin = GPIO_PIN_13;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF5_SPI2;
        HAL_GPIO_Init(GPIOB, &gpio_init);

        /* Configure SPI2 MISO (PB14) */
        gpio_init.Pin = GPIO_PIN_14;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &gpio_init);

        /* Configure SPI2 MOSI (PB15) */
        gpio_init.Pin = GPIO_PIN_15;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &gpio_init);

    } else if (hspi->Instance == SPI3) {
        /* Enable SPI3 clock (APB1) */
        __HAL_RCC_SPI3_CLK_ENABLE();
        /* Enable GPIOC clock for SCK (PC10), MISO (PC11), MOSI (PC12) */
        __HAL_RCC_GPIOC_CLK_ENABLE();

        /* Configure SPI3 SCK (PC10) */
        gpio_init.Pin = GPIO_PIN_10;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF6_SPI3;
        HAL_GPIO_Init(GPIOC, &gpio_init);

        /* Configure SPI3 MISO (PC11) */
        gpio_init.Pin = GPIO_PIN_11;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOC, &gpio_init);

        /* Configure SPI3 MOSI (PC12) */
        gpio_init.Pin = GPIO_PIN_12;
        gpio_init.Mode = GPIO_MODE_AF_PP;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOC, &gpio_init);
    }
}

/**
 * \brief           SPI MSP De-Initialization
 * \note            This function is called by HAL_SPI_DeInit() to release
 *                  resources used by the SPI peripheral.
 * \param[in]       hspi: SPI handle pointer
 */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi) {
    if (hspi->Instance == SPI1) {
        __HAL_RCC_SPI1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);
    } else if (hspi->Instance == SPI2) {
        __HAL_RCC_SPI2_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    } else if (hspi->Instance == SPI3) {
        __HAL_RCC_SPI3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
    }
}

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_spi_init(hal_spi_instance_t instance,
                          const hal_spi_config_t* config) {
    spi_data_t* data;
    HAL_StatusTypeDef status;
    uint32_t cpol, cpha;

    /* Parameter validation */
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (instance >= SPI_MAX_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    data = spi_get_data(instance);
    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Map SPI mode to CPOL/CPHA */
    map_spi_mode(config->mode, &cpol, &cpha);

    /* Configure ST HAL SPI_HandleTypeDef */
    data->hspi.Instance = spi_get_instance(instance);
    data->hspi.Init.Mode = map_role(config->role);
    data->hspi.Init.Direction = SPI_DIRECTION_2LINES;
    data->hspi.Init.DataSize = map_data_width(config->data_width);
    data->hspi.Init.CLKPolarity = cpol;
    data->hspi.Init.CLKPhase = cpha;
    data->hspi.Init.NSS = SPI_NSS_SOFT; /* Software CS control */
    data->hspi.Init.BaudRatePrescaler =
        calculate_prescaler(instance, config->clock_hz);
    data->hspi.Init.FirstBit = map_bit_order(config->bit_order);
    data->hspi.Init.TIMode = SPI_TIMODE_DISABLE;
    data->hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    data->hspi.Init.CRCPolynomial = 7;

    /* Call ST HAL SPI Init */
    status = HAL_SPI_Init(&data->hspi);
    if (status != HAL_OK) {
        return map_hal_status(status);
    }

    /* Store configuration */
    data->config = *config;
    data->tx_callback = NULL;
    data->rx_callback = NULL;
    data->tx_context = NULL;
    data->rx_context = NULL;
    data->cs_configured = false;
    data->initialized = true;

    return HAL_OK;
}

hal_status_t hal_spi_deinit(hal_spi_instance_t instance) {
    spi_data_t* data = spi_get_data(instance);
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Disable NVIC interrupt */
    HAL_NVIC_DisableIRQ(spi_get_irqn(instance));

    /* Call ST HAL SPI DeInit */
    status = HAL_SPI_DeInit(&data->hspi);
    if (status != HAL_OK) {
        return map_hal_status(status);
    }

    /* Clear state */
    data->initialized = false;
    data->tx_callback = NULL;
    data->rx_callback = NULL;
    data->tx_context = NULL;
    data->rx_context = NULL;
    data->cs_configured = false;

    return HAL_OK;
}

hal_status_t hal_spi_transmit(hal_spi_instance_t instance,
                              const uint8_t* tx_data, size_t len,
                              uint32_t timeout_ms) {
    spi_data_t* data = spi_get_data(instance);
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (tx_data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }
    if (len == 0) {
        return HAL_OK;
    }

    /* Use ST HAL blocking transmit */
    status = HAL_SPI_Transmit(&data->hspi, (uint8_t*)tx_data, (uint16_t)len,
                              timeout_ms);

    if (status != HAL_OK) {
        return map_hal_status(status);
    }

    return HAL_OK;
}

hal_status_t hal_spi_receive(hal_spi_instance_t instance, uint8_t* rx_data,
                             size_t len, uint32_t timeout_ms) {
    spi_data_t* data = spi_get_data(instance);
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (rx_data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }
    if (len == 0) {
        return HAL_OK;
    }

    /* Use ST HAL blocking receive */
    status = HAL_SPI_Receive(&data->hspi, rx_data, (uint16_t)len, timeout_ms);

    if (status != HAL_OK) {
        return map_hal_status(status);
    }

    return HAL_OK;
}

hal_status_t hal_spi_transfer(hal_spi_instance_t instance,
                              const uint8_t* tx_data, uint8_t* rx_data,
                              size_t len, uint32_t timeout_ms) {
    spi_data_t* data = spi_get_data(instance);
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (tx_data == NULL || rx_data == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }
    if (len == 0) {
        return HAL_OK;
    }

    /* Use ST HAL full-duplex transfer */
    status = HAL_SPI_TransmitReceive(&data->hspi, (uint8_t*)tx_data, rx_data,
                                     (uint16_t)len, timeout_ms);

    if (status != HAL_OK) {
        return map_hal_status(status);
    }

    return HAL_OK;
}

hal_status_t hal_spi_cs_control(hal_spi_instance_t instance, bool active) {
    spi_data_t* data = spi_get_data(instance);

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* If CS pin is not configured, configure default CS pins */
    if (!data->cs_configured) {
        GPIO_InitTypeDef gpio_init = {0};

        /* Configure default CS pins for each SPI instance */
        switch (instance) {
            case HAL_SPI_0:
                /* SPI1 CS: PA4 */
                __HAL_RCC_GPIOA_CLK_ENABLE();
                data->cs_port = GPIOA;
                data->cs_pin = GPIO_PIN_4;
                break;
            case HAL_SPI_1:
                /* SPI2 CS: PB12 */
                __HAL_RCC_GPIOB_CLK_ENABLE();
                data->cs_port = GPIOB;
                data->cs_pin = GPIO_PIN_12;
                break;
            case HAL_SPI_2:
                /* SPI3 CS: PA15 */
                __HAL_RCC_GPIOA_CLK_ENABLE();
                data->cs_port = GPIOA;
                data->cs_pin = GPIO_PIN_15;
                break;
            default:
                return HAL_ERROR_INVALID_PARAM;
        }

        /* Configure CS pin as output push-pull */
        gpio_init.Pin = data->cs_pin;
        gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(data->cs_port, &gpio_init);

        /* Set CS high (inactive) initially */
        HAL_GPIO_WritePin(data->cs_port, data->cs_pin, GPIO_PIN_SET);

        data->cs_configured = true;
    }

    /* Control CS pin: active=true means CS low (asserted) */
    if (active) {
        HAL_GPIO_WritePin(data->cs_port, data->cs_pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(data->cs_port, data->cs_pin, GPIO_PIN_SET);
    }

    return HAL_OK;
}

hal_status_t hal_spi_set_callback(hal_spi_instance_t instance,
                                  hal_spi_callback_t callback, void* context) {
    spi_data_t* data = spi_get_data(instance);

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Store both TX and RX callbacks (same callback for both) */
    data->tx_callback = callback;
    data->rx_callback = callback;
    data->tx_context = context;
    data->rx_context = context;

    if (callback != NULL) {
        /* Enable NVIC interrupt */
        HAL_NVIC_SetPriority(spi_get_irqn(instance), 5, 0);
        HAL_NVIC_EnableIRQ(spi_get_irqn(instance));
    } else {
        /* Disable NVIC interrupt */
        HAL_NVIC_DisableIRQ(spi_get_irqn(instance));
    }

    return HAL_OK;
}

/*===========================================================================*/
/* ST HAL Callback Implementations                                            */
/*===========================================================================*/

/**
 * \brief           ST HAL SPI TX Complete Callback
 * \note            Called by HAL_SPI_IRQHandler() when TX is complete
 * \param[in]       hspi: SPI handle pointer
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    /* Find which instance triggered the callback */
    for (hal_spi_instance_t i = 0; i < SPI_MAX_INSTANCES; i++) {
        spi_data_t* data = &spi_data[i];
        if (data->initialized && &data->hspi == hspi) {
            /* Invoke user callback */
            if (data->tx_callback != NULL) {
                data->tx_callback(i, data->tx_context);
            }
            break;
        }
    }
}

/**
 * \brief           ST HAL SPI RX Complete Callback
 * \note            Called by HAL_SPI_IRQHandler() when RX is complete
 * \param[in]       hspi: SPI handle pointer
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi) {
    /* Find which instance triggered the callback */
    for (hal_spi_instance_t i = 0; i < SPI_MAX_INSTANCES; i++) {
        spi_data_t* data = &spi_data[i];
        if (data->initialized && &data->hspi == hspi) {
            /* Invoke user callback */
            if (data->rx_callback != NULL) {
                data->rx_callback(i, data->rx_context);
            }
            break;
        }
    }
}

/**
 * \brief           ST HAL SPI TX/RX Complete Callback
 * \note            Called by HAL_SPI_IRQHandler() when full-duplex transfer
 *                  is complete
 * \param[in]       hspi: SPI handle pointer
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi) {
    /* Find which instance triggered the callback */
    for (hal_spi_instance_t i = 0; i < SPI_MAX_INSTANCES; i++) {
        spi_data_t* data = &spi_data[i];
        if (data->initialized && &data->hspi == hspi) {
            /* Invoke user callback (use TX callback for full-duplex) */
            if (data->tx_callback != NULL) {
                data->tx_callback(i, data->tx_context);
            }
            break;
        }
    }
}

/**
 * \brief           ST HAL SPI Error Callback
 * \note            Called by HAL_SPI_IRQHandler() when an error occurs
 * \param[in]       hspi: SPI handle pointer
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* hspi) {
    /* Find which instance triggered the callback */
    for (hal_spi_instance_t i = 0; i < SPI_MAX_INSTANCES; i++) {
        spi_data_t* data = &spi_data[i];
        if (data->initialized && &data->hspi == hspi) {
            /* Error handling - could add error callback in future */
            (void)data;
            break;
        }
    }
}

/*===========================================================================*/
/* IRQ Handlers - Using ST HAL SPI Handler                                    */
/*===========================================================================*/

/**
 * \brief           SPI1 IRQ Handler
 */
void SPI1_IRQHandler(void) {
    HAL_SPI_IRQHandler(&spi_data[HAL_SPI_0].hspi);
}

/**
 * \brief           SPI2 IRQ Handler
 */
void SPI2_IRQHandler(void) {
    HAL_SPI_IRQHandler(&spi_data[HAL_SPI_1].hspi);
}

/**
 * \brief           SPI3 IRQ Handler
 */
void SPI3_IRQHandler(void) {
    HAL_SPI_IRQHandler(&spi_data[HAL_SPI_2].hspi);
}
