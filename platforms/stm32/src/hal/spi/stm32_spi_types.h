/**
 * \file            stm32_spi_types.h
 * \brief           STM32 SPI driver type definitions
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef STM32_SPI_TYPES_H
#define STM32_SPI_TYPES_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_spi.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

/* Include ST HAL headers based on chip series */
#if defined(STM32F0xx) || defined(STM32F030x6) || defined(STM32F030x8) ||      \
    defined(STM32F030xC) || defined(STM32F031x6) || defined(STM32F038xx) ||    \
    defined(STM32F042x6) || defined(STM32F048xx) || defined(STM32F051x8) ||    \
    defined(STM32F058xx) || defined(STM32F070x6) || defined(STM32F070xB) ||    \
    defined(STM32F071xB) || defined(STM32F072xB) || defined(STM32F078xx) ||    \
    defined(STM32F091xC) || defined(STM32F098xx)
#include "stm32f0xx_hal.h"
#elif defined(STM32F1xx) || defined(STM32F100xB) || defined(STM32F100xE) ||    \
    defined(STM32F101x6) || defined(STM32F101xB) || defined(STM32F101xE) ||    \
    defined(STM32F101xG) || defined(STM32F102x6) || defined(STM32F102xB) ||    \
    defined(STM32F103x6) || defined(STM32F103xB) || defined(STM32F103xE) ||    \
    defined(STM32F103xG) || defined(STM32F105xC) || defined(STM32F107xC)
#include "stm32f1xx_hal.h"
#elif defined(STM32F2xx) || defined(STM32F205xx) || defined(STM32F215xx) ||    \
    defined(STM32F207xx) || defined(STM32F217xx)
#include "stm32f2xx_hal.h"
#elif defined(STM32F3xx) || defined(STM32F301x8) || defined(STM32F302x8) ||    \
    defined(STM32F302xC) || defined(STM32F302xE) || defined(STM32F303x8) ||    \
    defined(STM32F303xC) || defined(STM32F303xE) || defined(STM32F318xx) ||    \
    defined(STM32F328xx) || defined(STM32F334x8) || defined(STM32F358xx) ||    \
    defined(STM32F373xC) || defined(STM32F378xx) || defined(STM32F398xx)
#include "stm32f3xx_hal.h"
#elif defined(STM32F4xx) || defined(STM32F401xC) || defined(STM32F401xE) ||    \
    defined(STM32F405xx) || defined(STM32F407xx) || defined(STM32F410Cx) ||    \
    defined(STM32F410Rx) || defined(STM32F410Tx) || defined(STM32F411xE) ||    \
    defined(STM32F412Cx) || defined(STM32F412Rx) || defined(STM32F412Vx) ||    \
    defined(STM32F412Zx) || defined(STM32F413xx) || defined(STM32F415xx) ||    \
    defined(STM32F417xx) || defined(STM32F423xx) || defined(STM32F427xx) ||    \
    defined(STM32F429xx) || defined(STM32F437xx) || defined(STM32F439xx) ||    \
    defined(STM32F446xx) || defined(STM32F469xx) || defined(STM32F479xx)
#include "stm32f4xx_hal.h"
#elif defined(STM32F7xx) || defined(STM32F722xx) || defined(STM32F723xx) ||    \
    defined(STM32F730xx) || defined(STM32F732xx) || defined(STM32F733xx) ||    \
    defined(STM32F745xx) || defined(STM32F746xx) || defined(STM32F750xx) ||    \
    defined(STM32F756xx) || defined(STM32F765xx) || defined(STM32F767xx) ||    \
    defined(STM32F769xx) || defined(STM32F777xx) || defined(STM32F779xx)
#include "stm32f7xx_hal.h"
#elif defined(STM32G0xx) || defined(STM32G030xx) || defined(STM32G031xx) ||    \
    defined(STM32G041xx) || defined(STM32G050xx) || defined(STM32G051xx) ||    \
    defined(STM32G061xx) || defined(STM32G070xx) || defined(STM32G071xx) ||    \
    defined(STM32G081xx) || defined(STM32G0B0xx) || defined(STM32G0B1xx) ||    \
    defined(STM32G0C1xx)
#include "stm32g0xx_hal.h"
#elif defined(STM32G4xx) || defined(STM32G431xx) || defined(STM32G441xx) ||    \
    defined(STM32G471xx) || defined(STM32G473xx) || defined(STM32G474xx) ||    \
    defined(STM32G483xx) || defined(STM32G484xx) || defined(STM32G491xx) ||    \
    defined(STM32G4A1xx)
#include "stm32g4xx_hal.h"
#elif defined(STM32H5xx) || defined(STM32H503xx) || defined(STM32H523xx) ||    \
    defined(STM32H533xx) || defined(STM32H562xx) || defined(STM32H563xx) ||    \
    defined(STM32H573xx)
#include "stm32h5xx_hal.h"
#elif defined(STM32H7xx) || defined(STM32H723xx) || defined(STM32H725xx) ||    \
    defined(STM32H730xx) || defined(STM32H733xx) || defined(STM32H735xx) ||    \
    defined(STM32H742xx) || defined(STM32H743xx) || defined(STM32H745xx) ||    \
    defined(STM32H747xx) || defined(STM32H750xx) || defined(STM32H753xx) ||    \
    defined(STM32H755xx) || defined(STM32H757xx) || defined(STM32H7A3xx) ||    \
    defined(STM32H7A3xxQ) || defined(STM32H7B0xx) || defined(STM32H7B0xxQ) ||  \
    defined(STM32H7B3xx) || defined(STM32H7B3xxQ)
#include "stm32h7xx_hal.h"
#elif defined(STM32L0xx) || defined(STM32L010x4) || defined(STM32L010x6) ||    \
    defined(STM32L010x8) || defined(STM32L010xB) || defined(STM32L011xx) ||    \
    defined(STM32L021xx) || defined(STM32L031xx) || defined(STM32L041xx) ||    \
    defined(STM32L051xx) || defined(STM32L052xx) || defined(STM32L053xx) ||    \
    defined(STM32L061xx) || defined(STM32L062xx) || defined(STM32L063xx) ||    \
    defined(STM32L071xx) || defined(STM32L072xx) || defined(STM32L073xx) ||    \
    defined(STM32L081xx) || defined(STM32L082xx) || defined(STM32L083xx)
#include "stm32l0xx_hal.h"
#elif defined(STM32L1xx) || defined(STM32L100xB) || defined(STM32L100xBA) ||   \
    defined(STM32L100xC) || defined(STM32L151xB) || defined(STM32L151xBA) ||   \
    defined(STM32L151xC) || defined(STM32L151xCA) || defined(STM32L151xD) ||   \
    defined(STM32L151xDX) || defined(STM32L151xE) || defined(STM32L152xB) ||   \
    defined(STM32L152xBA) || defined(STM32L152xC) || defined(STM32L152xCA) ||  \
    defined(STM32L152xD) || defined(STM32L152xDX) || defined(STM32L152xE) ||   \
    defined(STM32L162xC) || defined(STM32L162xCA) || defined(STM32L162xD) ||   \
    defined(STM32L162xDX) || defined(STM32L162xE)
#include "stm32l1xx_hal.h"
#elif defined(STM32L4xx) || defined(STM32L412xx) || defined(STM32L422xx) ||    \
    defined(STM32L431xx) || defined(STM32L432xx) || defined(STM32L433xx) ||    \
    defined(STM32L442xx) || defined(STM32L443xx) || defined(STM32L451xx) ||    \
    defined(STM32L452xx) || defined(STM32L462xx) || defined(STM32L471xx) ||    \
    defined(STM32L475xx) || defined(STM32L476xx) || defined(STM32L485xx) ||    \
    defined(STM32L486xx) || defined(STM32L496xx) || defined(STM32L4A6xx) ||    \
    defined(STM32L4P5xx) || defined(STM32L4Q5xx) || defined(STM32L4R5xx) ||    \
    defined(STM32L4R7xx) || defined(STM32L4R9xx) || defined(STM32L4S5xx) ||    \
    defined(STM32L4S7xx) || defined(STM32L4S9xx)
#include "stm32l4xx_hal.h"
#elif defined(STM32L5xx) || defined(STM32L552xx) || defined(STM32L562xx)
#include "stm32l5xx_hal.h"
#elif defined(STM32U0xx) || defined(STM32U031xx) || defined(STM32U073xx) ||    \
    defined(STM32U083xx)
#include "stm32u0xx_hal.h"
#elif defined(STM32U3xx) || defined(STM32U535xx) || defined(STM32U545xx) ||    \
    defined(STM32U575xx) || defined(STM32U585xx) || defined(STM32U595xx) ||    \
    defined(STM32U599xx) || defined(STM32U5A5xx) || defined(STM32U5A9xx) ||    \
    defined(STM32U5F7xx) || defined(STM32U5F9xx) || defined(STM32U5G7xx) ||    \
    defined(STM32U5G9xx)
#include "stm32u3xx_hal.h"
#elif defined(STM32U5xx) || defined(STM32U535xx) || defined(STM32U545xx) ||    \
    defined(STM32U575xx) || defined(STM32U585xx) || defined(STM32U595xx) ||    \
    defined(STM32U599xx) || defined(STM32U5A5xx) || defined(STM32U5A9xx) ||    \
    defined(STM32U5F7xx) || defined(STM32U5F9xx) || defined(STM32U5G7xx) ||    \
    defined(STM32U5G9xx)
#include "stm32u5xx_hal.h"
#elif defined(STM32WBxx) || defined(STM32WB10xx) || defined(STM32WB15xx) ||    \
    defined(STM32WB1Mxx) || defined(STM32WB30xx) || defined(STM32WB35xx) ||    \
    defined(STM32WB50xx) || defined(STM32WB55xx) || defined(STM32WB5Mxx)
#include "stm32wbxx_hal.h"
#elif defined(STM32WLxx) || defined(STM32WL54xx) || defined(STM32WL55xx) ||    \
    defined(STM32WLE4xx) || defined(STM32WLE5xx)
#include "stm32wlxx_hal.h"
#else
#error "Unsupported STM32 series"
#endif

/* Include OSAL headers if enabled */
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
#include "osal/osal_mutex.h"
#include "osal/osal_sem.h"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Forward Declarations                                                      */
/*---------------------------------------------------------------------------*/

/* Forward declare device type */
typedef struct nx_device_s nx_device_t;

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct stm32_spi_platform_config_s {
    SPI_TypeDef* spi_base;    /**< SPI base address */
    uint8_t spi_index;        /**< SPI instance index */
    uint32_t mode;            /**< SPI mode (Master/Slave) */
    uint32_t direction;       /**< SPI direction (Full Duplex/Half Duplex) */
    uint32_t data_size;       /**< Data size (8-bit/16-bit) */
    uint32_t clk_polarity;    /**< Clock polarity (CPOL) */
    uint32_t clk_phase;       /**< Clock phase (CPHA) */
    uint32_t nss;             /**< NSS management (Software/Hardware) */
    uint32_t baud_prescaler;  /**< Baud rate prescaler */
    uint32_t first_bit;       /**< First bit (MSB/LSB) */
    uint32_t ti_mode;         /**< TI mode enable */
    uint32_t crc_calculation; /**< CRC calculation enable */
    uint32_t crc_polynomial;  /**< CRC polynomial */
    bool use_osal;            /**< Use OSAL for synchronization */
    bool use_dma;             /**< Use DMA for transfers */
} stm32_spi_platform_config_t;

/*---------------------------------------------------------------------------*/
/* SPI State Structure                                                       */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct stm32_spi_state_s {
    uint8_t instance; /**< Instance index */
    bool initialized; /**< Initialization flag */
    bool suspended;   /**< Suspend flag */
    bool busy;        /**< Busy flag */
} stm32_spi_state_t;

/*---------------------------------------------------------------------------*/
/* DMA Support Structure                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           SPI DMA configuration structure
 */
typedef struct stm32_spi_dma_s {
    DMA_HandleTypeDef hdma_tx; /**< TX DMA handle */
    DMA_HandleTypeDef hdma_rx; /**< RX DMA handle */
    bool dma_tx_enabled;       /**< DMA TX enable flag */
    bool dma_rx_enabled;       /**< DMA RX enable flag */
} stm32_spi_dma_t;

/*---------------------------------------------------------------------------*/
/* SPI Implementation Structure                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           STM32 SPI implementation structure
 *
 * Contains all interfaces, ST HAL handle, and optional OSAL objects.
 */
typedef struct stm32_spi_impl_s {
    nx_spi_bus_t base;                     /**< Base SPI bus interface */
    nx_tx_async_t tx_async;                /**< TX async interface */
    nx_tx_rx_async_t tx_rx_async;          /**< TX/RX async interface */
    nx_tx_sync_t tx_sync;                  /**< TX sync interface */
    nx_tx_rx_sync_t tx_rx_sync;            /**< TX/RX sync interface */
    nx_lifecycle_t lifecycle;              /**< Lifecycle interface */
    nx_power_t power;                      /**< Power interface */
    SPI_HandleTypeDef hspi;                /**< ST HAL SPI handle */
    stm32_spi_state_t* state;              /**< State pointer */
    nx_device_t* device;                   /**< Device descriptor */
    stm32_spi_dma_t dma;                   /**< DMA configuration */
    nx_spi_device_config_t current_config; /**< Current device config */

    /* Optional OSAL synchronization objects */
#ifdef NX_CONFIG_STM32_SPI_USE_OSAL
    osal_mutex_handle_t mutex; /**< Mutex for thread safety */
    osal_sem_handle_t dma_sem; /**< DMA completion semaphore */
#endif
} stm32_spi_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* STM32_SPI_TYPES_H */
