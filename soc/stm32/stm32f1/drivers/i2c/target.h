/**
 * \file            target.h
 * \brief           Private F103 I2C1 finite target owner.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_I2C_TARGET_H
#define NEXUS_STM32F1_I2C_TARGET_H

#include "instance.h"
#include "nexus/io/i2c_target.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief One software target with private cold receipts and one request. */
typedef struct {
    nx_stm32f1_i2c_t bus;
    nx_i2c_target_request_t* active;
    uint64_t generation;
    uint32_t faults;
    uint16_t received;
    uint16_t count;
    uint8_t address;
    uint8_t prior_event_priority;
    uint8_t prior_error_priority;
    uint8_t phases;
    bool vectors_owned;
    bool live;
    bool closing;
    nx_result_t terminal;
} nx_stm32f1_i2c_target_t;

extern const nx_i2c_target_ops_t nx_stm32f1_i2c_target_ops;

/** \brief Acquire the default I2C1 route under the reviewed HSI8 profile. */
nx_result_t nx_stm32f1_i2c_target_open(nx_stm32f1_i2c_target_t* state,
                                       uint32_t address);
/** \brief Withdraw interrupts and requests before releasing the bus receipt. */
nx_result_t nx_stm32f1_i2c_target_close(nx_stm32f1_i2c_target_t* state);
/** \brief Service the common exact I2C1 event or error vector. */
void nx_stm32f1_i2c_target_irq(nx_stm32f1_i2c_target_t* state);

#ifdef __cplusplus
}
#endif
#endif
