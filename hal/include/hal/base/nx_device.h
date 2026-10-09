/**
 * \file            nx_device.h
 * \brief           Kconfig-driven device registration mechanism
 * \author          Nexus Team
 *
 * \details         This file provides a simplified device registration
 *                  mechanism driven by Kconfig.Devices are registered at
 *                  compile time using macros that traverse Kconfig-enabled
 *                  instances.
 */

#ifndef NX_DEVICE_H
#define NX_DEVICE_H

#include "hal/nx_status.h"
#include "hal/nx_types.h"
#include "hal/interface/nx_lifecycle.h"
#include "hal/interface/nx_uart.h"
#include "hal/interface/nx_spi.h"
#include "hal/interface/nx_i2c.h"
#include "hal/interface/nx_flash.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Device Class and Capabilities                                             */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Public device class, independent of provider state
 */
typedef enum nx_device_class_e {
    NX_DEVICE_CLASS_UNKNOWN = 0,
    NX_DEVICE_CLASS_GPIO, NX_DEVICE_CLASS_GPIO_READ, NX_DEVICE_CLASS_GPIO_WRITE,
    NX_DEVICE_CLASS_UART, NX_DEVICE_CLASS_SPI, NX_DEVICE_CLASS_I2C,
    NX_DEVICE_CLASS_TIMER, NX_DEVICE_CLASS_TIMER_PWM, NX_DEVICE_CLASS_TIMER_ENCODER,
    NX_DEVICE_CLASS_ADC, NX_DEVICE_CLASS_ADC_BUFFER, NX_DEVICE_CLASS_DAC,
    NX_DEVICE_CLASS_FLASH, NX_DEVICE_CLASS_CAN, NX_DEVICE_CLASS_USB,
    NX_DEVICE_CLASS_RTC, NX_DEVICE_CLASS_CRC, NX_DEVICE_CLASS_SDIO,
    NX_DEVICE_CLASS_WATCHDOG, NX_DEVICE_CLASS_OPTION_BYTES,
    NX_DEVICE_CLASS_COUNT
} nx_device_class_t;

/** Class is a type proof; flags only advertise explicitly implemented features.
 * UART operation capabilities are queried from its operation port after open.
 */
typedef struct nx_device_caps_s {
    nx_device_class_t device_class;
    uint32_t flags;
} nx_device_caps_t;
#define NX_DEVICE_CAP_UART_OPERATIONS (1u << 0)
#define NX_DEVICE_CAP_UART_CANCEL (1u << 1)
#define NX_DEVICE_CAP_UART_RX_EVENTS (1u << 2)
#define NX_DEVICE_CAP_SPI_DEVICES (1u << 3)
#define NX_DEVICE_CAP_SPI_QUEUE (1u << 4)
#define NX_DEVICE_CAP_SPI_CANCEL (1u << 5)
#define NX_DEVICE_CAP_I2C_DEVICES (1u << 6)
#define NX_DEVICE_CAP_I2C_QUEUE (1u << 7)
#define NX_DEVICE_CAP_I2C_CANCEL (1u << 8)
#define NX_DEVICE_CAP_FLASH_GEOMETRY (1u << 9)
#define NX_DEVICE_CAP_FLASH_PROGRAM (1u << 10)
#define NX_DEVICE_CAP_FLASH_ERASE (1u << 11)
/*---------------------------------------------------------------------------*/
/* Device Descriptor Structure                                               */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Device descriptor structure
 *
 * An opaque, constant registry identity. Implementors declare descriptors in
 * hal/provider/nx_device_provider.h. Consumers obtain immutable metadata with
 * nx_device_describe(), and operate through owned references.
 */
typedef struct nx_device_s nx_device_t;


/** Immutable discovery metadata. Reading it does not bind or start hardware. */
typedef struct nx_device_info_s {
    const char* name;
    nx_device_class_t device_class;
    uint32_t declared_capabilities;
} nx_device_info_t;
nx_status_t nx_device_describe(const nx_device_t*, nx_device_info_t*);

/** Caller-held exclusive reference. Copies are allowed; successful close makes
 * every copy stale. It is not an implementation pointer or an ISR handle. */
typedef struct nx_device_ref_s {
    const nx_device_t* descriptor;
    uintptr_t owner;
    uint64_t generation;
    nx_device_class_t device_class;
} nx_device_ref_t;

/** All typed operations are task-only. Discover/query do not initialize
 * hardware. Open/close are no-wait admissions; lifecycle callbacks must be
 * bounded and nonblocking. BUSY leaves ownership unchanged. On open hardware
 * failure the core attempts deinit; failed cleanup quarantines the instance.
 * A quarantined instance can only be explicitly recovered with recover(). */
nx_status_t nx_device_discover(const char* name, nx_device_class_t expected,
                               const nx_device_t** out);
nx_status_t nx_device_open(const char* name, nx_device_class_t expected,
                           uintptr_t owner, nx_device_ref_t* out);
nx_status_t nx_device_query(nx_device_ref_t ref, nx_device_caps_t* out);
nx_status_t nx_device_close(nx_device_ref_t ref);
nx_status_t nx_device_recover(const char* name, uintptr_t owner);

/** Board-independent GPIO actions. No implementation pointer escapes. */
nx_status_t nx_device_gpio_read(nx_device_ref_t ref, uint8_t* value);
nx_status_t nx_device_gpio_write(nx_device_ref_t ref, uint8_t value);
nx_status_t nx_device_gpio_toggle(nx_device_ref_t ref);

/** Submit borrows TX storage until poll reports settled or cancel succeeds.
 * A device has one outstanding ticket; close and registry reset return BUSY
 * while its lease is held. The application must poll to enforce deadlines.
 * Terminal poll may be repeated until a later submit replaces the ticket. */
nx_status_t nx_device_uart_submit(nx_device_ref_t ref, const uint8_t* data,
                                  size_t len, uint32_t timeout_ms,
                                  nx_uart_ticket_t* ticket);
nx_status_t nx_device_uart_poll(nx_device_ref_t ref, nx_uart_ticket_t ticket,
                                nx_uart_result_t* result);
nx_status_t nx_device_uart_cancel(nx_device_ref_t ref, nx_uart_ticket_t ticket);
/** Recovery for a provider contract violation (admitted zero ticket). Task-only.
 * Deinitialization must settle hardware before NX_OK invalidates the reference
 * and returns buffer ownership. Failed recovery retains the borrowed storage;
 * normal requests must use their ticket/cancel path. Reopen after recovery. */
nx_status_t nx_device_uart_recover(nx_device_ref_t ref);
nx_status_t nx_device_uart_receive_event(nx_device_ref_t ref,
                                         nx_uart_rx_event_t* event);

/** SPI child references are caller-owned values. No provider pointer escapes.
 * Close invalidates copies; the parent remains owned until every child closes.
 * The global child pool is bounded and performs no allocation on any action. */
#ifndef NX_DEVICE_SPI_MAX_CHILDREN
#define NX_DEVICE_SPI_MAX_CHILDREN 16u
#endif
typedef struct nx_device_spi_ref_s {
    nx_device_ref_t controller;
    uint64_t generation;
    uint32_t slot; /**< One-based pool identity; zero is invalid. */
} nx_device_spi_ref_t;
typedef struct nx_device_spi_ticket_s {
    uint64_t sequence;
    uint64_t generation;
    uint32_t slot;
} nx_device_spi_ticket_t;
typedef struct nx_device_spi_result_s {
    nx_status_t status;
    bool settled;
} nx_device_spi_result_t;

/** All SPI actions are task-context only. transfer()/service() also reject
 * an architecture interrupt mask because hardware/ticks may not progress.
 * transfer() is blocking, with the
 * provider's single total timeout budget including bus contention. It settles
 * storage on every return. A zero budget does not start hardware. submit() is
 * no-wait and requires a finite nonzero budget; queue delay consumes that same
 * budget. The callback is optional at this facade (poll may observe completion).
 * The application must drive service() from one dedicated task; no hidden worker
 * enforces a queued deadline. No DMA, wire telemetry or length result is implied.
 *
 * Async storage is borrowed until its terminal callback returns, or poll reports
 * settled. cancel() requests queued cancellation, including from another task
 * during service; NX_OK is NOT settlement. An error never releases the lease.
 * cancel_transfer() requests cancellation of the current blocking call on this
 * child; its caller must still wait for transfer() to return before reclaiming
 * buffers. A provider may reject cancellation with NOT_SUPPORTED.
 * Close and callback reentry on the same child report BUSY while work is active.
 * A callback runs outside metadata locks, after provider hardware drain/unlock.
 * Synchronous callback reentry also retains both child and controller ownership.
 * Tickets are scoped to a child generation and never repeat for its pool slot.
 * Failed close retains the reference. recover() retries only quarantined child
 * opens where the provider returned an incomplete handle and cleanup failed. */
nx_status_t nx_device_spi_open(nx_device_ref_t controller,
                              const nx_spi_device_config_t* config,
                              nx_device_spi_ref_t* out);
nx_status_t nx_device_spi_close(nx_device_spi_ref_t ref);
nx_status_t nx_device_spi_query(nx_device_spi_ref_t ref, nx_device_caps_t* caps);
nx_status_t nx_device_spi_recover(nx_device_ref_t controller);
nx_status_t nx_device_spi_transfer(nx_device_spi_ref_t ref,
                                  const nx_spi_transaction_t* transaction);
nx_status_t nx_device_spi_submit(nx_device_spi_ref_t ref,
                                const nx_spi_transaction_t* transaction,
                                nx_device_spi_ticket_t* ticket);
nx_status_t nx_device_spi_poll(nx_device_spi_ref_t ref,
                              nx_device_spi_ticket_t ticket,
                              nx_device_spi_result_t* result);
nx_status_t nx_device_spi_cancel(nx_device_spi_ref_t ref,
                                nx_device_spi_ticket_t ticket);
nx_status_t nx_device_spi_cancel_transfer(nx_device_spi_ref_t ref);
nx_status_t nx_device_spi_service(nx_device_ref_t controller);

/** Typed I2C uses the same parent/child lease and task-only settlement rules as
 * SPI. The address is unshifted 7-bit and copied at open. No MCU support is
 * inferred from legacy getters: absent modern provider ports return UNSUPPORTED.
 * A caller drives service(); queue delay consumes the original timeout. */
#ifndef NX_DEVICE_I2C_MAX_CHILDREN
#define NX_DEVICE_I2C_MAX_CHILDREN 16u
#endif
typedef struct nx_device_i2c_ref_s {
    nx_device_ref_t controller;
    uint64_t generation;
    uint32_t slot;
} nx_device_i2c_ref_t;
typedef struct nx_device_i2c_ticket_s {
    uint64_t sequence;
    uint64_t generation;
    uint32_t slot;
} nx_device_i2c_ticket_t;
typedef struct nx_device_i2c_result_s {
    nx_status_t status;
    bool settled;
} nx_device_i2c_result_t;
nx_status_t nx_device_i2c_open(nx_device_ref_t controller, uint8_t address,
                              nx_device_i2c_ref_t* out);
nx_status_t nx_device_i2c_close(nx_device_i2c_ref_t ref);
nx_status_t nx_device_i2c_query(nx_device_i2c_ref_t ref, nx_device_caps_t* caps);
nx_status_t nx_device_i2c_recover(nx_device_ref_t controller);
nx_status_t nx_device_i2c_transfer(nx_device_i2c_ref_t ref,
                                  const nx_i2c_transaction_t* transaction);
nx_status_t nx_device_i2c_submit(nx_device_i2c_ref_t ref,
                                const nx_i2c_transaction_t* transaction,
                                nx_device_i2c_ticket_t* ticket);
nx_status_t nx_device_i2c_poll(nx_device_i2c_ref_t ref,
                              nx_device_i2c_ticket_t ticket,
                              nx_device_i2c_result_t* result);
nx_status_t nx_device_i2c_cancel(nx_device_i2c_ref_t ref,
                                nx_device_i2c_ticket_t ticket);
nx_status_t nx_device_i2c_cancel_transfer(nx_device_i2c_ref_t ref);
nx_status_t nx_device_i2c_service(nx_device_ref_t controller);

/** Flash regions are bounded leases, not caller-mutable address structures.
 * Region close returns BUSY during I/O; parent close waits for every region.
 * New write-capable regions must not overlap existing regions. A region that
 * allows erase must begin/end at complete physical erase-block boundaries.
 * The external application selects layout and grants permissions explicitly. */
#ifndef NX_DEVICE_FLASH_MAX_REGIONS
#define NX_DEVICE_FLASH_MAX_REGIONS 16u
#endif
#define NX_FLASH_REGION_READ (1u << 0)
#define NX_FLASH_REGION_PROGRAM (1u << 1)
#define NX_FLASH_REGION_ERASE (1u << 2)
typedef struct nx_device_flash_region_s {
    nx_device_ref_t controller;
    uint64_t generation;
    uint32_t slot;
} nx_device_flash_region_t;
nx_status_t nx_device_flash_geometry(nx_device_ref_t ref,
                                     nx_flash_geometry_t* out);
nx_status_t nx_device_flash_block(nx_device_ref_t ref, uint32_t offset,
                                  nx_flash_block_t* out);
nx_status_t nx_device_flash_set_write_enabled(nx_device_ref_t ref, bool enabled);
nx_status_t nx_device_flash_region_open(nx_device_ref_t controller,
                                        uint32_t offset, uint32_t size,
                                        uint32_t permissions,
                                        nx_device_flash_region_t* out);
nx_status_t nx_device_flash_region_close(nx_device_flash_region_t ref);
nx_status_t nx_device_flash_read(nx_device_flash_region_t ref, uint32_t offset,
                                 uint8_t* data, size_t len);
nx_status_t nx_device_flash_program(nx_device_flash_region_t ref, uint32_t offset,
                                    const uint8_t* data, size_t len,
                                    uint32_t timeout_ms);
nx_status_t nx_device_flash_erase(nx_device_flash_region_t ref, uint32_t offset,
                                  size_t len, uint32_t timeout_ms);
nx_status_t nx_device_flash_sync(nx_device_ref_t ref, uint32_t timeout_ms);

/** Preflight a global platform shutdown. Does not release ownership. */
nx_status_t nx_device_shutdown_check(void);
/** Shutdown admission fence. begin fails BUSY while an owner/construction is
 * active; success rejects new open/construction/registration until end. */
nx_status_t nx_device_shutdown_begin(void);
void nx_device_shutdown_end(void);

/** Validate integer linker bounds before interpreting a registry section.
 * Missing start+end denotes an empty registry; a single missing bound, reverse
 * bounds, bad pointer alignment or an incomplete descriptor is rejected. */
nx_status_t nx_device_validate_registry_region(uintptr_t start, uintptr_t end,
                                              size_t* count);

/*---------------------------------------------------------------------------*/
/* Device Lookup Functions                                                   */
/*---------------------------------------------------------------------------*/

/**
 * \brief           Find device descriptor by name
 * \param[in]       name: Device name
 * \return          Device descriptor pointer, NULL if not found
 * \note            This function only finds the device, does not initialize
 */
const nx_device_t* nx_device_find(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* NX_DEVICE_H */
