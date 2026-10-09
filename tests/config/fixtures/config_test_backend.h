#ifndef NEXUS_CONFIG_TEST_BACKEND_H
#define NEXUS_CONFIG_TEST_BACKEND_H
#include "config/config_backend.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Test-only error-injection backend. Never part of a firmware target. */
const config_backend_t* config_backend_mock_get(void);
void config_backend_mock_reset(void);
#ifdef __cplusplus
}
#endif
#endif
