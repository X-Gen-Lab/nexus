/**
 * \file            clock_profile.h
 * \brief           Shared compile-time agreement for F1 startup and System.
 * \author          Nexus Team
 */
#ifndef NEXUS_STM32F1_CLOCK_PROFILE_H
#define NEXUS_STM32F1_CLOCK_PROFILE_H

#include "nexus_config.h"
#include "stm32f1xx.h"

/* The host model may inherit an unrelated clock source from Native. A real
 * reviewed F1 HSE profile always projects an explicit bypass selection. */
#if defined(NEXUS_CLOCK_HSE_BYPASS)
#if !defined(NEXUS_CLOCK_SOURCE_HSE)
#error "An F1 HSE profile requires the reviewed external source"
#elif NEXUS_CLOCK_HSE_BYPASS == 1 && defined(STM32F103xB)
#define NX_STM32F1_PROFILE_CORE_HZ  72000000U
#define NX_STM32F1_PROFILE_INPUT_HZ 8000000U
#elif NEXUS_CLOCK_HSE_BYPASS == 0 &&                                           \
    (defined(STM32F105xC) || defined(STM32F107xC))
#define NX_STM32F1_PROFILE_CORE_HZ   72000000U
#define NX_STM32F1_PROFILE_INPUT_HZ  25000000U
#define NX_STM32F1_CONNECTIVITY_PLL2 1
#elif NEXUS_CLOCK_HSE_BYPASS == 0 && defined(STM32F101xB)
#define NX_STM32F1_PROFILE_CORE_HZ  36000000U
#define NX_STM32F1_PROFILE_INPUT_HZ 8000000U
#elif NEXUS_CLOCK_HSE_BYPASS == 0 && defined(STM32F102xB)
#define NX_STM32F1_PROFILE_CORE_HZ  48000000U
#define NX_STM32F1_PROFILE_INPUT_HZ 8000000U
#else
#error "No reviewed F1 HSE path for the selected device and input"
#endif
#endif

#endif
