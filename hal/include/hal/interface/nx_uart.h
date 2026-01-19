/**
 * \file            nx_uart.h
 * \brief           UART device interface definition
 * \author          Nexus Team
 */

#ifndef NX_UART_H
#define NX_UART_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_diagnostic.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* UART Configuration                                                        */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART parity enumeration
 */
typedef enum nx_uart_parity_e {
    NX_UART_PARITY_NONE = 0, /**< No parity */
    NX_UART_PARITY_ODD,      /**< Odd parity */
    NX_UART_PARITY_EVEN,     /**< Even parity */
} nx_uart_parity_t;

/**
 * \brief           UART flow control enumeration
 */
typedef enum nx_uart_flow_ctrl_e {
    NX_UART_FLOW_NONE = 0, /**< No flow control */
    NX_UART_FLOW_RTS,      /**< RTS only */
    NX_UART_FLOW_CTS,      /**< CTS only */
    NX_UART_FLOW_RTS_CTS,  /**< RTS and CTS */
} nx_uart_flow_ctrl_t;

/**
 * \brief           UART statistics structure
 */
typedef struct nx_uart_stats_s {
    bool tx_busy;            /**< TX busy flag */
    bool rx_busy;            /**< RX busy flag */
    uint32_t tx_count;       /**< Total bytes transmitted */
    uint32_t rx_count;       /**< Total bytes received */
    uint32_t tx_errors;      /**< TX error count */
    uint32_t rx_errors;      /**< RX error count */
    uint32_t overrun_errors; /**< Overrun error count */
    uint32_t framing_errors; /**< Framing error count */
} nx_uart_stats_t;

/*---------------------------------------------------------------------------*/
/* UART Device Interface                                                     */
/*---------------------------------------------------------------------------*/

/**
 * \brief           UART device interface
 *
 * Provides access to UART communication through async/sync interfaces
 * and base lifecycle/power/diagnostic interfaces.
 */
typedef struct nx_uart_s nx_uart_t;
struct nx_uart_s {
    /**
     * \brief           Get async transmit interface
     * \param[in]       self: UART device pointer
     * \return          Async TX interface pointer
     */
    nx_tx_async_t* (*get_tx_async)(nx_uart_t* self);

    /**
     * \brief           Get async receive interface
     * \param[in]       self: UART device pointer
     * \return          Async RX interface pointer
     */
    nx_rx_async_t* (*get_rx_async)(nx_uart_t* self);

    /**
     * \brief           Get sync transmit interface
     * \param[in]       self: UART device pointer
     * \return          Sync TX interface pointer
     */
    nx_tx_sync_t* (*get_tx_sync)(nx_uart_t* self);

    /**
     * \brief           Get sync receive interface
     * \param[in]       self: UART device pointer
     * \return          Sync RX interface pointer
     */
    nx_rx_sync_t* (*get_rx_sync)(nx_uart_t* self);

    /**
     * \brief           Get lifecycle interface
     * \param[in]       self: UART device pointer
     * \return          Lifecycle interface pointer
     */
    nx_lifecycle_t* (*get_lifecycle)(nx_uart_t* self);

    /**
     * \brief           Get power interface
     * \param[in]       self: UART device pointer
     * \return          Power interface pointer
     */
    nx_power_t* (*get_power)(nx_uart_t* self);

    /**
     * \brief           Get diagnostic interface
     * \param[in]       self: UART device pointer
     * \return          Diagnostic interface pointer
     */
    nx_diagnostic_t* (*get_diagnostic)(nx_uart_t* self);
};

/*---------------------------------------------------------------------------*/
/* UART Initialization Macro                                                 */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Initialize UART device interface
 * \param[in]       p: Pointer to nx_uart_t structure
 * \param[in]       _get_tx_async: Get TX async function pointer
 * \param[in]       _get_rx_async: Get RX async function pointer
 * \param[in]       _get_tx_sync: Get TX sync function pointer
 * \param[in]       _get_rx_sync: Get RX sync function pointer
 * \param[in]       _get_lifecycle: Get lifecycle function pointer
 * \param[in]       _get_power: Get power function pointer
 * \param[in]       _get_diagnostic: Get diagnostic function pointer
 */
#define NX_INIT_UART(p, _get_tx_async, _get_rx_async, _get_tx_sync,            \
                     _get_rx_sync, _get_lifecycle, _get_power,                 \
                     _get_diagnostic)                                          \
    do {                                                                       \
        (p)->get_tx_async = (_get_tx_async);                                   \
        (p)->get_rx_async = (_get_rx_async);                                   \
        (p)->get_tx_sync = (_get_tx_sync);                                     \
        (p)->get_rx_sync = (_get_rx_sync);                                     \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
        (p)->get_diagnostic = (_get_diagnostic);                               \
        NX_ASSERT((p)->get_tx_async != NULL);                                  \
        NX_ASSERT((p)->get_rx_async != NULL);                                  \
        NX_ASSERT((p)->get_tx_sync != NULL);                                   \
        NX_ASSERT((p)->get_rx_sync != NULL);                                   \
        NX_ASSERT((p)->get_lifecycle != NULL);                                 \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* NX_UART_H */
