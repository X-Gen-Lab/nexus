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
#define NEXUS_CLOCK_SOURCE_HSE       0U
#define NEXUS_CLOCK_PLL_ENABLED      0U
#define NEXUS_CLOCK_INPUT_HZ         16000000U
#define NEXUS_CLOCK_PLLM             16U
#define NEXUS_CLOCK_PLLN             192U
#define NEXUS_CLOCK_PLLP             2U
#define NEXUS_CLOCK_PLLQ             4U
#define NEXUS_CLOCK_SYSCLK_PLL       0U
#define NEXUS_CLOCK_AHB_DIV          1U
#define NEXUS_CLOCK_APB1_DIV         1U
#define NEXUS_CLOCK_APB2_DIV         1U
#define NEXUS_CLOCK_ADC_DIV          4U
#define NEXUS_CLOCK_CSS              0U
#define NEXUS_FLASH_WAIT_STATES      0U
#define NEXUS_APB1_HZ                16000000U
#define NEXUS_APB2_HZ                16000000U
#define NEXUS_APB1_TIMER_HZ          16000000U
#define NEXUS_APB2_TIMER_HZ          16000000U
#define NEXUS_ADC_HZ                 4000000U
#define NEXUS_PLL48_HZ               0U
#endif
