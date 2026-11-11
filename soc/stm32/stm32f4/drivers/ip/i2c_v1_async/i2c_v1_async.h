/**
 * \file            i2c_v1_async.h
 * \brief           Private reviewed STM32F4 I2C V1 register algorithm
 * \author          Nexus Team
 */
#ifndef NEXUS_I2C_V1_ASYNC_H
#define NEXUS_I2C_V1_ASYNC_H
#include "nexus/io/dma.h"
#include "nexus/io/i2c.h"

/** \brief Resolve clock counts under the authored nominal SCL maximum.
 * \param[in] mhz: Reviewed nominal whole-MHz PCLK, between 2 and 50.
 * \param[in] maximum_hz: Nonzero nominal SCL ceiling, at most 400 kHz.
 * \param[out] ccr: Standard or Fast-mode duty-2 CCR on success only.
 * \param[out] rise: ST's standard 1 us or Fast-mode 300 ns TRISE formula.
 * \return True for a representable 12-bit divisor; otherwise no outputs.
 * \note Standard mode is capped at 88 kHz for ES0182 2.10.4 repeated STARTs,
 *       independently of endpoint width. The authored maximum is an upper
 *       bound, not an exact rate; Fast mode remains an explicit selection.
 *       Pure arithmetic with no hardware or retained references. Oscillator
 *       tolerance and electrical rise times need separate Board qualification.
 */
static inline bool nx_i2c_v1_timing(uint8_t mhz, uint32_t maximum_hz,
                                    uint16_t* ccr, uint8_t* rise) {
    if (mhz < 2U || mhz > 50U || maximum_hz == 0U || maximum_hz > 400000U ||
        ccr == NULL || rise == NULL) {
        return false;
    }
    bool fast = maximum_hz > 100000U;
    if (fast && mhz < 4U) {
        return false;
    }
    if (!fast && maximum_hz > 88000U) {
        maximum_hz = 88000U;
    }
    uint32_t clocks = (uint32_t)mhz * 1000000U;
    uint32_t denominator = maximum_hz * (fast ? 3U : 2U);
    uint32_t count =
        clocks / denominator + (clocks % denominator != 0U ? 1U : 0U);
    /* RM0090 27.6.8 requires a multiple of 10 MHz to reach 400 kHz.
     * Other clocks stay strictly below that boundary. */
    if (fast && mhz % 10U != 0U) {
        uint32_t minimum = clocks / 1200000U + 1U;
        if (count < minimum) {
            count = minimum;
        }
    }
    if (count < (fast ? 1U : 4U) || count > 0xFFFU) {
        return false;
    }
    *ccr = (uint16_t)(count | (fast ? 0x8000U : 0U));
    *rise =
        (uint8_t)(fast ? (uint32_t)mhz * 3U / 10U + 1U : (uint32_t)mhz + 1U);
    return true;
}

/** \brief Pure endpoint admission; width zero never selects an implicit mode.
 */
static inline bool nx_i2c_v1_address_valid(uint16_t address, uint8_t bits) {
    return bits == 7U ? address >= 8U && address <= 119U
                      : bits == 10U && address <= 1023U;
}

/** \brief ES0182 2.10.4 excludes 88--100 kHz Standard repeated STARTs.
 * \note Check the actual CCR profile, including direct C callers. Fast-mode
 *       timing has its separate electrical Board qualification.
 */
static inline bool nx_i2c_v1_restart_timing(uint16_t ccr, uint8_t mhz) {
    uint32_t divisor = (uint32_t)ccr & 0xFFFU;
    return divisor != 0U && ((ccr & 0x8000U) != 0U ||
                             divisor * 176000U >= (uint32_t)mhz * 1000000U);
}

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
    NX_I2C_V1_TEN_LOW,
    NX_I2C_V1_TEN_ADDRESS,
    NX_I2C_V1_TEN_READ_START,
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
    /** \brief Cold CCR profile retained through recovery, never IRQ-mutated. */
    uint16_t timing_ccr;
    uint8_t message;
    nx_result_t terminal;
    nx_i2c_v1_phase_t phase;
    uint16_t address;
    uint8_t address_bits;
    uint8_t peripheral_mhz;
    uint16_t wake_publishers;
    bool initialized;
    bool stopping;
    bool faulted;
} nx_i2c_v1_state_t;

/** \brief Immutable addressed child with exact controller identity. */
typedef struct {
    nx_i2c_v1_state_t* port;
    uint16_t address;
    uint8_t address_bits;
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
