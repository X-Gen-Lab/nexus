/**
 * \file            h743_flash_ram_closure_model.h
 * \brief           External CPU/MMIO ports for the actual RAM pulse leaf
 * \author          Nexus Team
 */
#ifndef NX_H743_FLASH_RAM_CLOSURE_MODEL_H
#define NX_H743_FLASH_RAM_CLOSURE_MODEL_H
#include "h743_peripheral_model.h"
#include "stm32h743_flash_ram.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t nx_h743_flash_ram_model_primask(void);
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
uint8_t nx_h743_flash_model_read8(const volatile uint8_t* address);
/* Test-only expected interfaces. Actual declarations belong to the private
 * production header only after root executes the intended missing-symbol RED.
 */
nx_result_t nx_stm32h743_flash_ram_initialize(
    nx_stm32h743_flash_state_t* port,
    nx_stm32h743_flash_ram_workspace_t* execution);
nx_result_t
nx_stm32h743_flash_ram_stop(nx_stm32h743_flash_ram_workspace_t* execution);
nx_result_t nx_stm32h743_flash_ram_transaction(
    nx_stm32h743_flash_ram_workspace_t* execution);
nx_result_t
nx_stm32h743_flash_ram_return(nx_stm32h743_flash_ram_workspace_t* execution,
                              uint32_t original, nx_result_t result);
void nx_stm32h743_flash_ram_nmi(void);
void nx_stm32h743_flash_ram_hardfault(void);
nx_stm32h743_flash_ram_workspace_t* nx_h743_flash_ram_closure_current(void);
uint32_t nx_h743_flash_ram_closure_control(void);
uint32_t nx_h743_flash_ram_closure_ipsr(void);
uintptr_t nx_h743_flash_ram_closure_msp(void);
uintptr_t nx_h743_flash_ram_closure_psp(void);
uint32_t nx_h743_flash_ram_closure_basepri(void);
uint32_t nx_h743_flash_ram_closure_faultmask(void);
uint32_t nx_h743_flash_ram_closure_fpccr(void);
uint32_t nx_h743_flash_ram_closure_mpu(void);
uintptr_t nx_h743_flash_ram_closure_symbol(const char* name);
uintptr_t nx_h743_flash_ram_closure_address(const void* object);
uint8_t nx_h743_flash_ram_closure_read8(uintptr_t address);
void nx_h743_flash_ram_closure_write8(uintptr_t address, uint8_t value);
uint32_t nx_h743_flash_ram_closure_read32(uintptr_t address);
nx_result_t
nx_h743_flash_ram_closure_enter(nx_stm32h743_flash_ram_workspace_t* execution);
#define NX_H743_FLASH_RAM_ENTER(execution)                                     \
    nx_h743_flash_ram_closure_enter(execution)
#define NX_H743_FLASH_RAM_CURRENT()     nx_h743_flash_ram_closure_current()
#define NX_H743_FLASH_RAM_CONTROL()     nx_h743_flash_ram_closure_control()
#define NX_H743_FLASH_RAM_IPSR()        nx_h743_flash_ram_closure_ipsr()
#define NX_H743_FLASH_RAM_MSP()         nx_h743_flash_ram_closure_msp()
#define NX_H743_FLASH_RAM_PSP()         nx_h743_flash_ram_closure_psp()
#define NX_H743_FLASH_RAM_BASEPRI()     nx_h743_flash_ram_closure_basepri()
#define NX_H743_FLASH_RAM_FAULTMASK()   nx_h743_flash_ram_closure_faultmask()
#define NX_H743_FLASH_RAM_FPCCR()       nx_h743_flash_ram_closure_fpccr()
#define NX_H743_FLASH_RAM_MPU_CONTROL() nx_h743_flash_ram_closure_mpu()
#define NX_H743_FLASH_RAM_SYMBOL(name)  nx_h743_flash_ram_closure_symbol(#name)
#define NX_H743_FLASH_RAM_ADDRESS(object)                                      \
    nx_h743_flash_ram_closure_address(object)
#define NX_H743_FLASH_RAM_READ8(address)                                       \
    nx_h743_flash_ram_closure_read8(address)
#define NX_H743_FLASH_RAM_WRITE8(address, value)                               \
    nx_h743_flash_ram_closure_write8(address, value)
#define NX_H743_FLASH_RAM_READ32(address)                                      \
    nx_h743_flash_ram_closure_read32(address)
#ifndef NX_H743_FLASH_RAM_BASE_READ
#define NX_H743_FLASH_RAM_BASE_READ(reg) nx_h743_model_read(reg)
#endif
#ifndef NX_H743_FLASH_RAM_BASE_WRITE
#define NX_H743_FLASH_RAM_BASE_WRITE(reg, value) nx_h743_model_write(reg, value)
#endif
#define NX_H743_FLASH_RAM_MODEL     1
#define NX_H743_FLASH_RAM_PRIMASK() nx_h743_flash_ram_model_primask()
#define NX_H743_FLASH_RAM_SP()      nx_h743_flash_ram_model_sp()
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
#undef NX_H743_FLASH_READ8
#define NX_H743_FLASH_READ8(address) nx_h743_flash_model_read8(address)
#define NX_H743_READ(reg)            nx_h743_flash_ram_model_read(reg)
#define NX_H743_WRITE(reg, value)    nx_h743_flash_ram_model_write((reg), (value))
#define NX_H743_FLASH_WRITE32(address, value)                                  \
    nx_h743_flash_ram_model_store((address), (value))
#ifdef __cplusplus
}
#endif
#endif
