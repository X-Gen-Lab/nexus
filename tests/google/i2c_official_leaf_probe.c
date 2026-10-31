/**
 * \file            i2c_official_leaf_probe.c
 * \brief           Unchanged official I2C inline bodies from host C
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-10-30
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#include "i2c_official_leaf_model.h"

uint8_t nx_i2c_leaf_official_receive(I2C_TypeDef* registers) {
    return LL_I2C_ReceiveData8(registers);
}

void nx_i2c_leaf_official_transmit(I2C_TypeDef* registers, uint8_t value) {
    LL_I2C_TransmitData8(registers, value);
}

#if defined(NX_I2C_LEAF_V1)
void nx_i2c_leaf_official_clear_address(I2C_TypeDef* registers) {
    LL_I2C_ClearFlag_ADDR(registers);
}
#endif
