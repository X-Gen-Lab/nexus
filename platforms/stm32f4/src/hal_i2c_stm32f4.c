/**
 * \file            hal_i2c_stm32f4.c
 * \brief           STM32F4 I2C HAL Implementation (ST HAL Wrapper)
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-15
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * This implementation wraps ST HAL I2C functions to provide the Nexus HAL
 * interface. It uses HAL_I2C_Init(), HAL_I2C_Master_Transmit(),
 * HAL_I2C_Master_Receive(), HAL_I2C_Mem_Write(), HAL_I2C_Mem_Read(),
 * HAL_I2C_IsDeviceReady(), and HAL_I2C_IRQHandler() from the ST HAL library.
 */

#include "hal/hal_i2c.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"

/*===========================================================================*/
/* Local definitions                                                          */
/*===========================================================================*/

/**
 * \brief           Maximum number of I2C instances
 */
#define I2C_MAX_INSTANCES 3

/**
 * \brief           Default timeout for operations (ms)
 */
#define I2C_DEFAULT_TIMEOUT 1000

/**
 * \brief           I2C event types for callback
 */
#define I2C_EVENT_TX_COMPLETE 0x01
#define I2C_EVENT_RX_COMPLETE 0x02
#define I2C_EVENT_ERROR       0x04

/**
 * \brief           I2C instance data structure - wraps ST HAL Handle
 */
typedef struct {
    I2C_HandleTypeDef hi2c;      /**< ST HAL I2C Handle */
    hal_i2c_config_t config;     /**< Nexus configuration */
    hal_i2c_callback_t callback; /**< Event callback */
    void* context;               /**< Callback context */
    bool initialized;            /**< Initialization flag */
} i2c_data_t;

/**
 * \brief           I2C instance data array
 */
static i2c_data_t i2c_data[I2C_MAX_INSTANCES];

/*===========================================================================*/
/* Local functions                                                            */
/*===========================================================================*/

/**
 * \brief           Get I2C data by instance
 * \param[in]       instance: I2C instance
 * \return          Pointer to I2C data or NULL
 */
static i2c_data_t* i2c_get_data(hal_i2c_instance_t instance) {
    if (instance >= I2C_MAX_INSTANCES) {
        return NULL;
    }
    return &i2c_data[instance];
}

/**
 * \brief           Get I2C peripheral pointer by instance
 * \param[in]       instance: I2C instance
 * \return          I2C peripheral pointer
 */
static I2C_TypeDef* i2c_get_instance(hal_i2c_instance_t instance) {
    switch (instance) {
        case HAL_I2C_0:
            return I2C1;
        case HAL_I2C_1:
            return I2C2;
        case HAL_I2C_2:
            return I2C3;
        default:
            return NULL;
    }
}

/**
 * \brief           Map Nexus I2C speed to ST HAL clock speed
 * \param[in]       speed: Nexus I2C speed mode
 * \return          Clock speed in Hz
 */
static uint32_t map_speed(hal_i2c_speed_t speed) {
    switch (speed) {
        case HAL_I2C_SPEED_STANDARD:
            return 100000; /* 100 kHz */
        case HAL_I2C_SPEED_FAST:
            return 400000; /* 400 kHz */
        case HAL_I2C_SPEED_FAST_PLUS:
            return 1000000; /* 1 MHz */
        default:
            return 100000;
    }
}

/**
 * \brief           Map Nexus address mode to ST HAL addressing mode
 * \param[in]       addr_mode: Nexus address mode
 * \return          ST HAL addressing mode
 */
static uint32_t map_addr_mode(hal_i2c_addr_mode_t addr_mode) {
    switch (addr_mode) {
        case HAL_I2C_ADDR_10BIT:
            return I2C_ADDRESSINGMODE_10BIT;
        case HAL_I2C_ADDR_7BIT:
        default:
            return I2C_ADDRESSINGMODE_7BIT;
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
 * \brief           Map ST HAL I2C error to Nexus HAL error
 * \param[in]       error: ST HAL I2C error code
 * \return          Nexus HAL error code
 */
static hal_status_t map_i2c_error(uint32_t error) {
    if (error & HAL_I2C_ERROR_AF) {
        return HAL_ERROR_IO; /* NACK received */
    }
    if (error & HAL_I2C_ERROR_BERR) {
        return HAL_ERROR_IO; /* Bus error */
    }
    if (error & HAL_I2C_ERROR_ARLO) {
        return HAL_ERROR_IO; /* Arbitration lost */
    }
    if (error & HAL_I2C_ERROR_OVR) {
        return HAL_ERROR_OVERRUN;
    }
    if (error & HAL_I2C_ERROR_TIMEOUT) {
        return HAL_ERROR_TIMEOUT;
    }
    return HAL_ERROR_IO;
}

/**
 * \brief           Get NVIC IRQ number for I2C event interrupt
 */
static IRQn_Type i2c_get_ev_irqn(hal_i2c_instance_t instance) {
    switch (instance) {
        case HAL_I2C_0:
            return I2C1_EV_IRQn;
        case HAL_I2C_1:
            return I2C2_EV_IRQn;
        case HAL_I2C_2:
            return I2C3_EV_IRQn;
        default:
            return I2C1_EV_IRQn;
    }
}

/**
 * \brief           Get NVIC IRQ number for I2C error interrupt
 */
static IRQn_Type i2c_get_er_irqn(hal_i2c_instance_t instance) {
    switch (instance) {
        case HAL_I2C_0:
            return I2C1_ER_IRQn;
        case HAL_I2C_1:
            return I2C2_ER_IRQn;
        case HAL_I2C_2:
            return I2C3_ER_IRQn;
        default:
            return I2C1_ER_IRQn;
    }
}

/*===========================================================================*/
/* ST HAL MSP Functions (Clock and GPIO Configuration)                        */
/*===========================================================================*/

/**
 * \brief           I2C MSP Initialization
 * \note            This function is called by HAL_I2C_Init() to configure
 *                  clocks and GPIO pins for the I2C peripheral.
 * \param[in]       hi2c: I2C handle pointer
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c) {
    GPIO_InitTypeDef gpio_init = {0};

    if (hi2c->Instance == I2C1) {
        /* Enable I2C1 clock (APB1) */
        __HAL_RCC_I2C1_CLK_ENABLE();
        /* Enable GPIOB clock for SCL (PB6) and SDA (PB7) */
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* Configure I2C1 SCL (PB6) */
        gpio_init.Pin = GPIO_PIN_6;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Pull = GPIO_PULLUP;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &gpio_init);

        /* Configure I2C1 SDA (PB7) */
        gpio_init.Pin = GPIO_PIN_7;
        HAL_GPIO_Init(GPIOB, &gpio_init);

    } else if (hi2c->Instance == I2C2) {
        /* Enable I2C2 clock (APB1) */
        __HAL_RCC_I2C2_CLK_ENABLE();
        /* Enable GPIOB clock for SCL (PB10) and SDA (PB11) */
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* Configure I2C2 SCL (PB10) */
        gpio_init.Pin = GPIO_PIN_10;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Pull = GPIO_PULLUP;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOB, &gpio_init);

        /* Configure I2C2 SDA (PB11) */
        gpio_init.Pin = GPIO_PIN_11;
        HAL_GPIO_Init(GPIOB, &gpio_init);

    } else if (hi2c->Instance == I2C3) {
        /* Enable I2C3 clock (APB1) */
        __HAL_RCC_I2C3_CLK_ENABLE();
        /* Enable GPIOA clock for SCL (PA8) */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        /* Enable GPIOC clock for SDA (PC9) */
        __HAL_RCC_GPIOC_CLK_ENABLE();

        /* Configure I2C3 SCL (PA8) */
        gpio_init.Pin = GPIO_PIN_8;
        gpio_init.Mode = GPIO_MODE_AF_OD;
        gpio_init.Pull = GPIO_PULLUP;
        gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init.Alternate = GPIO_AF4_I2C3;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        /* Configure I2C3 SDA (PC9) */
        gpio_init.Pin = GPIO_PIN_9;
        HAL_GPIO_Init(GPIOC, &gpio_init);
    }
}

/**
 * \brief           I2C MSP De-Initialization
 * \note            This function is called by HAL_I2C_DeInit() to release
 *                  resources used by the I2C peripheral.
 * \param[in]       hi2c: I2C handle pointer
 */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c) {
    if (hi2c->Instance == I2C1) {
        __HAL_RCC_I2C1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);
    } else if (hi2c->Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
    } else if (hi2c->Instance == I2C3) {
        __HAL_RCC_I2C3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_8);
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_9);
    }
}

/*===========================================================================*/
/* Public functions                                                           */
/*===========================================================================*/

hal_status_t hal_i2c_init(hal_i2c_instance_t instance,
                          const hal_i2c_config_t* config) {
    i2c_data_t* data;
    HAL_StatusTypeDef status;

    /* Parameter validation */
    if (config == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (instance >= I2C_MAX_INSTANCES) {
        return HAL_ERROR_INVALID_PARAM;
    }

    data = i2c_get_data(instance);
    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }

    /* Configure ST HAL I2C_HandleTypeDef */
    data->hi2c.Instance = i2c_get_instance(instance);
    data->hi2c.Init.ClockSpeed = map_speed(config->speed);
    data->hi2c.Init.DutyCycle = I2C_DUTYCYCLE_2;
    data->hi2c.Init.OwnAddress1 = config->own_addr;
    data->hi2c.Init.AddressingMode = map_addr_mode(config->addr_mode);
    data->hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    data->hi2c.Init.OwnAddress2 = 0;
    data->hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    data->hi2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    /* Call ST HAL I2C Init */
    status = HAL_I2C_Init(&data->hi2c);
    if (status != HAL_OK) {
        return map_hal_status(status);
    }

    /* Store configuration */
    data->config = *config;
    data->callback = NULL;
    data->context = NULL;
    data->initialized = true;

    return HAL_OK;
}

hal_status_t hal_i2c_deinit(hal_i2c_instance_t instance) {
    i2c_data_t* data = i2c_get_data(instance);
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Disable NVIC interrupts */
    HAL_NVIC_DisableIRQ(i2c_get_ev_irqn(instance));
    HAL_NVIC_DisableIRQ(i2c_get_er_irqn(instance));

    /* Call ST HAL I2C DeInit */
    status = HAL_I2C_DeInit(&data->hi2c);
    if (status != HAL_OK) {
        return map_hal_status(status);
    }

    /* Clear state */
    data->initialized = false;
    data->callback = NULL;
    data->context = NULL;

    return HAL_OK;
}

hal_status_t hal_i2c_master_transmit(hal_i2c_instance_t instance,
                                     uint16_t dev_addr, const uint8_t* data_buf,
                                     size_t len, uint32_t timeout_ms) {
    i2c_data_t* data = i2c_get_data(instance);
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data_buf == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }
    if (len == 0) {
        return HAL_OK;
    }

    /* Use ST HAL blocking master transmit */
    /* Note: ST HAL expects 7-bit address shifted left by 1 */
    status =
        HAL_I2C_Master_Transmit(&data->hi2c, (uint16_t)(dev_addr << 1),
                                (uint8_t*)data_buf, (uint16_t)len, timeout_ms);

    if (status != HAL_OK) {
        /* Check for specific I2C errors */
        uint32_t error = HAL_I2C_GetError(&data->hi2c);
        if (error != HAL_I2C_ERROR_NONE) {
            return map_i2c_error(error);
        }
        return map_hal_status(status);
    }

    return HAL_OK;
}

hal_status_t hal_i2c_master_receive(hal_i2c_instance_t instance,
                                    uint16_t dev_addr, uint8_t* data_buf,
                                    size_t len, uint32_t timeout_ms) {
    i2c_data_t* data = i2c_get_data(instance);
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data_buf == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }
    if (len == 0) {
        return HAL_OK;
    }

    /* Use ST HAL blocking master receive */
    /* Note: ST HAL expects 7-bit address shifted left by 1 */
    status = HAL_I2C_Master_Receive(&data->hi2c, (uint16_t)(dev_addr << 1),
                                    data_buf, (uint16_t)len, timeout_ms);

    if (status != HAL_OK) {
        /* Check for specific I2C errors */
        uint32_t error = HAL_I2C_GetError(&data->hi2c);
        if (error != HAL_I2C_ERROR_NONE) {
            return map_i2c_error(error);
        }
        return map_hal_status(status);
    }

    return HAL_OK;
}

hal_status_t hal_i2c_mem_write(hal_i2c_instance_t instance, uint16_t dev_addr,
                               uint16_t mem_addr, uint8_t mem_addr_size,
                               const uint8_t* data_buf, size_t len,
                               uint32_t timeout_ms) {
    i2c_data_t* data = i2c_get_data(instance);
    HAL_StatusTypeDef status;
    uint16_t mem_size;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data_buf == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }
    if (mem_addr_size != 1 && mem_addr_size != 2) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (len == 0) {
        return HAL_OK;
    }

    /* Map memory address size */
    mem_size =
        (mem_addr_size == 1) ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT;

    /* Use ST HAL memory write */
    /* Note: ST HAL expects 7-bit address shifted left by 1 */
    status = HAL_I2C_Mem_Write(&data->hi2c, (uint16_t)(dev_addr << 1), mem_addr,
                               mem_size, (uint8_t*)data_buf, (uint16_t)len,
                               timeout_ms);

    if (status != HAL_OK) {
        /* Check for specific I2C errors */
        uint32_t error = HAL_I2C_GetError(&data->hi2c);
        if (error != HAL_I2C_ERROR_NONE) {
            return map_i2c_error(error);
        }
        return map_hal_status(status);
    }

    return HAL_OK;
}

hal_status_t hal_i2c_mem_read(hal_i2c_instance_t instance, uint16_t dev_addr,
                              uint16_t mem_addr, uint8_t mem_addr_size,
                              uint8_t* data_buf, size_t len,
                              uint32_t timeout_ms) {
    i2c_data_t* data = i2c_get_data(instance);
    HAL_StatusTypeDef status;
    uint16_t mem_size;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (data_buf == NULL) {
        return HAL_ERROR_NULL_POINTER;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }
    if (mem_addr_size != 1 && mem_addr_size != 2) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (len == 0) {
        return HAL_OK;
    }

    /* Map memory address size */
    mem_size =
        (mem_addr_size == 1) ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT;

    /* Use ST HAL memory read */
    /* Note: ST HAL expects 7-bit address shifted left by 1 */
    status = HAL_I2C_Mem_Read(&data->hi2c, (uint16_t)(dev_addr << 1), mem_addr,
                              mem_size, data_buf, (uint16_t)len, timeout_ms);

    if (status != HAL_OK) {
        /* Check for specific I2C errors */
        uint32_t error = HAL_I2C_GetError(&data->hi2c);
        if (error != HAL_I2C_ERROR_NONE) {
            return map_i2c_error(error);
        }
        return map_hal_status(status);
    }

    return HAL_OK;
}

hal_status_t hal_i2c_is_device_ready(hal_i2c_instance_t instance,
                                     uint16_t dev_addr, uint8_t retries,
                                     uint32_t timeout_ms) {
    i2c_data_t* data = i2c_get_data(instance);
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    /* Use ST HAL device ready check */
    /* Note: ST HAL expects 7-bit address shifted left by 1 */
    status = HAL_I2C_IsDeviceReady(&data->hi2c, (uint16_t)(dev_addr << 1),
                                   (uint32_t)retries, timeout_ms);

    if (status != HAL_OK) {
        /* Check for specific I2C errors */
        uint32_t error = HAL_I2C_GetError(&data->hi2c);
        if (error & HAL_I2C_ERROR_AF) {
            return HAL_ERROR_IO; /* NACK - device not responding */
        }
        return map_hal_status(status);
    }

    return HAL_OK;
}

hal_status_t hal_i2c_set_callback(hal_i2c_instance_t instance,
                                  hal_i2c_callback_t callback, void* context) {
    i2c_data_t* data = i2c_get_data(instance);

    if (data == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    if (!data->initialized) {
        return HAL_ERROR_NOT_INIT;
    }

    data->callback = callback;
    data->context = context;

    if (callback != NULL) {
        /* Enable NVIC interrupts for I2C events and errors */
        HAL_NVIC_SetPriority(i2c_get_ev_irqn(instance), 5, 0);
        HAL_NVIC_EnableIRQ(i2c_get_ev_irqn(instance));
        HAL_NVIC_SetPriority(i2c_get_er_irqn(instance), 5, 0);
        HAL_NVIC_EnableIRQ(i2c_get_er_irqn(instance));
    } else {
        /* Disable NVIC interrupts */
        HAL_NVIC_DisableIRQ(i2c_get_ev_irqn(instance));
        HAL_NVIC_DisableIRQ(i2c_get_er_irqn(instance));
    }

    return HAL_OK;
}

/*===========================================================================*/
/* ST HAL Callback Implementations                                            */
/*===========================================================================*/

/**
 * \brief           ST HAL I2C Master TX Complete Callback
 * \note            Called by HAL_I2C_EV_IRQHandler() when TX is complete
 * \param[in]       hi2c: I2C handle pointer
 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef* hi2c) {
    /* Find which instance triggered the callback */
    for (hal_i2c_instance_t i = 0; i < I2C_MAX_INSTANCES; i++) {
        i2c_data_t* data = &i2c_data[i];
        if (data->initialized && &data->hi2c == hi2c) {
            /* Invoke user callback */
            if (data->callback != NULL) {
                data->callback(i, I2C_EVENT_TX_COMPLETE, data->context);
            }
            break;
        }
    }
}

/**
 * \brief           ST HAL I2C Master RX Complete Callback
 * \note            Called by HAL_I2C_EV_IRQHandler() when RX is complete
 * \param[in]       hi2c: I2C handle pointer
 */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef* hi2c) {
    /* Find which instance triggered the callback */
    for (hal_i2c_instance_t i = 0; i < I2C_MAX_INSTANCES; i++) {
        i2c_data_t* data = &i2c_data[i];
        if (data->initialized && &data->hi2c == hi2c) {
            /* Invoke user callback */
            if (data->callback != NULL) {
                data->callback(i, I2C_EVENT_RX_COMPLETE, data->context);
            }
            break;
        }
    }
}

/**
 * \brief           ST HAL I2C Memory TX Complete Callback
 * \note            Called by HAL_I2C_EV_IRQHandler() when memory TX is complete
 * \param[in]       hi2c: I2C handle pointer
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef* hi2c) {
    /* Find which instance triggered the callback */
    for (hal_i2c_instance_t i = 0; i < I2C_MAX_INSTANCES; i++) {
        i2c_data_t* data = &i2c_data[i];
        if (data->initialized && &data->hi2c == hi2c) {
            /* Invoke user callback */
            if (data->callback != NULL) {
                data->callback(i, I2C_EVENT_TX_COMPLETE, data->context);
            }
            break;
        }
    }
}

/**
 * \brief           ST HAL I2C Memory RX Complete Callback
 * \note            Called by HAL_I2C_EV_IRQHandler() when memory RX is complete
 * \param[in]       hi2c: I2C handle pointer
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef* hi2c) {
    /* Find which instance triggered the callback */
    for (hal_i2c_instance_t i = 0; i < I2C_MAX_INSTANCES; i++) {
        i2c_data_t* data = &i2c_data[i];
        if (data->initialized && &data->hi2c == hi2c) {
            /* Invoke user callback */
            if (data->callback != NULL) {
                data->callback(i, I2C_EVENT_RX_COMPLETE, data->context);
            }
            break;
        }
    }
}

/**
 * \brief           ST HAL I2C Error Callback
 * \note            Called by HAL_I2C_ER_IRQHandler() when an error occurs
 * \param[in]       hi2c: I2C handle pointer
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* hi2c) {
    /* Find which instance triggered the callback */
    for (hal_i2c_instance_t i = 0; i < I2C_MAX_INSTANCES; i++) {
        i2c_data_t* data = &i2c_data[i];
        if (data->initialized && &data->hi2c == hi2c) {
            /* Invoke user callback with error event */
            if (data->callback != NULL) {
                data->callback(i, I2C_EVENT_ERROR, data->context);
            }
            break;
        }
    }
}

/*===========================================================================*/
/* IRQ Handlers - Using ST HAL I2C Handler                                    */
/*===========================================================================*/

/**
 * \brief           I2C1 Event IRQ Handler
 */
void I2C1_EV_IRQHandler(void) {
    HAL_I2C_EV_IRQHandler(&i2c_data[HAL_I2C_0].hi2c);
}

/**
 * \brief           I2C1 Error IRQ Handler
 */
void I2C1_ER_IRQHandler(void) {
    HAL_I2C_ER_IRQHandler(&i2c_data[HAL_I2C_0].hi2c);
}

/**
 * \brief           I2C2 Event IRQ Handler
 */
void I2C2_EV_IRQHandler(void) {
    HAL_I2C_EV_IRQHandler(&i2c_data[HAL_I2C_1].hi2c);
}

/**
 * \brief           I2C2 Error IRQ Handler
 */
void I2C2_ER_IRQHandler(void) {
    HAL_I2C_ER_IRQHandler(&i2c_data[HAL_I2C_1].hi2c);
}

/**
 * \brief           I2C3 Event IRQ Handler
 */
void I2C3_EV_IRQHandler(void) {
    HAL_I2C_EV_IRQHandler(&i2c_data[HAL_I2C_2].hi2c);
}

/**
 * \brief           I2C3 Error IRQ Handler
 */
void I2C3_ER_IRQHandler(void) {
    HAL_I2C_ER_IRQHandler(&i2c_data[HAL_I2C_2].hi2c);
}
