/**
 * \file            u575_memory_model.h
 * \brief           U575 reset memory external MMIO boundary
 * \author          Nexus Team
 */
#ifndef NX_U575_MEMORY_MODEL_H
#define NX_U575_MEMORY_MODEL_H
#include "stm32u575xx.h"
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    RCC_TypeDef rcc;
    PWR_TypeDef power;
    FLASH_TypeDef flash;
    ICACHE_TypeDef icache;
    DCACHE_TypeDef dcache;
    uint16_t flash_kib;
    volatile uint32_t* refused;
    bool refuse_sram4;
    bool refuse_dma;
    void (*dma_enable_observer)(void* context);
    void* observer_context;
    uint32_t writes;
    uint32_t unclocked_power_reads;
} nx_u575_memory_model_t;
extern nx_u575_memory_model_t g_u575_memory_model;
uint32_t nx_u575_memory_read(volatile uint32_t* reg);
void nx_u575_memory_write(volatile uint32_t* reg, uint32_t value);
#define NEXUS_U575_MEMORY_MODEL          1
#define NX_U575_MEMORY_RCC               (&g_u575_memory_model.rcc)
#define NX_U575_MEMORY_PWR               (&g_u575_memory_model.power)
#define NX_U575_MEMORY_FLASH             (&g_u575_memory_model.flash)
#define NX_U575_MEMORY_ICACHE            (&g_u575_memory_model.icache)
#define NX_U575_MEMORY_DCACHE            (&g_u575_memory_model.dcache)
#define NX_U575_MEMORY_FLASH_KIB()       (g_u575_memory_model.flash_kib)
#define NX_U575_MEMORY_READ(reg)         nx_u575_memory_read(reg)
#define NX_U575_MEMORY_WRITE(reg, value) nx_u575_memory_write(reg, value)
#ifdef __cplusplus
}
#endif
#endif
