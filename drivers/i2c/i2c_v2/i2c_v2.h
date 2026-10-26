/**
 * \file            i2c_v2.h
 * \brief           Private reviewed V2 finite polling controller algorithm
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-16
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NX_I2C_V2_H
#define NX_I2C_V2_H
#include "nexus/io/i2c.h"
#include "nexus/io/i2c_long.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Register layout must be independently asserted by each SoC port. */
typedef struct {
    volatile uint32_t cr1;
    volatile uint32_t cr2;
    volatile uint32_t oar1;
    volatile uint32_t oar2;
    volatile uint32_t timing;
    volatile uint32_t timeout;
    volatile uint32_t isr;
    volatile uint32_t icr;
    volatile uint32_t pecr;
    volatile uint32_t rxdr;
    volatile uint32_t txdr;
} nx_i2c_v2_registers_t;
/** \brief Reviewed immutable resources, timing and exact errata policy. */
typedef struct {
    nx_i2c_v2_registers_t* registers;
    volatile uint32_t* reset;
    uint32_t reset_mask;
    uint32_t timing;
    uint32_t poll_limit;
    /** \brief Exact SoC errata permits master-only spurious BERR. */
    bool ignore_master_berr;
    /** \brief Exact SoC errata requires NBYTES1 throughout reloaded RX. */
    bool receive_reload_one;
} nx_i2c_v2_config_t;
/** \brief One exclusive executor with immutable configuration and runtime.
 * \note SoC owns clocks, pin wiring and electrical timing. No IRQ or DMA.
 */
typedef struct {
    const nx_i2c_v2_config_t* config;
    bool initialized;
    bool retained;
    bool active;
    bool fault;
} nx_i2c_v2_state_t;
typedef struct {
    nx_i2c_v2_state_t* port;
    uint8_t address;
} nx_i2c_v2_endpoint_state_t;
extern const nx_i2c_ops_t nx_i2c_v2_ops;
extern const nx_i2c_endpoint_ops_t nx_i2c_v2_endpoint_ops;
extern const nx_i2c_long_ops_t nx_i2c_v2_long_endpoint_ops;
/** \brief Bind fixed resources only while the serialized owner is cold.
 * \param[in,out] port: Static runtime; live or retained owners reject changes.
 * \param[in] config: Immutable static resources with the runtime's lifetime.
 * \return Success without hardware effects, or an unchanged rejection.
 * \note Stop releases hardware ownership, not the configuration binding.
 */
nx_result_t nx_i2c_v2_bind(nx_i2c_v2_state_t* port,
                           const nx_i2c_v2_config_t* config);
/** \brief Acquire cold disabled hardware after the SoC has acquired its gate.
 * \param[in,out] port: Exact static context with reviewed finite poll budget.
 * \return Success or rejection; retained requires ordinary stop cleanup.
 * \note Privileged task only. Failed transfer drain never releases a caller.
 */
nx_result_t nx_i2c_v2_initialize(nx_i2c_v2_state_t* port);
#ifndef NX_I2C_V2_READ
#define NX_I2C_V2_READ(reg) (*(reg))
#endif
#ifndef NX_I2C_V2_WRITE
#define NX_I2C_V2_WRITE(reg, value) (*(reg) = (uint32_t)(value))
#endif
#ifndef NX_I2C_V2_POLL
#define NX_I2C_V2_POLL(port) ((void)(port))
#endif
#ifdef __cplusplus
}
#endif
#endif
