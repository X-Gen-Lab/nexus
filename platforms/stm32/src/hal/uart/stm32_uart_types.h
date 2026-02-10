/**
 * \file            stm32_uart_types.h
 * \brief           STM32 UART driver type definitions
 * \author          Nexus Team
 */

/*
 * Copyright (c) 2026 Nexus Team
 */

#ifndef STM32_UART_TYPES_H
#define STM32_UART_TYPES_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/interface/nx_uart.h"
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
#ifdef NX_CONFIG_STM32_UART_USE_OSAL
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

struct nx_device_s; /* Forward declaration only */

/*---------------------------------------------------------------------------*/
/* Peripheral Type Detection                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART peripheral type enumeration
 */
typedef enum {
    UART_PERIPH_USART = 0, /**< USART peripheral (sync/async) */
    UART_PERIPH_UART = 1,  /**< UART peripheral (async only) */
    UART_PERIPH_LPUART = 2 /**< LPUART peripheral (low power) */
} uart_periph_type_t;

/**
 * \brief           Check if instance is USART peripheral
 * \note            Undefine ST HAL macros to avoid conflicts
 */
#ifdef IS_USART_INSTANCE
#undef IS_USART_INSTANCE
#endif

#if defined(USART1)
#define IS_USART_INSTANCE(instance)                                            \
    (((instance) == USART1) || ((instance) == USART2)                          \
                                   _NX_USART3_CHECK _NX_USART6_CHECK)

#ifdef USART3
#define _NX_USART3_CHECK || ((instance) == USART3)
#else
#define _NX_USART3_CHECK
#endif

#ifdef USART6
#define _NX_USART6_CHECK || ((instance) == USART6)
#else
#define _NX_USART6_CHECK
#endif

#else
#define IS_USART_INSTANCE(instance) (0)
#endif

/**
 * \brief           Check if instance is UART peripheral
 * \note            Undefine ST HAL macros to avoid conflicts
 */
#ifdef IS_UART_INSTANCE
#undef IS_UART_INSTANCE
#endif

#if defined(UART4)
#define IS_UART_INSTANCE(instance)                                             \
    (0 _NX_UART4_CHECK _NX_UART5_CHECK _NX_UART7_CHECK _NX_UART8_CHECK         \
         _NX_UART9_CHECK _NX_UART10_CHECK)

#ifdef UART4
#define _NX_UART4_CHECK || ((instance) == UART4)
#else
#define _NX_UART4_CHECK
#endif

#ifdef UART5
#define _NX_UART5_CHECK || ((instance) == UART5)
#else
#define _NX_UART5_CHECK
#endif

#ifdef UART7
#define _NX_UART7_CHECK || ((instance) == UART7)
#else
#define _NX_UART7_CHECK
#endif

#ifdef UART8
#define _NX_UART8_CHECK || ((instance) == UART8)
#else
#define _NX_UART8_CHECK
#endif

#ifdef UART9
#define _NX_UART9_CHECK || ((instance) == UART9)
#else
#define _NX_UART9_CHECK
#endif

#ifdef UART10
#define _NX_UART10_CHECK || ((instance) == UART10)
#else
#define _NX_UART10_CHECK
#endif

#else
#define IS_UART_INSTANCE(instance) (0)
#endif

/**
 * \brief           Check if instance is LPUART peripheral
 */
#if defined(LPUART1)
#define IS_LPUART_INSTANCE(instance)                                           \
    ((defined(LPUART1) && ((instance) == LPUART1)) ||                          \
     (defined(LPUART2) && ((instance) == LPUART2)))
#else
#define IS_LPUART_INSTANCE(instance) (0)
#endif

/*---------------------------------------------------------------------------*/
/* Platform Configuration Structure                                          */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART platform configuration structure
 *
 * Contains compile-time configuration from Kconfig.
 */
typedef struct stm32_uart_platform_config_s {
    USART_TypeDef* usart_base; /**< UART/USART base address */
    uint8_t uart_index;        /**< UART instance index */
    uint32_t baudrate;         /**< Baud rate */
    uint32_t word_length;      /**< Word length (data bits) */
    uint32_t stop_bits;        /**< Stop bits */
    uint32_t parity;           /**< Parity setting */
    uint32_t mode;             /**< UART mode (TX/RX/TX_RX) */
    uint32_t hw_flow_ctl;      /**< Hardware flow control */
    uint32_t over_sampling;    /**< Oversampling mode */
    bool use_osal;             /**< Use OSAL for synchronization */
    bool use_dma;              /**< Use DMA for transfers */
    size_t tx_buf_size;        /**< TX buffer size (0 = disabled) */
    size_t rx_buf_size;        /**< RX buffer size (0 = disabled) */
} stm32_uart_platform_config_t;

/*---------------------------------------------------------------------------*/
/* Circular Buffer Structure                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Buffer overflow policy
 */
typedef enum {
    UART_OVERFLOW_DROP_OLD = 0, /**< Drop old data (overwrite) */
    UART_OVERFLOW_DROP_NEW = 1, /**< Drop new data (reject) */
    UART_OVERFLOW_ERROR = 2,    /**< Return error */
} uart_overflow_policy_t;

/**
 * \brief           Circular buffer structure
 */
typedef struct stm32_uart_buffer_s {
    uint8_t* data;                 /**< Buffer data pointer */
    size_t size;                   /**< Buffer size */
    size_t head;                   /**< Write position */
    size_t tail;                   /**< Read position */
    size_t count;                  /**< Number of bytes in buffer */
    uart_overflow_policy_t policy; /**< Overflow policy */
    size_t high_watermark;         /**< High watermark threshold */
    size_t low_watermark;          /**< Low watermark threshold */
    size_t peak_usage;             /**< Peak buffer usage */
    uint32_t overflow_count;       /**< Overflow event count */
    bool high_water_flag;          /**< High watermark reached */
    bool low_water_flag;           /**< Low watermark reached */
} stm32_uart_buffer_t;

/*---------------------------------------------------------------------------*/
/* UART Configuration Structure                                              */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART runtime configuration structure
 */
typedef struct stm32_uart_config_s {
    uint32_t baudrate;    /**< Baud rate */
    uint32_t word_length; /**< Word length (data bits) */
    uint32_t stop_bits;   /**< Stop bits */
    uint32_t parity;      /**< Parity setting */
    uint32_t mode;        /**< UART mode */
    uint32_t hw_flow_ctl; /**< Hardware flow control */
    bool dma_tx_enable;   /**< DMA TX enable flag */
    bool dma_rx_enable;   /**< DMA RX enable flag */
    size_t tx_buf_size;   /**< TX buffer size */
    size_t rx_buf_size;   /**< RX buffer size */
} stm32_uart_config_t;

/*---------------------------------------------------------------------------*/
/* UART State Structure                                                      */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART state structure
 *
 * Contains runtime state and statistics.
 */
typedef struct stm32_uart_state_s {
    uint8_t instance;           /**< Instance index */
    stm32_uart_config_t config; /**< Configuration */
    stm32_uart_buffer_t tx_buf; /**< TX circular buffer */
    stm32_uart_buffer_t rx_buf; /**< RX circular buffer */
    bool initialized;           /**< Initialization flag */
    bool suspended;             /**< Suspend flag */
    bool tx_busy;               /**< TX busy flag */
    bool rx_busy;               /**< RX busy flag */
} stm32_uart_state_t;

/*---------------------------------------------------------------------------*/
/* DMA Support Structure                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART DMA configuration structure
 */
typedef struct stm32_uart_dma_s {
    DMA_HandleTypeDef hdma_tx; /**< TX DMA handle */
    DMA_HandleTypeDef hdma_rx; /**< RX DMA handle */
    bool dma_tx_enabled;       /**< DMA TX enable flag */
    bool dma_rx_enabled;       /**< DMA RX enable flag */
    uint8_t* dma_tx_buffer;    /**< DMA TX buffer pointer */
    uint8_t* dma_rx_buffer;    /**< DMA RX buffer pointer */
    size_t dma_tx_size;        /**< DMA TX buffer size */
    size_t dma_rx_size;        /**< DMA RX buffer size */
} stm32_uart_dma_t;

/*---------------------------------------------------------------------------*/
/* Callback Structure                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART callback structure
 */
typedef struct stm32_uart_callback_s stm32_uart_callback_t;
struct stm32_uart_callback_s {
    void (*tx_complete_cb)(void* user_data); /**< TX complete callback */
    void (*rx_complete_cb)(void* user_data); /**< RX complete callback */
    void (*error_cb)(void* user_data,
                     uint32_t error_code); /**< Error callback */
    void* user_data;                       /**< User data pointer */
};

/*---------------------------------------------------------------------------*/
/* UART Implementation Structure                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           STM32 UART implementation structure
 *
 * Contains all interfaces, ST HAL handle, and optional OSAL objects.
 */
typedef struct stm32_uart_impl_s {
    nx_uart_t base;           /**< Base UART interface */
    nx_tx_async_t tx_async;   /**< TX async interface */
    nx_rx_async_t rx_async;   /**< RX async interface */
    nx_tx_sync_t tx_sync;     /**< TX sync interface */
    nx_rx_sync_t rx_sync;     /**< RX sync interface */
    nx_lifecycle_t lifecycle; /**< Lifecycle interface */
    nx_power_t power;         /**< Power interface */

    UART_HandleTypeDef huart;        /**< ST HAL UART handle */
    stm32_uart_state_t* state;       /**< State pointer */
    struct nx_device_s* device;      /**< Device descriptor */
    stm32_uart_callback_t callbacks; /**< User callbacks */
    stm32_uart_dma_t dma;            /**< DMA configuration */

    /* Optional OSAL synchronization objects */
#ifdef NX_CONFIG_STM32_UART_USE_OSAL
    osal_mutex_handle_t mutex; /**< Mutex for thread safety */
    osal_sem_handle_t tx_sem;  /**< TX completion semaphore */
    osal_sem_handle_t rx_sem;  /**< RX completion semaphore */
#endif
} stm32_uart_impl_t;

#ifdef __cplusplus
}
#endif

#endif /* STM32_UART_TYPES_H */
