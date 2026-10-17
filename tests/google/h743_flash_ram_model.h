/**
 * \file            h743_flash_ram_model.h
 * \brief           External CPU/MMIO ports for the actual RAM pulse leaf
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_RAM_MODEL_H
#define NX_H743_FLASH_RAM_MODEL_H
#include "h743_peripheral_model.h"
#include "stm32h743_flash_ram.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_h743_flash_ram_model_primask(void);
uint32_t nx_h743_flash_ram_model_control(void);
uint32_t nx_h743_flash_ram_model_ipsr(void);
uint32_t nx_h743_flash_ram_model_basepri(void);
uint32_t nx_h743_flash_ram_model_faultmask(void);
uintptr_t nx_h743_flash_ram_model_sp(void);
uint32_t nx_h743_flash_ram_model_vtor(
    const nx_stm32h743_flash_ram_workspace_t* execution);
void nx_h743_flash_ram_model_poll(
    nx_stm32h743_flash_ram_workspace_t* execution);
void nx_h743_flash_ram_model_barrier(void);
void nx_h743_flash_ram_model_terminal_observe(
    nx_stm32h743_flash_ram_workspace_t* execution);
uint32_t nx_h743_flash_ram_model_read(volatile uint32_t* reg);
void nx_h743_flash_ram_model_write(volatile uint32_t* reg, uint32_t value);
void nx_h743_flash_ram_model_store(volatile uint32_t* address, uint32_t value);
#define NX_H743_FLASH_RAM_MODEL       1
#define NX_H743_FLASH_RAM_PRIMASK()   nx_h743_flash_ram_model_primask()
#define NX_H743_FLASH_RAM_CONTROL()   nx_h743_flash_ram_model_control()
#define NX_H743_FLASH_RAM_IPSR()      nx_h743_flash_ram_model_ipsr()
#define NX_H743_FLASH_RAM_BASEPRI()   nx_h743_flash_ram_model_basepri()
#define NX_H743_FLASH_RAM_FAULTMASK() nx_h743_flash_ram_model_faultmask()
#define NX_H743_FLASH_RAM_SP()        nx_h743_flash_ram_model_sp()
#define NX_H743_FLASH_RAM_VTOR(execution)                                      \
    nx_h743_flash_ram_model_vtor(execution)
#define NX_H743_FLASH_RAM_POLL(execution)                                      \
    nx_h743_flash_ram_model_poll(execution)
#define NX_H743_FLASH_RAM_BARRIER() nx_h743_flash_ram_model_barrier()
#define NX_H743_FLASH_RAM_MODEL_TERMINAL(execution)                            \
    nx_h743_flash_ram_model_terminal_observe(execution)
#undef NX_H743_READ
#undef NX_H743_WRITE
#undef NX_H743_FLASH_WRITE32
#define NX_H743_READ(reg)         nx_h743_flash_ram_model_read(reg)
#define NX_H743_WRITE(reg, value) nx_h743_flash_ram_model_write((reg), (value))
#define NX_H743_FLASH_WRITE32(address, value)                                  \
    nx_h743_flash_ram_model_store((address), (value))
#ifdef __cplusplus
}
#endif
#endif
