/**
 * \file            internal.h
 * \brief           Private Host configuration ownership and diagnostics
 * \author          Nexus Team
 * \version         0.1.0
 * \date            2026-11-07
 * \copyright       Copyright (c) 2026 Nexus Team
 */
#ifndef NXCONFIG_INTERNAL_H
#define NXCONFIG_INTERNAL_H

#include "nxconfig/nxconfig.h"
#include <cJSON.h>
#include <toml.h>

#define NXCONFIG_INPUT_COUNT 5U
#define NXCONFIG_MAX_INPUT   UINT32_C(4194304)

typedef struct {
    char* path;
    char* bytes;
    size_t size;
    char sha256[65];
} nxconfig_input_t;

struct nxconfig_assembly {
    nxconfig_platform_t platform;
    nxconfig_controller_t* controllers;
    size_t controller_count;
    nxconfig_spi_endpoint_t* endpoints;
    size_t endpoint_count;
    nxconfig_input_t inputs[NXCONFIG_INPUT_COUNT];
    size_t input_count;
    toml_table_t* authored;
    cJSON* board;
    cJSON* soc;
    cJSON* routes;
    cJSON* devices;
};

nxconfig_result_t nxconfig_error(nxconfig_diagnostic_t* diagnostic,
                                 nxconfig_result_t code, const char* source,
                                 const char* context, const char* format, ...);
void nxconfig_sha256(const void* data, size_t size, char output[65]);
bool nxconfig_inputs_unchanged(const nxconfig_assembly_t* assembly);
void nxconfig_identifier(const char* input, char* output, bool uppercase);

#endif /* NXCONFIG_INTERNAL_H */
