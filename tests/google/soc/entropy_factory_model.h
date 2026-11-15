/**
 * \file            entropy_factory_model.h
 * \brief           External RCC, RNG, CPU and Platform clock boundaries
 * \author          Nexus Team
 */
#ifndef NEXUS_TEST_ENTROPY_FACTORY_MODEL_H
#define NEXUS_TEST_ENTROPY_FACTORY_MODEL_H
#include "entropy_model.h"
#include "stm32_official_model.h"
#define NX_PLATFORM_REGISTER_WRITE(reg, value)                                 \
    nx_entropy_model_write(&(reg), (uint32_t)(value))
#endif
