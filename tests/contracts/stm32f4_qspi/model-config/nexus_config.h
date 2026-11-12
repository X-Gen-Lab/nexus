/**
 * \file            nexus_config.h
 * \brief           Explicit F479 HSI16 plan for the Host QSPI MMIO model
 * \author          Nexus Team
 */
#ifndef NEXUS_QSPI_MODEL_CONFIG_H
#define NEXUS_QSPI_MODEL_CONFIG_H
#define NEXUS_IRQ_KERNEL_POLICY      0U
#define NEXUS_CPU_EXTERNAL_IRQ_COUNT 93U
#define NEXUS_IRQ_PRIORITY_BITS      4U
#define NEXUS_CORE_HZ                16000000U
#define NEXUS_CLOCK_PLLCFGR          0x04003010U
#define NEXUS_CLOCK_SOURCE_HSE       0U
#define NEXUS_CLOCK_PLL_ENABLED      0U
#define NEXUS_CLOCK_INPUT_HZ         16000000U
#define NEXUS_CLOCK_CFGR             0U
#define NEXUS_CLOCK_SW               0U
#define NEXUS_CLOCK_AHB_DIV          1U
#define NEXUS_CLOCK_CSS              0U
#define NEXUS_FLASH_WAIT_STATES      0U
#endif
