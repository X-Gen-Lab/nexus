/**
 * \file            software_spi.h
 * \brief           Caller-owned bounded GPIO mode-zero SPI endpoint
 * \author          Nexus Team
 */
#ifndef NEXUS_COMPONENTS_SOFTWARE_SPI_H
#define NEXUS_COMPONENTS_SOFTWARE_SPI_H

#include "nexus/core/time.h"
#include "nexus/io/gpio.h"
#include "nexus/io/spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One stable initialized GPIO face and exactly one authorized bit. */
typedef struct {
    const nx_gpio_port_t* port;
    uint32_t mask;
} nx_software_spi_pin_t;

/**
 * \brief           Immutable mode-zero, MSB-first, active-low-CS wiring
 * \note            Caller owns pads, pull configuration and clock lifetime.
 *                  Clock/MOSI/CS are outputs and MISO is an input. Known pin
 *                  identities must be disjoint; aliases hidden behind different
 *                  contexts remain the caller's responsibility. Delays are
 *                  minimum microseconds, never a measured frequency or WCET.
 *                  Each wait has at most poll_limit clock reads; a stalled
 *                  clock fails rather than spinning indefinitely. All fields,
 *                  faces and contexts stay alive and immutable through stop.
 */
typedef struct {
    nx_software_spi_pin_t sck;
    nx_software_spi_pin_t mosi;
    nx_software_spi_pin_t miso;
    nx_software_spi_pin_t cs;
    nx_clock_t clock;
    uint32_t half_period_us;
    uint32_t select_setup_us;
    uint32_t select_hold_us;
    uint32_t poll_limit;
    uint16_t max_bytes;
} nx_software_spi_config_t;

/**
 * \brief           Exact zero-initialized single-task owner, without a worker
 * \note            Fields are implementation state. No transfer retains a
 *                  payload pointer after return. Failed GPIO withdrawal keeps
 *                  the same pin owner faulted until recovery or stop succeeds.
 *                  No concurrent, IRQ or reentrant calls are supported; a
 *                  callback reentry is rejected BUSY before another pin write.
 */
typedef struct {
    const nx_software_spi_config_t* config;
    nx_time_us_t last_clock;
    bool initialized;
    bool in_call;
    bool faulted;
} nx_software_spi_t;

/**
 * \brief           Validate exact pins and establish inactive CS/clock/MOSI
 * \param[in,out]   owner: Fresh zero or successfully stopped owner
 * \param[in]       config: Stable disjoint configuration; no copied pin state
 * \return          SUCCESS, INVALID, BUSY, or the exact GPIO error
 * \note            Validation failure writes no pin. Once a pin write is
 *                  attempted, failure retains the config and a faulted owner;
 *                  retry recovery/stop before rebinding. No clock/IRQ/OS setup
 *                  or physical electrical proof is inferred.
 */
nx_result_t nx_software_spi_init(nx_software_spi_t* owner,
                                 const nx_software_spi_config_t* config);

/**
 * \brief           Project the existing synchronous SPI endpoint face
 * \param[in,out]   owner: Owner storage that outlives the returned face
 * \return          A value face with shared immutable methods
 * \note            Lookup allocates nothing and starts no hardware. The only
 *                  supported endpoint method is polling transfer. Length is
 *                  1..max_bytes (at most 256); deadline must be finite. Exact
 *                  in-place TX/RX is allowed, partial overlap is rejected.
 *                  Output buffers can contain completed bytes on failure;
 *                  transferred reports only complete bytes. Any active error
 *                  latches a fault even when idle-pin cleanup succeeds.
 */
nx_spi_endpoint_t nx_software_spi_endpoint(nx_software_spi_t* owner);

/**
 * \brief           Project existing SPI owner recovery and stop methods
 * \param[in,out]   owner: Stable owner storage, never an endpoint substitute
 * \return          A value face with shared immutable methods
 * \note            Recover/stop reject callback reentry, retry inactive CS,
 *                  SCK-low and MOSI-low without replaying a byte, and preserve
 *                  the original config on failure. Stop clears the binding
 *                  only after all three writes succeed. Neither operation
 *                  restores Board pad modes or ends an external GPIO lease.
 */
nx_spi_port_t nx_software_spi_port(nx_software_spi_t* owner);

/**
 * \brief           Observe retained bus recovery responsibility
 * \param[in]       owner: Stable single-task owner, or NULL
 * \return          True only when a non-NULL owner has a latched fault
 */
bool nx_software_spi_faulted(const nx_software_spi_t* owner);

#ifdef __cplusplus
}
#endif

#endif
