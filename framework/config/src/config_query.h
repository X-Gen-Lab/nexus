/**
 * \file            config_query.h
 * \brief           Config Manager Query Internal Interface
 * \author          Nexus Team
 * \version         1.0.0
 * \date            2026-01-14
 *
 * \copyright       Copyright (c) 2026 Nexus Team
 *
 * \details         Internal header for query and enumeration functionality.
 *                  This header is not part of the public API.
 */

#ifndef CONFIG_QUERY_H
#define CONFIG_QUERY_H

#include "config/config_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Note: The query functions (config_exists, config_get_type, config_delete,
 * config_get_count) are implemented in config.c as they are simple wrappers
 * around config_store functions.
 *
 * The iteration functions (config_iterate, config_ns_iterate) are implemented
 * in config_query.c as they require more complex adapter logic.
 */

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_QUERY_H */
