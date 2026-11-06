/**
 * \file            f407_sram_boot_model.h
 * \brief           External reset CPU and MMIO ports for actual SRAM bootstrap
 * \author          Nexus Team
 */
#ifndef NX_F407_SRAM_BOOT_MODEL_H
#define NX_F407_SRAM_BOOT_MODEL_H
#include "stm32_official_model.h"
#include "stm32f407_sram_boot.h"
#include "stm32f4xx_ll_fsmc.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uintptr_t msp;
    uint32_t device_id;
    uint32_t flash_kib;
    uint32_t control;
    uint32_t ipsr;
    uint32_t primask;
    uint32_t basepri;
    uint32_t faultmask;
    uint32_t writes;
    uint32_t barriers;
    uint32_t fsmc_gate_reads;
    uint32_t transient_gate_read;
    bool refuse_gpio_gate;
    bool refuse_fsmc_gate;
    bool refuse_bank;
} nx_f407_sram_boot_model_t;
extern nx_f407_sram_boot_model_t g_f407_sram_boot_model;
uint32_t nx_f407_sram_boot_read(volatile uint32_t* reg);
void nx_f407_sram_boot_write(volatile uint32_t* reg, uint32_t value);
void nx_f407_sram_boot_barrier(void);
#ifdef __cplusplus
}
#endif
/* Keep actual register layouts; only the ASan-shadow-gap MMIO base relocates.
 */
#undef FSMC_Bank1
#undef FSMC_Bank1E
#if defined(NX_SRAM_SYSTEM_TEST)
#define NX_SRAM_MODEL_FSMC_BASE UINT32_C(0x61000000)
#else
#define NX_SRAM_MODEL_FSMC_BASE UINT32_C(0x60000000)
#endif
#define FSMC_Bank1 ((FSMC_Bank1_TypeDef*)(uintptr_t)NX_SRAM_MODEL_FSMC_BASE)
#define FSMC_Bank1E                                                            \
    ((FSMC_Bank1E_TypeDef*)(uintptr_t)(NX_SRAM_MODEL_FSMC_BASE +               \
                                       UINT32_C(0x104)))
/* Keep CPU register layouts inside host sanitizer-addressable low memory. */
#undef NVIC
#undef SCB
#undef SysTick
#define NX_SRAM_MODEL_SCS_BASE UINT32_C(0x62000000)
#define NVIC                   ((NVIC_Type*)(uintptr_t)(NX_SRAM_MODEL_SCS_BASE + UINT32_C(0x100)))
#define SCB                    ((SCB_Type*)(uintptr_t)(NX_SRAM_MODEL_SCS_BASE + UINT32_C(0xd00)))
#define SysTick                                                                \
    ((SysTick_Type*)(uintptr_t)(NX_SRAM_MODEL_SCS_BASE + UINT32_C(0x10)))
#undef READ_REG
#undef WRITE_REG
#undef MODIFY_REG
#undef SET_BIT
#undef CLEAR_BIT
#define READ_REG(reg)         nx_f407_sram_boot_read(&(reg))
#define WRITE_REG(reg, value) nx_f407_sram_boot_write(&(reg), (uint32_t)(value))
#define MODIFY_REG(reg, clear, set)                                            \
    WRITE_REG((reg), (READ_REG(reg) & ~(uint32_t)(clear)) | (uint32_t)(set))
#define SET_BIT(reg, bits)   MODIFY_REG((reg), 0U, (bits))
#define CLEAR_BIT(reg, bits) MODIFY_REG((reg), (bits), 0U)
#undef __FSMC_NORSRAM_DISABLE
#define __FSMC_NORSRAM_DISABLE(device, bank)                                   \
    CLEAR_BIT((device)->BTCR[(bank)], (uint32_t)FSMC_BCR1_MBKEN)
#define NX_SRAM_BOOT_READ(p)         nx_f407_sram_boot_read(p)
#define NX_SRAM_BOOT_WRITE(p, value) nx_f407_sram_boot_write((p), (value))
#define NX_SRAM_BOOT_BARRIER()       nx_f407_sram_boot_barrier()
#define NX_SRAM_BOOT_MSP()           g_f407_sram_boot_model.msp
#define NX_SRAM_BOOT_CONTROL()       g_f407_sram_boot_model.control
#define NX_SRAM_BOOT_IPSR()          g_f407_sram_boot_model.ipsr
#define NX_SRAM_BOOT_PRIMASK()       g_f407_sram_boot_model.primask
#define NX_SRAM_BOOT_BASEPRI()       g_f407_sram_boot_model.basepri
#define NX_SRAM_BOOT_FAULTMASK()     g_f407_sram_boot_model.faultmask
#if defined(NX_SRAM_SYSTEM_TEST)
#ifdef __cplusplus
extern "C" {
#endif
void nx_f407_sram_boot_failure(uint32_t reason);
#ifdef __cplusplus
}
#endif
#define NX_STM32_BOOT_FAILURE(reason) nx_f407_sram_boot_failure(reason)
#endif
#define NX_SRAM_BOOT_DEVICE_ID() g_f407_sram_boot_model.device_id
#define NX_SRAM_BOOT_FLASH_KIB() g_f407_sram_boot_model.flash_kib
#endif
