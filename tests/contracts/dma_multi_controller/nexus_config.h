/**
 * \file            nexus_config.h
 * \brief           Host-only CPU/IRQ facts for DMA controller isolation
 * \author          Nexus Team
 * \note            No board, product or factory configuration is selected.
 */
#ifndef NEXUS_DMA_MULTI_CONTROLLER_TEST_CONFIG_H
#define NEXUS_DMA_MULTI_CONTROLLER_TEST_CONFIG_H
#define NEXUS_IRQ_KERNEL_POLICY    2U
#define NEXUS_IRQ_PRIORITY_BITS    4U
#define NEXUS_IRQ_SYSCALL_PRIORITY 5U
#if defined(NX_DMA_MULTI_STM32)
#define NEXUS_CPU_EXTERNAL_IRQ_COUNT 82U
#else
#define NEXUS_CPU_EXTERNAL_IRQ_COUNT 91U
#endif
#endif
