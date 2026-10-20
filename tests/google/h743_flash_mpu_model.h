/**
 * \file            h743_flash_mpu_model.h
 * \brief           External banked MPU and CPU ports for the RAM transaction
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_MPU_MODEL_H
#define NX_H743_FLASH_MPU_MODEL_H
#include "h743_peripheral_model.h"
#include "stm32h743_flash_mpu.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_h743_flash_mpu_read(const volatile uint32_t* reg);
void nx_h743_flash_mpu_write(volatile uint32_t* reg, uint32_t value);
MPU_Type* nx_h743_flash_mpu_registers(void);
uintptr_t nx_h743_flash_mpu_pc(void);
uintptr_t nx_h743_flash_mpu_address(const void* object);
void nx_h743_flash_mpu_dsb(void);
void nx_h743_flash_mpu_isb(void);
void nx_h743_flash_mpu_poll(nx_stm32h743_flash_ram_workspace_t* execution);
void nx_h743_flash_mpu_store(volatile uint32_t* address, uint32_t value);
void nx_h743_flash_mpu_terminal(nx_stm32h743_flash_ram_workspace_t* execution,
                                nx_stm32h743_flash_mpu_state_t* guard);
void nx_h743_flash_mpu_model_reset(
    nx_stm32h743_flash_ram_workspace_t* execution);
/* r6 is frozen. Its external MMIO delegates through these new ports only in
 * this additional target; no original test or source is rewritten. */
#define NX_H743_FLASH_RAM_BASE_READ(reg) nx_h743_flash_mpu_read(reg)
#define NX_H743_FLASH_RAM_BASE_WRITE(reg, value)                               \
    nx_h743_flash_mpu_write((reg), (value))
#define NX_H743_FLASH_RAM_EXTERNAL_RESET(execution)                            \
    nx_h743_flash_mpu_model_reset(execution)
#include "h743_flash_ram_closure_model.h"
#define NX_H743_FLASH_MPU_REGISTERS() nx_h743_flash_mpu_registers()
#define NX_H743_FLASH_MPU_PC()        nx_h743_flash_mpu_pc()
#undef NX_H743_FLASH_RAM_ADDRESS
#define NX_H743_FLASH_RAM_ADDRESS(object) nx_h743_flash_mpu_address(object)
#define NX_H743_FLASH_MPU_READ(reg)       nx_h743_flash_mpu_read(reg)
#define NX_H743_FLASH_MPU_WRITE(reg, value)                                    \
    nx_h743_flash_mpu_write((reg), (value))
#define NX_H743_FLASH_MPU_DSB()           nx_h743_flash_mpu_dsb()
#define NX_H743_FLASH_MPU_ISB()           nx_h743_flash_mpu_isb()
#define NX_H743_FLASH_MPU_POLL(execution) nx_h743_flash_mpu_poll(execution)
#undef NX_H743_FLASH_RAM_POLL
#define NX_H743_FLASH_RAM_POLL(execution) nx_h743_flash_mpu_poll(execution)
#define NX_H743_FLASH_MPU_MODEL_TERMINAL(execution, guard)                     \
    nx_h743_flash_mpu_terminal((execution), (guard))
#undef NX_H743_FLASH_WRITE32
#define NX_H743_FLASH_WRITE32(address, value)                                  \
    nx_h743_flash_mpu_store((address), (value))
#ifdef __cplusplus
}
#endif
#endif
