/**
 * \file            mpu_port_model.h
 * \brief           Register boundaries for exact production MPU port bodies
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_MPU_PORT_MODEL_H
#define NEXUS_TEST_MPU_PORT_MODEL_H

#include <stdint.h>

enum {
    NX_MPU_REG_TYPE,
    NX_MPU_REG_BASE,
    NX_MPU_REG_ATTRIBUTES,
    NX_MPU_REG_CONTROL,
    NX_MPU_REG_FAULT,
    NX_MPU_REG_PENDSV,
    NX_MPU_REG_COUNT
};

volatile uint32_t* nx_mpu_model_register(unsigned index);
uint32_t nx_mpu_model_link_address(const void* symbol);
uint8_t nx_mpu_model_opcode(uint32_t pc);
void nx_mpu_model_barrier(void);
void nx_mpu_model_setup(void);
void nx_mpu_model_dispatch(uint32_t* frame, uint32_t return_state);

#endif /* NEXUS_TEST_MPU_PORT_MODEL_H */
