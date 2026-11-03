/**
 * \file            application.h
 * \brief           Generic selected RX profile consumption without supplier
 * layout \author          Nexus Team \version         1.0.0 \date 2026-11-02
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef SDK_LOANED_APPLICATION_H
#define SDK_LOANED_APPLICATION_H
#include "nexus/network/lwip_loaned_rx.h"
/** \brief One serialized link-only lifecycle; caller owns all storage. */
nx_result_t loaned_api_run(nx_lwip_adapter_t* adapter,
                           const nx_lwip_loaned_config_t* config);
#endif
