/**
 * \file            lifecycle_adapter.h
 * \brief           Private C model integration; never a product include
 * \author          Nexus Team
 */
#ifndef NEXUS_REFERENCE_LIFECYCLE_ADAPTER_H
#define NEXUS_REFERENCE_LIFECYCLE_ADAPTER_H
#include "nexus_reference/lifecycle_probe.h"
#include "provider.h"
/** \brief Prepare one private instance; injected IO rejects before effects. */
nx_result_t nx_reference_gpio_prepare(nx_native_gpio_state_t* port,
                                      uint32_t mask, uint32_t initial,
                                      bool output);
/** \brief Stop one initialized view before its generated context is reused. */
nx_result_t nx_reference_gpio_stop(nx_native_gpio_state_t* port,
                                   uint32_t inactive);
#endif
