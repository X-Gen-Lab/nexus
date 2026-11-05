/**
 * \file            h563_i3c_port_model.h
 * \brief           Host-only I3C FIFO, command and W1C register boundary
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_H563_I3C_PORT_MODEL_H
#define NEXUS_TEST_H563_I3C_PORT_MODEL_H
#include "stm32h5xx.h"
#ifdef __cplusplus
extern "C" {
#endif
/** \brief Consume modeled FIFO bytes and observe changing hardware flags. */
uint32_t nx_h563_i3c_model_read(const volatile uint32_t* address);
/** \brief Record commands/FIFO bytes and implement event write-one clearing. */
void nx_h563_i3c_model_write(volatile uint32_t* address, uint32_t value);
#ifdef __cplusplus
}
#endif
#undef READ_REG
#undef WRITE_REG
#undef READ_BIT
#undef SET_BIT
#undef CLEAR_BIT
#undef MODIFY_REG
#define READ_REG(reg)         nx_h563_i3c_model_read(&(reg))
#define WRITE_REG(reg, value) nx_h563_i3c_model_write(&(reg), (uint32_t)(value))
#define READ_BIT(reg, bits)   (READ_REG(reg) & (uint32_t)(bits))
#define SET_BIT(reg, bits)    WRITE_REG(reg, READ_REG(reg) | (uint32_t)(bits))
#define CLEAR_BIT(reg, bits)  WRITE_REG(reg, READ_REG(reg) & ~(uint32_t)(bits))
#define MODIFY_REG(reg, clear, set)                                            \
    WRITE_REG(reg, (READ_REG(reg) & ~(uint32_t)(clear)) | (uint32_t)(set))
#endif /* NEXUS_TEST_H563_I3C_PORT_MODEL_H */
