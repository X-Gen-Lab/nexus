/**
 * \file            i2c_v1_async.h
 * \brief           Private shared STM32F407/GD32F470 register algorithm
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_V1_ASYNC_H
#define NEXUS_I2C_V1_ASYNC_H
#include "nexus/io/dma.h"
#include "nexus/io/i2c.h"

/** \brief Cold physical DMA facts, never a runtime stream allocator. */
typedef struct {
    volatile uint32_t* control;
    volatile uint32_t* count;
    volatile uint32_t* peripheral;
    volatile uint32_t* memory;
    volatile uint32_t* fifo;
    const volatile uint32_t* flags;
    volatile uint32_t* clear;
    uint32_t configuration;
    uint8_t flag_shift;
} nx_i2c_v1_dma_engine_t;

/** \brief Immutable exact route and visible memory, absent in IRQ mode. */
typedef struct {
    nx_i2c_v1_dma_engine_t tx;
    nx_i2c_v1_dma_engine_t rx;
    const nx_dma_memory_region_t* regions;
    size_t region_count;
} nx_i2c_v1_dma_t;

/** \brief Protocol windows keep waits outside the bounded interrupt path. */
typedef enum {
    NX_I2C_V1_IDLE,
    NX_I2C_V1_START,
    NX_I2C_V1_ADDRESS,
    NX_I2C_V1_WRITE,
    NX_I2C_V1_WRITE_ACK,
    NX_I2C_V1_READ,
    NX_I2C_V1_READ_ONE,
    NX_I2C_V1_READ_TWO,
    NX_I2C_V1_DMA_READ,
    NX_I2C_V1_DMA_WRITE,
    NX_I2C_V1_DMA_WRITE_ACK,
    NX_I2C_V1_DRAIN
} nx_i2c_v1_phase_t;

/**
 * \brief           Exact selected asynchronous controller state
 * \note            Registers use the identical reviewed V1 scalar layout;
 *                  SoC wrappers assert SDK offsets and masks. Cold callbacks
 *                  validate publisher facts and detach NVIC pending sources.
 *                  No per-byte callback dispatch, worker, pool or allocation.
 */
typedef struct {
    nx_time_us_t drain_deadline;
    volatile uint32_t* registers;
    const volatile uint32_t* line_inputs;
    uint32_t line_mask;
    const nx_i2c_v1_dma_t* dma;
    nx_i2c_request_t* active;
    const nx_irq_wake_t* wake;
    nx_result_t (*validate_wake)(void*, const nx_irq_wake_t*, uint8_t);
    void (*ack_irqs)(void*);
    nx_result_t (*stop_irqs)(void*);
    uint16_t index;
    uint16_t transferred;
    uint8_t message;
    nx_result_t terminal;
    nx_i2c_v1_phase_t phase;
    uint8_t address;
    uint8_t peripheral_mhz;
    uint16_t wake_publishers;
    bool initialized;
    bool stopping;
    bool faulted;
} nx_i2c_v1_state_t;

/** \brief Immutable addressed child with exact controller identity. */
typedef struct {
    nx_i2c_v1_state_t* port;
    uint8_t address;
} nx_i2c_v1_endpoint_t;

#ifdef __cplusplus
extern "C" {
#endif
extern const nx_i2c_ops_t nx_i2c_v1_async_ops;
extern const nx_i2c_endpoint_ops_t nx_i2c_v1_async_endpoint_ops;
/** \brief Validate cold facts before activating any interrupt source. */
nx_result_t nx_i2c_v1_initialize(nx_i2c_v1_state_t* state);
/** \brief Consume at most two DATA bytes, preserving final receive windows. */
void nx_i2c_v1_irq(nx_i2c_v1_state_t* state);
/** \brief DMA terminal is separate from protocol completion and STOP drain. */
void nx_i2c_v1_dma_irq(nx_i2c_v1_state_t* state, bool receive);
#ifdef __cplusplus
}
#endif
#endif
