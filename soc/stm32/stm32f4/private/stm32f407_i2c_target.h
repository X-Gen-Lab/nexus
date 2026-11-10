/**
 * \file            stm32f407_i2c_target.h
 * \brief           Exclusive F407 target/SMBus state and static cold receipt
 * \author          Nexus Team
 */
#ifndef NX_STM32F407_I2C_TARGET_H
#define NX_STM32F407_I2C_TARGET_H
#include "nexus/io/i2c_target.h"
#include "stm32f407_platform_cold.h"
#ifdef __cplusplus
extern "C" {
#endif

/** \brief Static hardware facts; SCL/SDA and optional SMBA use reviewed AF4.
 * \note Clock stretching is always enabled. irq_latency_us includes IRQ entry
 *       and all nonwaiting work through PEC positioning; PEC requires a bound
 *       before the following byte's ACK window. It is declared integration
 *       input, never a measured HIL claim. There is no DMA or OS callback.
 */
typedef struct {
    I2C_TypeDef* registers;
    RCC_TypeDef* rcc;
    const nx_stm32_platform_pin_config_t* pins;
    size_t pin_count;
    uint32_t clock_hz;
    uint32_t maximum_hz;
    uint32_t irq_latency_us;
    uint16_t own_address;
    uint16_t second_address;
    uint8_t priority;
    bool ten_bit;
    bool smbus;
    bool pec;
    bool alert;
} nx_stm32_i2c_target_config_t;

typedef struct nx_stm32_i2c_target_cold nx_stm32_i2c_target_cold_t;

/** \brief One request and bounded IRQ facts, never a runtime controller pool.
 */
typedef struct {
    const nx_stm32_i2c_target_config_t* config;
    nx_stm32_i2c_target_cold_t* cold;
    nx_i2c_target_request_t* active;
    uint64_t generation;
    uint64_t time_epoch;
    size_t received;
    size_t loaded;
    size_t transmitted;
    uint32_t faults;
    nx_result_t terminal;
    uint16_t matched_address;
    uint8_t phases;
    uint8_t crc;
    bool initialized;
    bool closing;
    bool faulted;
    bool terminal_latched;
    bool transmitter;
    bool pec_started;
    bool pec_received;
    bool ara_started;
} nx_stm32_i2c_target_state_t;

/** \brief Actual attempted effects survive every failed restoration. */
struct nx_stm32_i2c_target_cold {
    const nx_stm32_i2c_target_state_t* owner;
    uint32_t registers[6];
    uint32_t apb;
    uint32_t gpio_clocks;
    uint32_t gpio_mask;
    uint8_t priorities[2];
    uint8_t captured;
    bool owned;
    bool registers_captured;
    bool vectors_captured;
};

extern const nx_i2c_target_ops_t nx_stm32_i2c_target_ops;
/** \brief Readonly admission of one exclusive cold controller and its pads.
 * \return SUCCESS or rejection before new references or register writes.
 * \note Privileged unmasked serialized Thread; no warm foreign-state adoption.
 */
nx_result_t
nx_stm32_i2c_target_admit(const nx_stm32_i2c_target_state_t* state,
                          const nx_stm32_i2c_target_config_t* config,
                          const nx_stm32_i2c_target_cold_t* receipt);
/** \brief Prepare the admitted cold owner, retaining every attempted effect.
 * \note Caller owns reset authority for the selected controller. Gated state
 *       is reset before enabling its clock; enabled foreign activity rejects.
 *       A failed acquire requires close before releasing any static storage.
 */
nx_result_t
nx_stm32_i2c_target_prepare(nx_stm32_i2c_target_state_t* state,
                            const nx_stm32_i2c_target_config_t* config,
                            nx_stm32_i2c_target_cold_t* receipt);
/** \brief Drain and restore exact powered state, pads, priorities and gates.
 * \return SUCCESS after withdrawal, or retained BUSY/IO duty for a retry.
 * \note Caller joins producers/observers first. Generation survives close;
 *       failed cleanup never changes the binding or reclaims borrowed memory.
 */
nx_result_t
nx_stm32_i2c_target_close(nx_stm32_i2c_target_state_t* state,
                          const nx_stm32_i2c_target_config_t* config,
                          nx_stm32_i2c_target_cold_t* receipt);
/** \brief Both EV and ER vectors dispatch bounded work to this exact owner.
 * \note IRQ transfers bytes and latches facts; only Thread service settles.
 */
void nx_stm32_i2c_target_irq(nx_stm32_i2c_target_state_t* state);
#ifdef __cplusplus
}
#endif
#endif /* NX_STM32F407_I2C_TARGET_H */
