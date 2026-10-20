/**
 * \file            flash_program_model.h
 * \brief           External STM32 Flash status and pulse observations
 * \author          Nexus Team
 */
#ifndef NEXUS_FLASH_PROGRAM_MODEL_H
#define NEXUS_FLASH_PROGRAM_MODEL_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void nx_flash_program_model_poll(unsigned kind, void* context);
uint32_t nx_flash_program_model_read(const volatile uint32_t* reg);
void nx_flash_program_model_write(volatile uint32_t* reg, uint32_t value);
void nx_flash_program_model_store(volatile uint32_t* address, uint32_t value);
bool nx_flash_program_model_irq_idle(void);
bool nx_flash_program_model_irq_clear(void);
bool nx_flash_program_model_irq_active(void);
bool nx_flash_program_model_privileged(void);
#ifdef __cplusplus
}
#endif
#define nx_arch_is_privileged nx_flash_program_model_privileged
#if defined(NX_FLASH_PROGRAM_STM32)
#define NX_STM32_IO_POLL(kind, context)                                        \
    nx_flash_program_model_poll((kind), (context))
#define NX_STM32_CLEAR_FLAGS(reg, mask)                                        \
    nx_flash_program_model_write(&(reg), (mask))
#define NX_STM32_FLASH_READ(reg) nx_flash_program_model_read((reg))
#define NX_STM32_FLASH_WRITE(reg, value)                                       \
    nx_flash_program_model_write((reg), (value))
#define NX_STM32_FLASH_STORE(address, value)                                   \
    nx_flash_program_model_store((address), (value))
#define NX_STM32_FLASH_IRQ_IDLE()   nx_flash_program_model_irq_idle()
#define NX_STM32_FLASH_IRQ_CLEAR()  nx_flash_program_model_irq_clear()
#define NX_STM32_FLASH_IRQ_ACTIVE() nx_flash_program_model_irq_active()
#else
#define NX_GD32_FLASH_READ(reg) nx_flash_program_model_read((reg))
#define NX_GD32_FLASH_WRITE(reg, value)                                        \
    nx_flash_program_model_write((reg), (value))
#define NX_GD32_FLASH_IRQ_IDLE()   nx_flash_program_model_irq_idle()
#define NX_GD32_FLASH_IRQ_CLEAR()  nx_flash_program_model_irq_clear()
#define NX_GD32_FLASH_IRQ_ACTIVE() nx_flash_program_model_irq_active()
#endif
#endif
