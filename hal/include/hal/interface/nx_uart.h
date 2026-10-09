/**
 * \file            nx_uart.h
 * \brief           UART device interface definition
 * \author          Nexus Team
 */

#ifndef NX_UART_H
#define NX_UART_H

#include "hal/base/nx_comm.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_power.h"
#include "hal/nx_status.h"
#include "hal/nx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A ticket is valid only for the UART instance that issued it. Sequence zero
 * is invalid. Sequence values must never repeat in an instance lifetime;
 * exhaustion returns NO_RESOURCE. Copying a ticket does not extend a device
 * reference lifetime. */
typedef struct { uint64_t sequence; } nx_uart_ticket_t;

typedef struct {
    nx_status_t status; /**< BUSY until settled; then OK/CANCELLED/TIMEOUT/error. */
    size_t transferred; /**< Bytes accepted by controller; cancellation may truncate the wire frame. */
    bool settled;      /**< No hardware or callback can touch the TX buffer. */
    bool wire_idle;    /**< True only after TC, or transmitter has been disabled. */
} nx_uart_result_t;

/** RX events are ordered, single-byte records. timestamp_us is sampled in the
 * RX IRQ (MCU) or explicit event producer (Native), not when the application
 * drains the ring. resolution_us describes
 * the clock precision. Error/overflow records have has_data=false; raw_error
 * contains backend diagnostic flags. Data never implies an error-free frame. */
typedef struct {
    uint64_t timestamp_us;
    uint32_t resolution_us;
    uint32_t raw_error;
    uint8_t data;
    bool has_data;
    nx_status_t status;
} nx_uart_rx_event_t;

typedef struct nx_uart_operations_s nx_uart_operations_t;
struct nx_uart_operations_s {
    /** Task context, nonblocking. On OK, data remains borrowed until poll says
     * settled. timeout_ms is a total transfer budget, at most INT32_MAX ms.
     * Service/poll must run to enforce the deadline; there is no hidden timer. */
    nx_status_t (*submit)(nx_uart_operations_t* self, const uint8_t* data,
                          size_t len, uint32_t timeout_ms, nx_uart_ticket_t* ticket);
    /** Task context. OK means the result was queried, not that TX succeeded. */
    nx_status_t (*poll)(nx_uart_operations_t* self, nx_uart_ticket_t ticket,
                        nx_uart_result_t* result);
    /** Task context. OK guarantees settlement; failure retains ownership
     * until a subsequent poll reports settled. Cancellation can truncate the
     * electrical frame and is not a successful transmission. */
    nx_status_t (*cancel)(nx_uart_operations_t* self, nx_uart_ticket_t ticket);
    /** Task context, nonblocking; NO_DATA when no event is available. */
    nx_status_t (*receive_event)(nx_uart_operations_t* self,
                                 nx_uart_rx_event_t* event);
};

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
    /** Optional complete operation contract; NULL means unsupported. */
    nx_uart_operations_t* (*get_operations)(nx_uart_t* self);
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
 */
#define NX_INIT_UART(p, _get_tx_async, _get_rx_async, _get_tx_sync,            \
                     _get_rx_sync, _get_lifecycle, _get_power)                 \
    do {                                                                       \
        (p)->get_operations = NULL;                                           \
        (p)->get_tx_async = (_get_tx_async);                                   \
        (p)->get_rx_async = (_get_rx_async);                                   \
        (p)->get_tx_sync = (_get_tx_sync);                                     \
        (p)->get_rx_sync = (_get_rx_sync);                                     \
        (p)->get_lifecycle = (_get_lifecycle);                                 \
        (p)->get_power = (_get_power);                                         \
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
