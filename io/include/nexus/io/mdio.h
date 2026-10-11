/**
 * \file            mdio.h
 * \brief           Narrow Clause22 station management with absolute deadlines
 * \author          Nexus Team
 */
#ifndef NEXUS_IO_MDIO_H
#define NEXUS_IO_MDIO_H
#include "nexus/core/time.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    nx_result_t (*read)(void*, uint8_t, uint8_t, uint16_t*, nx_time_us_t);
    nx_result_t (*write)(void*, uint8_t, uint8_t, uint16_t, nx_time_us_t);
} nx_mdio_ops_t;
typedef struct {
    const nx_mdio_ops_t* ops;
    void* context;
} nx_mdio_port_t;
/**
 * \brief Read one Clause22 PHY register without retaining output storage.
 * \param[in] port: Stable face and caller-owned execution context.
 * \param[in] address: Clause22 PHY address, from 0 through 31.
 * \param[in] reg: Clause22 register address, from 0 through 31.
 * \param[out] value: Written only after timely observed completion.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Success, BUSY without a new command, or timeout/input/context error.
 * \note Serialized privileged task, incoming masks clear; finite absolute
 *       deadline in the provider clock. Address and register are 0..31. BUSY
 *       rejects before new MMIO effects; timeout does not pretend busy cleared.
 */
nx_result_t nx_mdio_read(const nx_mdio_port_t* port, uint8_t address,
                         uint8_t reg, uint16_t* value, nx_time_us_t deadline);
/**
 * \brief Write one Clause22 PHY register, not a PHY-specific policy sequence.
 * \param[in] port: Stable face and caller-owned execution context.
 * \param[in] address: Clause22 PHY address, from 0 through 31.
 * \param[in] reg: Clause22 register address, from 0 through 31.
 * \param[in] value: Exact 16-bit register value.
 * \param[in] deadline: Finite absolute provider-clock deadline.
 * \return Success, BUSY without a new command, or timeout/input/context error.
 * \note Same task/deadline rules as read. No IRQ-masked busy wait or fallback.
 */
nx_result_t nx_mdio_write(const nx_mdio_port_t* port, uint8_t address,
                          uint8_t reg, uint16_t value, nx_time_us_t deadline);
#ifdef __cplusplus
}
#endif
#endif
