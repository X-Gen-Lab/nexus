/**
 * \file            nexus_config.h
 * \brief           Explicit host-only CPU/IRQ boundary for provider isolation
 * \author          Nexus Team
 * \note            This fixture does not select a board, factory or device
 *                  graph. Real firmware receives its configuration from TOML.
 */
#ifndef NEXUS_MULTI_CONTROLLER_TEST_CONFIG_H
#define NEXUS_MULTI_CONTROLLER_TEST_CONFIG_H

#define NEXUS_IRQ_KERNEL_POLICY    2U
#define NEXUS_IRQ_PRIORITY_BITS    4U
#define NEXUS_IRQ_SYSCALL_PRIORITY 5U
#if defined(NX_I2C_MULTI_STM32)
#define NEXUS_CPU_EXTERNAL_IRQ_COUNT 82U
#else
#define NEXUS_CPU_EXTERNAL_IRQ_COUNT 91U
#endif

#endif
