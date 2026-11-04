/**
 * \file            registers.h
 *
 * \brief           Reviewed current-domain Cortex-M register address facts.
 *
 * \author          Nexus Team
 *
 * \note            CMSIS-Core 6.4.0 is the independent layout reference in
 *                  arch_cmsis_layout_probe.c. These addresses imply neither
 *                  presence nor permission. CPU facts and context admission
 *                  remain mandatory before any access; no Non-secure alias
 *                  or runtime register discovery is provided here.
 */
#ifndef NEXUS_ARCH_CORTEX_M_REGISTERS_H
#define NEXUS_ARCH_CORTEX_M_REGISTERS_H
#include <stdint.h>

#define NX_ARCH_REG_DWT_CTRL  ((uintptr_t)0xe0001000U)
#define NX_ARCH_REG_DWT_COUNT ((uintptr_t)0xe0001004U)
#define NX_ARCH_REG_CPUID     ((uintptr_t)0xe000ed00U)
#define NX_ARCH_REG_SCR       ((uintptr_t)0xe000ed10U)
#define NX_ARCH_REG_CCR       ((uintptr_t)0xe000ed14U)
#define NX_ARCH_REG_CLIDR     ((uintptr_t)0xe000ed78U)
#define NX_ARCH_REG_CCSIDR    ((uintptr_t)0xe000ed80U)
#define NX_ARCH_REG_CSSELR    ((uintptr_t)0xe000ed84U)
#define NX_ARCH_REG_MPU_TYPE  ((uintptr_t)0xe000ed90U)
#define NX_ARCH_REG_MPU_CTRL  (NX_ARCH_REG_MPU_TYPE + 4U)
#define NX_ARCH_REG_MPU_RNR   (NX_ARCH_REG_MPU_TYPE + 8U)
#define NX_ARCH_REG_MPU_RBAR  (NX_ARCH_REG_MPU_TYPE + 12U)
#define NX_ARCH_REG_MPU_ATTR  (NX_ARCH_REG_MPU_TYPE + 16U)
#define NX_ARCH_REG_MPU_MAIR  (NX_ARCH_REG_MPU_TYPE + 48U)
#define NX_ARCH_REG_SAU_CTRL  ((uintptr_t)0xe000edd0U)
#define NX_ARCH_REG_SAU_TYPE  (NX_ARCH_REG_SAU_CTRL + 4U)
#define NX_ARCH_REG_SAU_RNR   (NX_ARCH_REG_SAU_CTRL + 8U)
#define NX_ARCH_REG_SAU_RBAR  (NX_ARCH_REG_SAU_CTRL + 12U)
#define NX_ARCH_REG_SAU_RLAR  (NX_ARCH_REG_SAU_CTRL + 16U)
#define NX_ARCH_REG_DEMCR     ((uintptr_t)0xe000edfcU)
#define NX_ARCH_REG_ICIALLU   ((uintptr_t)0xe000ef50U)
#define NX_ARCH_REG_ICIMVAU   ((uintptr_t)0xe000ef58U)
#define NX_ARCH_REG_DCIMVAC   ((uintptr_t)0xe000ef5cU)
#define NX_ARCH_REG_DCISW     ((uintptr_t)0xe000ef60U)
#define NX_ARCH_REG_DCCMVAC   ((uintptr_t)0xe000ef68U)
#define NX_ARCH_REG_DCCIMVAC  ((uintptr_t)0xe000ef70U)
#define NX_ARCH_REG_DCCISW    ((uintptr_t)0xe000ef74U)

#define NX_ARCH_CCR_DC (1U << 16)
#define NX_ARCH_CCR_IC (1U << 17)
#endif
