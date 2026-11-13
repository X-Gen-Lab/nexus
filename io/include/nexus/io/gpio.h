/**
 * \file            gpio.h
 *
 * \brief           Fixed GPIO authorization masks and bounded port operations.
 *
 * \author          Nexus Team
 */
#ifndef NEXUS_GPIO_H
#define NEXUS_GPIO_H

#include "nexus/core/status.h"
#include "nexus/io/fwd.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Shared read-only methods using one provider state.
 *
 * \note            Methods follow each public operation's ownership contract.
 *                  Missing optional methods report UNSUPPORTED.
 */
typedef struct {
    nx_result_t (*write)(void* context, uint32_t set_mask, uint32_t reset_mask);
    nx_result_t (*read)(const void* context, uint32_t* value);
    nx_result_t (*toggle)(void* context, uint32_t mask);
} nx_gpio_ops_t;
/**
 * \brief           Immutable interface pointing to caller-owned state.
 *
 * \note            Face and state outlive callers, IRQs and retained borrows.
 *                  Factory lookup neither initializes nor acquires hardware.
 */
struct nx_gpio_port {
    const nx_gpio_ops_t* ops;
    void* context;
};

/**
 * \brief Immutable logical signal binding supplied by Board integration.
 * \note One nonzero mask bit names the authorized pin. Consumers use logical
 *       activity; physical polarity stays in this binding. Binding and port
 *       outlive calls; this adds no mutable state, lock or resource ownership.
 */
typedef struct {
    const nx_gpio_port_t* port;
    uint32_t mask;
    bool active_high;
} nx_gpio_signal_t;

/**
 * \brief Set one logical signal to its requested active or inactive state.
 * \param[in] signal: Immutable single-pin binding and live initialized port.
 * \param[in] active: Logical activity, independent of electrical polarity.
 * \return SUCCESS or the port error; malformed binding returns INVALID.
 * \note Same bounded Task/IRQ and serialized-writer contract as port write.
 *       No storage is retained; the underlying provider checks authorization.
 */
nx_result_t nx_gpio_signal_write(const nx_gpio_signal_t* signal, bool active);

/**
 * \brief Sample a logical input without exposing its physical pin mask.
 * \param[in] signal: Immutable single-pin binding and live initialized port.
 * \param[out] active: Separate writable result, unchanged on error.
 * \return SUCCESS or the port error; malformed arguments return INVALID.
 * \note Bounded Task/IRQ call; no retained storage or debounce policy. Output
 *       is disjoint from the binding, face and all provider control storage.
 */
nx_result_t nx_gpio_signal_read(const nx_gpio_signal_t* signal, bool* active);
/**
 * \brief           Atomically set/reset one port's authorized output mask.
 *
 * \param[in]       port: Fixed initialized binding, no registry or OS lock.
 *
 * \param[in]       set_mask: Authorized bits to set.
 *
 * \param[in]       reset_mask: Authorized bits to reset; disjoint from
 *                  set_mask.
 *
 * \return          NX_SUCCESS or INVALID/PERMISSION before any register write.
 *
 * \note            Task/IRQ bounded, no wait. One serialized writer owns the
 *                  binding. Stop/mode changes require writers and IRQ
 *                  quiescence.
 */
nx_result_t nx_gpio_port_write(const nx_gpio_port_t* port, uint32_t set_mask,
                               uint32_t reset_mask);
/**
 * \brief           Read one input snapshot restricted to the binding mask.
 *
 * \param[in]       port: Fixed initialized binding.
 *
 * \param[out]      value: Masked input snapshot; unchanged on error.
 *
 * \return          NX_SUCCESS or NX_ERROR_INVALID. No cross-port simultaneity.
 */
nx_result_t nx_gpio_port_read(const nx_gpio_port_t* port, uint32_t* value);
/**
 * \brief           Toggle authorized outputs under the single-writer contract.
 *
 * \param[in]       port: Fixed output binding with a serialized writer.
 *
 * \param[in]       mask: Authorized subset to toggle.
 *
 * \return          NX_SUCCESS or INVALID/PERMISSION before register writes.
 *
 * \note            Read-modify-write is not atomic against unrelated writes.
 */
nx_result_t nx_gpio_port_toggle(const nx_gpio_port_t* port, uint32_t mask);
#ifdef __cplusplus
}
#endif

#endif
