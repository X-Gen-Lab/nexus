/**
 * \file            master_irq.h
 * \brief           Cold receipt for the shared F103RB I2C1 IRQ writer.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_I2C_MASTER_IRQ_H
#define NEXUS_STM32F1_I2C_MASTER_IRQ_H
#include "drivers/i2c/instance.h"
#include "i2c_v1_async.h"

typedef struct {
    nx_stm32f1_i2c_t bus;
    uint8_t before_event;
    uint8_t before_error;
    bool vectors;
} nx_stm32f1_i2c_irq_cold_t;

#ifdef __cplusplus
extern "C" {
#endif
/** \brief Acquire one exact I2C1 owner on PB6/7 and fixed HSI8. */
nx_result_t nx_stm32f1_i2c_irq_open(nx_i2c_v1_state_t* port,
                                    nx_stm32f1_i2c_irq_cold_t* cold);
/** \brief Retire local transfers and restore only owned clock/pads/vectors. */
nx_result_t nx_stm32f1_i2c_irq_close(nx_i2c_v1_state_t* port,
                                     nx_stm32f1_i2c_irq_cold_t* cold);
#ifdef __cplusplus
}
#endif
#endif /* NEXUS_STM32F1_I2C_MASTER_IRQ_H */
